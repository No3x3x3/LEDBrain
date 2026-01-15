#include "wled_effects.hpp"
#include "ledfx_effects.hpp"
#include "ddp_tx.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_engine/audio_pipeline.hpp"
#include "wled_discovery.hpp"
#include "esp_random.h"
#include "esp_timer.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace {

constexpr uint16_t kDefaultDdpPort = 4048;
static const char* TAG = "wled_fx";

// Cache to track which devices have DDP mode enabled (to avoid spamming API)
static std::unordered_map<std::string, uint64_t> ddp_mode_enabled_cache;
// Cache to store WLED state before enabling DDP mode (to restore later)
static std::unordered_map<std::string, std::string> wled_state_cache;
// Mutex to protect cache access (enable/disable can be called from different contexts)
static std::mutex cache_mutex;

std::string get_wled_state(const std::string& ip) {
  if (ip.empty()) {
    return "";
  }
  
  char url[96];
  snprintf(url, sizeof(url), "http://%s/json/state", ip.c_str());
  
  esp_http_client_config_t cfg = {};
  cfg.url = url;
  cfg.timeout_ms = 2000;
  cfg.skip_cert_common_name_check = true;
  cfg.method = HTTP_METHOD_GET;
  
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (!client) {
    ESP_LOGW(TAG, "Failed to init HTTP client for state get: %s", ip.c_str());
    return "";
  }
  
  std::string state_json;
  esp_err_t err = esp_http_client_perform(client);
  if (err == ESP_OK) {
    const int status_code = esp_http_client_get_status_code(client);
    if (status_code == 200) {
      const int content_length = esp_http_client_get_content_length(client);
      if (content_length > 0 && content_length < 4096) {  // Reasonable size limit
        char* buffer = static_cast<char*>(malloc(content_length + 1));
        if (buffer) {
          int data_read = esp_http_client_read_response(client, buffer, content_length);
          if (data_read > 0) {
            buffer[data_read] = '\0';
            state_json = buffer;
          }
          free(buffer);
        }
      }
    } else {
      ESP_LOGW(TAG, "Failed to get WLED state from %s: HTTP %d", ip.c_str(), status_code);
    }
  } else {
    ESP_LOGW(TAG, "Failed to get WLED state from %s: %s", ip.c_str(), esp_err_to_name(err));
  }
  
  esp_http_client_cleanup(client);
  return state_json;
}

void disable_wled_ddp_mode(const std::string& ip);
void enable_wled_ddp_mode(const std::string& ip) {
  if (ip.empty()) {
    return;
  }
  
  const uint64_t now_us = esp_timer_get_time();
  
  // Check cache - only enable DDP mode once when starting streaming
  // Don't spam the API - just enable once and let DDP packets keep it alive
  {
    std::lock_guard<std::mutex> lock(cache_mutex);
    auto it = ddp_mode_enabled_cache.find(ip);
    if (it != ddp_mode_enabled_cache.end()) {
      const uint64_t age_us = now_us - it->second;
      if (age_us < 300'000'000ULL) {  // 5 minutes - only re-enable if very stale
        return;  // Already enabled
      }
    }
  }
  
  // IMPORTANT: Always save current WLED state BEFORE enabling DDP mode
  // This ensures we have the latest state to restore when disabling DDP
  // We save it even if cache exists, to ensure we always have the most recent state
  {
    std::lock_guard<std::mutex> lock(cache_mutex);
    if (wled_state_cache.find(ip) == wled_state_cache.end()) {
      std::string current_state = get_wled_state(ip);
      if (!current_state.empty()) {
        wled_state_cache[ip] = current_state;
        ESP_LOGI(TAG, "Saved WLED state for %s before enabling DDP", ip.c_str());
      } else {
        ESP_LOGW(TAG, "Failed to get WLED state for %s, will restore default state", ip.c_str());
      }
    }
  }
  
  // Send HTTP POST to WLED API to enable DDP receive mode
  char url[96];
  snprintf(url, sizeof(url), "http://%s/json/state", ip.c_str());
  
  esp_http_client_config_t cfg = {};
  cfg.url = url;
  cfg.timeout_ms = 2000;
  cfg.skip_cert_common_name_check = true;
  cfg.method = HTTP_METHOD_POST;
  
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (!client) {
    ESP_LOGW(TAG, "Failed to init HTTP client for DDP enable: %s", ip.c_str());
    return;
  }
  
  // JSON payload: prepare WLED for DDP streaming
  // Turn on and set to solid black - DDP will override the pixels
  const char* json_payload = "{\"on\":true,\"bri\":255,\"seg\":[{\"fx\":0,\"col\":[[0,0,0],[0,0,0],[0,0,0]]}]}";
  esp_http_client_set_post_field(client, json_payload, strlen(json_payload));
  esp_http_client_set_header(client, "Content-Type", "application/json");
  
  esp_err_t err = esp_http_client_perform(client);
  if (err == ESP_OK) {
    const int status_code = esp_http_client_get_status_code(client);
    if (status_code == 200) {
      ESP_LOGI(TAG, "Enabled DDP receive mode for WLED device: %s", ip.c_str());
      std::lock_guard<std::mutex> lock(cache_mutex);
      ddp_mode_enabled_cache[ip] = now_us;
    } else {
      ESP_LOGW(TAG, "Failed to enable DDP mode for %s: HTTP %d", ip.c_str(), status_code);
    }
  } else {
    ESP_LOGW(TAG, "Failed to enable DDP mode for %s: %s", ip.c_str(), esp_err_to_name(err));
  }
  
  esp_http_client_cleanup(client);
}

void disable_wled_ddp_mode(const std::string& ip) {
  if (ip.empty()) {
    return;
  }
  
  bool restored = false;
  std::string saved_state_json;
  
  // Get saved state from cache (with mutex protection)
  {
    std::lock_guard<std::mutex> lock(cache_mutex);
    auto state_it = wled_state_cache.find(ip);
    if (state_it != wled_state_cache.end() && !state_it->second.empty()) {
      saved_state_json = state_it->second;
    }
  }
  
  // Restore previous WLED state if we saved it
  if (!saved_state_json.empty()) {
    // Parse saved state and restore it, but ensure live=false
    cJSON* saved_state = cJSON_Parse(saved_state_json.c_str());
    if (saved_state) {
      // Ensure live mode is disabled
      cJSON* live = cJSON_GetObjectItem(saved_state, "live");
      if (live) {
        cJSON_SetBoolValue(live, false);
      } else {
        cJSON_AddBoolToObject(saved_state, "live", false);
      }
      
      // Convert back to JSON string
      char* restored_json = cJSON_PrintUnformatted(saved_state);
      if (restored_json) {
        char url[96];
        snprintf(url, sizeof(url), "http://%s/json/state", ip.c_str());
        
        esp_http_client_config_t cfg = {};
        cfg.url = url;
        cfg.timeout_ms = 2000;
        cfg.skip_cert_common_name_check = true;
        cfg.method = HTTP_METHOD_POST;
        
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (client) {
          esp_http_client_set_post_field(client, restored_json, strlen(restored_json));
          esp_http_client_set_header(client, "Content-Type", "application/json");
          
          esp_err_t err = esp_http_client_perform(client);
          if (err == ESP_OK) {
            const int status_code = esp_http_client_get_status_code(client);
            if (status_code == 200) {
              ESP_LOGI(TAG, "Restored previous WLED state for %s (disabled DDP mode)", ip.c_str());
              restored = true;
            } else {
              ESP_LOGW(TAG, "Failed to restore WLED state for %s: HTTP %d", ip.c_str(), status_code);
            }
          } else {
            ESP_LOGW(TAG, "Failed to restore WLED state for %s: %s", ip.c_str(), esp_err_to_name(err));
          }
          esp_http_client_cleanup(client);
        }
        free(restored_json);
      }
      cJSON_Delete(saved_state);
    }
  }
  
  // Fallback: simple disable if we don't have saved state or restore failed
  if (!restored) {
    // Try to get current state from WLED and restore it (without live mode)
    std::string current_state = get_wled_state(ip);
    if (!current_state.empty()) {
      // Parse current state and ensure live=false, on=true
      cJSON* state = cJSON_Parse(current_state.c_str());
      if (state) {
        cJSON* live = cJSON_GetObjectItem(state, "live");
        if (live) {
          cJSON_SetBoolValue(live, false);
        } else {
          cJSON_AddBoolToObject(state, "live", false);
        }
        // Ensure WLED is on (but keep other settings like effect, colors, brightness)
        cJSON* on = cJSON_GetObjectItem(state, "on");
        if (on) {
          cJSON_SetBoolValue(on, true);
        } else {
          cJSON_AddBoolToObject(state, "on", true);
        }
        
        char* restored_json = cJSON_PrintUnformatted(state);
        if (restored_json) {
          char url[96];
          snprintf(url, sizeof(url), "http://%s/json/state", ip.c_str());
          
          esp_http_client_config_t cfg = {};
          cfg.url = url;
          cfg.timeout_ms = 2000;
          cfg.skip_cert_common_name_check = true;
          cfg.method = HTTP_METHOD_POST;
          
          esp_http_client_handle_t client = esp_http_client_init(&cfg);
          if (client) {
            esp_http_client_set_post_field(client, restored_json, strlen(restored_json));
            esp_http_client_set_header(client, "Content-Type", "application/json");
            
            esp_err_t err = esp_http_client_perform(client);
            if (err == ESP_OK) {
              const int status_code = esp_http_client_get_status_code(client);
              if (status_code == 200) {
                ESP_LOGI(TAG, "Restored current WLED state for %s (disabled DDP mode)", ip.c_str());
                restored = true;
              } else {
                ESP_LOGW(TAG, "Failed to restore WLED state for %s: HTTP %d", ip.c_str(), status_code);
              }
            } else {
              ESP_LOGW(TAG, "Failed to restore WLED state for %s: %s", ip.c_str(), esp_err_to_name(err));
            }
            esp_http_client_cleanup(client);
          }
          free(restored_json);
        }
        cJSON_Delete(state);
      }
    }
    
    // Final fallback: minimal restore (only disable live mode, keep everything else)
    if (!restored) {
      char url[96];
      snprintf(url, sizeof(url), "http://%s/json/state", ip.c_str());
      
      esp_http_client_config_t cfg = {};
      cfg.url = url;
      cfg.timeout_ms = 2000;
      cfg.skip_cert_common_name_check = true;
      cfg.method = HTTP_METHOD_POST;
      
      esp_http_client_handle_t client = esp_http_client_init(&cfg);
      if (!client) {
        ESP_LOGW(TAG, "Failed to init HTTP client for DDP disable: %s", ip.c_str());
        return;
      }
      
      // JSON payload: only disable live mode, don't change anything else
      // This allows WLED to continue with its current effect/colors
      const char* json_payload = "{\"live\":false}";
      esp_http_client_set_post_field(client, json_payload, strlen(json_payload));
      esp_http_client_set_header(client, "Content-Type", "application/json");
      
      esp_err_t err = esp_http_client_perform(client);
      if (err == ESP_OK) {
        const int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
          ESP_LOGI(TAG, "Disabled DDP receive mode for WLED device: %s (kept current state)", ip.c_str());
        } else {
          ESP_LOGW(TAG, "Failed to disable DDP mode for %s: HTTP %d", ip.c_str(), status_code);
        }
      } else {
        ESP_LOGW(TAG, "Failed to disable DDP mode for %s: %s", ip.c_str(), esp_err_to_name(err));
      }
      
      esp_http_client_cleanup(client);
    }
  }
  
  // Remove from caches (with mutex protection)
  {
    std::lock_guard<std::mutex> lock(cache_mutex);
    ddp_mode_enabled_cache.erase(ip);
    wled_state_cache.erase(ip);
  }
}

