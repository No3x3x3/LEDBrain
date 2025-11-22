#include "wled_effects.hpp"
#include "ddp_tx.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_engine/audio_pipeline.hpp"
#include "wled_discovery.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>

namespace {

constexpr uint16_t kDefaultDdpPort = 4048;
static const char* TAG = "wled_fx";

struct Rgb {
  float r{1.0f};
  float g{1.0f};
  float b{1.0f};
};

float clamp01(float v) {
  return std::max(0.0f, std::min(1.0f, v));
}

uint8_t to_byte(float v) {
  const int value = static_cast<int>(v * 255.0f);
  return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

std::string lower_copy(const std::string& value) {
  std::string out = value;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return out;
}

int from_hex(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
  if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
  return -1;
}

Rgb parse_hex_color(const std::string& text, const Rgb& fallback) {
  size_t start = 0;
  if (text.empty()) {
    return fallback;
  }
  if (text[0] == '#') {
    start = 1;
  }
  if (text.size() - start < 6) {
    return fallback;
  }
  int r1 = from_hex(text[start]);
  int r2 = from_hex(text[start + 1]);
  int g1 = from_hex(text[start + 2]);
  int g2 = from_hex(text[start + 3]);
  int b1 = from_hex(text[start + 4]);
  int b2 = from_hex(text[start + 5]);
  if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0) {
    return fallback;
  }
  const float r = (static_cast<float>((r1 << 4) + r2)) / 255.0f;
  const float g = (static_cast<float>((g1 << 4) + g2)) / 255.0f;
  const float b = (static_cast<float>((b1 << 4) + b2)) / 255.0f;
  return Rgb{r, g, b};
}

}  // namespace

esp_err_t WledEffectsRuntime::start(AppConfig* cfg, LedEngineRuntime* led_runtime) {
  cfg_ref_ = cfg;
  led_runtime_ = led_runtime;
  update_config(*cfg);
  running_ = true;
  if (!task_) {
    const BaseType_t res =
        xTaskCreatePinnedToCore(task_entry, "wled_fx", 4096, this, 4, &task_, tskNO_AFFINITY);
    if (res != pdPASS) {
      running_ = false;
      task_ = nullptr;
      ESP_LOGE(TAG, "Failed to start WLED FX task");
      return ESP_FAIL;
    }
  }
  return ESP_OK;
}

void WledEffectsRuntime::stop() {
  running_ = false;
  if (task_) {
    vTaskDelay(pdMS_TO_TICKS(10));
    vTaskDelete(task_);
    task_ = nullptr;
  }
}

void WledEffectsRuntime::update_config(const AppConfig& cfg) {
  std::lock_guard<std::mutex> lock(mutex_);
  effects_ = cfg.wled_effects;
  devices_ = cfg.wled_devices;
}

void WledEffectsRuntime::task_entry(void* arg) {
  auto* self = reinterpret_cast<WledEffectsRuntime*>(arg);
  if (self) {
    self->task_loop();
  }
  vTaskDelete(nullptr);
}

bool WledEffectsRuntime::should_run() const {
  return !led_runtime_ || led_runtime_->enabled();
}

bool WledEffectsRuntime::resolve_device(const std::string& device_id,
                                        const std::vector<WledDeviceConfig>& devices,
                                        const std::vector<WledDeviceStatus>& status,
                                        WledDeviceConfig& out,
                                        std::string& ip) const {
  auto it = std::find_if(devices.begin(), devices.end(), [&](const WledDeviceConfig& cfg) {
    return cfg.id == device_id;
  });
  if (it == devices.end()) {
    return false;
  }
  out = *it;
  if (!out.active) {
    return false;
  }
  for (const auto& st : status) {
    if (st.config.id == device_id || (!st.ip.empty() && st.ip == out.address) ||
        (!st.config.address.empty() && st.config.address == out.address)) {
      if (!st.ip.empty()) {
        ip = st.ip;
      }
      break;
    }
  }
  if (ip.empty()) {
    ip = out.address;
  }
  return !ip.empty();
}

