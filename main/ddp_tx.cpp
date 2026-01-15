#include "ddp_tx.hpp"
#include "esp_log.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "esp_netif.h"
#include "lwip/netif.h"
#include "eth_init.hpp"  // For extern esp_netif_t* netif
#include <cmath>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <cerrno>

static const char* TAG = "ddp";

// Network statistics tracking
static uint64_t s_tx_bytes = 0;
static uint64_t s_rx_bytes = 0;  // For future use (audio receive)

#pragma pack(push, 1)
// DDP Header structure (10 bytes - WLED compatible format)
// WLED uses 10-byte DDP header (without optional timecode)
// Reference: https://kno.wled.ge/interfaces/ddp/
struct DDPHeader {
    uint8_t flags;       // Bit 0: push (1=push, 0=hold), Bits 6-7: version (01=v1)
                         // 0x41 = version(01) + push(1) = 01000001
    uint8_t seq;         // Sequence number (4-bit, 1-15, upper 4 bits unused)
    uint8_t data_type;   // Pixel format: 0x00 = undefined (use device default), typically RGB
    uint8_t dest_id;     // Destination/Output ID (1 = default)
    uint32_t data_offset;// Offset in LED array (big-endian, in bytes)
    uint16_t data_len;   // Payload length in bytes (big-endian)
    // Total: 1 + 1 + 1 + 1 + 4 + 2 = 10 bytes
};
#pragma pack(pop)

namespace {

// Static socket for reuse - avoid creating new socket for each frame
static int s_ddp_sock = -1;

bool ddp_send_frame_internal(const struct sockaddr* addr,
                              socklen_t addr_len,
                              uint16_t port,
                              const uint8_t* payload,
                              size_t bytes,
                              uint32_t channel,
                              uint32_t data_offset,
                              uint8_t seq,
                              bool push_flag) {
    // Reuse socket if possible (much faster than creating new socket each frame)
    if (s_ddp_sock < 0) {
        s_ddp_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (s_ddp_sock < 0) {
            ESP_LOGE(TAG, "Unable to create UDP socket");
            return false;
        }
        // Set socket to non-blocking for faster sends
        int flags = fcntl(s_ddp_sock, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(s_ddp_sock, F_SETFL, flags | O_NONBLOCK);
        }
    }
    
    // Ensure port is set in address (for cached addresses, port might not be set)
    struct sockaddr_storage addr_with_port;
    std::memcpy(&addr_with_port, addr, addr_len);
    if (addr->sa_family == AF_INET) {
        reinterpret_cast<struct sockaddr_in*>(&addr_with_port)->sin_port = htons(port);
    } else if (addr->sa_family == AF_INET6) {
        reinterpret_cast<struct sockaddr_in6*>(&addr_with_port)->sin6_port = htons(port);
    }
    const struct sockaddr* final_addr = reinterpret_cast<const struct sockaddr*>(&addr_with_port);

    std::vector<uint8_t> buf(sizeof(DDPHeader) + bytes);
    auto* h = reinterpret_cast<DDPHeader*>(buf.data());
    
    // Set DDP header fields (10-byte WLED format)
    // Flags: version 1 (0x40) + push flag (0x01) when last packet
    // Same as LedFX: 0x41 = push, 0x40 = no push
    h->flags = push_flag ? 0x41 : 0x40;
    h->seq = seq & 0x0F;  // 4-bit sequence like LedFX
    h->data_type = 0x01;  // 0x01 = RGB, 8 bits per channel (same as LedFX)
    h->dest_id = 1;  // Default destination
    h->data_offset = htonl(data_offset);  // Offset in bytes (big-endian)
    h->data_len = htons(static_cast<uint16_t>(bytes));  // Payload length (big-endian)

    if (bytes > 0 && payload) {
        // Copy RGB data from render buffer to DDP payload
        // Payload format: RGB bytes (R, G, B, R, G, B, ...)
        memcpy(buf.data() + sizeof(DDPHeader), payload, bytes);
    }

    // Send DDP packet with proper port set (use persistent socket)
    int sent = sendto(s_ddp_sock, buf.data(), buf.size(), 0, final_addr, addr_len);
    
    static uint32_t s_send_count = 0;
    static uint32_t s_fail_count = 0;
    s_send_count++;
    
    if (sent < 0) {
        s_fail_count++;
        // On error, close and invalidate socket so it gets recreated next time
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ESP_LOGE(TAG, "DDP send error (errno=%d, %s)", errno, strerror(errno));
            close(s_ddp_sock);
            s_ddp_sock = -1;
        }
        return false;
    }

