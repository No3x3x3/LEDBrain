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
  // FFT magnitude spectrum for custom frequency range calculation
  // Stored as vector of magnitude values (one per FFT bin, up to Nyquist frequency)
  std::vector<float> magnitude_spectrum{};  // magnitude spectrum for custom frequency calculations
  uint32_t sample_rate{0};  // sample rate used for FFT
};

esp_err_t led_audio_apply_config(const AudioConfig& cfg);
AudioDiagnostics led_audio_get_diagnostics();
AudioMetrics led_audio_get_metrics();
void led_audio_set_metrics(const AudioMetrics& metrics);
// Calculate energy from custom frequency range (Hz)
// Returns energy from freq_min to freq_max, or 0.0f if range is invalid or spectrum not available
float led_audio_get_custom_energy(float freq_min, float freq_max);
