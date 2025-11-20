#pragma once
#include "led_engine/types.hpp"
#include "esp_err.h"

struct AudioDiagnostics {
  AudioSourceType source{AudioSourceType::None};
  uint32_t sample_rate{0};
  bool stereo{false};
  bool running{false};
};

esp_err_t led_audio_apply_config(const AudioConfig& cfg);
AudioDiagnostics led_audio_get_diagnostics();
