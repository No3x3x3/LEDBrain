#pragma once

#include "config.hpp"
#include "led_engine.hpp"
#include "wled_discovery.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

class WledEffectsRuntime {
 public:
  esp_err_t start(AppConfig* cfg, LedEngineRuntime* led_runtime);
  void update_config(const AppConfig& cfg);
  void stop();

 private:
  static void task_entry(void* arg);
  void task_loop();
  bool should_run() const;
  bool resolve_device(const std::string& device_id,
                      const std::vector<WledDeviceConfig>& devices,
                      const std::vector<WledDeviceStatus>& status,
                      WledDeviceConfig& out,
                      std::string& ip) const;
  bool render_and_send(const WledEffectBinding& binding,
                       const WledDeviceConfig& device,
                       const std::string& ip,
                       uint32_t frame_idx,
                       uint8_t global_brightness,
                       uint16_t fps);
  std::vector<uint8_t> render_frame(const WledEffectBinding& binding,
                                    uint16_t led_count,
                                    uint32_t frame_idx,
                                    uint8_t global_brightness,
                                    uint16_t fps);
  float apply_envelope(const std::string& key, float input, uint16_t fps, uint16_t attack_ms, uint16_t release_ms);
  std::string envelope_key(const WledEffectBinding& binding) const;

  AppConfig* cfg_ref_{nullptr};
  LedEngineRuntime* led_runtime_{nullptr};
  std::mutex mutex_;
  WledEffectsConfig effects_{};
  std::vector<WledDeviceConfig> devices_{};
  TaskHandle_t task_{nullptr};
  bool running_{false};
  uint8_t seq_{0};
  std::unordered_map<std::string, float> envelope_state_;
};
