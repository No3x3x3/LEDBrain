#include "led_engine/audio_pipeline.hpp"
#include "esp_log.h"

static const char* TAG = "led-audio";
static AudioDiagnostics g_diag{};

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
