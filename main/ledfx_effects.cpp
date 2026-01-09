#include "ledfx_effects.hpp"
#include "led_engine/audio_pipeline.hpp"
#include "esp_log.h"
#include "esp_random.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <unordered_map>

namespace ledfx_effects {

static const char* TAG = "ledfx_effects";

namespace {

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

// Per-effect state storage (keyed by effect name + segment)
static std::unordered_map<std::string, std::vector<float>> s_effect_state;

std::vector<float>& get_state(const std::string& key, size_t size) {
  auto& state = s_effect_state[key];
  if (state.size() != size) {
    state.resize(size, 0.0f);
  }
  return state;
}

}  // namespace

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

std::vector<GradientStop> palette_gradient(const std::string& name, const Rgb& c1, const Rgb& c2, const Rgb& c3) {
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
  for (size_t i = 0; i < stops.size() - 1; ++i) {
    if (x >= stops[i].pos && x <= stops[i + 1].pos) {
      a = stops[i];
      b = stops[i + 1];
      break;
    }
  }
  const float range = b.pos - a.pos;
  const float mix = range > 0.0001f ? (x - a.pos) / range : 0.0f;
  return Rgb{
      a.color.r * (1.0f - mix) + b.color.r * mix,
      a.color.g * (1.0f - mix) + b.color.g * mix,
      a.color.b * (1.0f - mix) + b.color.b * mix,
  };
}

std::vector<uint8_t> render_effect(const std::string& effect_name,
                                    const EffectAssignment& effect,
                                    uint16_t led_count,
                                    uint32_t frame_idx,
                                    uint8_t global_brightness,
                                    uint16_t fps,
                                    const std::vector<GradientStop>& gradient,
                                    const Rgb& c1,
                                    const Rgb& c2,
                                    const Rgb& c3,
                                    float brightness,
                                    float audio_mod,
                                    float speed,
                                    float intensity,
                                    float direction) {
  const uint16_t pixels = led_count == 0 ? 60 : led_count;
  std::vector<uint8_t> frame(pixels * 3, 0);
  uint8_t* dst = frame.data();
  
  const std::string name_lower = lower_copy(effect_name);
  AudioMetrics metrics = led_audio_get_metrics();

  // Fire / Flame effect (LEDFx style)
  if (name_lower.find("fire") != std::string::npos && name_lower.find("2012") == std::string::npos) {
    const float fire_intensity = effect.audio_link ? (metrics.bass * 0.6f + metrics.mid * 0.4f) : intensity;
    
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      const float fire_base = pos * 2.0f - 1.0f;  // -1 to 1
      const float fire_height = 1.0f - std::abs(fire_base);
      
      // Fire noise using multiple sine waves
      const float noise1 = sinf((frame_idx * speed * 0.3f) + (pos * 8.0f)) * 0.5f + 0.5f;
      const float noise2 = sinf((frame_idx * speed * 0.5f) + (pos * 12.0f)) * 0.3f + 0.7f;
      const float noise3 = sinf((frame_idx * speed * 0.7f) + (pos * 6.0f)) * 0.2f + 0.8f;
      const float fire_noise = (noise1 * 0.4f + noise2 * 0.4f + noise3 * 0.2f);
      
      // Fire color gradient: red at bottom, orange/yellow in middle, white at top
      float fire_level = fire_height * fire_noise * fire_intensity;
      fire_level = std::max(0.0f, std::min(1.0f, fire_level));
      
      Rgb fire_color;
      if (fire_level < 0.3f) {
        fire_color = {fire_level * 3.33f, 0.0f, 0.0f};
      } else if (fire_level < 0.7f) {
        const float t = (fire_level - 0.3f) / 0.4f;
        fire_color = {1.0f, t * 0.5f, 0.0f};
      } else {
        const float t = (fire_level - 0.7f) / 0.3f;
        fire_color = {1.0f, 0.5f + t * 0.5f, t * 0.3f};
      }
      
      *dst++ = to_byte(fire_color.r * brightness * audio_mod);
      *dst++ = to_byte(fire_color.g * brightness * audio_mod);
      *dst++ = to_byte(fire_color.b * brightness * audio_mod);
    }
    return frame;
  }

