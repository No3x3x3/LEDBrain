#pragma once

#include "config.hpp"
#include <vector>
#include <cstdint>
#include "lwip/sockets.h"

struct sockaddr_storage;

void ddp_tx_init(const MqttConfig& cfg);
bool ddp_send_test(const std::string& host, uint16_t port, uint16_t pixels);
bool ddp_send_frame(const std::string& host,
                    uint16_t port,
                    const std::vector<uint8_t>& payload,
                    uint32_t channel = 1,
                    uint32_t data_offset = 0,
                    uint8_t seq = 0,
                    bool push_flag = true);
// Optimized version with cached address (for multiple devices)
bool ddp_send_frame_cached(const struct sockaddr_storage* addr,
                           socklen_t addr_len,
                           uint16_t port,
                           const std::vector<uint8_t>& payload,
                           uint32_t channel = 1,
                           uint32_t data_offset = 0,
                           uint8_t seq = 0,
                           bool push_flag = true);
// Send complete frame, automatically splitting into multiple packets if needed (max 1440 bytes per packet)
bool ddp_send_complete_frame(const std::string& host,
                             uint16_t port,
                             const std::vector<uint8_t>& payload,
                             uint32_t channel = 1,
                             uint8_t seq = 0);
bool ddp_send_complete_frame_cached(const struct sockaddr_storage* addr,
                                    socklen_t addr_len,
                                    uint16_t port,
                                    const std::vector<uint8_t>& payload,
                                    uint32_t channel = 1,
                                    uint8_t seq = 0);
// Cache DNS resolution (returns true if cached, false if needs resolution)
bool ddp_cache_resolve(const std::string& host, uint16_t port, struct sockaddr_storage* out_addr, socklen_t* out_addr_len);// Get network statistics (bytes sent via DDP)
void ddp_get_stats(uint64_t* tx_bytes, uint64_t* rx_bytes);
