#pragma once
#include "led_engine/types.hpp"
#include "esp_err.h"

struct AudioDiagnostics {
  AudioSourceType source{AudioSourceType::None};
  uint32_t sample_rate{0};
  bool stereo{false};
  bool running{false};
};

struct AudioMetrics {
  float energy{0.0f};      // overall normalized 0..1
  float energy_left{0.0f};
  float energy_right{0.0f};
  float bass{0.0f};
  float mid{0.0f};
  float treble{0.0f};
  float beat{0.0f};        // 0..1 envelope
  float tempo_bpm{0.0f};   // estimated tempo
};

esp_err_t led_audio_apply_config(const AudioConfig& cfg);
AudioDiagnostics led_audio_get_diagnostics();
AudioMetrics led_audio_get_metrics();
void led_audio_set_metrics(const AudioMetrics& metrics);
