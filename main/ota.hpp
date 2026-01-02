#pragma once

#include "esp_err.h"

// Trigger OTA update from the given HTTPS URL (expects a GitHub release asset).
// Returns ESP_ERR_INVALID_STATE if a previous update is still running.
esp_err_t ota_trigger_update(const char* url);

// Start OTA update from uploaded file data (multipart/form-data).
// Call ota_write_data() repeatedly with chunks, then ota_finish().
// Returns ESP_ERR_INVALID_STATE if a previous update is still running.
esp_err_t ota_start_upload();
esp_err_t ota_write_data(const void* data, size_t len);
esp_err_t ota_finish_upload();
esp_err_t ota_abort_upload();  // Abort current upload if in progress

// Status helpers for UI/diagnostics.
const char* ota_state_string();
bool ota_in_progress();
esp_err_t ota_last_error();
float ota_progress();  // 0.0 to 1.0

// Default OTA binary location (latest GitHub release asset).
extern const char* kDefaultOtaUrl;
