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

struct GradientStop {
  float pos{0.0f};
  Rgb color{};
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

std::vector<std::string> split_colors(const std::string& text) {
  std::vector<std::string> parts;
  std::string current;
  for (char ch : text) {
    if (ch == ',' || ch == ';' || ch == ' ' || ch == '|') {
      if (!current.empty()) {
        parts.push_back(current);
        current.clear();
      }
    } else if (ch == '-') {
      if (!current.empty()) {
        parts.push_back(current);
        current.clear();
      }
    } else {
      current.push_back(ch);
    }
  }
  if (!current.empty()) {
    parts.push_back(current);
  }
  return parts;
}

std::vector<GradientStop> build_gradient_from_string(const std::string& text, const Rgb& fallback) {
  std::vector<GradientStop> stops;
  const auto tokens = split_colors(text);
  if (tokens.size() < 2) {
    return stops;
  }
  const size_t last = tokens.size() - 1;
  for (size_t i = 0; i < tokens.size(); ++i) {
    const float pos = last == 0 ? 0.0f : static_cast<float>(i) / static_cast<float>(last);
    const Rgb color = parse_hex_color(tokens[i], fallback);
    stops.push_back(GradientStop{pos, color});
  }
  return stops;
}

std::vector<GradientStop> palette_gradient(const std::string& name,
                                           const Rgb& c1,
                                           const Rgb& c2,
                                           const Rgb& c3) {
  const std::string key = lower_copy(name);
  if (key == "sunset") {
    return {{0.0f, c1}, {0.45f, Rgb{1.0f, 0.45f, 0.0f}}, {1.0f, Rgb{0.6f, 0.05f, 0.2f}}};
  }
  if (key == "fire") {
    return {{0.0f, Rgb{1.0f, 0.2f, 0.0f}}, {0.5f, Rgb{1.0f, 0.6f, 0.0f}}, {1.0f, Rgb{1.0f, 1.0f, 0.3f}}};
  }
  if (key == "icy") {
    return {{0.0f, Rgb{0.2f, 0.6f, 1.0f}}, {0.5f, Rgb{0.4f, 0.8f, 1.0f}}, {1.0f, Rgb{0.8f, 1.0f, 1.0f}}};
  }
  if (key == "forest") {
    return {{0.0f, Rgb{0.0f, 0.3f, 0.1f}}, {0.5f, Rgb{0.0f, 0.5f, 0.0f}}, {1.0f, Rgb{0.4f, 0.8f, 0.2f}}};
  }
  if (key == "ocean" || key == "aqua") {
    return {{0.0f, Rgb{0.0f, 0.2f, 0.4f}}, {0.5f, Rgb{0.0f, 0.6f, 0.7f}}, {1.0f, Rgb{0.0f, 0.9f, 1.0f}}};
  }
  if (key == "party") {
    return {{0.0f, Rgb{1.0f, 0.0f, 0.6f}}, {0.33f, Rgb{0.0f, 0.6f, 1.0f}}, {0.66f, Rgb{0.5f, 0.0f, 1.0f}}, {1.0f, Rgb{1.0f, 0.4f, 0.0f}}};
  }
  return {{0.0f, c1}, {0.5f, c2}, {1.0f, c3}};
}

Rgb sample_gradient(const std::vector<GradientStop>& stops, float t) {
  if (stops.empty()) {
    return Rgb{};
  }
  if (stops.size() == 1) {
    return stops.front().color;
  }
  const float x = clamp01(t);
  GradientStop a = stops.front();
  GradientStop b = stops.back();
  for (size_t i = 1; i < stops.size(); ++i) {
    if (stops[i].pos >= x) {
      a = stops[i - 1];
      b = stops[i];
      break;
    }
  }
  const float span = std::max(0.0001f, b.pos - a.pos);
  const float local = clamp01((x - a.pos) / span);
  Rgb out;
  out.r = a.color.r + (b.color.r - a.color.r) * local;
  out.g = a.color.g + (b.color.g - a.color.g) * local;
  out.b = a.color.b + (b.color.b - a.color.b) * local;
  return out;
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
                                                      uint8_t global_brightness,
                                                      uint16_t fps) {
  const uint16_t pixels = led_count == 0 ? 60 : led_count;
  std::vector<uint8_t> frame(pixels * 3, 0);

  const Rgb c1 = parse_hex_color(binding.effect.color1, Rgb{1.0f, 1.0f, 1.0f});
  const Rgb c2 = parse_hex_color(binding.effect.color2, Rgb{0.6f, 0.4f, 0.0f});
  const Rgb c3 = parse_hex_color(binding.effect.color3, Rgb{0.0f, 0.2f, 1.0f});
  std::vector<GradientStop> gradient;
  if (!binding.effect.gradient.empty()) {
    gradient = build_gradient_from_string(binding.effect.gradient, c1);
  } else if (!binding.effect.palette.empty()) {
    gradient = palette_gradient(binding.effect.palette, c1, c2, c3);
  }

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
    float bass = metrics.bass > 0.0f ? metrics.bass : energy * 0.8f;
    float treble = metrics.treble > 0.0f ? metrics.treble : energy * 0.6f;
    float mid = metrics.mid > 0.0f ? metrics.mid : energy * 0.7f;

    // Apply band gains
    bass *= binding.effect.band_gain_low;
    mid *= binding.effect.band_gain_mid;
    treble *= binding.effect.band_gain_high;

    float weighted = 0.0f;
    const std::string reactive = lower_copy(binding.effect.reactive_mode);
    if (reactive == "bass") {
      weighted = bass;
    } else if (reactive == "mids") {
      weighted = mid;
    } else if (reactive == "treble") {
      weighted = treble;
    } else {
      weighted = (energy * 0.4f + mid * 0.25f + bass * 0.2f + treble * 0.15f);
    }
    weighted *= (0.6f + beat * 0.4f);
    const float profile_gain = binding.effect.audio_profile == "wled_bass_boost" ? 1.4f
                             : binding.effect.audio_profile == "wled_reactive" ? 1.2f
                             : binding.effect.audio_profile == "ledfx_energy" ? 1.1f
                             : 1.0f;
    audio_mod = clamp01(0.4f + weighted * 0.8f * profile_gain);
    audio_mod *= binding.effect.amplitude_scale > 0.0f ? binding.effect.amplitude_scale : 1.0f;
    if (binding.effect.brightness_compress > 0.0f) {
      const float gamma = 1.0f + binding.effect.brightness_compress;
      audio_mod = powf(audio_mod, 1.0f / gamma);
    }
    if (binding.effect.beat_response) {
      audio_mod *= (0.6f + beat * 0.4f);
    }
  }

