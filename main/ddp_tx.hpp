#pragma once

#include "config.hpp"
#include <vector>

void ddp_tx_init(const MqttConfig& cfg);
bool ddp_send_test(const std::string& host, uint16_t port, uint16_t pixels);
bool ddp_send_frame(const std::string& host,
                    uint16_t port,
                    const std::vector<uint8_t>& payload,
                    uint32_t channel = 1,
                    uint32_t data_offset = 0,
                    uint8_t seq = 0);
