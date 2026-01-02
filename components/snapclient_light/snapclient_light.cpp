#include "snapclient_light.hpp"
#include "esp_log.h"
#include "led_engine/audio_pipeline.hpp"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "esp_timer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <vector>

namespace {

static const char* TAG = "snapclient";
static TaskHandle_t s_task = nullptr;
static bool s_running = false;
static SnapcastConfig s_cfg{};

struct FftBin {
  float re{0.0f};
  float im{0.0f};
};

void apply_hann(std::vector<float>& buf) {
  const size_t N = buf.size();
  for (size_t i = 0; i < N; ++i) {
    float w = 0.5f * (1.0f - cosf(2.0f * static_cast<float>(M_PI) * i / (N - 1)));
    buf[i] *= w;
  }
}

void fft(std::vector<FftBin>& data) {
  const size_t n = data.size();
  for (size_t i = 1, j = 0; i < n; ++i) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      std::swap(data[i], data[j]);
    }
  }
  for (size_t len = 2; len <= n; len <<= 1) {
    const float ang = -2.0f * static_cast<float>(M_PI) / len;
    const FftBin wlen{cosf(ang), sinf(ang)};
    for (size_t i = 0; i < n; i += len) {
      FftBin w{1.0f, 0.0f};
      for (size_t j = 0; j < len / 2; ++j) {
        FftBin u = data[i + j];
        FftBin v{data[i + j + len / 2].re * w.re - data[i + j + len / 2].im * w.im,
                 data[i + j + len / 2].re * w.im + data[i + j + len / 2].im * w.re};
        data[i + j].re = u.re + v.re;
        data[i + j].im = u.im + v.im;
        data[i + j + len / 2].re = u.re - v.re;
        data[i + j + len / 2].im = u.im - v.im;
        const float new_re = w.re * wlen.re - w.im * wlen.im;
        w.im = w.re * wlen.im + w.im * wlen.re;
        w.re = new_re;
      }
    }
  }
}