    // Track bytes sent
    if (sent > 0) {
        s_tx_bytes += static_cast<uint64_t>(sent);
    }
    
    // Log every 60 packets (~1 second at 60fps)
    if (s_send_count % 60 == 0) {
        ESP_LOGI(TAG, "DDP stats: sent=%lu, fails=%lu, bytes=%lu, last_pkt=%d bytes (flags=0x%02X seq=%d type=%d)",
                 (unsigned long)s_send_count, (unsigned long)s_fail_count, 
                 (unsigned long)s_tx_bytes, sent,
                 h->flags, h->seq, h->data_type);
    }

    // Don't close socket - reuse it for next frame
    return sent == static_cast<int>(buf.size());
}

}  // namespace

bool ddp_send_frame(const std::string& host,
                    uint16_t port,
                    const std::vector<uint8_t>& payload,
                    uint32_t channel,
                    uint32_t data_offset,
                    uint8_t seq,
                    bool push_flag) {
    // Resolve hostname
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo* res = nullptr;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int err = getaddrinfo(host.c_str(), port_str, &hints, &res);
    if (err != 0 || !res) {
        ESP_LOGE(TAG, "Failed to resolve %s (err=%d)", host.c_str(), err);
        return false;
    }

    bool ok = ddp_send_frame_internal(res->ai_addr, res->ai_addrlen, port, payload.data(), payload.size(), channel, data_offset, seq, push_flag);
    freeaddrinfo(res);
    return ok;
}

bool ddp_send_frame_cached(const struct sockaddr_storage* addr,
                           socklen_t addr_len,
                           uint16_t port,
                           const std::vector<uint8_t>& payload,
                           uint32_t channel,
                           uint32_t data_offset,
                           uint8_t seq,
                           bool push_flag) {
    return ddp_send_frame_internal(reinterpret_cast<const struct sockaddr*>(addr), addr_len, port, 
                                   payload.data(), payload.size(), channel, data_offset, seq, push_flag);
}

// DDP maximum payload size: 1440 bytes (480 RGB pixels × 3 bytes)
constexpr size_t DDP_MAX_PAYLOAD = 1440;

bool ddp_send_complete_frame(const std::string& host,
                             uint16_t port,
                             const std::vector<uint8_t>& payload,
                             uint32_t channel,
                             uint8_t seq) {
    const size_t total_bytes = payload.size();
    if (total_bytes == 0) {
        return false;
    }
    
    // If payload fits in one packet, send it with push flag
    if (total_bytes <= DDP_MAX_PAYLOAD) {
        return ddp_send_frame(host, port, payload, channel, 0, seq, true);
    }
    
    // Split into multiple packets
    bool all_ok = true;
    uint32_t offset = 0;
    uint8_t current_seq = seq;
    
    while (offset < total_bytes) {
        const size_t chunk_size = std::min<size_t>(DDP_MAX_PAYLOAD, total_bytes - offset);
        std::vector<uint8_t> chunk(payload.begin() + offset, payload.begin() + offset + chunk_size);
        
        // Push flag only on last packet
        const bool is_last = (offset + chunk_size >= total_bytes);
        const bool ok = ddp_send_frame(host, port, chunk, channel, offset, current_seq++, is_last);
        
        if (!ok) {
            all_ok = false;
            ESP_LOGW(TAG, "DDP packet send failed at offset %lu", static_cast<unsigned long>(offset));
        }
        
        offset += chunk_size;
    }
    
    return all_ok;
}

