#pragma once
#include "led_engine/pinout.hpp"
#include "led_engine/types.hpp"
#include "esp_err.h"
#include <mutex>
#include <vector>

struct LedEngineStatus {
  bool initialized{false};
  uint16_t target_fps{0};
  size_t segment_count{0};
  uint32_t global_current_ma{0};
  uint8_t global_brightness{255};
  bool enabled{true};
};

class LedEngineRuntime {
public:
  esp_err_t init(const LedHardwareConfig& cfg);
  esp_err_t update_config(const LedHardwareConfig& cfg);
  LedEngineStatus status() const;
  esp_err_t set_enabled(bool enabled);
  esp_err_t set_brightness(uint8_t brightness);
  uint8_t brightness() const;
  bool enabled() const;

private:
  esp_err_t configure_driver(const LedHardwareConfig& cfg);
  void log_segment(const LedSegmentConfig& seg) const;

  LedHardwareConfig cfg_{};
  bool initialized_{false};
  bool enabled_{true};
  uint8_t brightness_{255};
  mutable std::mutex mutex_;
};
