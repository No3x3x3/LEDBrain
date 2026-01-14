#pragma once

#include "config.hpp"
#include "led_engine/audio_pipeline.hpp"
#include <cstdint>
#include <vector>

// LEDFx effects renderer
// These are audio-reactive effects inspired by LEDFx project
namespace ledfx_effects {

struct Rgb {
  float r{1.0f};
  float g{1.0f};
  float b{1.0f};
};

struct GradientStop {
  float pos{0.0f};
  Rgb color{};
};

// Render LEDFx effect
// time_s: time in seconds since effect start (for time-based animation normalization)
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
                                    float direction);

// Helper functions
Rgb parse_hex_color(const std::string& text, const Rgb& fallback);
std::vector<GradientStop> build_gradient_from_string(const std::string& text, const Rgb& fallback);
std::vector<GradientStop> palette_gradient(const std::string& name, const Rgb& c1, const Rgb& c2, const Rgb& c3);
Rgb sample_gradient(const std::vector<GradientStop>& stops, float t);

}  // namespace ledfx_effects





