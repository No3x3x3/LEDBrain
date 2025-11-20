#pragma once

#include "config.hpp"
#include <cstdint>
#include <string>
#include <vector>

struct WledDeviceStatus {
  WledDeviceConfig config{};
  bool online{false};
  std::string ip{};
  std::string version{};
  uint64_t last_seen_ms{0};
  bool http_verified{false};
};

void wled_discovery_start(AppConfig& cfg);
void wled_discovery_trigger_scan();
std::vector<WledDeviceStatus> wled_discovery_status();
void wled_discovery_register_manual(const WledDeviceConfig& cfg);
