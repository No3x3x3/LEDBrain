#pragma once

#include <string>
#include <unordered_map>

// Effect metadata for automatic engine selection
struct EffectMetadata {
  bool audio_reactive{false};        // Effect is designed for audio reactivity
  bool supports_audio_toggle{false}; // Effect can work with or without audio
  std::string default_engine{"wled"}; // Default engine: "wled" or "ledfx"
  std::string category{"Classic"};     // Effect category
};

// Get effect metadata
const EffectMetadata* get_effect_metadata(const std::string& effect_name);

// Automatically select engine based on effect and audio_link setting
// Returns: "wled" or "ledfx"
std::string select_engine_auto(const std::string& effect_name, bool audio_link);

// Check if effect supports audio toggle
bool effect_supports_audio_toggle(const std::string& effect_name);

// Check if effect is audio-reactive
bool effect_is_audio_reactive(const std::string& effect_name);
