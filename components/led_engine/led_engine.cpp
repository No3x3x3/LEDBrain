#include "led_engine.hpp"
#include "led_engine/audio_pipeline.hpp"
#include "led_engine/pinout.hpp"
#include "led_engine/rmt_driver.hpp"
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

  // Deinitialize old segments first
  rmt_driver_deinit_all();

  // Initialize RMT driver for each segment
  for (const auto& seg : cfg.segments) {
    if (!led_pin_is_allowed(seg.gpio)) {
      ESP_LOGW(TAG, "Segment %s pin %d nie moze byc uzyty", seg.name.c_str(), seg.gpio);
      status = ESP_ERR_INVALID_ARG;
      continue;
    }
    
    if (cfg.driver == LedDriverType::EspRmt) {
      const esp_err_t rmt_err = rmt_driver_init_segment(seg);
      if (rmt_err != ESP_OK) {
        ESP_LOGW(TAG, "RMT init failed for segment %s: %s", seg.name.c_str(), esp_err_to_name(rmt_err));
        status = rmt_err;
      }
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

esp_err_t LedEngineRuntime::render_frame(const std::vector<uint8_t>& rgb,
                                         const LedSegmentConfig& segment,
                                         size_t start,
                                         size_t length) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!initialized_) {
    ESP_LOGW(TAG, "Render ignored: engine not initialized");
    return ESP_ERR_INVALID_STATE;
  }
  if (!enabled_) {
    ESP_LOGD(TAG, "Render ignored: engine disabled");
    return ESP_OK;
  }
  const size_t max_leds = segment.led_count;
  if (start >= max_leds) {
    ESP_LOGW(TAG,
             "Render ignored: segment %s start %u beyond length %u",
             segment.id.c_str(),
             static_cast<unsigned>(start),
             static_cast<unsigned>(max_leds));
    return ESP_ERR_INVALID_ARG;
  }
  const size_t pixels = std::min(length == 0 ? max_leds - start : length, max_leds - start);
  const size_t expected_bytes = pixels * 3;
  if (rgb.size() < expected_bytes) {
    ESP_LOGW(TAG,
             "Render ignored: frame too small (%u bytes, need %u) for segment %s",
             static_cast<unsigned>(rgb.size()),
             static_cast<unsigned>(expected_bytes),
             segment.id.c_str());
    return ESP_ERR_INVALID_SIZE;
  }

  // Render via RMT driver if configured
  if (cfg_.driver == LedDriverType::EspRmt) {
    const esp_err_t rmt_err = rmt_driver_render(segment, rgb, start, pixels);
    if (rmt_err != ESP_OK) {
      ESP_LOGW(TAG, "RMT render failed for segment %s: %s", segment.id.c_str(), esp_err_to_name(rmt_err));
      return rmt_err;
    }
    return ESP_OK;
  }

  // Fallback: log if driver not implemented
  ESP_LOGD(TAG,
           "Render hook segment=%s start=%u len=%u (bytes=%u) - driver %s not implemented",
           segment.id.c_str(),
           static_cast<unsigned>(start),
           static_cast<unsigned>(pixels),
           static_cast<unsigned>(expected_bytes),
           driver_name(cfg_.driver));
  return ESP_OK;
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