bool ddp_send_complete_frame_cached(const struct sockaddr_storage* addr,
                                    socklen_t addr_len,
                                    uint16_t port,
                                    const std::vector<uint8_t>& payload,
                                    uint32_t channel,
                                    uint8_t seq) {
    const size_t total_bytes = payload.size();
    if (total_bytes == 0) {
        return false;
    }
    
    // If payload fits in one packet, send it with push flag
    if (total_bytes <= DDP_MAX_PAYLOAD) {
        return ddp_send_frame_cached(addr, addr_len, port, payload, channel, 0, seq, true);
    }
    
    // Split into multiple packets
    bool all_ok = true;
    uint32_t offset = 0;
    uint8_t current_seq = seq;
    
    while (offset < total_bytes) {
        const size_t chunk_size = std::min<size_t>(DDP_MAX_PAYLOAD, total_bytes - offset);
        std::vector<uint8_t> chunk(payload.begin() + offset, payload.begin() + offset + chunk_size);
        
        // Push flag only on last packet
        const bool is_last = (offset + chunk_size >= total_bytes);
        const bool ok = ddp_send_frame_cached(addr, addr_len, port, chunk, channel, offset, current_seq++, is_last);
        
        if (!ok) {
            all_ok = false;
            ESP_LOGW(TAG, "DDP packet send failed at offset %lu", static_cast<unsigned long>(offset));
        }
        
        offset += chunk_size;
    }
    
    return all_ok;
}

bool ddp_cache_resolve(const std::string& host, uint16_t port, struct sockaddr_storage* out_addr, socklen_t* out_addr_len) {
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo* res = nullptr;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int err = getaddrinfo(host.c_str(), port_str, &hints, &res);
    if (err != 0 || !res) {
        return false;
    }

    if (res->ai_addrlen <= sizeof(*out_addr)) {
        std::memcpy(out_addr, res->ai_addr, res->ai_addrlen);
        *out_addr_len = res->ai_addrlen;
        
        // Ensure port is explicitly set in cached address
        if (res->ai_family == AF_INET) {
            reinterpret_cast<struct sockaddr_in*>(out_addr)->sin_port = htons(port);
        } else if (res->ai_family == AF_INET6) {
            reinterpret_cast<struct sockaddr_in6*>(out_addr)->sin6_port = htons(port);
        }
        
        freeaddrinfo(res);
        return true;
    }
    
    freeaddrinfo(res);
    return false;
}

void ddp_get_stats(uint64_t* tx_bytes, uint64_t* rx_bytes) {
    if (tx_bytes) *tx_bytes = s_tx_bytes;
    if (rx_bytes) *rx_bytes = s_rx_bytes;
}

// ------------------------------------------------------------
// Send test DDP frame (simple rainbow animation)
// ------------------------------------------------------------
bool ddp_send_test(const std::string& host, uint16_t port, uint16_t pixels)
{
    if (pixels == 0) pixels = 120;  // default

    const int bytes = pixels * 3;
    std::vector<uint8_t> buf(bytes);

    // --- generate rainbow animation ---
    static float t = 0.0f;
    t += 0.05f;
    uint8_t* p = buf.data();
    for (int i = 0; i < pixels; i++) {
        float x = (i / (float)pixels) * 6.2831f + t;
        uint8_t r = (uint8_t)((sinf(x) * 0.5f + 0.5f) * 255);
        uint8_t g = (uint8_t)((sinf(x + 2.094f) * 0.5f + 0.5f) * 255);
        uint8_t b = (uint8_t)((sinf(x + 4.188f) * 0.5f + 0.5f) * 255);
        *p++ = r;
        *p++ = g;
        *p++ = b;
    }

    static uint8_t seq = 0;
    bool ok = ddp_send_complete_frame(host, port, buf, 1, seq++);
    if (ok) {
      ESP_LOGI(TAG, "DDP frame sent (%u bytes) -> %s:%u", static_cast<unsigned>(buf.size()), host.c_str(), port);
    }
    return ok;
}

void ddp_tx_init(const MqttConfig& cfg) {
    if (!cfg.configured || cfg.ddp_target.empty()) {
        ESP_LOGI(TAG, "DDP disabled (no target configured)");
        return;
    }
    ESP_LOGI(TAG, "DDP sanity check -> %s:%u", cfg.ddp_target.c_str(), cfg.ddp_port);
    ddp_send_test(cfg.ddp_target, cfg.ddp_port, 120);
}