struct Rgb {
  float r{1.0f};
  float g{1.0f};
  float b{1.0f};
};

struct GradientStop {
  float pos{0.0f};
  Rgb color{};
};

// Per-device effect state storage (avoids static vectors shared between devices)
static std::unordered_map<std::string, std::vector<uint8_t>> s_wled_effect_state;

std::vector<uint8_t>& get_wled_state(const std::string& key, size_t size) {
  auto& state = s_wled_effect_state[key];
  if (state.size() != size) {
    state.resize(size, 0);
  }
  return state;
}

float clamp01(float v) {
  return std::max(0.0f, std::min(1.0f, v));
}

uint8_t to_byte(float v) {
  const int value = static_cast<int>(v * 255.0f);
  return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

std::string lower_copy(const std::string& value) {
  std::string out = value;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return out;
}

int from_hex(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
  if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
  return -1;
}

Rgb parse_hex_color(const std::string& text, const Rgb& fallback) {
  size_t start = 0;
  if (text.empty()) {
    return fallback;
  }
  if (text[0] == '#') {
    start = 1;
  }
  if (text.size() - start < 6) {
    return fallback;
  }
  int r1 = from_hex(text[start]);
  int r2 = from_hex(text[start + 1]);
  int g1 = from_hex(text[start + 2]);
  int g2 = from_hex(text[start + 3]);
  int b1 = from_hex(text[start + 4]);
  int b2 = from_hex(text[start + 5]);
  if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0) {
    return fallback;
  }
  const float r = (static_cast<float>((r1 << 4) + r2)) / 255.0f;
  const float g = (static_cast<float>((g1 << 4) + g2)) / 255.0f;
  const float b = (static_cast<float>((b1 << 4) + b2)) / 255.0f;
  return Rgb{r, g, b};
}

std::vector<std::string> split_colors(const std::string& text) {
  std::vector<std::string> parts;
  std::string current;
  for (char ch : text) {
    if (ch == ',' || ch == ';' || ch == ' ' || ch == '|') {
      if (!current.empty()) {
        parts.push_back(current);
        current.clear();
      }
    } else if (ch == '-') {
      if (!current.empty()) {
        parts.push_back(current);
        current.clear();
      }
    } else {
      current.push_back(ch);
    }
  }
  if (!current.empty()) {
    parts.push_back(current);
  }
  return parts;
}

std::vector<GradientStop> build_gradient_from_string(const std::string& text, const Rgb& fallback) {
  std::vector<GradientStop> stops;
  const auto tokens = split_colors(text);
  if (tokens.size() < 2) {
    return stops;
  }
  const size_t last = tokens.size() - 1;
  for (size_t i = 0; i < tokens.size(); ++i) {
    const float pos = last == 0 ? 0.0f : static_cast<float>(i) / static_cast<float>(last);
    const Rgb color = parse_hex_color(tokens[i], fallback);
    stops.push_back(GradientStop{pos, color});
  }
  return stops;
}

std::vector<GradientStop> palette_gradient(const std::string& name,
                                           const Rgb& c1,
                                           const Rgb& c2,
                                           const Rgb& c3) {
  const std::string key = lower_copy(name);
  if (key == "sunset") {
    return {{0.0f, c1}, {0.45f, Rgb{1.0f, 0.45f, 0.0f}}, {1.0f, Rgb{0.6f, 0.05f, 0.2f}}};
  }
  if (key == "fire") {
    return {{0.0f, Rgb{1.0f, 0.2f, 0.0f}}, {0.5f, Rgb{1.0f, 0.6f, 0.0f}}, {1.0f, Rgb{1.0f, 1.0f, 0.3f}}};
  }
  if (key == "icy") {
    return {{0.0f, Rgb{0.2f, 0.6f, 1.0f}}, {0.5f, Rgb{0.4f, 0.8f, 1.0f}}, {1.0f, Rgb{0.8f, 1.0f, 1.0f}}};
  }
  if (key == "forest") {
    return {{0.0f, Rgb{0.0f, 0.3f, 0.1f}}, {0.5f, Rgb{0.0f, 0.5f, 0.0f}}, {1.0f, Rgb{0.4f, 0.8f, 0.2f}}};
  }
  if (key == "ocean" || key == "aqua") {
    return {{0.0f, Rgb{0.0f, 0.2f, 0.4f}}, {0.5f, Rgb{0.0f, 0.6f, 0.7f}}, {1.0f, Rgb{0.0f, 0.9f, 1.0f}}};
  }
  if (key == "party") {
    return {{0.0f, Rgb{1.0f, 0.0f, 0.6f}}, {0.33f, Rgb{0.0f, 0.6f, 1.0f}}, {0.66f, Rgb{0.5f, 0.0f, 1.0f}}, {1.0f, Rgb{1.0f, 0.4f, 0.0f}}};
  }
  return {{0.0f, c1}, {0.5f, c2}, {1.0f, c3}};
}

Rgb sample_gradient(const std::vector<GradientStop>& stops, float t) {
  if (stops.empty()) {
    return Rgb{};
  }
  if (stops.size() == 1) {
    return stops.front().color;
  }
  const float x = clamp01(t);
  GradientStop a = stops.front();
  GradientStop b = stops.back();
  for (size_t i = 1; i < stops.size(); ++i) {
    if (stops[i].pos >= x) {
      a = stops[i - 1];
      b = stops[i];
      break;
    }
  }
  const float span = std::max(0.0001f, b.pos - a.pos);
  const float local = clamp01((x - a.pos) / span);
  Rgb out;
  out.r = a.color.r + (b.color.r - a.color.r) * local;
  out.g = a.color.g + (b.color.g - a.color.g) * local;
  out.b = a.color.b + (b.color.b - a.color.b) * local;
  return out;
}

}  // namespace

// Get next DDP sequence number (1-15, cycling)
uint8_t WledEffectsRuntime::next_seq() {
  uint8_t s = seq_;
  seq_ = (seq_ >= 15) ? 1 : seq_ + 1;
  return s;
}

esp_err_t WledEffectsRuntime::start(AppConfig* cfg, LedEngineRuntime* led_runtime) {
  cfg_ref_ = cfg;
  led_runtime_ = led_runtime;
  update_config(*cfg);
  running_ = true;
  if (!task_) {
    // Pin to Core 1 for real-time LED rendering (isolated from network/system tasks)
    const BaseType_t res =
        xTaskCreatePinnedToCore(task_entry, "wled_fx", 8192, this, 5, &task_, 1);
    if (res != pdPASS) {
      running_ = false;
      task_ = nullptr;
      ESP_LOGE(TAG, "Failed to start WLED FX task");
      return ESP_FAIL;
    }
  }
  return ESP_OK;
}