  // Attack/release envelope to smooth out audio reactivity
  if (binding.effect.audio_link && (binding.effect.attack_ms > 0 || binding.effect.release_ms > 0) && fps > 0) {
    audio_mod = apply_envelope(envelope_key(binding), audio_mod, fps, binding.effect.attack_ms, binding.effect.release_ms);
  }

  const std::string effect_name = lower_copy(binding.effect.effect);
  const float speed = 0.02f + (binding.effect.speed / 255.0f) * 0.25f;
  const float intensity = binding.effect.intensity / 255.0f;
  const float direction = lower_copy(binding.effect.direction) == "reverse" ? -1.0f : 1.0f;

  uint8_t* dst = frame.data();
  if (effect_name.find("rainbow") != std::string::npos) {
    const float base_phase = frame_idx * speed * direction;
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
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      const float phase = frame_idx * speed * 0.25f + pos * 0.5f * direction;
      const float sample_pos = phase - std::floor(phase);
      const Rgb base = gradient.empty() ? c1 : sample_gradient(gradient, sample_pos);
      *dst++ = to_byte(base.r * brightness * pulse * audio_mod);
      *dst++ = to_byte(base.g * brightness * pulse * audio_mod);
      *dst++ = to_byte(base.b * brightness * pulse * audio_mod);
    }
    return frame;
  }

  const float move = frame_idx * speed * 0.5f * direction;
  for (uint16_t i = 0; i < pixels; ++i) {
    const float pos = static_cast<float>(i) / static_cast<float>(pixels);
    float t = pos + move;
    t = t - std::floor(t);
    const float mix_c3 = (sinf((t + move) * 6.2831f) * 0.5f + 0.5f) * intensity;
    const float mix_c2 = 1.0f - mix_c3;
    Rgb base = gradient.empty() ? Rgb{} : sample_gradient(gradient, t);
    if (gradient.empty()) {
      base.r = c1.r * (1.0f - t) + c2.r * t;
      base.g = c1.g * (1.0f - t) + c2.g * t;
      base.b = c1.b * (1.0f - t) + c2.b * t;
      base.r = base.r * mix_c2 + c3.r * mix_c3;
      base.g = base.g * mix_c2 + c3.g * mix_c3;
      base.b = base.b * mix_c2 + c3.b * mix_c3;
    } else {
      // Add subtle third color modulation when gradient provided
      base.r = base.r * mix_c2 + c3.r * 0.25f * mix_c3;
      base.g = base.g * mix_c2 + c3.g * 0.25f * mix_c3;
      base.b = base.b * mix_c2 + c3.b * 0.25f * mix_c3;
    }
    const float twinkle = 0.85f + (sinf((move + pos) * 10.0f) * 0.5f + 0.5f) * 0.15f * intensity;
    *dst++ = to_byte(base.r * brightness * twinkle * audio_mod);
    *dst++ = to_byte(base.g * brightness * twinkle * audio_mod);
    *dst++ = to_byte(base.b * brightness * twinkle * audio_mod);
  }
  return frame;
}

