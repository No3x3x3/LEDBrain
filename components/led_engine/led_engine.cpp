#include "led_engine.hpp"
#include "led_engine/audio_pipeline.hpp"
#include "led_engine/pinout.hpp"
#include "esp_log.h"
#include <algorithm>

namespace {

const char* driver_name(LedDriverType type) {
  switch (type) {
    case LedDriverType::EspRmt:
      return "esp_rmt";
    case LedDriverType::NeoPixelBus:
      return "neopixelbus";
    case LedDriverType::External:
      return "external";
  }
  return "esp_rmt";
}

}  // namespace

static const char* TAG = "led-engine";

esp_err_t LedEngineRuntime::init(const LedHardwareConfig& cfg) {
  std::lock_guard<std::mutex> lock(mutex_);
  cfg_ = cfg;
  brightness_ = std::clamp<int>(cfg_.global_brightness, 0, 255);
  cfg_.global_brightness = brightness_;
  enabled_ = true;
  ESP_ERROR_CHECK_WITHOUT_ABORT(led_audio_apply_config(cfg.audio));
  const esp_err_t err = configure_driver(cfg_);
  initialized_ = (err == ESP_OK);
  return err;
}

esp_err_t LedEngineRuntime::update_config(const LedHardwareConfig& cfg) {
  std::lock_guard<std::mutex> lock(mutex_);
  cfg_ = cfg;
  brightness_ = std::clamp<int>(cfg_.global_brightness, 0, 255);
  cfg_.global_brightness = brightness_;
  ESP_ERROR_CHECK_WITHOUT_ABORT(led_audio_apply_config(cfg.audio));
  const esp_err_t err = configure_driver(cfg_);
  initialized_ = (err == ESP_OK);
  return err;
}

LedEngineStatus LedEngineRuntime::status() const {
  std::lock_guard<std::mutex> lock(mutex_);
  LedEngineStatus st{};
  st.initialized = initialized_;
  st.target_fps = cfg_.max_fps;
  st.segment_count = cfg_.segments.size();
  st.global_current_ma = cfg_.global_current_limit_ma;
  st.global_brightness = brightness_;
  st.enabled = enabled_;
  return st;
}

esp_err_t LedEngineRuntime::configure_driver(const LedHardwareConfig& cfg) {
  esp_err_t status = ESP_OK;
  ESP_LOGI(TAG,
           "Configuring LED driver=%s fps=%u outputs=%u dma=%d",
           driver_name(cfg.driver),
           cfg.max_fps,
           cfg.parallel_outputs,
           cfg.enable_dma);

  for (const auto& seg : cfg.segments) {
    if (!led_pin_is_allowed(seg.gpio)) {
      ESP_LOGW(TAG, "Segment %s pin %d nie moze byc uzyty", seg.name.c_str(), seg.gpio);
      status = ESP_ERR_INVALID_ARG;
      continue;
    }
    log_segment(seg);
  }
  return status;
}

esp_err_t LedEngineRuntime::set_enabled(bool enabled) {
  std::lock_guard<std::mutex> lock(mutex_);
  enabled_ = enabled;
  // TODO: wire with real driver when rendering core is implemented
  return ESP_OK;
}

bool LedEngineRuntime::enabled() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return enabled_;
}

esp_err_t LedEngineRuntime::set_brightness(uint8_t brightness) {
  std::lock_guard<std::mutex> lock(mutex_);
  brightness_ = std::clamp<int>(brightness, 0, 255);
  cfg_.global_brightness = brightness_;
  // TODO: push brightness to rendering backend when available
  return ESP_OK;
}

uint8_t LedEngineRuntime::brightness() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return brightness_;
}

void LedEngineRuntime::log_segment(const LedSegmentConfig& seg) const {
  ESP_LOGI(TAG,
           "Segment %s leds=%u gpio=%d rmt=%u matrix=%d current=%u mA",
           seg.name.c_str(),
           seg.led_count,
           seg.gpio,
           seg.rmt_channel,
           seg.matrix_enabled,
           seg.power_limit_ma);
}