std::vector<uint8_t> WledEffectsRuntime::render_frame(const WledEffectBinding& binding,
                                                      uint16_t led_count,
                                                      uint32_t frame_idx,
                                                      uint8_t global_brightness) {
  const uint16_t pixels = led_count == 0 ? 60 : led_count;
  std::vector<uint8_t> frame(pixels * 3, 0);

  const Rgb c1 = parse_hex_color(binding.effect.color1, Rgb{1.0f, 1.0f, 1.0f});
  const Rgb c2 = parse_hex_color(binding.effect.color2, Rgb{0.6f, 0.4f, 0.0f});
  const Rgb c3 = parse_hex_color(binding.effect.color3, Rgb{0.0f, 0.2f, 1.0f});

  const float brightness_override =
      binding.effect.brightness_override > 0 ? binding.effect.brightness_override : binding.effect.brightness;
  float brightness = clamp01(brightness_override / 255.0f) * clamp01(global_brightness / 255.0f);
  if (brightness <= 0.0f) {
    brightness = 0.01f;
  }

  float audio_mod = 1.0f;
  AudioMetrics metrics{};
  if (binding.effect.audio_link) {
    metrics = led_audio_get_metrics();
  }
  if (binding.effect.audio_link) {
    float energy = metrics.energy;
    const std::string channel = lower_copy(binding.audio_channel);
    if (channel == "left") {
      energy = metrics.energy_left > 0.0f ? metrics.energy_left : metrics.energy * 0.8f;
    } else if (channel == "right") {
      energy = metrics.energy_right > 0.0f ? metrics.energy_right : metrics.energy * 0.8f;
    }
    if (energy <= 0.0001f) {
      energy = 0.35f + 0.35f * (sinf(frame_idx * 0.15f) * 0.5f + 0.5f);
    }
    const float beat = metrics.beat > 0.0f ? metrics.beat : (sinf(frame_idx * 0.12f) * 0.5f + 0.5f);
    const float bass = metrics.bass > 0.0f ? metrics.bass : energy * 0.8f;
    const float treble = metrics.treble > 0.0f ? metrics.treble : energy * 0.6f;
    float weighted = (energy * 0.5f + bass * 0.3f + treble * 0.2f) * (0.6f + beat * 0.4f);
    const float profile_gain = binding.effect.audio_profile == "wled_bass_boost" ? 1.4f
                             : binding.effect.audio_profile == "wled_reactive" ? 1.2f
                             : binding.effect.audio_profile == "ledfx_energy" ? 1.1f
                             : 1.0f;
    audio_mod = clamp01(0.4f + weighted * 0.8f * profile_gain);
  }

  const std::string effect_name = lower_copy(binding.effect.effect);
  const float speed = 0.02f + (binding.effect.speed / 255.0f) * 0.25f;
  const float intensity = binding.effect.intensity / 255.0f;

  uint8_t* dst = frame.data();
  if (effect_name.find("rainbow") != std::string::npos) {
    const float base_phase = frame_idx * speed;
    for (uint16_t i = 0; i < pixels; ++i) {
      const float x = base_phase + (static_cast<float>(i) / static_cast<float>(pixels));
      const float r = sinf((x) * 6.2831f) * 0.5f + 0.5f;
      const float g = sinf((x + 0.33f) * 6.2831f) * 0.5f + 0.5f;
      const float b = sinf((x + 0.66f) * 6.2831f) * 0.5f + 0.5f;
      *dst++ = to_byte(r * brightness * audio_mod);
      *dst++ = to_byte(g * brightness * audio_mod);
      *dst++ = to_byte(b * brightness * audio_mod);
    }
    return frame;
  }

  if (effect_name.find("solid") != std::string::npos) {
    const float pulse = 0.6f + (sinf(frame_idx * speed * 1.5f) * 0.5f + 0.5f) * 0.4f * intensity;
    for (uint16_t i = 0; i < pixels; ++i) {
      *dst++ = to_byte(c1.r * brightness * pulse * audio_mod);
      *dst++ = to_byte(c1.g * brightness * pulse * audio_mod);
      *dst++ = to_byte(c1.b * brightness * pulse * audio_mod);
    }
    return frame;
  }

  const float move = frame_idx * speed * 0.5f;
  for (uint16_t i = 0; i < pixels; ++i) {
    const float pos = static_cast<float>(i) / static_cast<float>(pixels);
    float t = pos + move;
    t = t - std::floor(t);
    const float mix_c3 = (sinf((t + move) * 6.2831f) * 0.5f + 0.5f) * intensity;
    const float mix_c2 = 1.0f - mix_c3;
    float r = c1.r * (1.0f - t) + c2.r * t;
    float g = c1.g * (1.0f - t) + c2.g * t;
    float b = c1.b * (1.0f - t) + c2.b * t;
    r = r * mix_c2 + c3.r * mix_c3;
    g = g * mix_c2 + c3.g * mix_c3;
    b = b * mix_c2 + c3.b * mix_c3;
    const float twinkle = 0.85f + (sinf((move + pos) * 10.0f) * 0.5f + 0.5f) * 0.15f * intensity;
    *dst++ = to_byte(r * brightness * twinkle * audio_mod);
    *dst++ = to_byte(g * brightness * twinkle * audio_mod);
    *dst++ = to_byte(b * brightness * twinkle * audio_mod);
  }
  return frame;
}