void compute_metrics(const int16_t* pcm, size_t samples, uint32_t sample_rate, bool stereo) {
  if (!pcm || samples == 0) {
    return;
  }
  const size_t fft_size = 1024;
  std::vector<float> mono;
  mono.reserve(fft_size);

  float energy_l = 0.0f, energy_r = 0.0f;
  size_t count = 0;
  for (size_t i = 0; i + (stereo ? 1 : 0) < samples && count < fft_size; i += (stereo ? 2 : 1)) {
    const float l = static_cast<float>(pcm[i]) / 32768.0f;
    const float r = stereo ? static_cast<float>(pcm[i + 1]) / 32768.0f : l;
    mono.push_back((l + r) * 0.5f);
    energy_l += l * l;
    energy_r += r * r;
    ++count;
  }
  if (mono.size() < fft_size) {
    mono.resize(fft_size, 0.0f);
  }
  apply_hann(mono);
  std::vector<FftBin> bins(fft_size);
  for (size_t i = 0; i < fft_size; ++i) {
    bins[i].re = mono[i];
  }
  fft(bins);

  const float norm = 1.0f / static_cast<float>(fft_size);
  std::vector<float> magn(fft_size / 2);
  for (size_t i = 0; i < magn.size(); ++i) {
    magn[i] = sqrtf(bins[i].re * bins[i].re + bins[i].im * bins[i].im) * norm;
  }

  auto band_energy = [&](float f_low, float f_high) {
    const float bin_hz = static_cast<float>(sample_rate) / fft_size;
    size_t i_low = static_cast<size_t>(f_low / bin_hz);
    size_t i_high = static_cast<size_t>(f_high / bin_hz);
    i_low = std::min(i_low, magn.size() - 1);
    i_high = std::min(i_high, magn.size() - 1);
    float acc = 0.0f;
    size_t n = 0;
    for (size_t i = i_low; i <= i_high; ++i) {
      acc += magn[i];
      ++n;
    }
    return n > 0 ? acc / n : 0.0f;
  };

  AudioMetrics metrics{};
  metrics.energy = std::min(1.0f, sqrtf((energy_l + energy_r) / std::max<size_t>(1, count)));  // RMS instead of raw
  metrics.energy_left = std::min(1.0f, sqrtf(energy_l / std::max<size_t>(1, count)));
  metrics.energy_right = std::min(1.0f, sqrtf(energy_r / std::max<size_t>(1, count)));
  
  // More precise frequency bands (like LEDFx)
  const float sub_bass = band_energy(20.0f, 60.0f);
  const float bass_low = band_energy(60.0f, 120.0f);
  const float bass_high = band_energy(120.0f, 250.0f);
  metrics.bass = std::min(1.5f, (sub_bass * 6.0f + bass_low * 5.0f + bass_high * 4.0f) / 3.0f);
  
  const float mid_low = band_energy(250.0f, 500.0f);
  const float mid_mid = band_energy(500.0f, 1000.0f);
  const float mid_high = band_energy(1000.0f, 2000.0f);
  metrics.mid = std::min(1.5f, (mid_low * 3.0f + mid_mid * 2.8f + mid_high * 2.5f) / 3.0f);
  
  const float treble_low = band_energy(2000.0f, 4000.0f);
  const float treble_mid = band_energy(4000.0f, 8000.0f);
  const float treble_high = band_energy(8000.0f, 12000.0f);
  metrics.treble = std::min(1.5f, (treble_low * 2.2f + treble_mid * 2.0f + treble_high * 1.8f) / 3.0f);

  // Improved beat detection - use bass energy changes and frequency analysis
  static float prev_energy = 0.0f;
  static float prev_bass = 0.0f;
  static float beat_history[8] = {0.0f};
  static size_t beat_history_idx = 0;
  static uint64_t last_beat_us = 0;
  static float beat_envelope = 0.0f;
  
  const float delta_energy = metrics.energy - prev_energy;
  const float delta_bass = metrics.bass - prev_bass;
  prev_energy = metrics.energy;
  prev_bass = metrics.bass;
  
  // Beat detection: look for sharp increases in bass (most reliable for beats)
  const float bass_threshold = 0.15f;
  const float bass_beat_strength = std::max(0.0f, delta_bass - bass_threshold) * 8.0f;
  
  // Also check overall energy spike
  const float energy_spike = std::max(0.0f, delta_energy - 0.05f) * 5.0f;
  
  // Combine both with bass priority
  float beat_trigger = std::min(1.0f, bass_beat_strength * 0.7f + energy_spike * 0.3f);
  
  const uint64_t now_us = esp_timer_get_time();
  if (beat_trigger > 0.3f && (now_us - last_beat_us) > 100'000) {  // Min 100ms between beats
    beat_history[beat_history_idx] = beat_trigger;
    beat_history_idx = (beat_history_idx + 1) % 8;
    last_beat_us = now_us;
    beat_envelope = std::min(1.0f, beat_trigger);
  } else {
    // Decay envelope
    beat_envelope *= 0.88f;  // Smooth decay
  }
  
  // Calculate tempo from beat intervals
  static uint64_t beat_times[16] = {0};
  static size_t beat_time_idx = 0;
  static bool beat_times_filled = false;
  
  if (beat_trigger > 0.5f) {
    beat_times[beat_time_idx] = now_us;
    beat_time_idx = (beat_time_idx + 1) % 16;
    if (beat_time_idx == 0) beat_times_filled = true;
  }
  
  float tempo_bpm = 0.0f;
  if (beat_times_filled || beat_time_idx > 4) {
    size_t count_valid = beat_times_filled ? 16 : beat_time_idx;
    uint64_t total_interval = 0;
    size_t intervals = 0;
    for (size_t i = 1; i < count_valid; ++i) {
      size_t prev_idx = (beat_time_idx + 15 - i) % 16;
      size_t curr_idx = (beat_time_idx + 16 - i) % 16;
      if (beat_times[curr_idx] > beat_times[prev_idx]) {
        total_interval += (beat_times[curr_idx] - beat_times[prev_idx]);
        intervals++;
      }
    }
    if (intervals > 0) {
      const float avg_interval_ms = (total_interval / static_cast<float>(intervals)) / 1000.0f;
      if (avg_interval_ms > 0.0f) {
        tempo_bpm = 60000.0f / avg_interval_ms;
        tempo_bpm = std::max(60.0f, std::min(200.0f, tempo_bpm));  // Clamp to reasonable range
      }
    }
  }
  
  metrics.beat = std::min(1.0f, beat_envelope);
  metrics.tempo_bpm = tempo_bpm;
  
  // Store magnitude spectrum for custom frequency range calculations
  metrics.magnitude_spectrum = magn;
  metrics.sample_rate = sample_rate;

  led_audio_set_metrics(metrics);
}

int connect_snap(const SnapcastConfig& cfg) {
  struct addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* res = nullptr;
  char port_str[8];
  snprintf(port_str, sizeof(port_str), "%u", cfg.port == 0 ? 1704 : cfg.port);
  int err = getaddrinfo(cfg.host.c_str(), port_str, &hints, &res);
  if (err != 0 || !res) {
    ESP_LOGE(TAG, "resolve failed %s:%s err=%d", cfg.host.c_str(), port_str, err);
    return -1;
  }
  int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sock < 0) {
    ESP_LOGE(TAG, "socket failed");
    freeaddrinfo(res);
    return -1;
  }
  if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
    ESP_LOGE(TAG, "connect failed");
    close(sock);
    freeaddrinfo(res);
    return -1;
  }
  freeaddrinfo(res);
  return sock;
}

