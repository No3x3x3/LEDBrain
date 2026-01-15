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
                                    float time_s,
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
  
  const float t = time_s;  // Time in seconds (real-time)
  const std::string name_lower = lower_copy(effect_name);
  AudioMetrics metrics = led_audio_get_metrics();
  
  // Audio reactivity: use audio_mod for LEDFx effects when audio_link is enabled
  const float audio = effect.audio_link ? audio_mod : 1.0f;
  const float bass = effect.audio_link ? std::max(0.3f, metrics.bass) : 0.5f;
  const float mid = effect.audio_link ? std::max(0.3f, metrics.mid) : 0.5f;
  const float treble = effect.audio_link ? std::max(0.3f, metrics.treble) : 0.5f;
  const float energy = effect.audio_link ? std::max(0.3f, metrics.energy) : 0.5f;
  const float beat = effect.audio_link ? metrics.beat : 0.0f;

  // Helper: HSV to RGB
  auto hsv_to_rgb = [](float h, float s, float v) -> Rgb {
    h = h - std::floor(h);
    const float c = v * s;
    const float x = c * (1.0f - std::abs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float r = 0, g = 0, b = 0;
    const int hi = static_cast<int>(h * 6.0f) % 6;
    switch (hi) {
      case 0: r = c; g = x; b = 0; break;
      case 1: r = x; g = c; b = 0; break;
      case 2: r = 0; g = c; b = x; break;
      case 3: r = 0; g = x; b = c; break;
      case 4: r = x; g = 0; b = c; break;
      case 5: r = c; g = 0; b = x; break;
    }
    return Rgb{r + m, g + m, b + m};
  };

  // ==================== LEDFx AUDIO-REACTIVE EFFECTS ====================

  // Energy - audio-reactive mirrored bars from center (classic LedFX effect)
  if (name_lower.find("energy") != std::string::npos) {
    // LedFX Energy: mirrored visualization from center
    const uint16_t half = pixels / 2;
    const float spread = energy * intensity;  // 0-1 range
    const uint16_t lit_leds = static_cast<uint16_t>(spread * half);
    
    for (uint16_t i = 0; i < pixels; ++i) {
      const uint16_t dist_from_center = (i < half) ? (half - 1 - i) : (i - half);
      float level = 0.0f;
      
      if (dist_from_center < lit_leds) {
        // Gradient from center (bright) to edge (dim)
        level = 1.0f - (static_cast<float>(dist_from_center) / std::max(1.0f, static_cast<float>(lit_leds)));
      }
      
      // Color based on position in gradient (center = start, edge = end)
      const float grad_pos = static_cast<float>(dist_from_center) / static_cast<float>(half);
      const Rgb col = gradient.empty() ? c1 : sample_gradient(gradient, grad_pos);
      *dst++ = to_byte(col.r * brightness * level);
      *dst++ = to_byte(col.g * brightness * level);
      *dst++ = to_byte(col.b * brightness * level);
    }
    return frame;
  }

  // Spectrum / Bars - frequency bands visualization (LedFX style)
  if (name_lower.find("spectrum") != std::string::npos || name_lower.find("bar") != std::string::npos) {
    // LedFX Bars: each LED represents a frequency band
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      
      // Interpolate between bass, mid, treble
      float band_level;
      if (pos < 0.33f) {
        const float local = pos * 3.0f;
        band_level = bass * (1.0f - local) + mid * local;
      } else if (pos < 0.66f) {
        const float local = (pos - 0.33f) * 3.0f;
        band_level = mid * (1.0f - local) + treble * local;
      } else {
        band_level = treble;
      }
      
      const float level = band_level * intensity;
      const Rgb col = gradient.empty() ? hsv_to_rgb(pos, 1.0f, 1.0f) : sample_gradient(gradient, pos);
      *dst++ = to_byte(col.r * brightness * level);
      *dst++ = to_byte(col.g * brightness * level);
      *dst++ = to_byte(col.b * brightness * level);
    }
    return frame;
  }

  // Scroll - audio-reactive scrolling gradient (LedFX style)
  if (name_lower.find("scroll") != std::string::npos) {
    std::string state_key = "scroll_" + effect_name + "_" + std::to_string(led_count);
    auto& scroll_buf = get_state(state_key, pixels * 3);
    
    // Shift pixels in direction
    if (direction > 0) {
      for (int i = pixels - 1; i > 0; --i) {
        scroll_buf[i * 3 + 0] = scroll_buf[(i - 1) * 3 + 0];
        scroll_buf[i * 3 + 1] = scroll_buf[(i - 1) * 3 + 1];
        scroll_buf[i * 3 + 2] = scroll_buf[(i - 1) * 3 + 2];
      }
    } else {
      for (int i = 0; i < static_cast<int>(pixels) - 1; ++i) {
        scroll_buf[i * 3 + 0] = scroll_buf[(i + 1) * 3 + 0];
        scroll_buf[i * 3 + 1] = scroll_buf[(i + 1) * 3 + 1];
        scroll_buf[i * 3 + 2] = scroll_buf[(i + 1) * 3 + 2];
      }
    }
    
    // Insert new pixel based on audio energy
    // LedFX scroll: color based on gradient position cycling with audio
    const float hue_offset = std::fmod(t * speed * 0.1f, 1.0f);
    const float color_pos = std::fmod(hue_offset + energy * 0.5f, 1.0f);
    const Rgb new_col = gradient.empty() ? hsv_to_rgb(color_pos, 1.0f, 1.0f) : sample_gradient(gradient, color_pos);
    const float new_brightness = 0.2f + energy * 0.8f * intensity;
    
    const int insert_idx = direction > 0 ? 0 : (pixels - 1);
    scroll_buf[insert_idx * 3 + 0] = new_col.r * new_brightness;
    scroll_buf[insert_idx * 3 + 1] = new_col.g * new_brightness;
    scroll_buf[insert_idx * 3 + 2] = new_col.b * new_brightness;
    
    // Render
    for (uint16_t i = 0; i < pixels; ++i) {
      *dst++ = to_byte(scroll_buf[i * 3 + 0] * brightness);
      *dst++ = to_byte(scroll_buf[i * 3 + 1] * brightness);
      *dst++ = to_byte(scroll_buf[i * 3 + 2] * brightness);
    }
    return frame;
  }

  // Power - bass-reactive expanding bars from center (LedFX style)
  if (name_lower.find("power") != std::string::npos) {
    // LedFX Power: similar to Energy but more responsive to bass
    const uint16_t half = pixels / 2;
    const float power_level = 0.1f + bass * 0.9f * intensity;
    const uint16_t lit_leds = static_cast<uint16_t>(power_level * half);
    
    for (uint16_t i = 0; i < pixels; ++i) {
      const uint16_t dist_from_center = (i < half) ? (half - 1 - i) : (i - half);
      float level = 0.0f;
      
      if (dist_from_center < lit_leds) {
        level = 1.0f - (static_cast<float>(dist_from_center) / std::max(1.0f, static_cast<float>(lit_leds))) * 0.3f;
      }
      
      const float grad_pos = static_cast<float>(dist_from_center) / static_cast<float>(half);
      const Rgb col = gradient.empty() ? c1 : sample_gradient(gradient, grad_pos);
      *dst++ = to_byte(col.r * brightness * level);
      *dst++ = to_byte(col.g * brightness * level);
      *dst++ = to_byte(col.b * brightness * level);
    }
    return frame;
  }

  // Magnitude - fills strip based on overall audio level (LedFX style)
  if (name_lower.find("magnitude") != std::string::npos) {
    const float mag_level = energy * intensity;
    const uint16_t lit_leds = static_cast<uint16_t>(mag_level * pixels);
    
    for (uint16_t i = 0; i < pixels; ++i) {
      const uint16_t idx = direction > 0 ? i : (pixels - 1 - i);
      float level = 0.0f;
      
      if (idx < lit_leds) {
        level = 1.0f;
      } else if (idx < lit_leds + 3 && lit_leds > 0) {
        // Soft edge
        level = 1.0f - (static_cast<float>(idx - lit_leds) / 3.0f);
      }
      
      const float grad_pos = static_cast<float>(i) / static_cast<float>(pixels);
      const Rgb col = gradient.empty() ? hsv_to_rgb(grad_pos * 0.3f, 1.0f, 1.0f) : sample_gradient(gradient, grad_pos);
      *dst++ = to_byte(col.r * brightness * level);
      *dst++ = to_byte(col.g * brightness * level);
      *dst++ = to_byte(col.b * brightness * level);
    }
    return frame;
  }

  // Single Color - solid color with optional audio modulation
  if (name_lower.find("single") != std::string::npos || name_lower.find("solid") != std::string::npos) {
    const float level = effect.audio_link ? (0.3f + energy * 0.7f * intensity) : intensity;
    for (uint16_t i = 0; i < pixels; ++i) {
      *dst++ = to_byte(c1.r * brightness * level);
      *dst++ = to_byte(c1.g * brightness * level);
      *dst++ = to_byte(c1.b * brightness * level);
    }
    return frame;
  }

  // Wavelength - maps frequency to position (LedFX style)
  if (name_lower.find("wavelength") != std::string::npos) {
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      
      // Map position to frequency: left=bass, center=mid, right=treble
      float freq_val;
      if (pos < 0.33f) {
        freq_val = bass;
      } else if (pos < 0.66f) {
        freq_val = mid;
      } else {
        freq_val = treble;
      }
      
      // Add wave modulation
      const float wave = sinf((pos * 4.0f + t * speed * 0.5f) * 6.2831f) * 0.3f + 0.7f;
      const float level = freq_val * wave * intensity;
      
      const Rgb col = gradient.empty() ? c1 : sample_gradient(gradient, pos);
      *dst++ = to_byte(col.r * brightness * level);
      *dst++ = to_byte(col.g * brightness * level);
      *dst++ = to_byte(col.b * brightness * level);
    }
    return frame;
  }

  // Blade - sharp moving scanner with audio width modulation (LedFX style)
  if (name_lower.find("blade") != std::string::npos) {
    // Blade width based on bass
    const float blade_width = 0.05f + bass * 0.2f * intensity;
    // Position bounces back and forth
    const float cycle = std::fmod(t * speed * 0.5f, 2.0f);
    const float blade_pos = cycle < 1.0f ? cycle : 2.0f - cycle;
    
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      const float dist = std::abs(pos - blade_pos);
      float level = 0.0f;
      
      if (dist < blade_width) {
        level = 1.0f - (dist / blade_width);
        level = level * level;  // Sharp falloff
      }
      
      const Rgb col = gradient.empty() ? c1 : sample_gradient(gradient, pos);
      *dst++ = to_byte(col.r * brightness * level);
      *dst++ = to_byte(col.g * brightness * level);
      *dst++ = to_byte(col.b * brightness * level);
    }
    return frame;
  }

  // Strobe - beat-synchronized strobe (LedFX style)
  if (name_lower.find("strobe") != std::string::npos) {
    // LedFX strobe: flash on beat detection
    static float strobe_decay = 0.0f;
    
    if (beat > 0.7f) {
      strobe_decay = 1.0f;
    }
    
    const float level = strobe_decay * intensity;
    strobe_decay *= 0.7f;  // Fast decay
    
    for (uint16_t i = 0; i < pixels; ++i) {
      *dst++ = to_byte(c1.r * brightness * level);
      *dst++ = to_byte(c1.g * brightness * level);
      *dst++ = to_byte(c1.b * brightness * level);
    }
    return frame;
  }

  // Pulse - beat-synchronized expanding pulse (LedFX style)
  if (name_lower.find("pulse") != std::string::npos) {
    static float pulse_radius = 0.0f;
    static float pulse_brightness = 0.0f;
    
    // Trigger on beat
    if (beat > 0.7f && pulse_radius < 0.1f) {
      pulse_radius = 0.01f;
      pulse_brightness = 1.0f;
    }
    
    // Expand and fade
    pulse_radius += 0.03f * speed;
    pulse_brightness *= 0.95f;
    
    if (pulse_radius > 1.0f) {
      pulse_radius = 0.0f;
    }
    
    const float center = 0.5f;
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      const float dist = std::abs(pos - center);
      float level = 0.0f;
      
      // Ring effect
      if (std::abs(dist - pulse_radius * 0.5f) < 0.05f) {
        level = pulse_brightness * intensity;
      }
      
      const Rgb col = gradient.empty() ? c1 : sample_gradient(gradient, pos);
      *dst++ = to_byte(col.r * brightness * level);
      *dst++ = to_byte(col.g * brightness * level);
      *dst++ = to_byte(col.b * brightness * level);
    }
    return frame;
  }

  // Melt - flowing gradient with audio distortion (LedFX style)
  if (name_lower.find("melt") != std::string::npos) {
    const float base_flow = t * speed * 0.3f;
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      
      // Flowing gradient with audio-reactive distortion
      float phase = pos + base_flow;
      // Add sine distortion based on audio
      phase += sinf(pos * 6.0f + t * speed * 2.0f) * mid * 0.2f;
      phase += sinf(pos * 12.0f - t * speed) * treble * 0.1f;
      phase = std::fmod(phase, 1.0f);
      if (phase < 0) phase += 1.0f;
      
      const Rgb col = gradient.empty() ? hsv_to_rgb(phase, 1.0f, 1.0f) : sample_gradient(gradient, phase);
      const float level = 0.4f + energy * 0.6f * intensity;
      *dst++ = to_byte(col.r * brightness * level);
      *dst++ = to_byte(col.g * brightness * level);
      *dst++ = to_byte(col.b * brightness * level);
    }
    return frame;
  }

  // Fade - smooth color transitions (LedFX style)
  if (name_lower.find("fade") != std::string::npos) {
    const float phase = std::fmod(t * speed * 0.2f, 1.0f);
    const Rgb col = gradient.empty() ? hsv_to_rgb(phase, 1.0f, 1.0f) : sample_gradient(gradient, phase);
    const float level = effect.audio_link ? (0.3f + energy * 0.7f * intensity) : intensity;
    
    for (uint16_t i = 0; i < pixels; ++i) {
      *dst++ = to_byte(col.r * brightness * level);
      *dst++ = to_byte(col.g * brightness * level);
      *dst++ = to_byte(col.b * brightness * level);
    }
    return frame;
  }

  // Blocks - audio-reactive color blocks (LedFX style)
  if (name_lower.find("block") != std::string::npos) {
    const uint8_t block_size = std::max<uint8_t>(1, pixels / 8);
    
    for (uint16_t i = 0; i < pixels; ++i) {
      const uint8_t block_idx = i / block_size;
      // Alternate blocks respond to different frequencies
      float block_level;
      switch (block_idx % 3) {
        case 0: block_level = bass; break;
        case 1: block_level = mid; break;
        default: block_level = treble; break;
      }
      block_level *= intensity;
      
      const float grad_pos = static_cast<float>(block_idx * block_size) / static_cast<float>(pixels);
      const Rgb col = gradient.empty() ? hsv_to_rgb(grad_pos, 1.0f, 1.0f) : sample_gradient(gradient, grad_pos);
      *dst++ = to_byte(col.r * brightness * block_level);
      *dst++ = to_byte(col.g * brightness * block_level);
      *dst++ = to_byte(col.b * brightness * block_level);
    }
    return frame;
  }

  // Beat - flash on beat detection (LedFX style)
  if (name_lower.find("beat") != std::string::npos && name_lower.find("heart") == std::string::npos) {
    static float beat_level = 0.0f;
    
    if (beat > 0.7f) {
      beat_level = 1.0f;
    }
    
    const float level = beat_level * intensity;
    beat_level *= 0.85f;  // Decay
    
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      const Rgb col = gradient.empty() ? c1 : sample_gradient(gradient, pos);
      *dst++ = to_byte(col.r * brightness * level);
      *dst++ = to_byte(col.g * brightness * level);
      *dst++ = to_byte(col.b * brightness * level);
    }
    return frame;
  }

  // Fire - audio-reactive fire
  if (name_lower.find("fire") != std::string::npos) {
    std::string state_key = "fire_ledfx_" + std::to_string(led_count);
    auto& heat = get_state(state_key, pixels);
    
    const float cooling_base = 20.0f + (1.0f - intensity) * 30.0f;
    const float sparking = 50.0f + bass * 150.0f * intensity;
    
    // Cool down
    for (uint16_t i = 0; i < pixels; ++i) {
      const float cool = (static_cast<float>(esp_random() % 100) / 100.0f) * cooling_base / pixels;
      heat[i] = std::max(0.0f, heat[i] - cool);
    }
    
    // Heat rises
    for (int k = pixels - 1; k >= 2; --k) {
      heat[k] = (heat[k - 1] + heat[k - 2] * 2.0f) / 3.0f;
    }
    
    // Sparks
    if ((static_cast<float>(esp_random()) / 0xFFFFFFFF) < sparking / 255.0f) {
      const int y = esp_random() % std::min(7, static_cast<int>(pixels));
      heat[y] = std::min(1.0f, heat[y] + 0.6f + (static_cast<float>(esp_random()) / 0xFFFFFFFF) * 0.4f);
    }
    
    // Render
    for (uint16_t i = 0; i < pixels; ++i) {
      const uint16_t idx = direction > 0 ? i : (pixels - 1 - i);
      const float h = heat[idx];
      // Heat to color
      Rgb col;
      if (h < 0.33f) {
        col = {h * 3.0f, 0.0f, 0.0f};
      } else if (h < 0.66f) {
        col = {1.0f, (h - 0.33f) * 3.0f * 0.5f, 0.0f};
      } else {
        col = {1.0f, 0.5f + (h - 0.66f) * 1.5f, (h - 0.66f) * 0.9f};
      }
      *dst++ = to_byte(col.r * brightness);
      *dst++ = to_byte(col.g * brightness);
      *dst++ = to_byte(col.b * brightness);
    }
    return frame;
  }

  // Rainbow - gradient flow (non-audio)
  if (name_lower.find("rainbow") != std::string::npos || name_lower.find("gradient") != std::string::npos) {
    const float offset = t * speed * 0.15f * direction;
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      const float hue = std::fmod(pos + offset, 1.0f);
      const Rgb col = gradient.empty() ? hsv_to_rgb(hue, 1.0f, 1.0f) : sample_gradient(gradient, hue);
      *dst++ = to_byte(col.r * brightness * intensity);
      *dst++ = to_byte(col.g * brightness * intensity);
      *dst++ = to_byte(col.b * brightness * intensity);
    }
    return frame;
  }

  // Plasma - animated plasma
  if (name_lower.find("plasma") != std::string::npos) {
    const float time_factor = t * speed * 0.5f;
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / static_cast<float>(pixels);
      float v = sinf(pos * 10.0f + time_factor);
      v += sinf((pos * 10.0f + time_factor * 0.5f) * 0.5f);
      v += sinf((pos * 10.0f * 0.3f + time_factor * 0.3f) * 1.5f);
      v = (v + 3.0f) / 6.0f;
      
      const Rgb col = gradient.empty() ? hsv_to_rgb(v + time_factor * 0.05f, 0.8f, 1.0f) : sample_gradient(gradient, v);
      *dst++ = to_byte(col.r * brightness * intensity);
      *dst++ = to_byte(col.g * brightness * intensity);
      *dst++ = to_byte(col.b * brightness * intensity);
    }
    return frame;
  }

  // Default: gradient flow
  const float offset = t * speed * 0.2f * direction;
  for (uint16_t i = 0; i < pixels; ++i) {
    const float pos = static_cast<float>(i) / static_cast<float>(pixels);
    float phase = std::fmod(pos + offset, 1.0f);
    const Rgb col = gradient.empty() ?
      Rgb{c1.r * (1.0f - phase) + c2.r * phase,
          c1.g * (1.0f - phase) + c2.g * phase,
          c1.b * (1.0f - phase) + c2.b * phase} :
      sample_gradient(gradient, phase);
    *dst++ = to_byte(col.r * brightness * intensity);
    *dst++ = to_byte(col.g * brightness * intensity);
    *dst++ = to_byte(col.b * brightness * intensity);
  }
  return frame;
}

}  // namespace ledfx_effects