void WledEffectsRuntime::stop() {
  running_ = false;
  
  // Disable DDP mode for all active devices before stopping
  std::lock_guard<std::mutex> lock(mutex_);
  if (!active_ddp_devices_.empty()) {
    ESP_LOGI(TAG, "Stopping WLED effects runtime - disabling DDP mode for %zu devices", active_ddp_devices_.size());
    for (const auto& ip : active_ddp_devices_) {
      disable_wled_ddp_mode(ip);
    }
    active_ddp_devices_.clear();
  }
  
  if (task_) {
    vTaskDelay(pdMS_TO_TICKS(10));
    vTaskDelete(task_);
    task_ = nullptr;
  }
}

void WledEffectsRuntime::update_config(const AppConfig& cfg) {
  std::lock_guard<std::mutex> lock(mutex_);
  effects_ = cfg.wled_effects;
  devices_ = cfg.wled_devices;
}

void WledEffectsRuntime::task_entry(void* arg) {
  auto* self = reinterpret_cast<WledEffectsRuntime*>(arg);
  if (self) {
    self->task_loop();
  }
  vTaskDelete(nullptr);
}

bool WledEffectsRuntime::should_run() const {
  // WLED effects should run independently of local LED effects
  // They can be enabled/disabled per device via binding.enabled
  // Always return true - individual bindings control their own enabled state
  return true;
}

bool WledEffectsRuntime::resolve_device(const std::string& device_id,
                                        const std::vector<WledDeviceConfig>& devices,
                                        const std::vector<WledDeviceStatus>& status,
                                        WledDeviceConfig& out,
                                        std::string& ip) const {
  // Try to find device by id first, then by address (GUI may send IP address as device_id)
  auto it = std::find_if(devices.begin(), devices.end(), [&](const WledDeviceConfig& cfg) {
    return cfg.id == device_id || cfg.address == device_id;
  });
  if (it == devices.end()) {
    ESP_LOGW(TAG, "Device %s not found in devices list (%zu devices)", device_id.c_str(), devices.size());
    return false;
  }
  out = *it;
  if (!out.active) {
    ESP_LOGW(TAG, "Device %s is not active (id=%s, address=%s)", device_id.c_str(), out.id.c_str(), out.address.c_str());
    return false;
  }
  for (const auto& st : status) {
    if (st.config.id == device_id || (!st.ip.empty() && st.ip == out.address) ||
        (!st.config.address.empty() && st.config.address == out.address)) {
      if (!st.ip.empty()) {
        ip = st.ip;
        ESP_LOGD(TAG, "Resolved device %s -> IP %s (from status)", device_id.c_str(), ip.c_str());
      }
      break;
    }
  }
  if (ip.empty()) {
    ip = out.address;
    if (!ip.empty()) {
      ESP_LOGD(TAG, "Resolved device %s -> IP %s (from config address)", device_id.c_str(), ip.c_str());
    }
  }
  if (ip.empty()) {
    ESP_LOGW(TAG, "Device %s has no IP address configured (id=%s, address=%s, status_count=%zu)", 
             device_id.c_str(), out.id.c_str(), out.address.c_str(), status.size());
    return false;
  }
  return true;
}

