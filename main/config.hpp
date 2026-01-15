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
  // WiFi configuration
  std::string wifi_ssid{};
  std::string wifi_password{};
  bool wifi_enabled{false};
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

// LED layout types for preview mapping
enum class LedLayoutType : uint8_t {
  Line = 0,      // Single horizontal line (default)
  Matrix = 1,    // 2D matrix (rows x cols)
  Circle = 2,    // Circular arrangement
  Custom = 3,    // Custom x,y coordinates per LED
};

struct LedLayoutConfig {
  LedLayoutType type{LedLayoutType::Line};
  uint16_t width{0};   // For matrix: columns, for circle: unused
  uint16_t height{0};  // For matrix: rows, for circle: unused
  bool serpentine{false};  // For matrix: alternating row direction
  bool start_corner{0};    // 0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right
  // Custom coordinates stored as JSON string (parsed in frontend)
  std::string custom_map{};
};

struct WledDeviceConfig {
  std::string id{};
  std::string name{};
  std::string address{};
  uint16_t leds{0};
  uint16_t segments{1};
  bool active{true};
  bool auto_discovered{false};
  LedLayoutConfig layout{};  // LED arrangement for preview
};

struct WledEffectBinding {
  std::string device_id{};
  uint16_t segment_index{0};
  bool enabled{true};
  bool ddp{true};
  std::string audio_channel{"mix"};  // mix | left | right
  uint16_t fps{60};  // Per-device FPS (1-120, default 60)
  EffectAssignment effect{};
};

struct WledEffectsConfig {
  uint16_t target_fps{60};
  std::vector<WledEffectBinding> bindings{};
};

struct AppConfig {
  std::string lang{"pl"};
  NetworkConfig network{};
  MqttConfig mqtt{};
  bool autostart{false};
  std::vector<WledDeviceConfig> wled_devices{};
  WledEffectsConfig wled_effects{};
  std::vector<VirtualSegmentConfig> virtual_segments{};
  LedHardwareConfig led_engine{};
  uint32_t schema_version{8};
};

esp_err_t config_service_init();
esp_err_t config_load(AppConfig &cfg);
esp_err_t config_save(const AppConfig &cfg);
void config_reset_defaults(AppConfig &cfg);
esp_err_t config_apply_json(AppConfig &cfg, const char* data, size_t len);
std::string config_to_json(const AppConfig &cfg);
