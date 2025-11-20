#pragma once

#include "esp_err.h"

// Trigger OTA update from the given HTTPS URL (expects a GitHub release asset).
// Returns ESP_ERR_INVALID_STATE if a previous update is still running.
esp_err_t ota_trigger_update(const char* url);

// Status helpers for UI/diagnostics.
const char* ota_state_string();
bool ota_in_progress();
esp_err_t ota_last_error();

// Default OTA binary location (latest GitHub release asset).
extern const char* kDefaultOtaUrl;
