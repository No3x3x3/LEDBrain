#include "ota.hpp"

#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
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

// For file upload OTA
esp_ota_handle_t s_ota_handle = 0;
const esp_partition_t* s_update_partition = nullptr;
size_t s_total_size = 0;
size_t s_written_size = 0;

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
  // Core 0: System task (OTA updates - network intensive)
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

float ota_progress() {
  if (s_state != OtaState::kInProgress || s_total_size == 0) {
    return 0.0f;
  }
  return static_cast<float>(s_written_size) / static_cast<float>(s_total_size);
}

esp_err_t ota_start_upload() {
  if (s_state == OtaState::kInProgress) {
    return ESP_ERR_INVALID_STATE;
  }
  
  s_update_partition = esp_ota_get_next_update_partition(nullptr);
  if (!s_update_partition) {
    ESP_LOGE(TAG, "No OTA partition found");
    return ESP_ERR_NOT_FOUND;
  }
  
  ESP_LOGI(TAG, "Starting OTA update to partition %s (offset 0x%08lx, size %lu)",
           s_update_partition->label,
           static_cast<unsigned long>(s_update_partition->address),
           static_cast<unsigned long>(s_update_partition->size));
  
  esp_err_t err = esp_ota_begin(s_update_partition, OTA_WITH_SEQUENTIAL_WRITES, &s_ota_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
    s_update_partition = nullptr;
    return err;
  }
  
  s_state = OtaState::kInProgress;
  s_last_err = ESP_OK;
  s_total_size = 0;
  s_written_size = 0;
  
  return ESP_OK;
}

esp_err_t ota_write_data(const void* data, size_t len) {
  if (s_state != OtaState::kInProgress || !s_ota_handle) {
    return ESP_ERR_INVALID_STATE;
  }
  
  if (s_total_size == 0) {
    // Estimate total size from partition size (will be updated when we know actual size)
    s_total_size = s_update_partition->size;
  }
  
  esp_err_t err = esp_ota_write(s_ota_handle, data, len);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
    s_last_err = err;
    esp_ota_abort(s_ota_handle);
    s_ota_handle = 0;
    s_state = OtaState::kFailed;
    return err;
  }
  
  s_written_size += len;
  ESP_LOGD(TAG, "OTA write progress: %lu / %lu bytes (%.1f%%)",
           static_cast<unsigned long>(s_written_size),
           static_cast<unsigned long>(s_total_size),
           ota_progress() * 100.0f);
  
  return ESP_OK;
}

esp_err_t ota_finish_upload() {
  if (s_state != OtaState::kInProgress || !s_ota_handle) {
    return ESP_ERR_INVALID_STATE;
  }
  
  esp_err_t err = esp_ota_end(s_ota_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
    if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
      ESP_LOGE(TAG, "Image validation failed. Image is corrupted");
    }
    s_last_err = err;
    s_ota_handle = 0;
    s_state = OtaState::kFailed;
    return err;
  }
  
  err = esp_ota_set_boot_partition(s_update_partition);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
    s_last_err = err;
    s_state = OtaState::kFailed;
    return err;
  }
  
  ESP_LOGI(TAG, "OTA update completed successfully, rebooting...");
  s_state = OtaState::kSucceeded;
  s_ota_handle = 0;
  s_update_partition = nullptr;
  s_total_size = 0;
  s_written_size = 0;
  
  // Give time for response to be sent
  vTaskDelay(pdMS_TO_TICKS(500));
  esp_restart();
  
  return ESP_OK;
}

esp_err_t ota_abort_upload() {
  if (s_state != OtaState::kInProgress || !s_ota_handle) {
    return ESP_ERR_INVALID_STATE;
  }
  
  ESP_LOGW(TAG, "Aborting OTA update");
  esp_ota_abort(s_ota_handle);
  s_ota_handle = 0;
  s_update_partition = nullptr;
  s_state = OtaState::kFailed;
  s_last_err = ESP_FAIL;
  s_total_size = 0;
  s_written_size = 0;
  
  return ESP_OK;
}
