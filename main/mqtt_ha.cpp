#include "mqtt_client.h"
#include "esp_log.h"
#include "config.hpp"

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t client = nullptr;

// --- zwracanie klienta MQTT (używane np. przez heartbeat.cpp)
esp_mqtt_client_handle_t mqtt_handle() {
    return client;
}

// --- callback zdarzeń MQTT
static void mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Połączono z brokerem MQTT");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Rozłączono z brokerem MQTT");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT dane: %.*s", event->data_len, event->data);
            break;
        default:
            break;
    }
}

// --- start klienta MQTT na podstawie konfiguracji z AppConfig
bool mqtt_start(const AppConfig &cfg)
{
    ESP_LOGI(TAG, "Inicjalizacja MQTT...");

    if (cfg.mqtt.host.empty()) {
        ESP_LOGW(TAG, "Brak adresu brokera MQTT w konfiguracji");
        return false;
    }

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.hostname = cfg.mqtt.host.c_str();
    mqtt_cfg.broker.address.port = cfg.mqtt.port;
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    mqtt_cfg.broker.address.uri = nullptr;

    if (!cfg.mqtt.username.empty()) mqtt_cfg.credentials.username = cfg.mqtt.username.c_str();
    if (!cfg.mqtt.password.empty()) mqtt_cfg.credentials.authentication.password = cfg.mqtt.password.c_str();

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (!client) {
        ESP_LOGE(TAG, "Nie udało się zainicjalizować klienta MQTT");
        return false;
    }

    // rejestracja event handlera
    esp_mqtt_client_register_event(
        client,
        MQTT_EVENT_ANY,
        [](void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
            mqtt_event_handler_cb((esp_mqtt_event_handle_t)event_data);
        },
        nullptr
    );

    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Błąd uruchamiania klienta MQTT: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "MQTT klient uruchomiony %s:%u", cfg.mqtt.host.c_str(), cfg.mqtt.port);
    return true;
}
