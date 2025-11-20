#include "ddp_tx.hpp"
#include "esp_log.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

static const char* TAG = "ddp";

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

// ------------------------------------------------------------
// Send test DDP frame (simple rainbow animation)
// ------------------------------------------------------------
bool ddp_send_test(const std::string& host, uint16_t port, uint16_t pixels)
{
    if (pixels == 0) pixels = 120;  // default

    // --- resolve hostname ---
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

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create UDP socket");
        freeaddrinfo(res);
        return false;
    }

    // --- prepare DDP packet ---
    const int bytes = pixels * 3;
    std::vector<uint8_t> buf(sizeof(DDPHeader) + bytes);
    auto* h = reinterpret_cast<DDPHeader*>(buf.data());
    h->flags = 0x41;
    static uint8_t seq = 0;
    h->seq = seq++;
    h->data_type = htons(0x0000);
    h->channel = htonl(1);
    h->data_offset = htonl(0);
    h->data_len = htons(bytes);
    h->unused = 0;

    // --- generate rainbow animation ---
    static float t = 0.0f;
    t += 0.05f;
    uint8_t* p = buf.data() + sizeof(DDPHeader);
    for (int i = 0; i < pixels; i++) {
        float x = (i / (float)pixels) * 6.2831f + t;
        uint8_t r = (uint8_t)((sinf(x) * 0.5f + 0.5f) * 255);
        uint8_t g = (uint8_t)((sinf(x + 2.094f) * 0.5f + 0.5f) * 255);
        uint8_t b = (uint8_t)((sinf(x + 4.188f) * 0.5f + 0.5f) * 255);
        *p++ = r;
        *p++ = g;
        *p++ = b;
    }

    // --- send frame ---
    int sent = sendto(sock, buf.data(), buf.size(), 0, res->ai_addr, res->ai_addrlen);
    if (sent < 0) {
        ESP_LOGE(TAG, "DDP send error -> %s:%u", host.c_str(), port);
        close(sock);
        freeaddrinfo(res);
        return false;
    }

    close(sock);
    freeaddrinfo(res);
    ESP_LOGI(TAG, "DDP frame sent (%d bytes) -> %s:%d", sent, host.c_str(), port);
    return sent == (int)buf.size();
}

void ddp_tx_init(const MqttConfig& cfg) {
    if (!cfg.configured || cfg.ddp_target.empty()) {
        ESP_LOGI(TAG, "DDP disabled (no target configured)");
        return;
    }
    ESP_LOGI(TAG, "DDP sanity check -> %s:%u", cfg.ddp_target.c_str(), cfg.ddp_port);
    ddp_send_test(cfg.ddp_target, cfg.ddp_port, 120);
}