std::vector<uint8_t> WledEffectsRuntime::render_frame(const WledEffectBinding& binding,
                                                      uint16_t led_count,
                                                      uint32_t frame_idx,
                                                      uint8_t global_brightness,
                                                      uint16_t fps,
                                                      const LedLayoutConfig& layout) {
  const uint16_t pixels = led_count == 0 ? 60 : led_count;
  std::vector<uint8_t> frame(pixels * 3, 0);

  // Matrix layout support
  const bool is_matrix = (layout.type == LedLayoutType::Matrix && layout.width > 0 && layout.height > 0);
  const uint16_t matrix_width = is_matrix ? layout.width : pixels;
  const uint16_t matrix_height = is_matrix ? layout.height : 1;
  const bool serpentine = layout.serpentine;

  // Helper lambda to get 2D coordinates from linear LED index
  auto get_coords = [&](uint16_t idx) -> std::pair<uint16_t, uint16_t> {
    if (!is_matrix) {
      return {idx, 0};  // col, row for linear strip
    }
    uint16_t row = idx / matrix_width;
    uint16_t col = idx % matrix_width;
    if (serpentine && (row % 2 == 1)) {
      col = matrix_width - 1 - col;  // Reverse direction on odd rows
    }
    return {col, row};
  };

  // Parse colors with fallback to visible defaults
  Rgb c1 = parse_hex_color(binding.effect.color1, Rgb{1.0f, 1.0f, 1.0f});
  Rgb c2 = parse_hex_color(binding.effect.color2, Rgb{0.6f, 0.4f, 0.0f});
  Rgb c3 = parse_hex_color(binding.effect.color3, Rgb{0.0f, 0.2f, 1.0f});
  
  // Ensure colors are not all black (minimum visibility)
  if (c1.r == 0.0f && c1.g == 0.0f && c1.b == 0.0f) {
    c1 = Rgb{1.0f, 1.0f, 1.0f};  // Default to white if color1 is black
  }
  std::vector<GradientStop> gradient;
  if (!binding.effect.gradient.empty()) {
    gradient = build_gradient_from_string(binding.effect.gradient, c1);
  } else if (!binding.effect.palette.empty()) {
    gradient = palette_gradient(binding.effect.palette, c1, c2, c3);
  }

  const float brightness_override =
      binding.effect.brightness_override > 0 ? binding.effect.brightness_override : binding.effect.brightness;
  float brightness = clamp01(brightness_override / 255.0f) * clamp01(global_brightness / 255.0f);
  // Ensure minimum brightness for visibility (at least 20% if effect brightness is set)
  if (brightness <= 0.0f) {
    // If brightness is 0, use a minimum visible level (10% of max)
    brightness = 0.1f;
  } else if (brightness < 0.2f && binding.effect.brightness > 0) {
    // If brightness is very low but effect brightness is set, ensure at least 20% visibility
    brightness = std::max(brightness, 0.2f);
  }

  // Audio reactivity: ONLY for LEDFx effects, NOT for WLED effects
  // For LEDFx effects, you can set which frequency range to react to (kick, bass, mids, treble, or custom range)
  // This works for both WLED devices (via DDP) and local physical segments
  float audio_mod = 1.0f;
  const std::string engine = lower_copy(binding.effect.engine);
  const bool is_ledfx = (engine == "ledfx");
  
  // Only process audio for LEDFx effects
  if (binding.effect.audio_link && is_ledfx) {
    // Get audio metrics with timestamp for synchronization
    AudioMetrics metrics = led_audio_get_metrics();
    float energy = metrics.energy;
    const std::string channel = lower_copy(binding.audio_channel);
    if (channel == "left") {
      energy = metrics.energy_left > 0.0f ? metrics.energy_left : metrics.energy * 0.8f;
    } else if (channel == "right") {
      energy = metrics.energy_right > 0.0f ? metrics.energy_right : metrics.energy * 0.8f;
    }
    if (energy <= 0.0001f) {
      const float time_approx = static_cast<float>(frame_idx) / 60.0f;  // Approximate time from frame_idx
      energy = 0.35f + 0.35f * (sinf(time_approx * 0.15f) * 0.5f + 0.5f);
    }
    const float time_approx = static_cast<float>(frame_idx) / 60.0f;
    const float beat = metrics.beat > 0.0f ? metrics.beat : (sinf(time_approx * 0.12f) * 0.5f + 0.5f);
    
    float weighted = 0.0f;
    // Check if selected bands are configured
    if (!binding.effect.selected_bands.empty()) {
      // Use selected bands - average all selected band values
      float sum = 0.0f;
      size_t count = 0;
      for (const auto& band_name : binding.effect.selected_bands) {
        float band_val = led_audio_get_band_value(band_name);
        if (band_val > 0.0f) {
          sum += band_val;
          ++count;
        }
      }
      weighted = count > 0 ? sum / static_cast<float>(count) : energy * 0.7f;
    } else if (binding.effect.freq_min > 0.0f && binding.effect.freq_max > 0.0f) {
      // Use custom frequency range
      weighted = led_audio_get_custom_energy(binding.effect.freq_min, binding.effect.freq_max);
      if (weighted <= 0.0001f) {
        weighted = energy * 0.7f;  // Fallback to overall energy
      }
    } else {
      // Use default frequency bands (bass, mid, treble) based on reactive_mode
      float bass = metrics.bass > 0.0f ? metrics.bass : energy * 0.8f;
      float treble = metrics.treble > 0.0f ? metrics.treble : energy * 0.6f;
      float mid = metrics.mid > 0.0f ? metrics.mid : energy * 0.7f;

      // Apply band gains
      bass *= binding.effect.band_gain_low;
      mid *= binding.effect.band_gain_mid;
      treble *= binding.effect.band_gain_high;

      const std::string reactive = lower_copy(binding.effect.reactive_mode);
      if (reactive == "kick") {
        // Kick: focus on very low frequencies (sub-bass) and beat detection
        // Use bass with emphasis on beat detection for kick drum response
        weighted = (bass * 1.2f * 0.7f + beat * 0.3f);  // Emphasize bass and beat
      } else if (reactive == "bass") {
        weighted = bass;
      } else if (reactive == "mids") {
        weighted = mid;
      } else if (reactive == "treble") {
        weighted = treble;
      } else {
        // Full spectrum (default)
        weighted = (energy * 0.4f + mid * 0.25f + bass * 0.2f + treble * 0.15f);
      }
    }
    weighted *= (0.6f + beat * 0.4f);
    const float profile_gain = binding.effect.audio_profile == "ledfx_energy" ? 1.1f
                             : binding.effect.audio_profile == "ledfx_tempo" ? 1.05f
                             : 1.0f;
    audio_mod = clamp01(0.4f + weighted * 0.8f * profile_gain);
    audio_mod *= binding.effect.amplitude_scale > 0.0f ? binding.effect.amplitude_scale : 1.0f;
    if (binding.effect.brightness_compress > 0.0f) {
      const float gamma = 1.0f + binding.effect.brightness_compress;
      audio_mod = powf(audio_mod, 1.0f / gamma);
    }
    if (binding.effect.beat_response) {
      audio_mod *= (0.6f + beat * 0.4f);
    }
  }

  // Attack/release envelope to smooth out audio reactivity (only for LEDFx)
  if (binding.effect.audio_link && is_ledfx && (binding.effect.attack_ms > 0 || binding.effect.release_ms > 0) && fps > 0) {
    audio_mod = apply_envelope(envelope_key(binding), audio_mod, fps, binding.effect.attack_ms, binding.effect.release_ms);
  }

  // Check if this is a LEDFx effect
  if (is_ledfx) {
    // Convert gradient to LEDFx format
    std::vector<ledfx_effects::GradientStop> ledfx_gradient;
    for (const auto& stop : gradient) {
      ledfx_gradient.push_back({stop.pos, {stop.color.r, stop.color.g, stop.color.b}});
    }
    ledfx_effects::Rgb ledfx_c1{c1.r, c1.g, c1.b};
    ledfx_effects::Rgb ledfx_c2{c2.r, c2.g, c2.b};
    ledfx_effects::Rgb ledfx_c3{c3.r, c3.g, c3.b};
    
    const float speed = 0.02f + (binding.effect.speed / 255.0f) * 0.25f;
    const float intensity = binding.effect.intensity / 255.0f;
    const float direction = lower_copy(binding.effect.direction) == "reverse" ? -1.0f : 1.0f;
    
    const float ledfx_time = static_cast<float>(frame_idx) / static_cast<float>(fps > 0 ? fps : 60);
    return ledfx_effects::render_effect(binding.effect.effect, binding.effect, led_count, ledfx_time,
                                        global_brightness, fps, ledfx_gradient, ledfx_c1, ledfx_c2, ledfx_c3,
                                        brightness, audio_mod, speed, intensity, direction);
  }

  // WLED effects - using frame_idx counter like original WLED firmware
  const std::string effect_name = lower_copy(binding.effect.effect);
  
  // WLED speed 0-255: used directly in counter calculations
  const uint8_t speed_val = binding.effect.speed;
  // Intensity: 0-255
  const uint8_t intensity_val = binding.effect.intensity;
  (void)intensity_val;  // Used in various effects
  const bool reverse = lower_copy(binding.effect.direction) == "reverse";

  uint8_t* dst = frame.data();
  
  // WLED style counter: frame_idx scaled by speed
  // Higher speed = faster counter increment
  const uint32_t counter = frame_idx * (1 + speed_val / 16);
  
  // Helper: color_wheel - WLED style 8-bit hue to RGB
  auto color_wheel = [](uint8_t pos) -> Rgb {
    pos = 255 - pos;
    if (pos < 85) {
      return Rgb{(255 - pos * 3) / 255.0f, 0.0f, (pos * 3) / 255.0f};
    } else if (pos < 170) {
      pos -= 85;
      return Rgb{0.0f, (pos * 3) / 255.0f, (255 - pos * 3) / 255.0f};
    } else {
      pos -= 170;
      return Rgb{(pos * 3) / 255.0f, (255 - pos * 3) / 255.0f, 0.0f};
    }
  };

  // ==================== WLED EFFECTS (frame_idx based) ====================
  
  // Rainbow - WLED style: counter-based hue rotation
  if (effect_name.find("rainbow") != std::string::npos) {
    const uint8_t hue_offset = static_cast<uint8_t>((counter >> 2) & 0xFF);
    for (uint16_t i = 0; i < pixels; ++i) {
      const uint8_t pixel_hue = hue_offset + static_cast<uint8_t>((i * 256) / pixels);
      const uint8_t final_hue = reverse ? (255 - pixel_hue) : pixel_hue;
      const Rgb col = color_wheel(final_hue);
      *dst++ = to_byte(col.r * brightness);
      *dst++ = to_byte(col.g * brightness);
      *dst++ = to_byte(col.b * brightness);
    }
    return frame;
  }

  // Solid - static color
  if (effect_name.find("solid") != std::string::npos) {
    for (uint16_t i = 0; i < pixels; ++i) {
      *dst++ = to_byte(c1.r * brightness);
      *dst++ = to_byte(c1.g * brightness);
      *dst++ = to_byte(c1.b * brightness);
    }
    return frame;
  }

  // Blink - WLED style: on/off based on counter
  if (effect_name.find("blink") != std::string::npos) {
    const bool on = ((counter >> 8) & 1) == 0;
    for (uint16_t i = 0; i < pixels; ++i) {
      if (on) {
        *dst++ = to_byte(c1.r * brightness);
        *dst++ = to_byte(c1.g * brightness);
        *dst++ = to_byte(c1.b * brightness);
      } else {
        *dst++ = to_byte(c2.r * brightness * 0.1f);
        *dst++ = to_byte(c2.g * brightness * 0.1f);
        *dst++ = to_byte(c2.b * brightness * 0.1f);
      }
    }
    return frame;
  }

  // Breathe - WLED style sine wave breathing
  if (effect_name.find("breathe") != std::string::npos) {
    // WLED uses sin8 lookup, we approximate with sinf
    const float phase = static_cast<float>(counter & 0xFFFF) / 65536.0f * 6.2831f;
    const float breath = (sinf(phase) + 1.0f) * 0.5f;
    const float level = breath * breath;  // Gamma for natural look
    for (uint16_t i = 0; i < pixels; ++i) {
      *dst++ = to_byte(c1.r * brightness * level);
      *dst++ = to_byte(c1.g * brightness * level);
      *dst++ = to_byte(c1.b * brightness * level);
    }
    return frame;
  }

  // Color Wipe - WLED style progressive fill
  if (effect_name.find("wipe") != std::string::npos) {
    const uint16_t cycle_frames = pixels * 2;
    const uint16_t pos_in_cycle = (counter / 2) % cycle_frames;
    const uint16_t fill_led = pos_in_cycle < pixels ? pos_in_cycle : (cycle_frames - pos_in_cycle - 1);
    
    for (uint16_t i = 0; i < pixels; ++i) {
      const uint16_t idx = reverse ? (pixels - 1 - i) : i;
      if (idx <= fill_led) {
        *dst++ = to_byte(c1.r * brightness);
        *dst++ = to_byte(c1.g * brightness);
        *dst++ = to_byte(c1.b * brightness);
      } else {
        *dst++ = to_byte(c2.r * brightness * 0.05f);
        *dst++ = to_byte(c2.g * brightness * 0.05f);
        *dst++ = to_byte(c2.b * brightness * 0.05f);
      }
    }
    return frame;
  }

  // Fire 2012 - classic FastLED/WLED fire (always use linear strip mode for reliability)
  if (effect_name.find("fire") != std::string::npos) {
    // Original WLED parameters
    const uint8_t COOLING = 20 + (speed_val / 3);
    const uint8_t SPARKING = 50 + (intensity_val * 2 / 3);

    // Use single heat array for all pixels (works for both strip and matrix)
    const std::string state_key = "fire_" + binding.device_id + "_" + std::to_string(pixels);
    auto& heat = get_wled_state(state_key, pixels);

    // Step 1: Cool down every cell
    for (uint16_t i = 0; i < pixels; ++i) {
      uint8_t cooldown = (esp_random() % (((COOLING * 10) / pixels) + 2));
      heat[i] = (heat[i] > cooldown) ? (heat[i] - cooldown) : 0;
    }

    // Step 2: Heat drifts up and diffuses
    for (int k = pixels - 1; k >= 2; --k) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }

    // Step 3: Randomly ignite sparks near bottom
    if ((esp_random() & 0xFF) < SPARKING) {
      int y = esp_random() % std::min(7, (int)pixels);
      uint16_t newHeat = heat[y] + 160 + (esp_random() % 96);
      heat[y] = (newHeat > 255) ? 255 : static_cast<uint8_t>(newHeat);
    }

    // Step 4: Convert heat to LED colors
    uint8_t* dst = frame.data();
    for (uint16_t i = 0; i < pixels; ++i) {
      const uint16_t idx = reverse ? i : (pixels - 1 - i);
      const uint8_t temperature = heat[idx];
      
      uint8_t r, g, b;
      uint8_t t192 = (temperature > 0) ? ((temperature * 191) / 255 + 1) : 0;
      uint8_t heatramp = (t192 & 0x3F) << 2;
      
      if (t192 > 0x80) { r = 255; g = 255; b = heatramp; }
      else if (t192 > 0x40) { r = 255; g = heatramp; b = 0; }
      else { r = heatramp; g = 0; b = 0; }
      
      *dst++ = static_cast<uint8_t>(r * brightness);
      *dst++ = static_cast<uint8_t>(g * brightness);
      *dst++ = static_cast<uint8_t>(b * brightness);
    }
    return frame;
  }

  // Meteor - WLED style with decay trail
  if (effect_name.find("meteor") != std::string::npos) {
    const std::string state_key = "meteor_" + binding.device_id + "_" + std::to_string(pixels);
    auto& trail = get_wled_state(state_key, pixels);
    
    const uint8_t meteor_size = 1 + (intensity_val >> 5);
    const uint8_t decay = 128 + (intensity_val >> 1);
    const int meteor_pos = (counter >> 3) % (pixels + meteor_size * 2);
    
    // Decay trail randomly
    for (uint16_t i = 0; i < pixels; ++i) {
      if ((esp_random() & 0x0F) > 5) {
        const uint8_t fade = 255 - decay;
        trail[i] = (trail[i] > fade) ? trail[i] - fade : 0;
      }
    }
    
    // Draw meteor head
    for (uint8_t j = 0; j < meteor_size; ++j) {
      int idx = reverse ? (pixels - 1 - meteor_pos + j) : (meteor_pos - j);
      if (idx >= 0 && idx < static_cast<int>(pixels)) {
        trail[idx] = 255;
      }
    }
    
    // Render
    for (uint16_t i = 0; i < pixels; ++i) {
      const float level = trail[i] / 255.0f;
      *dst++ = to_byte(c1.r * brightness * level);
      *dst++ = to_byte(c1.g * brightness * level);
      *dst++ = to_byte(c1.b * brightness * level);
    }
    return frame;
  }

  // Scanner / Larson - WLED style bounce
  if (effect_name.find("scanner") != std::string::npos || effect_name.find("larson") != std::string::npos) {
    const uint16_t cycle_len = pixels * 2;
    const uint16_t pos_in_cycle = (counter >> 2) % cycle_len;
    const uint16_t scan_led = pos_in_cycle < pixels ? pos_in_cycle : (cycle_len - pos_in_cycle - 1);
    const uint8_t width = 1 + (intensity_val >> 5);
    
    for (uint16_t i = 0; i < pixels; ++i) {
      const int dist = std::abs(static_cast<int>(i) - static_cast<int>(scan_led));
      float level = 0.0f;
      if (dist < width) {
        level = 1.0f - static_cast<float>(dist) / width;
      }
      *dst++ = to_byte(c1.r * brightness * level);
      *dst++ = to_byte(c1.g * brightness * level);
      *dst++ = to_byte(c1.b * brightness * level);
    }
    return frame;
  }

  // Theater Chase - WLED classic marquee
  if (effect_name.find("theater") != std::string::npos || effect_name.find("chase") != std::string::npos) {
    const int spacing = 3;
    int offset = (counter >> 4) % spacing;
    if (reverse) offset = spacing - 1 - offset;
    
    for (uint16_t i = 0; i < pixels; ++i) {
      if (((i + offset) % spacing) == 0) {
        *dst++ = to_byte(c1.r * brightness);
        *dst++ = to_byte(c1.g * brightness);
        *dst++ = to_byte(c1.b * brightness);
      } else {
        *dst++ = to_byte(c2.r * brightness * 0.05f);
        *dst++ = to_byte(c2.g * brightness * 0.05f);
        *dst++ = to_byte(c2.b * brightness * 0.05f);
      }
    }
    return frame;
  }

  // Twinkle - WLED random sparkling
  if (effect_name.find("twinkle") != std::string::npos) {
    // Per-device twinkle state
    const std::string state_key = "twinkle_" + binding.device_id + "_" + std::to_string(pixels);
    auto& twinkle_state = get_wled_state(state_key, pixels);
    
    // Randomly spawn new twinkles based on intensity
    const uint8_t spawn_chance = intensity_val >> 2;
    for (uint16_t i = 0; i < pixels; ++i) {
      if (twinkle_state[i] == 0 && (esp_random() & 0xFF) < spawn_chance) {
        twinkle_state[i] = 255;
      }
      if (twinkle_state[i] > 0) {
        twinkle_state[i] = std::max(0, static_cast<int>(twinkle_state[i]) - 8);
      }
    }
    
    for (uint16_t i = 0; i < pixels; ++i) {
      const float level = twinkle_state[i] / 255.0f;
      *dst++ = to_byte(c1.r * brightness * level);
      *dst++ = to_byte(c1.g * brightness * level);
      *dst++ = to_byte(c1.b * brightness * level);
    }
    return frame;
  }

  // Sparkle - single random flash per frame
  if (effect_name.find("sparkle") != std::string::npos) {
    // Background
    for (uint16_t i = 0; i < pixels; ++i) {
      *dst++ = to_byte(c2.r * brightness * 0.05f);
      *dst++ = to_byte(c2.g * brightness * 0.05f);
      *dst++ = to_byte(c2.b * brightness * 0.05f);
    }
    // One random sparkle
    const uint16_t sparkle_idx = esp_random() % pixels;
    uint8_t* p = frame.data() + sparkle_idx * 3;
    p[0] = to_byte(c1.r * brightness);
    p[1] = to_byte(c1.g * brightness);
    p[2] = to_byte(c1.b * brightness);
    return frame;
  }

  // Strobe - WLED fast flash
  if (effect_name.find("strobe") != std::string::npos) {
    // Flash duration based on intensity (higher = longer on time)
    const uint8_t on_frames = 1 + (intensity_val >> 5);
    const uint8_t cycle_frames = 8 + ((255 - speed_val) >> 3);
    const bool on = ((counter >> 2) % cycle_frames) < on_frames;
    
    for (uint16_t i = 0; i < pixels; ++i) {
      if (on) {
        *dst++ = to_byte(c1.r * brightness);
        *dst++ = to_byte(c1.g * brightness);
        *dst++ = to_byte(c1.b * brightness);
      } else {
        *dst++ = 0; *dst++ = 0; *dst++ = 0;
      }
    }
    return frame;
  }

  // Gradient - smooth scrolling gradient
  if (effect_name.find("gradient") != std::string::npos) {
    const uint8_t offset = static_cast<uint8_t>((counter >> 3) & 0xFF);
    for (uint16_t i = 0; i < pixels; ++i) {
      const uint8_t pos = offset + static_cast<uint8_t>((i * 256) / pixels);
      const uint8_t final_pos = reverse ? (255 - pos) : pos;
      const float t = final_pos / 255.0f;
      const Rgb col = gradient.empty() ?
        Rgb{c1.r * (1.0f - t) + c2.r * t, c1.g * (1.0f - t) + c2.g * t, c1.b * (1.0f - t) + c2.b * t} :
        sample_gradient(gradient, t);
      *dst++ = to_byte(col.r * brightness);
      *dst++ = to_byte(col.g * brightness);
      *dst++ = to_byte(col.b * brightness);
    }
    return frame;
  }

  // Running Lights - WLED sine wave
  if (effect_name.find("running") != std::string::npos || effect_name.find("sine") != std::string::npos) {
    const uint8_t wave_offset = static_cast<uint8_t>((counter >> 2) & 0xFF);
    const uint8_t wave_count = 2 + (intensity_val >> 5);
    
    for (uint16_t i = 0; i < pixels; ++i) {
      const uint8_t pos = wave_offset + static_cast<uint8_t>((i * wave_count * 256) / pixels);
      const uint8_t final_pos = reverse ? (255 - pos) : pos;
      // sin8 approximation: use lookup or sine
      const float phase = final_pos / 255.0f * 6.2831f;
      const float wave = (sinf(phase) + 1.0f) * 0.5f;
      *dst++ = to_byte(c1.r * brightness * wave);
      *dst++ = to_byte(c1.g * brightness * wave);
      *dst++ = to_byte(c1.b * brightness * wave);
    }
    return frame;
  }

  // Comet - head with trailing tail
  if (effect_name.find("comet") != std::string::npos) {
    // Per-device comet trail state
    const std::string state_key = "comet_" + binding.device_id + "_" + std::to_string(pixels);
    auto& comet_trail = get_wled_state(state_key, pixels);
    
    const uint8_t tail_len = 5 + (intensity_val >> 4);
    const int head_pos = (counter >> 3) % (pixels + tail_len);
    
    // Fade trail
    for (uint16_t i = 0; i < pixels; ++i) {
      comet_trail[i] = (comet_trail[i] > 20) ? comet_trail[i] - 20 : 0;
    }
    
    // Draw head
    const int idx = reverse ? (pixels - 1 - head_pos) : head_pos;
    if (idx >= 0 && idx < static_cast<int>(pixels)) {
      comet_trail[idx] = 255;
    }
    
    for (uint16_t i = 0; i < pixels; ++i) {
      const float level = comet_trail[i] / 255.0f;
      *dst++ = to_byte(c1.r * brightness * level);
      *dst++ = to_byte(c1.g * brightness * level);
      *dst++ = to_byte(c1.b * brightness * level);
    }
    return frame;
  }

  // Colorloop - all LEDs cycle through hue together
  if (effect_name.find("colorloop") != std::string::npos) {
    const uint8_t hue = static_cast<uint8_t>((counter >> 3) & 0xFF);
    const Rgb col = color_wheel(hue);
    for (uint16_t i = 0; i < pixels; ++i) {
      *dst++ = to_byte(col.r * brightness);
      *dst++ = to_byte(col.g * brightness);
      *dst++ = to_byte(col.b * brightness);
    }
    return frame;
  }

  // Pride - FastLED/WLED pride effect
  if (effect_name.find("pride") != std::string::npos) {
    const uint16_t t1 = counter;
    for (uint16_t i = 0; i < pixels; ++i) {
      // Multiple overlapping sine waves
      uint8_t hue = static_cast<uint8_t>((i * 256 / pixels) + (t1 >> 4));
      hue += static_cast<uint8_t>(sinf(i * 0.1f + (t1 >> 6) * 0.1f) * 20);
      const Rgb col = color_wheel(hue);
      *dst++ = to_byte(col.r * brightness);
      *dst++ = to_byte(col.g * brightness);
      *dst++ = to_byte(col.b * brightness);
    }
    return frame;
  }

  // Plasma - animated plasma effect
  if (effect_name.find("plasma") != std::string::npos) {
    const float t1 = static_cast<float>(counter) * 0.01f;
    for (uint16_t i = 0; i < pixels; ++i) {
      const float pos = static_cast<float>(i) / pixels * 10.0f;
      float v = sinf(pos + t1);
      v += sinf((pos * 0.5f + t1 * 0.5f) * 2.0f);
      v += sinf((pos * 0.3f + t1 * 0.3f) * 3.0f);
      v = (v + 3.0f) / 6.0f;
      const uint8_t hue = static_cast<uint8_t>(v * 255) + static_cast<uint8_t>((counter >> 4) & 0xFF);
      const Rgb col = color_wheel(hue);
      *dst++ = to_byte(col.r * brightness);
      *dst++ = to_byte(col.g * brightness);
      *dst++ = to_byte(col.b * brightness);
    }
    return frame;
  }

  // Default fallback - gradient scroll
  const uint8_t offset = static_cast<uint8_t>((counter >> 4) & 0xFF);
  for (uint16_t i = 0; i < pixels; ++i) {
    const uint8_t pos = offset + static_cast<uint8_t>((i * 256) / pixels);
    const float t = pos / 255.0f;
    const Rgb col = gradient.empty() ?
      Rgb{c1.r * (1.0f - t) + c2.r * t, c1.g * (1.0f - t) + c2.g * t, c1.b * (1.0f - t) + c2.b * t} :
      sample_gradient(gradient, t);
    *dst++ = to_byte(col.r * brightness);
    *dst++ = to_byte(col.g * brightness);
    *dst++ = to_byte(col.b * brightness);
  }
  return frame;
}