bool WledEffectsRuntime::render_and_send(const WledEffectBinding& binding,
                                         const WledDeviceConfig& device,
                                         const std::string& ip,
                                         uint32_t frame_idx,
                                         uint8_t global_brightness) {
  if (!binding.ddp) {
    return false;
  }
  const uint16_t leds = device.leds == 0 ? 60 : device.leds;
  const auto frame = render_frame(binding, leds, frame_idx, global_brightness);
  uint16_t port = kDefaultDdpPort;
  if (cfg_ref_ && cfg_ref_->mqtt.ddp_port > 0) {
    port = cfg_ref_->mqtt.ddp_port;
  }
  const uint32_t channel = static_cast<uint32_t>(binding.segment_index == 0 ? 1 : binding.segment_index);
  const bool ok = ddp_send_frame(ip, port, frame, channel, 0, seq_++);
  if (!ok) {
    ESP_LOGW(TAG, "DDP send failed -> %s:%u (device %s)", ip.c_str(), port, device.id.c_str());
  }
  return ok;
}

void WledEffectsRuntime::task_loop() {
  uint32_t frame_idx = 0;
  while (running_) {
    WledEffectsConfig fx{};
    std::vector<WledDeviceConfig> devices_snapshot;
    uint8_t global_brightness = 255;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      fx = effects_;
      devices_snapshot = devices_;
      if (cfg_ref_) {
        global_brightness = cfg_ref_->led_engine.global_brightness;
      }
      if (led_runtime_) {
        global_brightness = led_runtime_->brightness();
      }
    }

    const uint16_t fps = fx.target_fps == 0 ? 60 : std::clamp<uint16_t>(fx.target_fps, 1, 240);
    const TickType_t delay = pdMS_TO_TICKS(1000 / fps);

    if (fx.bindings.empty() || devices_snapshot.empty()) {
      vTaskDelay(pdMS_TO_TICKS(400));
      continue;
    }
    if (!should_run()) {
      vTaskDelay(delay);
      continue;
    }

    const auto status = wled_discovery_status();
    for (const auto& binding : fx.bindings) {
      if (!binding.enabled || binding.device_id.empty()) {
        continue;
      }
      WledDeviceConfig dev{};
      std::string ip;
      if (!resolve_device(binding.device_id, devices_snapshot, status, dev, ip)) {
        continue;
      }
      render_and_send(binding, dev, ip, frame_idx, global_brightness);
    }
    frame_idx++;
    vTaskDelay(delay);
  }
}
