#include "led_engine/audio_pipeline.hpp"
#include "esp_log.h"

static const char* TAG = "led-audio";
static AudioDiagnostics g_diag{};
static AudioMetrics g_metrics{};
static bool g_audio_force_disabled = false;  // Allow manual disable even if source is configured

esp_err_t led_audio_apply_config(const AudioConfig& cfg) {
  g_diag.source = cfg.source;
  g_diag.sample_rate = cfg.sample_rate;
  g_diag.stereo = cfg.stereo;
  // Only set running if not manually disabled AND source is configured
  g_diag.running = !g_audio_force_disabled &&
                   cfg.source != AudioSourceType::None &&
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

esp_err_t led_audio_set_running(bool running) {
  g_audio_force_disabled = !running;
  // Update running state: only true if not force-disabled AND source is configured
  // Note: snapcast.enabled check is done in led_audio_apply_config, here we just respect the force flag
  g_diag.running = !g_audio_force_disabled && g_diag.source != AudioSourceType::None;
  ESP_LOGI(TAG, "Audio running state set to %s (force_disabled=%d, source=%d)", 
           g_diag.running ? "true" : "false", 
           g_audio_force_disabled ? 1 : 0,
           static_cast<int>(g_diag.source));
  return ESP_OK;
}

float led_audio_get_band_value(const std::string& band_name) {
  if (g_metrics.magnitude_spectrum.empty() || g_metrics.sample_rate == 0) {
    return 0.0f;
  }
  
  const size_t fft_size = g_metrics.magnitude_spectrum.size() * 2;
  const float bin_hz = static_cast<float>(g_metrics.sample_rate) / static_cast<float>(fft_size);
  
  auto band_energy = [&](float f_low, float f_high) {
    size_t i_low = static_cast<size_t>(f_low / bin_hz);
    size_t i_high = static_cast<size_t>(f_high / bin_hz);
    i_low = std::min(i_low, g_metrics.magnitude_spectrum.size() - 1);
    i_high = std::min(i_high, g_metrics.magnitude_spectrum.size() - 1);
    if (i_low > i_high) return 0.0f;
    float acc = 0.0f;
    size_t n = 0;
    for (size_t i = i_low; i <= i_high; ++i) {
      acc += g_metrics.magnitude_spectrum[i];
      ++n;
    }
    return n > 0 ? acc / static_cast<float>(n) : 0.0f;
  };
  
  // Individual bands
  if (band_name == "sub_bass") return std::min(1.5f, band_energy(20.0f, 60.0f) * 6.0f);
  if (band_name == "bass_low") return std::min(1.5f, band_energy(60.0f, 120.0f) * 5.0f);
  if (band_name == "bass_high") return std::min(1.5f, band_energy(120.0f, 250.0f) * 4.0f);
  if (band_name == "mid_low") return std::min(1.5f, band_energy(250.0f, 500.0f) * 3.0f);
  if (band_name == "mid_mid") return std::min(1.5f, band_energy(500.0f, 1000.0f) * 2.8f);
  if (band_name == "mid_high") return std::min(1.5f, band_energy(1000.0f, 2000.0f) * 2.5f);
  if (band_name == "treble_low") return std::min(1.5f, band_energy(2000.0f, 4000.0f) * 2.2f);
  if (band_name == "treble_mid") return std::min(1.5f, band_energy(4000.0f, 8000.0f) * 2.0f);
  if (band_name == "treble_high") return std::min(1.5f, band_energy(8000.0f, 12000.0f) * 1.8f);
  
  // Composite bands
  if (band_name == "bass") return g_metrics.bass;
  if (band_name == "mid") return g_metrics.mid;
  if (band_name == "treble") return g_metrics.treble;
  
  // Metrics
  if (band_name == "energy") return g_metrics.energy;
  if (band_name == "beat") return g_metrics.beat;
  if (band_name == "tempo_bpm") return g_metrics.tempo_bpm;
  
  return 0.0f;
}