#include "led_engine/audio_pipeline.hpp"
#include "esp_log.h"

static const char* TAG = "led-audio";
static AudioDiagnostics g_diag{};
static AudioMetrics g_metrics{};

esp_err_t led_audio_apply_config(const AudioConfig& cfg) {
  g_diag.source = cfg.source;
  g_diag.sample_rate = cfg.sample_rate;
  g_diag.stereo = cfg.stereo;
  g_diag.running = cfg.source != AudioSourceType::None &&
                   (cfg.source != AudioSourceType::Snapcast || cfg.snapcast.enabled);

  ESP_LOGI(TAG,
           "Audio pipeline: source=%s sr=%u stereo=%d latency=%u ms",
           g_diag.source == AudioSourceType::Snapcast
               ? "snapcast"
               : (g_diag.source == AudioSourceType::LineInput ? "line" : "none"),
           g_diag.sample_rate,
           g_diag.stereo,
           cfg.snapcast.latency_ms);
  return ESP_OK;
}

AudioDiagnostics led_audio_get_diagnostics() {
  return g_diag;
}

AudioMetrics led_audio_get_metrics() {
  return g_metrics;
}

void led_audio_set_metrics(const AudioMetrics& metrics) {
  g_metrics = metrics;
  // Clamp to sane range
  auto clamp01f = [](float v) { return v < 0.0f ? 0.0f : (v > 1.5f ? 1.5f : v); };
  g_metrics.energy = clamp01f(g_metrics.energy);
  g_metrics.energy_left = clamp01f(g_metrics.energy_left);
  g_metrics.energy_right = clamp01f(g_metrics.energy_right);
  g_metrics.bass = clamp01f(g_metrics.bass);
  g_metrics.mid = clamp01f(g_metrics.mid);
  g_metrics.treble = clamp01f(g_metrics.treble);
  g_metrics.beat = clamp01f(g_metrics.beat);
  // magnitude_spectrum and sample_rate are stored as-is (no clamping needed)
}

float led_audio_get_custom_energy(float freq_min, float freq_max) {
  if (freq_min <= 0.0f || freq_max <= 0.0f || freq_min >= freq_max) {
    return 0.0f;
  }
  if (g_metrics.magnitude_spectrum.empty() || g_metrics.sample_rate == 0) {
    return 0.0f;
  }
  const size_t fft_size = g_metrics.magnitude_spectrum.size() * 2;  // FFT size (magnitude is half)
  const float bin_hz = static_cast<float>(g_metrics.sample_rate) / static_cast<float>(fft_size);
  size_t i_low = static_cast<size_t>(freq_min / bin_hz);
  size_t i_high = static_cast<size_t>(freq_max / bin_hz);
  i_low = std::min(i_low, g_metrics.magnitude_spectrum.size() - 1);
  i_high = std::min(i_high, g_metrics.magnitude_spectrum.size() - 1);
  if (i_low > i_high) {
    return 0.0f;
  }
  float acc = 0.0f;
  size_t n = 0;
  for (size_t i = i_low; i <= i_high; ++i) {
    acc += g_metrics.magnitude_spectrum[i];
    ++n;
  }
  return n > 0 ? std::min(1.5f, acc / static_cast<float>(n)) : 0.0f;
}