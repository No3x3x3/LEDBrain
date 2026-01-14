#pragma once

#include "esp_err.h"
#include <cstdint>

// Initialize temperature sensor (TSENS) for ESP32-P4
esp_err_t temperature_monitor_init();

// Get current CPU temperature in Celsius
// Returns ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
esp_err_t temperature_monitor_get_cpu_temp(float* temp_celsius);

// Deinitialize temperature sensor
void temperature_monitor_deinit();