void snap_task(void*) {
  ESP_LOGI(TAG, "Snapclient light starting -> %s:%u", s_cfg.host.c_str(), s_cfg.port);
  while (s_running) {
    const int sock = connect_snap(s_cfg);
    if (sock < 0) {
      vTaskDelay(pdMS_TO_TICKS(1500));
      continue;
    }
    ESP_LOGI(TAG, "Snapclient connected (raw PCM expected)");
    const uint32_t sample_rate = 48000;
    const bool stereo = true;
    const size_t frame_samples = 2048;
    std::vector<int16_t> buffer(frame_samples);
    std::deque<int16_t> pcm_buffer;
    const uint32_t target_delay_ms = s_cfg.latency_ms > 0 ? s_cfg.latency_ms : 60;
    const uint32_t min_delay_ms = target_delay_ms > 12 ? target_delay_ms - 12 : target_delay_ms;
    const uint32_t max_delay_ms = target_delay_ms + 12;
    const size_t max_buffer_samples = sample_rate * 2;  // ~1s safety
    uint64_t last_adjust_us = esp_timer_get_time();
    uint64_t last_tick_us = esp_timer_get_time();
    uint64_t samples_seen = 0;

    while (s_running) {
      const int to_read = buffer.size() * sizeof(int16_t);
      int r = recv(sock, reinterpret_cast<char*>(buffer.data()), to_read, 0);
      if (r <= 0) {
        ESP_LOGW(TAG, "Snap recv ended");
        break;
      }
      const size_t samples = r / sizeof(int16_t);
      for (size_t i = 0; i < samples; ++i) {
        if (pcm_buffer.size() >= max_buffer_samples) {
          pcm_buffer.pop_front();
        }
        pcm_buffer.push_back(buffer[i]);
        samples_seen++;
      }

      const float buffered_ms = (pcm_buffer.size() / static_cast<float>(stereo ? 2 : 1)) * 1000.0f / sample_rate;
      if (buffered_ms < min_delay_ms) {
        continue;
      }

      // Lightweight jitter/latency control: trim or pad towards target window
      const uint64_t now_us = esp_timer_get_time();
      if (now_us - last_adjust_us > 10'000) {
        // Estimate effective sample clock vs wall time to nudge buffer
        const uint64_t elapsed_us = now_us - last_tick_us;
        if (elapsed_us > 0) {
          const float expected_samples = (elapsed_us / 1e6f) * sample_rate * (stereo ? 2 : 1);
          const float drift = (samples_seen - expected_samples) / std::max(1.0f, expected_samples);
          // Small PLL: if drift positive, trim buffer slightly; if negative, avoid trimming
          if (drift > 0.02f && pcm_buffer.size() > frame_samples * 2) {
            pcm_buffer.pop_front();
          }
          samples_seen = 0;
          last_tick_us = now_us;
        }
        if (buffered_ms > max_delay_ms) {
          const float excess_ms = buffered_ms - target_delay_ms;
          size_t drop_samples =
              std::min(static_cast<size_t>((excess_ms / 1000.0f) * sample_rate * (stereo ? 2 : 1)),
                       pcm_buffer.size() / 4);
          for (size_t i = 0; i < drop_samples && !pcm_buffer.empty(); ++i) {
            pcm_buffer.pop_front();
          }
        } else if (buffered_ms < min_delay_ms) {
          // Light padding: duplicate last samples to reach window (only if buffer very low)
          const float missing_ms = target_delay_ms - buffered_ms;
          const size_t add = static_cast<size_t>((missing_ms / 1000.0f) * sample_rate * (stereo ? 2 : 1));
          int16_t tail = pcm_buffer.empty() ? 0 : pcm_buffer.back();
          for (size_t i = 0; i < add && pcm_buffer.size() < max_buffer_samples; ++i) {
            pcm_buffer.push_back(tail);
          }
        }
        last_adjust_us = now_us;
      }

      const size_t chunk = std::min(pcm_buffer.size(), frame_samples);
      if (chunk >= frame_samples) {
        std::vector<int16_t> chunk_buf(frame_samples);
        for (size_t i = 0; i < frame_samples; ++i) {
          chunk_buf[i] = pcm_buffer.front();
          pcm_buffer.pop_front();
        }
        compute_metrics(chunk_buf.data(), frame_samples, sample_rate, stereo);
      }
    }
    close(sock);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  ESP_LOGI(TAG, "Snapclient light stopped");
  s_task = nullptr;
}

}  // namespace

esp_err_t snapclient_light_start(const SnapcastConfig& cfg) {
  s_cfg = cfg;
  if (!s_cfg.enabled || s_cfg.host.empty()) {
    ESP_LOGW(TAG, "Snapclient not started (disabled or host empty)");
    return ESP_ERR_INVALID_STATE;
  }
  if (s_task) {
    return ESP_OK;
  }
  s_running = true;
  const BaseType_t res = xTaskCreatePinnedToCore(snap_task, "snapclient", 4096, nullptr, 4, &s_task, tskNO_AFFINITY);
  if (res != pdPASS) {
    s_running = false;
    s_task = nullptr;
    return ESP_FAIL;
  }
  return ESP_OK;
}

void snapclient_light_stop() {
  s_running = false;
  if (s_task) {
    vTaskDelay(pdMS_TO_TICKS(50));
    vTaskDelete(s_task);
    s_task = nullptr;
  }
}
