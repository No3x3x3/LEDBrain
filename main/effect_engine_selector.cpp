#include "effect_engine_selector.hpp"
#include <algorithm>
#include <cstring>

namespace {

// Convert to lowercase for comparison
std::string lower(const std::string& s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

// Effect metadata database
static const std::unordered_map<std::string, EffectMetadata> s_effect_metadata = {
  // WLED Audio-Reactive Effects (can work with or without audio)
  {"Beat Pulse", {true, true, "wled", "Rhythm"}},
  {"Beat Bars", {true, true, "wled", "Rhythm"}},
  {"Beat Scatter", {true, true, "wled", "Rhythm"}},
  {"Beat Light", {true, true, "wled", "Rhythm"}},
  {"Energy Flow", {true, true, "wled", "Energy"}},
  {"Energy Burst", {true, true, "wled", "Energy"}},
  {"Energy Waves", {true, true, "wled", "Energy"}},
  {"Power+", {true, true, "wled", "Energy"}},
  {"Power Cycle", {true, true, "wled", "Energy"}},
  
  // WLED Non-Audio Effects (visual only)
  {"Solid", {false, false, "wled", "Classic"}},
  {"Blink", {false, false, "wled", "Classic"}},
  {"Breathe", {false, false, "wled", "Classic"}},
  {"Chase", {false, false, "wled", "Classic"}},
  {"Colorloop", {false, false, "wled", "Classic"}},
  {"Rainbow", {false, false, "wled", "Classic"}},
  {"Rainbow Runner", {false, false, "wled", "Classic"}},
  {"Rainbow Bands", {false, false, "wled", "Classic"}},
  {"Rain", {false, false, "wled", "Classic"}},
  {"Rain (Dual)", {false, false, "wled", "Classic"}},
  {"Meteor", {false, false, "wled", "Classic"}},
  {"Meteor Smooth", {false, false, "wled", "Classic"}},
  {"Candle", {false, false, "wled", "Classic"}},
  {"Candle Multi", {false, false, "wled", "Classic"}},
  {"Scanner", {false, false, "wled", "Classic"}},
  {"Scanner Dual", {false, false, "wled", "Classic"}},
  {"Theater", {false, false, "wled", "Classic"}},
  {"Noise", {false, false, "wled", "Classic"}},
  {"Sinelon", {false, false, "wled", "Classic"}},
  {"Fireworks", {false, false, "wled", "Classic"}},
  {"Fire 2012", {false, false, "wled", "Classic"}},
  {"Heartbeat", {false, false, "wled", "Classic"}},
  {"Ripple", {false, false, "wled", "Classic"}},
  {"Pacifica", {false, false, "wled", "Classic"}},
  {"Strobe", {false, false, "wled", "Rhythm"}},
  
  // LEDFx Audio-Reactive Effects (require audio)
  {"Energy Waves", {true, false, "ledfx", "Energy"}},
  {"Plasma", {true, false, "ledfx", "Ambient"}},
  {"Matrix", {true, false, "ledfx", "Ambient"}},
  {"Hyperspace", {true, false, "ledfx", "Energy"}},
  {"Paintbrush", {true, false, "ledfx", "Rhythm"}},
  {"3D GEQ", {true, false, "ledfx", "Rhythm"}},
  {"Waves", {true, false, "ledfx", "Ambient"}},
  {"Aura", {true, false, "ledfx", "Ambient"}},
  {"Ripple Flow", {true, false, "ledfx", "Ambient"}},
  
  // LEDFx Effects that can work without audio (but better with audio)
  {"Rain", {true, true, "ledfx", "Ambient"}},  // LEDFx Rain can work without audio
  {"Fire", {true, true, "ledfx", "Ambient"}},  // LEDFx Fire can work without audio
};

}  // namespace

const EffectMetadata* get_effect_metadata(const std::string& effect_name) {
  const std::string key = lower(effect_name);
  
  // Try exact match first
  auto it = s_effect_metadata.find(effect_name);
  if (it != s_effect_metadata.end()) {
    return &it->second;
  }
  
  // Try lowercase match
  it = s_effect_metadata.find(key);
  if (it != s_effect_metadata.end()) {
    return &it->second;
  }
  
  // Try partial match (for effects with variations)
  for (const auto& [name, meta] : s_effect_metadata) {
    if (lower(name).find(key) != std::string::npos || key.find(lower(name)) != std::string::npos) {
      return &meta;
    }
  }
  
  return nullptr;
}

std::string select_engine_auto(const std::string& effect_name, bool audio_link) {
  const EffectMetadata* meta = get_effect_metadata(effect_name);
  
  if (!meta) {
    // Unknown effect - default to WLED
    return "wled";
  }
  
  // If effect is LEDFx-only (requires audio), use LEDFx
  if (meta->default_engine == "ledfx" && !meta->supports_audio_toggle) {
    return "ledfx";
  }
  
  // If effect supports audio toggle and audio_link is enabled
  if (meta->supports_audio_toggle && audio_link) {
    // Use LEDFx for better audio reactivity, or WLED if it's a WLED audio-reactive effect
    if (meta->default_engine == "wled") {
      return "wled";  // WLED audio-reactive effects stay in WLED engine
    } else {
      return "ledfx";  // LEDFx effects with audio
    }
  }
  
  // If audio_link is disabled, use default engine
  if (!audio_link) {
    return meta->default_engine;
  }
  
  // If effect is audio-reactive and audio_link is enabled
  if (meta->audio_reactive && audio_link) {
    // Prefer LEDFx for better audio processing, unless it's a WLED-specific effect
    if (meta->default_engine == "wled") {
      return "wled";  // Keep WLED effects in WLED engine
    } else {
      return "ledfx";
    }
  }
  
  // Default: use default engine
  return meta->default_engine;
}

bool effect_supports_audio_toggle(const std::string& effect_name) {
  const EffectMetadata* meta = get_effect_metadata(effect_name);
  return meta ? meta->supports_audio_toggle : false;
}

bool effect_is_audio_reactive(const std::string& effect_name) {
  const EffectMetadata* meta = get_effect_metadata(effect_name);
  return meta ? meta->audio_reactive : false;
}
