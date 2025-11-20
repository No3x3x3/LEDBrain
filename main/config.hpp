#pragma once
#include "esp_err.h"
#include "led_engine/types.hpp"
#include <string>
#include <vector>

struct NetworkConfig {
  std::string hostname{"ledbrain"};
  bool use_dhcp{true};
  std::string static_ip{};
  std::string netmask{};
  std::string gateway{};
  std::string dns{};
};

struct MqttConfig {
  bool configured{false};
  std::string host{};
  uint16_t port{1883};
  std::string username{};
  std::string password{};
  std::string ddp_target{"192.168.1.100"};
  uint16_t ddp_port{4048};
};

struct WledDeviceConfig {
  std::string id{};
  std::string name{};
  std::string address{};
  uint16_t leds{0};
  uint16_t segments{1};
  bool active{true};
  bool auto_discovered{false};
};

struct AppConfig {
  std::string lang{"pl"};
  NetworkConfig network{};
  MqttConfig mqtt{};
  bool autostart{false};
  std::vector<WledDeviceConfig> wled_devices{};
  LedHardwareConfig led_engine{};
  uint32_t schema_version{5};
};

esp_err_t config_service_init();
esp_err_t config_load(AppConfig &cfg);
esp_err_t config_save(const AppConfig &cfg);
void config_reset_defaults(AppConfig &cfg);
esp_err_t config_apply_json(AppConfig &cfg, const char* data, size_t len);
std::string config_to_json(const AppConfig &cfg);