bool WledEffectsRuntime::render_and_send(const WledEffectBinding& binding,
                                         const WledDeviceConfig& device,
                                         const std::string& ip,
                                         float time_s,
                                         uint32_t frame_idx,
                                         uint8_t global_brightness,
                                         uint16_t fps) {
  if (!binding.ddp) {
    ESP_LOGD(TAG, "render_and_send: DDP disabled for device %s", device.id.c_str());
    return false;
  }
  
  // Frame cache: reuse frame if same effect + same LED count + same frame_idx (for caching only)
  // Note: frame_idx is used for cache key but animation uses time_s for consistent speed
  const uint16_t leds = device.leds == 0 ? 60 : device.leds;
  FrameCacheKey cache_key{binding.effect.effect, leds, frame_idx};
  std::vector<uint8_t> frame;
  
  auto cache_it = frame_cache_.find(cache_key);
  if (cache_it != frame_cache_.end()) {
    frame = cache_it->second;  // Reuse cached frame
  } else {
    // Render new frame and cache it (using frame_idx for animation like WLED)
    frame = render_frame(binding, leds, frame_idx, global_brightness, fps, device.layout);
    
    // Verify frame is not empty (all zeros)
    bool frame_empty = true;
    for (size_t i = 0; i < frame.size(); ++i) {
      if (frame[i] != 0) {
        frame_empty = false;
        break;
      }
    }
    if (frame_empty) {  // Note: frame_idx-based logging removed
      const float intensity_val = binding.effect.intensity / 255.0f;
      ESP_LOGW(TAG, "Rendered frame is empty (all zeros) for effect '%s' on device %s (brightness=%.2f, intensity=%.2f)",
               binding.effect.effect.c_str(), device.id.c_str(), 
               binding.effect.brightness_override > 0 ? binding.effect.brightness_override / 255.0f : binding.effect.brightness / 255.0f,
               intensity_val);
    }
    
    // Only cache if cache is not too large (limit to 10 entries to avoid memory issues)
    if (frame_cache_.size() < 10) {
      frame_cache_[cache_key] = frame;
    }
  }
  
  uint16_t port = kDefaultDdpPort;
  if (cfg_ref_ && cfg_ref_->mqtt.ddp_port > 0) {
    port = cfg_ref_->mqtt.ddp_port;
  }
  // WLED DDP: use channel 0 for default/all segments (WLED ignores non-zero channels unless specifically configured)
  // LedFX also uses channel 0 for compatibility
  const uint32_t channel = 0;
  
  // Use cached address if available (faster than DNS resolution every time)
  const uint64_t now_us = esp_timer_get_time();
  auto addr_it = ddp_addr_cache_.find(ip);
  bool use_cache = false;
  
  if (addr_it != ddp_addr_cache_.end() && addr_it->second.valid) {
    const uint64_t age_us = now_us - addr_it->second.cached_at_us;
    if (age_us < DNS_CACHE_TTL_US) {
      use_cache = true;
    } else {
      // Cache expired, remove it
      ddp_addr_cache_.erase(addr_it);
    }
  }
  
  bool ok = false;
  if (use_cache) {
    // Use cached address (much faster - no DNS resolution)
    // Use complete frame function to handle packet splitting automatically
    ok = ddp_send_complete_frame_cached(&addr_it->second.addr, addr_it->second.addr_len, port, frame, channel, next_seq());
  } else {
    // First time or cache expired - resolve and cache
    // Use complete frame function to handle packet splitting automatically
    ok = ddp_send_complete_frame(ip, port, frame, channel, next_seq());
    if (ok) {
      // Cache the resolved address for next time
      struct sockaddr_storage addr;
      socklen_t addr_len = sizeof(addr);
      if (ddp_cache_resolve(ip, port, &addr, &addr_len)) {
        CachedAddrInfo cached;
        std::memcpy(&cached.addr, &addr, sizeof(addr));
        cached.addr_len = addr_len;
        cached.cached_at_us = now_us;
        cached.valid = true;
        ddp_addr_cache_[ip] = cached;
      }
    }
  }
  
  if (!ok) {
    // Log failure (rate-limited in task_loop using frame_idx)
    ESP_LOGW(TAG, "DDP send failed -> %s:%u (device %s, effect: %s, enabled=%d, ddp=%d, active=%d, leds=%u)", 
             ip.c_str(), port, device.id.c_str(), binding.effect.effect.c_str(),
             binding.enabled ? 1 : 0, binding.ddp ? 1 : 0, device.active ? 1 : 0, static_cast<unsigned>(leds));
    // Invalidate cache on failure
    ddp_addr_cache_.erase(ip);
  } else if (frame_idx % 60 == 0) {  // Log success every 1 second for debugging
    // Check if frame has any non-zero data
    bool has_data = false;
    size_t non_zero_count = 0;
    for (size_t i = 0; i < frame.size(); ++i) {
      if (frame[i] != 0) {
        has_data = true;
        non_zero_count++;
      }
    }
    ESP_LOGI(TAG, "DDP send OK -> %s:%u (device %s, effect: %s, %u LEDs, channel %lu, has_data=%d, non_zero_bytes=%zu/%zu, brightness=%.2f)", 
             ip.c_str(), port, device.id.c_str(), binding.effect.effect.c_str(), 
             static_cast<unsigned>(device.leds == 0 ? 60 : device.leds),
             static_cast<unsigned long>(channel),
             has_data ? 1 : 0, non_zero_count, frame.size(),
             binding.effect.brightness_override > 0 ? binding.effect.brightness_override / 255.0f : binding.effect.brightness / 255.0f);
  }
  
  return ok;
}

