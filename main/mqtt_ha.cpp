#include "mqtt_ha.hpp"
#include "mqtt_client.h"
#include "esp_log.h"
#include <string>
#include <cctype>

static const char* TAG = "MQTT";
static esp_mqtt_client_handle_t client = nullptr;
static LedEngineRuntime* s_runtime = nullptr;
static std::string s_base_topic;

esp_mqtt_client_handle_t mqtt_handle() {
    return client;
}

static bool payload_is_true(const char* data, int len) {
    if (!data || len <= 0) return false;
    std::string v(data, len);
    for (auto& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return v == "on" || v == "1" || v == "true";
}

static void publish_state(bool enabled) {
    if (!client || s_base_topic.empty()) {
        return;
    }
    std::string topic = s_base_topic + "/state";
    const char* payload = enabled ? "ON" : "OFF";
    esp_mqtt_client_publish(client, topic.c_str(), payload, 0, 1, true);
}

static void mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Połączono z brokerem MQTT");
            if (!s_base_topic.empty()) {
                std::string cmd_topic = s_base_topic + "/set";
                esp_mqtt_client_subscribe(client, cmd_topic.c_str(), 1);
                publish_state(s_runtime ? s_runtime->enabled() : false);
            }
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Rozłączono z brokerem MQTT");
            break;
        case MQTT_EVENT_DATA:
            if (event->topic && s_runtime && !s_base_topic.empty()) {
                std::string topic(event->topic, event->topic_len);
                if (topic == s_base_topic + "/set") {
                    bool on = payload_is_true(event->data, event->data_len);
                    s_runtime->set_enabled(on);
                    publish_state(on);
                    ESP_LOGI(TAG, "MQTT set -> %s", on ? "ON" : "OFF");
                }
            }
            break;
        default:
            break;
    }
}

bool mqtt_start(const AppConfig& cfg, LedEngineRuntime* runtime) {
    ESP_LOGI(TAG, "Inicjalizacja MQTT...");

    if (cfg.mqtt.host.empty()) {
        ESP_LOGW(TAG, "Brak adresu brokera MQTT w konfiguracji");
        return false;
    }
    s_runtime = runtime;
    s_base_topic = "ledbrain/" + cfg.network.hostname + "/led";

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

    esp_mqtt_client_register_event(
        client,
        MQTT_EVENT_ANY,
        [](void*, esp_event_base_t, int32_t, void* event_data) {
            mqtt_event_handler_cb((esp_mqtt_event_handle_t)event_data);
        },
        nullptr);

    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Błąd uruchamiania klienta MQTT: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "MQTT klient uruchomiony %s:%u", cfg.mqtt.host.c_str(), cfg.mqtt.port);
    return true;
}

void mqtt_stop() {
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = nullptr;
    }
}

void mqtt_publish_state(bool enabled) {
    publish_state(enabled);
}
