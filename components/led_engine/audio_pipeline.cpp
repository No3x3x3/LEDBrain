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
}