void WledEffectsRuntime::task_loop() {
  uint32_t frame_idx = 0;  // Frame counter for logging/debugging only
  uint64_t start_time_us = esp_timer_get_time();  // Start time for time-based animation
  // Variables for future use (audio sync timing, performance monitoring)
  // [[maybe_unused]] uint64_t last_audio_timestamp_us = 0;
  // [[maybe_unused]] uint64_t last_render_time_us = 0;
  
  while (running_) {
    WledEffectsConfig fx{};
    std::vector<WledDeviceConfig> devices_snapshot;
    std::vector<VirtualSegmentConfig> virtuals;
    std::vector<LedSegmentConfig> segments;
    uint8_t global_brightness = 255;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      fx = effects_;
      devices_snapshot = devices_;
      if (cfg_ref_) {
        global_brightness = cfg_ref_->led_engine.global_brightness;
        virtuals = cfg_ref_->virtual_segments;
        segments = cfg_ref_->led_engine.segments;
      }
      if (led_runtime_) {
        global_brightness = led_runtime_->brightness();
      }
    }

    const uint16_t fps = fx.target_fps == 0 ? 60 : std::clamp<uint16_t>(fx.target_fps, 1, 240);
    const TickType_t delay = pdMS_TO_TICKS(1000 / fps);
    
    // Cleanup old frame cache entries (keep only recent frames to avoid memory issues)
    // Frame cache is per-frame, so we only need to keep a few frames
    if (frame_idx % 10 == 0 && frame_cache_.size() > 5) {
      // Remove oldest entries (simple: clear cache every 10 frames)
      frame_cache_.clear();
    }
    
    // Synchronization: Check if we should wait for audio timestamp
    // This ensures LED effects are synchronized with audio playback on other Snapcast clients
    AudioMetrics audio_metrics = led_audio_get_metrics();
    const uint64_t now_us = esp_timer_get_time();
    
    // If audio has a timestamp and we're rendering audio-reactive LEDFx effects, sync to it
    // NOTE: Audio frame sync should ONLY apply to LEDFx effects with audio_link=true, NOT to WLED effects
    bool needs_audio_sync = false;
    for (const auto& binding : fx.bindings) {
      if (binding.enabled && binding.effect.audio_link) {
        // Only sync for LEDFx effects, not WLED effects
        const std::string engine = lower_copy(binding.effect.engine);
        const bool is_ledfx = (engine == "ledfx");
        if (is_ledfx) {
          needs_audio_sync = true;
          break;
        }
      }
    }
    
    if (needs_audio_sync && audio_metrics.timestamp_us > 0) {
      // Calculate when we should render this frame based on audio timestamp
      // We want to render slightly before the audio plays (account for LED update time)
      const uint64_t led_update_time_us = 5000;  // ~5ms for LED update
      const uint64_t target_render_time = audio_metrics.timestamp_us - led_update_time_us;
      
      if (target_render_time > now_us) {
        // Wait until it's time to render (but don't wait too long - max 50ms)
        const uint64_t wait_us = std::min(target_render_time - now_us, 50000ULL);
        if (wait_us > 1000) {  // Only wait if > 1ms
          vTaskDelay(pdMS_TO_TICKS((wait_us / 1000) + 1));
        }
      }
      // last_audio_timestamp_us = audio_metrics.timestamp_us;  // Reserved for future use
    }
    
    // last_render_time_us = esp_timer_get_time();  // Reserved for future use

    // Continue even if no WLED devices - we may have local segments to render
    const bool has_wled_bindings = !fx.bindings.empty() && !devices_snapshot.empty();
    const bool has_local_segments = cfg_ref_ && led_runtime_ && !segments.empty();
    const bool has_virtual_segments = !virtuals.empty();
    
    if (!has_wled_bindings && !has_local_segments && !has_virtual_segments) {
      vTaskDelay(pdMS_TO_TICKS(400));
      continue;
    }
    const auto status = wled_discovery_status();
    
    // WLED effects run independently - check if any bindings are enabled
    // Individual bindings control their own enabled state via binding.enabled
    bool has_enabled_bindings = false;
    for (const auto& binding : fx.bindings) {
      if (binding.enabled && binding.ddp && !binding.device_id.empty()) {
        has_enabled_bindings = true;
        break;
      }
    }
    
    // Track which devices have active DDP mode for cleanup when all bindings are disabled
    static bool had_enabled_bindings = false;
    
    // If all bindings were just disabled, disable DDP mode for all active devices
    if (had_enabled_bindings && !has_enabled_bindings) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!active_ddp_devices_.empty()) {
        ESP_LOGI(TAG, "All WLED bindings disabled - disabling DDP mode for %zu WLED devices", active_ddp_devices_.size());
        for (const auto& ip : active_ddp_devices_) {
          disable_wled_ddp_mode(ip);
        }
        active_ddp_devices_.clear();
      }
    }
    had_enabled_bindings = has_enabled_bindings;
    
    // Continue even if no enabled bindings - we may have local/virtual segments to render
    // But skip WLED rendering if no enabled bindings
    
    // Render effects for WLED devices (via DDP)
    // This is the central controller: generates effects (WLED or LEDFx, audio-reactive if enabled) and sends to WLED devices
    // Each WLED device can have its own effect assignment - effects react to music from Snapcast if audio_link=true
    if (fx.bindings.empty()) {
      if (frame_idx % 300 == 0) {  // Log every 5 seconds
        ESP_LOGI(TAG, "No WLED effect bindings configured");
      }
    } else if (devices_snapshot.empty()) {
      if (frame_idx % 300 == 0) {  // Log every 5 seconds
        ESP_LOGI(TAG, "No WLED devices configured (%zu bindings)", fx.bindings.size());
      }
    } else if (frame_idx % 300 == 0) {  // Log every 5 seconds
      ESP_LOGI(TAG, "WLED render loop: %zu bindings, %zu devices, has_enabled_bindings=%d", 
               fx.bindings.size(), devices_snapshot.size(), has_enabled_bindings ? 1 : 0);
    }
    for (const auto& binding : fx.bindings) {
      // Resolve device first to get IP (needed for cleanup even if disabled)
      WledDeviceConfig dev{};
      std::string ip;
      bool device_resolved = false;
      if (!binding.device_id.empty()) {
        device_resolved = resolve_device(binding.device_id, devices_snapshot, status, dev, ip);
      } else {
        if (frame_idx % 300 == 0) {
          ESP_LOGW(TAG, "Binding has empty device_id");
        }
      }
      
      // Debug logging for DDP sending issues
      if (frame_idx % 300 == 0) {
        ESP_LOGI(TAG, "Binding check: device_id=%s, enabled=%d, ddp=%d, resolved=%d, ip=%s, effect=%s",
                 binding.device_id.c_str(), binding.enabled ? 1 : 0, binding.ddp ? 1 : 0,
                 device_resolved ? 1 : 0, ip.c_str(), binding.effect.effect.c_str());
      }
      
      // If this binding is disabled, just skip it (don't disable DDP - another binding may use same IP)
      if (!binding.enabled || !binding.ddp || !device_resolved || ip.empty()) {
        continue;
      }
      
      // Check if effect is assigned
      if (binding.effect.effect.empty()) {
        ESP_LOGW(TAG, "Binding for device %s has no effect assigned", binding.device_id.c_str());
        continue;
      }
      
      // Track this device as having active DDP mode (for cleanup when disabled)
      // NOTE: We don't call enable_wled_ddp_mode() - just send DDP packets like LedFX does
      // WLED automatically enters live mode when receiving DDP data
      {
        std::lock_guard<std::mutex> lock(mutex_);
        active_ddp_devices_.insert(ip);
      }
      
      // Calculate time in seconds since start for time-based animation
      const float time_s = static_cast<float>(esp_timer_get_time() - start_time_us) / 1'000'000.0f;
      if (frame_idx % 300 == 0) {  // Debug: log time_s every 5 seconds
        ESP_LOGI(TAG, "Animation time_s=%.2f, frame_idx=%lu for device %s", time_s, static_cast<unsigned long>(frame_idx), ip.c_str());
      }
      // Use per-device FPS if set, otherwise fall back to global FPS
      const uint16_t device_fps = binding.fps > 0 ? std::clamp<uint16_t>(binding.fps, 1, 120) : fps;
      const bool ok = render_and_send(binding, dev, ip, time_s, frame_idx, global_brightness, device_fps);
      if (ok && frame_idx % 300 == 0) {  // Log success every 5 seconds
        ESP_LOGI(TAG, "Successfully rendering effect '%s' to device %s (%s:%u, enabled=%d, ddp=%d)", 
                 binding.effect.effect.c_str(), binding.device_id.c_str(), ip.c_str(), 
                 cfg_ref_ && cfg_ref_->mqtt.ddp_port > 0 ? cfg_ref_->mqtt.ddp_port : 4048,
                 binding.enabled ? 1 : 0, binding.ddp ? 1 : 0);
      } else if (!ok) {
        // Log failure more frequently for debugging
        if (frame_idx % 60 == 0) {  // Every 1 second at 60fps
          ESP_LOGW(TAG, "Failed to render/send effect '%s' to device %s (%s:%u, enabled=%d, ddp=%d, active=%d)", 
                   binding.effect.effect.c_str(), binding.device_id.c_str(), ip.c_str(), 
                   cfg_ref_ && cfg_ref_->mqtt.ddp_port > 0 ? cfg_ref_->mqtt.ddp_port : 4048,
                   binding.enabled ? 1 : 0, binding.ddp ? 1 : 0, dev.active ? 1 : 0);
        }
      }
    }
    
    // Clean up devices that are no longer active (removed from bindings or device not found)
    // This is a backup cleanup - immediate cleanup happens above when binding is disabled
    // Check more frequently (every 5 seconds instead of 30) for faster response
    if (frame_idx % 300 == 0) {  // Check every 5 seconds at 60fps
      std::unordered_set<std::string> current_active_ips;
      for (const auto& binding : fx.bindings) {
        if (!binding.enabled || !binding.ddp || binding.device_id.empty()) {
          continue;
        }
        WledDeviceConfig dev{};
        std::string ip;
        if (resolve_device(binding.device_id, devices_snapshot, status, dev, ip) && !ip.empty()) {
          current_active_ips.insert(ip);
        }
      }
      // Disable DDP for devices that are no longer in active bindings
      std::lock_guard<std::mutex> lock(mutex_);
      for (auto it = active_ddp_devices_.begin(); it != active_ddp_devices_.end();) {
        if (current_active_ips.find(*it) == current_active_ips.end()) {
          ESP_LOGI(TAG, "Device %s no longer in active bindings - disabling DDP mode", it->c_str());
          disable_wled_ddp_mode(*it);
          it = active_ddp_devices_.erase(it);
        } else {
          ++it;
        }
      }
    }

    // Render effects for local physical segments
    // Physical LEDs can use WLED effects (for visual consistency with WLED devices) or LEDFx effects (audio-reactive)
    // When audio is enabled (audio_link=true), effects react to music from Snapcast
    // This is the central controller: generates effects and renders locally to physical LEDs
    // Optimized: batch render segments with same effect to reuse frames
    if (cfg_ref_ && led_runtime_) {
      const auto& assignments = cfg_ref_->led_engine.effects.assignments;
      
      // Group segments by effect (for frame caching optimization)
      std::unordered_map<std::string, std::vector<const LedSegmentConfig*>> segments_by_effect;
      for (const auto& seg : segments) {
        if (!seg.enabled || seg.effect_source != "local") {
          continue;
        }
        auto it = std::find_if(assignments.begin(), assignments.end(),
                               [&](const EffectAssignment& a) { return a.segment_id == seg.id; });
        if (it == assignments.end()) {
          continue;
        }
        // Create cache key for grouping (effect + LED count + audio state)
        std::string effect_key = it->effect + "_" + std::to_string(seg.led_count) + "_" + 
                                 (it->audio_link ? "audio" : "noaudio");
        segments_by_effect[effect_key].push_back(&seg);
      }
      
      // Render each group (reuse frame for segments with same effect)
      for (const auto& [effect_key, seg_group] : segments_by_effect) {
        if (seg_group.empty()) continue;
        
        const auto& first_seg = *seg_group[0];
        auto it = std::find_if(assignments.begin(), assignments.end(),
                               [&](const EffectAssignment& a) { return a.segment_id == first_seg.id; });
        if (it == assignments.end()) continue;
        
        // Convert EffectAssignment to WledEffectBinding for rendering
        WledEffectBinding local_binding{};
        local_binding.device_id = first_seg.id;
        local_binding.segment_index = 0;
        local_binding.enabled = true;
        local_binding.ddp = false;
        local_binding.audio_channel = "mix";
        local_binding.effect = *it;
        
        // Render frame once (reuse for all segments in group if same LED count)
        const uint16_t common_led_count = first_seg.led_count;
        FrameCacheKey cache_key{it->effect, common_led_count, frame_idx};
        std::vector<uint8_t> frame;
        
        auto cache_it = frame_cache_.find(cache_key);
        if (cache_it != frame_cache_.end()) {
          frame = cache_it->second;
        } else {
          frame = render_frame(local_binding, common_led_count, frame_idx, global_brightness, fps);
          if (frame_cache_.size() < 10) {
            frame_cache_[cache_key] = frame;
          }
        }
        
        // Render to all segments in group
        for (const auto* seg_ptr : seg_group) {
          if (!frame.empty()) {
            const esp_err_t res = led_runtime_->render_frame(frame, *seg_ptr, 0, seg_ptr->led_count);
            if (res != ESP_OK) {
              ESP_LOGD(TAG, "Local render error %s for segment %s", esp_err_to_name(res), seg_ptr->id.c_str());
            }
          }
        }
      }
    }

    // Virtual segments: render a single frame and distribute to members (WLED + physical)
    auto find_segment = [&](const VirtualSegmentMember& m) -> const LedSegmentConfig* {
      if (m.id.empty()) {
        return nullptr;
      }
      auto it = std::find_if(segments.begin(), segments.end(), [&](const LedSegmentConfig& s) {
        return s.id == m.id;
      });
      if (it != segments.end()) {
        return &(*it);
      }
      if (m.segment_index < segments.size()) {
        return &segments[m.segment_index];
      }
      return nullptr;
    };
    for (const auto& vseg : virtuals) {
      if (vseg.id.empty() || vseg.enabled == false) {
        continue;
      }
      // Sum total length; for WLED member with length 0, default to device LEDs
      size_t total_leds = 0;
      for (const auto& m : vseg.members) {
        if (m.type == "physical") {
          const auto* seg = find_segment(m);
          if (!seg) {
            continue;
          }
          const uint16_t available = seg->led_count;
          const uint16_t start = std::min<uint16_t>(m.start, available);
          const uint16_t len = m.length > 0 ? std::min<uint16_t>(m.length, static_cast<uint16_t>(available - start))
                                            : static_cast<uint16_t>(available - start);
          total_leds += len;
        } else if (m.type == "wled") {
          const auto dev_it = std::find_if(devices_snapshot.begin(), devices_snapshot.end(), [&](const WledDeviceConfig& d) { return d.id == m.id; });
          total_leds += (dev_it != devices_snapshot.end() && dev_it->leds > 0) ? dev_it->leds : 0;
        }
      }
      if (total_leds == 0) {
        continue;
        }
        WledEffectBinding fake{};
        fake.device_id = vseg.id;
        fake.effect = vseg.effect;
        fake.audio_channel = "mix";
        const auto frame = render_frame(fake, static_cast<uint16_t>(total_leds), frame_idx, global_brightness, fps);
        size_t cursor = 0;
        for (const auto& m : vseg.members) {
          uint16_t length = m.length;
          uint16_t start = m.start;
          if (m.type == "wled") {
            if (length == 0) {
              const auto dev_it = std::find_if(devices_snapshot.begin(), devices_snapshot.end(), [&](const WledDeviceConfig& d) { return d.id == m.id; });
              if (dev_it != devices_snapshot.end() && dev_it->leds > 0) {
                length = dev_it->leds;
              }
            }
          } else if (m.type == "physical") {
            const auto* seg = find_segment(m);
            if (seg) {
              const uint16_t available = seg->led_count;
              start = std::min<uint16_t>(start, available);
              const uint16_t fallback_len = static_cast<uint16_t>(available - start);
              length = length > 0 ? std::min<uint16_t>(length, fallback_len) : fallback_len;
            } else {
              length = 0;
            }
          }
          if (length == 0) continue;
          if (cursor + length * 3 > frame.size()) {
            length = static_cast<uint16_t>((frame.size() - cursor) / 3);
          }
          if (length == 0) break;
          if (m.type == "wled") {
            WledDeviceConfig dev{};
            std::string ip;
            if (!resolve_device(m.id, devices_snapshot, status, dev, ip)) {
              cursor += length * 3;
            continue;
          }
          const uint16_t port = cfg_ref_ && cfg_ref_->mqtt.ddp_port > 0 ? cfg_ref_->mqtt.ddp_port : kDefaultDdpPort;
          const uint32_t channel = 0;  // WLED DDP: use channel 0 for compatibility
            const std::vector<uint8_t> slice(frame.begin() + cursor, frame.begin() + cursor + length * 3);
            const bool ok = ddp_send_complete_frame(ip, port, slice, channel, next_seq());
            if (!ok) {
              ESP_LOGW(TAG, "DDP send failed (virtual %s) -> %s:%u", vseg.id.c_str(), ip.c_str(), port);
            }
          } else if (m.type == "physical") {
            const auto* seg = find_segment(m);
            const bool has_runtime = led_runtime_ != nullptr;
            if (!seg || !has_runtime) {
              ESP_LOGW(TAG,
                       "Virtual segment %s physical member %s skipped (runtime=%d, seg_found=%d)",
                       vseg.id.c_str(),
                       m.id.c_str(),
                       has_runtime ? 1 : 0,
                       seg ? 1 : 0);
              cursor += length * 3;
              continue;
            }
            const std::vector<uint8_t> slice(frame.begin() + cursor, frame.begin() + cursor + length * 3);
            const esp_err_t res = led_runtime_->render_frame(slice, *seg, start, length);
            if (res != ESP_OK) {
              ESP_LOGW(TAG,
                       "Render hook error %s for virtual %s member %s (start=%u len=%u)",
                       esp_err_to_name(res),
                       vseg.id.c_str(),
                       m.id.c_str(),
                       static_cast<unsigned>(start),
                       static_cast<unsigned>(length));
            }
          } else {
            ESP_LOGW(TAG, "Virtual segment %s member type %s not rendered (unsupported)", vseg.id.c_str(), m.type.c_str());
          }
          cursor += length * 3;
        if (cursor >= frame.size()) {
          break;
        }
      }
    }

    frame_idx++;
    vTaskDelay(delay);
  }
}

std::string WledEffectsRuntime::envelope_key(const WledEffectBinding& binding) const {
  return binding.device_id + ":" + std::to_string(binding.segment_index) + ":" + binding.effect.effect;
}

float WledEffectsRuntime::apply_envelope(const std::string& key,
                                         float input,
                                         uint16_t fps,
                                         uint16_t attack_ms,
                                         uint16_t release_ms) {
  if (fps == 0 || (attack_ms == 0 && release_ms == 0)) {
    return clamp01(input);
  }
  const float dt_ms = 1000.0f / static_cast<float>(fps);
  const float alpha_attack = std::min(1.0f, dt_ms / std::max(1.0f, static_cast<float>(attack_ms)));
  const float alpha_release = std::min(1.0f, dt_ms / std::max(1.0f, static_cast<float>(release_ms)));
  float level = 0.0f;
  if (auto it = envelope_state_.find(key); it != envelope_state_.end()) {
    level = it->second;
  }
  if (input > level) {
    level += (input - level) * alpha_attack;
  } else {
    level += (input - level) * alpha_release;
  }
  level = clamp01(level);
  envelope_state_[key] = level;
  return level;
}
