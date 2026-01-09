#include "heartbeat.hpp"
#include "esp_log.h"
#include "mqtt_ha.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"

static const char* TAG = "heartbeat";

void heartbeat_task(void*) {
    int counter = 0;

    while (true) {
        ++counter;
        ESP_LOGD(TAG, "alive (%d)", counter);

        // Pobierz klienta MQTT
        auto client = mqtt_handle();
        if (client) {
            // publikuj komunikat co sekundę
            char msg[64];
            snprintf(msg, sizeof(msg), "{\"uptime\":%d}", counter);
            esp_mqtt_client_publish(client, "ledbrain/status", msg, 0, 0, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 sekunda
    }
}
