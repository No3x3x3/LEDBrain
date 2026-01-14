#include "temperature_monitor.hpp"
#include "driver/temperature_sensor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "temp_monitor";
static temperature_sensor_handle_t s_temp_sensor = nullptr;
static float s_last_temperature = 0.0f;
static bool s_initialized = false;

esp_err_t temperature_monitor_init() {
    if (s_initialized) {
        ESP_LOGW(TAG, "Temperature monitor already initialized");
        return ESP_OK;
    }

    // Configure temperature sensor for ESP32-P4
    // Range: -10°C to 110°C (supports monitoring up to 100°C+ for CPU junction temperature)
    temperature_sensor_config_t temp_sensor_config = {
        .range_min = -10,
        .range_max = 110,
        .clk_src = TEMPERATURE_SENSOR_CLK_SRC_DEFAULT,
        .flags = {}
    };

    esp_err_t ret = temperature_sensor_install(&temp_sensor_config, &s_temp_sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install temperature sensor: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = temperature_sensor_enable(s_temp_sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable temperature sensor: %s", esp_err_to_name(ret));
        temperature_sensor_uninstall(s_temp_sensor);
        s_temp_sensor = nullptr;
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Temperature monitor initialized (TSENS)");

    // Perform initial reading
    float temp = 0.0f;
    if (temperature_sensor_get_celsius(s_temp_sensor, &temp) == ESP_OK) {
        s_last_temperature = temp;
        ESP_LOGI(TAG, "Initial CPU temperature: %.2f°C", temp);
    }

    return ESP_OK;
}

esp_err_t temperature_monitor_get_cpu_temp(float* temp_celsius) {
    if (!s_initialized || s_temp_sensor == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (temp_celsius == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = temperature_sensor_get_celsius(s_temp_sensor, temp_celsius);
    if (ret == ESP_OK) {
        s_last_temperature = *temp_celsius;
    } else {
        // Return last known temperature if read fails
        *temp_celsius = s_last_temperature;
    }

    return ret;
}

void temperature_monitor_deinit() {
    if (!s_initialized || s_temp_sensor == nullptr) {
        return;
    }

    temperature_sensor_disable(s_temp_sensor);
    temperature_sensor_uninstall(s_temp_sensor);
    s_temp_sensor = nullptr;
    s_initialized = false;
    ESP_LOGI(TAG, "Temperature monitor deinitialized");
}
