#include "ota.hpp"

#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <string>

static const char* TAG = "ota";
const char* kDefaultOtaUrl = "https://github.com/No3x3x3/LEDBrain/releases/latest/download/ledbrain.bin";

namespace {
enum class OtaState { kIdle, kInProgress, kSucceeded, kFailed };

OtaState s_state = OtaState::kIdle;
esp_err_t s_last_err = ESP_OK;
std::string s_url;

void ota_task(void*) {
  ESP_LOGI(TAG, "Starting OTA from %s", s_url.c_str());
  esp_http_client_config_t http_cfg = {};
  http_cfg.url = s_url.c_str();
  http_cfg.timeout_ms = 15000;
  http_cfg.skip_cert_common_name_check = false;
  http_cfg.crt_bundle_attach = esp_crt_bundle_attach;

  esp_https_ota_config_t ota_cfg = {};
  ota_cfg.http_config = &http_cfg;

  const esp_err_t ret = esp_https_ota(&ota_cfg);
  s_last_err = ret;
  if (ret == ESP_OK) {
    s_state = OtaState::kSucceeded;
    ESP_LOGI(TAG, "OTA succeeded, rebooting");
    esp_restart();
  } else {
    s_state = OtaState::kFailed;
    ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(ret));
  }
  vTaskDelete(nullptr);
}
}  // namespace

esp_err_t ota_trigger_update(const char* url) {
  if (!url || std::strlen(url) == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_state == OtaState::kInProgress) {
    return ESP_ERR_INVALID_STATE;
  }
  s_url = url;
  s_state = OtaState::kInProgress;
  s_last_err = ESP_OK;
  const BaseType_t task = xTaskCreatePinnedToCore(ota_task, "ota_task", 8192, nullptr, 5, nullptr, 0);
  if (task != pdPASS) {
    s_state = OtaState::kIdle;
    return ESP_FAIL;
  }
  return ESP_OK;
}

const char* ota_state_string() {
  switch (s_state) {
    case OtaState::kIdle:
      return "idle";
    case OtaState::kInProgress:
      return "in_progress";
    case OtaState::kSucceeded:
      return "done";
    case OtaState::kFailed:
      return "failed";
  }
  return "unknown";
}

bool ota_in_progress() {
  return s_state == OtaState::kInProgress;
}

esp_err_t ota_last_error() {
  return s_last_err;
}