bool WledEffectsRuntime::render_and_send(const WledEffectBinding& binding,
                                         const WledDeviceConfig& device,
                                         const std::string& ip,
                                         uint32_t frame_idx,
                                         uint8_t global_brightness,
                                         uint16_t fps) {
  if (!binding.ddp) {
    return false;
  }
  const uint16_t leds = device.leds == 0 ? 60 : device.leds;
  const auto frame = render_frame(binding, leds, frame_idx, global_brightness, fps);
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
    std::vector<VirtualSegmentConfig> virtuals;
    std::vector<LedSegmentConfig> segments;
    uint8_t global_brightness = 255;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      fx = effects_;
      devices_snapshot = devices_;
      if (cfg_ref_) {
        global_brightness = cfg_ref_->led_engine.global_brightness;
        virtuals = cfg_ref_->virtual_segments;
        segments = cfg_ref_->led_engine.segments;
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
      render_and_send(binding, dev, ip, frame_idx, global_brightness, fps);
    }

    // Virtual segments: render a single frame and distribute to members (WLED + physical)
    auto find_segment = [&](const VirtualSegmentMember& m) -> const LedSegmentConfig* {
      if (m.id.empty()) {
        return nullptr;
      }
      auto it = std::find_if(segments.begin(), segments.end(), [&](const LedSegmentConfig& s) {
        return s.id == m.id;
      });
      if (it != segments.end()) {
        return &(*it);
      }
      if (m.segment_index < segments.size()) {
        return &segments[m.segment_index];
      }
      return nullptr;
    };
    for (const auto& vseg : virtuals) {
      if (vseg.id.empty() || vseg.enabled == false) {
        continue;
      }
      // Sum total length; for WLED member with length 0, default to device LEDs
      size_t total_leds = 0;
      for (const auto& m : vseg.members) {
        if (m.type == "physical") {
          const auto* seg = find_segment(m);
          if (!seg) {
            continue;
          }
          const uint16_t available = seg->led_count;
          const uint16_t start = std::min<uint16_t>(m.start, available);
          const uint16_t len = m.length > 0 ? std::min<uint16_t>(m.length, static_cast<uint16_t>(available - start))
                                            : static_cast<uint16_t>(available - start);
          total_leds += len;
        } else if (m.type == "wled") {
          const auto dev_it = std::find_if(devices_snapshot.begin(), devices_snapshot.end(), [&](const WledDeviceConfig& d) { return d.id == m.id; });
          total_leds += (dev_it != devices_snapshot.end() && dev_it->leds > 0) ? dev_it->leds : 0;
        }
      }
      if (total_leds == 0) {
        continue;
        }
        WledEffectBinding fake{};
        fake.device_id = vseg.id;
        fake.effect = vseg.effect;
        fake.audio_channel = "mix";
        const auto frame = render_frame(fake, static_cast<uint16_t>(total_leds), frame_idx, global_brightness, fps);
        size_t cursor = 0;
        for (const auto& m : vseg.members) {
          uint16_t length = m.length;
          uint16_t start = m.start;
          if (m.type == "wled") {
            if (length == 0) {
              const auto dev_it = std::find_if(devices_snapshot.begin(), devices_snapshot.end(), [&](const WledDeviceConfig& d) { return d.id == m.id; });
              if (dev_it != devices_snapshot.end() && dev_it->leds > 0) {
                length = dev_it->leds;
              }
            }
          } else if (m.type == "physical") {
            const auto* seg = find_segment(m);
            if (seg) {
              const uint16_t available = seg->led_count;
              start = std::min<uint16_t>(start, available);
              const uint16_t fallback_len = static_cast<uint16_t>(available - start);
              length = length > 0 ? std::min<uint16_t>(length, fallback_len) : fallback_len;
            } else {
              length = 0;
            }
          }
          if (length == 0) continue;
          if (cursor + length * 3 > frame.size()) {
            length = static_cast<uint16_t>((frame.size() - cursor) / 3);
          }
          if (length == 0) break;
          if (m.type == "wled") {
            WledDeviceConfig dev{};
            std::string ip;
            if (!resolve_device(m.id, devices_snapshot, status, dev, ip)) {
              cursor += length * 3;
            continue;
          }
          const uint16_t port = cfg_ref_ && cfg_ref_->mqtt.ddp_port > 0 ? cfg_ref_->mqtt.ddp_port : kDefaultDdpPort;
          const uint32_t channel = static_cast<uint32_t>(m.segment_index == 0 ? 1 : m.segment_index);
            const std::vector<uint8_t> slice(frame.begin() + cursor, frame.begin() + cursor + length * 3);
            const bool ok = ddp_send_frame(ip, port, slice, channel, 0, seq_++);
            if (!ok) {
              ESP_LOGW(TAG, "DDP send failed (virtual %s) -> %s:%u", vseg.id.c_str(), ip.c_str(), port);
            }
          } else if (m.type == "physical") {
            const auto* seg = find_segment(m);
            const bool has_runtime = led_runtime_ != nullptr;
            if (!seg || !has_runtime) {
              ESP_LOGW(TAG,
                       "Virtual segment %s physical member %s skipped (runtime=%d, seg_found=%d)",
                       vseg.id.c_str(),
                       m.id.c_str(),
                       has_runtime ? 1 : 0,
                       seg ? 1 : 0);
              cursor += length * 3;
              continue;
            }
            const std::vector<uint8_t> slice(frame.begin() + cursor, frame.begin() + cursor + length * 3);
            const esp_err_t res = led_runtime_->render_frame(slice, *seg, start, length);
            if (res != ESP_OK) {
              ESP_LOGW(TAG,
                       "Render hook error %s for virtual %s member %s (start=%u len=%u)",
                       esp_err_to_name(res),
                       vseg.id.c_str(),
                       m.id.c_str(),
                       static_cast<unsigned>(start),
                       static_cast<unsigned>(length));
            }
          } else {
            ESP_LOGW(TAG, "Virtual segment %s member type %s not rendered (unsupported)", vseg.id.c_str(), m.type.c_str());
          }
          cursor += length * 3;
        if (cursor >= frame.size()) {
          break;
        }
      }
    }

    frame_idx++;
    vTaskDelay(delay);
  }
}

std::string WledEffectsRuntime::envelope_key(const WledEffectBinding& binding) const {
  return binding.device_id + ":" + std::to_string(binding.segment_index) + ":" + binding.effect.effect;
}

float WledEffectsRuntime::apply_envelope(const std::string& key,
                                         float input,
                                         uint16_t fps,
                                         uint16_t attack_ms,
                                         uint16_t release_ms) {
  if (fps == 0 || (attack_ms == 0 && release_ms == 0)) {
    return clamp01(input);
  }
  const float dt_ms = 1000.0f / static_cast<float>(fps);
  const float alpha_attack = std::min(1.0f, dt_ms / std::max(1.0f, static_cast<float>(attack_ms)));
  const float alpha_release = std::min(1.0f, dt_ms / std::max(1.0f, static_cast<float>(release_ms)));
  float level = 0.0f;
  if (auto it = envelope_state_.find(key); it != envelope_state_.end()) {
    level = it->second;
  }
  if (input > level) {
    level += (input - level) * alpha_attack;
  } else {
    level += (input - level) * alpha_release;
  }
  level = clamp01(level);
  envelope_state_[key] = level;
  return level;
}
