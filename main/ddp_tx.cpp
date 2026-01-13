#include "ddp_tx.hpp"
#include "esp_log.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include <cmath>
#include <unistd.h>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

static const char* TAG = "ddp";

// Network statistics tracking
static uint64_t s_tx_bytes = 0;
static uint64_t s_rx_bytes = 0;  // For future use (audio receive)

#pragma pack(push, 1)
struct DDPHeader {
    uint8_t flags;       // 0x41 = push + ver
    uint8_t seq;         // 0..255
    uint16_t data_type;  // 0x0000 = raw
    uint32_t channel;    // 1
    uint32_t data_offset;// 0
    uint16_t data_len;   // payload bytes
    uint16_t unused;     // 0
};
#pragma pack(pop)

namespace {

bool ddp_send_frame_internal(const struct sockaddr* addr,
                              socklen_t addr_len,
                              uint16_t port,
                              const uint8_t* payload,
                              size_t bytes,
                              uint32_t channel,
                              uint32_t data_offset,
                              uint8_t seq) {
    int sock = socket(addr->sa_family, SOCK_DGRAM, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create UDP socket");
        return false;
    }

    std::vector<uint8_t> buf(sizeof(DDPHeader) + bytes);
    auto* h = reinterpret_cast<DDPHeader*>(buf.data());
    h->flags = 0x41;
    h->seq = seq;
    h->data_type = htons(0x0000);
    h->channel = htonl(channel);
    h->data_offset = htonl(data_offset);
    h->data_len = htons(static_cast<uint16_t>(bytes));
    h->unused = 0;

    if (bytes > 0 && payload) {
        memcpy(buf.data() + sizeof(DDPHeader), payload, bytes);
    }

    int sent = sendto(sock, buf.data(), buf.size(), 0, addr, addr_len);
    if (sent < 0) {
        ESP_LOGE(TAG, "DDP send error");
        close(sock);
        return false;
    }

    // Track bytes sent
    if (sent > 0) {
        s_tx_bytes += static_cast<uint64_t>(sent);
    }

    close(sock);
    return sent == static_cast<int>(buf.size());
}

}  // namespace

bool ddp_send_frame(const std::string& host,
                    uint16_t port,
                    const std::vector<uint8_t>& payload,
                    uint32_t channel,
                    uint32_t data_offset,
                    uint8_t seq) {
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

    bool ok = ddp_send_frame_internal(res->ai_addr, res->ai_addrlen, port, payload.data(), payload.size(), channel, data_offset, seq);
    freeaddrinfo(res);
    return ok;
}

bool ddp_send_frame_cached(const struct sockaddr_storage* addr,
                           socklen_t addr_len,
                           uint16_t port,
                           const std::vector<uint8_t>& payload,
                           uint32_t channel,
                           uint32_t data_offset,
                           uint8_t seq) {
    return ddp_send_frame_internal(reinterpret_cast<const struct sockaddr*>(addr), addr_len, port, 
                                   payload.data(), payload.size(), channel, data_offset, seq);
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
    bool ok = ddp_send_frame(host, port, buf, 1, 0, seq++);
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