  // Matrix effect (falling code) - LEDFx style
  if (name_lower.find("matrix") != std::string::npos) {
    std::string state_key = "matrix_" + effect_name + "_" + std::to_string(led_count);
    auto& matrix_drops = get_state(state_key, pixels);
    
    const float drop_speed = speed * 0.8f;
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      
      // Randomly spawn new drops
      if (matrix_drops[i] <= 0.0f && (static_cast<float>(esp_random()) / 0xFFFFFFFF) < 0.02f * intensity) {
        matrix_drops[i] = 1.0f;
      }
      
      // Update drop position
      if (matrix_drops[i] > 0.0f) {
        matrix_drops[i] -= drop_speed * 0.02f;
        if (matrix_drops[i] < 0.0f) {
          matrix_drops[i] = 0.0f;
        }
      }
      
      // Render: bright head, fading trail
      float level = 0.0f;
      if (matrix_drops[i] > 0.0f) {
        const float head_pos = matrix_drops[i];
        const float dist = std::abs(pos - head_pos);
        if (dist < 0.15f) {
          level = 1.0f - (dist / 0.15f) * 0.7f;
        } else if (dist < 0.4f && head_pos < pos) {
          level = (0.4f - dist) / 0.25f * 0.3f;
        }
      }
      
      const Rgb base = gradient.empty() ? c1 : sample_gradient(gradient, pos);
      *dst++ = to_byte(base.r * brightness * level * audio_mod);
      *dst++ = to_byte(base.g * brightness * level * audio_mod);
      *dst++ = to_byte(base.b * brightness * level * audio_mod);
    }
    return frame;
  }

  // Waves effect (smooth rolling waves) - LEDFx style
  if (name_lower.find("wave") != std::string::npos && name_lower.find("energy") == std::string::npos) {
    const float move = frame_idx * speed * 0.3f * direction;
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      const float wave1 = sinf((pos * 3.0f + move) * 6.2831f) * 0.5f + 0.5f;
      const float wave2 = sinf((pos * 5.0f + move * 1.3f) * 6.2831f) * 0.3f + 0.7f;
      const float wave = (wave1 * 0.6f + wave2 * 0.4f) * intensity;
      
      const Rgb base = gradient.empty() ? c1 : sample_gradient(gradient, pos);
      *dst++ = to_byte(base.r * brightness * wave * audio_mod);
      *dst++ = to_byte(base.g * brightness * wave * audio_mod);
      *dst++ = to_byte(base.b * brightness * wave * audio_mod);
    }
    return frame;
  }

  // Plasma effect (animated gradients) - LEDFx style
  if (name_lower.find("plasma") != std::string::npos) {
    const float move = frame_idx * speed * 0.2f;
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      const float x = pos * 8.0f;
      const float plasma = sinf(x + move) * 0.5f + 
                          sinf(x * 2.0f + move * 1.3f) * 0.3f +
                          sinf(x * 0.5f + move * 0.7f) * 0.2f;
      const float t = (plasma + 1.0f) * 0.5f;
      
      const Rgb base = gradient.empty() ? c1 : sample_gradient(gradient, t);
      *dst++ = to_byte(base.r * brightness * intensity * audio_mod);
      *dst++ = to_byte(base.g * brightness * intensity * audio_mod);
      *dst++ = to_byte(base.b * brightness * intensity * audio_mod);
    }
    return frame;
  }

  // Ripple Flow effect - LEDFx style (different from WLED Ripple)
  if (name_lower.find("ripple") != std::string::npos && name_lower.find("flow") != std::string::npos) {
    std::string state_key = "ripple_flow_" + effect_name + "_" + std::to_string(led_count);
    auto& ripples = get_state(state_key, pixels);
    
    AudioMetrics metrics = led_audio_get_metrics();
    if (effect.audio_link && metrics.beat > 0.5f) {
      const float center = static_cast<float>(esp_random()) / 0xFFFFFFFF;
      ripples[static_cast<size_t>(center * pixels)] = 1.0f;
    }
    
    const float ripple_speed = speed * 0.05f;
    for (uint16_t i = 0; i < pixels; ++i) {
      float level = 0.0f;
      for (size_t r = 0; r < ripples.size(); ++r) {
        if (ripples[r] > 0.0f) {
          const float dist = std::abs(static_cast<float>(i) - static_cast<float>(r)) / static_cast<float>(pixels);
          const float radius = ripples[r];
          if (std::abs(dist - radius) < 0.1f) {
            level = std::max(level, (1.0f - radius) * 0.8f);
          }
          ripples[r] += ripple_speed;
          if (ripples[r] > 1.0f) {
            ripples[r] = 0.0f;
          }
        }
      }
      
      const Rgb base = gradient.empty() ? c1 : sample_gradient(gradient, static_cast<float>(i) / pixels);
      *dst++ = to_byte(base.r * brightness * level * audio_mod);
      *dst++ = to_byte(base.g * brightness * level * audio_mod);
      *dst++ = to_byte(base.b * brightness * level * audio_mod);
    }
    return frame;
  }

  // Rain effect - LEDFx style
  if (name_lower.find("rain") != std::string::npos) {
    std::string state_key = "rain_" + effect_name + "_" + std::to_string(led_count);
    auto& raindrops = get_state(state_key, pixels);
    
    const float rain_speed = speed * 0.4f;
    for (uint16_t i = 0; i < pixels; ++i) {
      // Spawn new drops
      if (raindrops[i] <= 0.0f && (static_cast<float>(esp_random()) / 0xFFFFFFFF) < 0.05f * intensity) {
        raindrops[i] = 1.0f;
      }
      
      // Update drop
      if (raindrops[i] > 0.0f) {
        raindrops[i] -= rain_speed * 0.02f;
        if (raindrops[i] < 0.0f) {
          raindrops[i] = 0.0f;
        }
      }
      
      const float level = raindrops[i] * intensity;
      const Rgb base = gradient.empty() ? Rgb{0.2f, 0.4f, 0.8f} : sample_gradient(gradient, static_cast<float>(i) / pixels);
      *dst++ = to_byte(base.r * brightness * level * audio_mod);
      *dst++ = to_byte(base.g * brightness * level * audio_mod);
      *dst++ = to_byte(base.b * brightness * level * audio_mod);
    }
    return frame;
  }

  // Aura effect - LEDFx style
  if (name_lower.find("aura") != std::string::npos) {
    const float move = frame_idx * speed * 0.15f;
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      const float aura = sinf((pos * 2.0f + move) * 6.2831f) * 0.3f + 0.7f;
      const float pulse = 0.6f + sinf(frame_idx * 0.1f) * 0.4f;
      
      const Rgb base = gradient.empty() ? c1 : sample_gradient(gradient, pos);
      *dst++ = to_byte(base.r * brightness * aura * pulse * audio_mod);
      *dst++ = to_byte(base.g * brightness * aura * pulse * audio_mod);
      *dst++ = to_byte(base.b * brightness * aura * pulse * audio_mod);
    }
    return frame;
  }

  // Hyperspace effect - LEDFx style
  if (name_lower.find("hyperspace") != std::string::npos) {
    const float move = frame_idx * speed * 1.2f * direction;
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      float t = std::fmod(pos + move, 1.0f);
      
      // Star streaks
      const float streak = std::pow(1.0f - t, 2.0f) * intensity;
      const float warp = sinf((pos * 4.0f + move * 2.0f) * 6.2831f) * 0.2f + 0.8f;
      
      const Rgb base = gradient.empty() ? Rgb{0.0f, 0.5f, 1.0f} : sample_gradient(gradient, pos);
      *dst++ = to_byte(base.r * brightness * streak * warp * audio_mod);
      *dst++ = to_byte(base.g * brightness * streak * warp * audio_mod);
      *dst++ = to_byte(base.b * brightness * streak * warp * audio_mod);
    }
    return frame;
  }

  // Default: return black frame if effect not found
  return frame;
}

}  // namespace ledfx_effects





