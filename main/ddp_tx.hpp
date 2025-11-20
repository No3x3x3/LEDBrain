#pragma once

#include "config.hpp"

void ddp_tx_init(const MqttConfig& cfg);
bool ddp_send_test(const std::string& host, uint16_t port, uint16_t pixels);
