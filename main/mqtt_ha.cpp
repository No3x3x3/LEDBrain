#include "mqtt_ha.hpp"
#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string>
#include <cctype>
#include <algorithm>

static const char* TAG = "MQTT";
static esp_mqtt_client_handle_t client = nullptr;
static LedEngineRuntime* s_runtime = nullptr;
static std::string s_base_topic;
static AppConfig* s_cfg = nullptr;

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
                if (s_cfg) {
                    mqtt_publish_ha_discovery(*s_cfg);
                }
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
    s_cfg = const_cast<AppConfig*>(&cfg);
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

static std::string sanitize_identifier(const std::string& input) {
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        if (std::isalnum(c) || c == '_' || c == '-') {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else if (c == ' ' || c == '.') {
            result += '_';
        }
    }
    return result;
}

bool mqtt_publish_ha_discovery(const AppConfig& cfg) {
    if (!client || cfg.mqtt.host.empty()) {
        return false;
    }

    std::string device_id = sanitize_identifier(cfg.network.hostname);
    std::string unique_id = "ledbrain_" + device_id;
    std::string discovery_topic = "homeassistant/switch/" + unique_id + "/config";
    std::string state_topic = s_base_topic + "/state";
    std::string command_topic = s_base_topic + "/set";

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return false;
    }

    cJSON_AddStringToObject(root, "name", ("LEDBrain " + cfg.network.hostname).c_str());
    cJSON_AddStringToObject(root, "unique_id", unique_id.c_str());
    cJSON_AddStringToObject(root, "state_topic", state_topic.c_str());
    cJSON_AddStringToObject(root, "command_topic", command_topic.c_str());
    cJSON_AddStringToObject(root, "payload_on", "ON");
    cJSON_AddStringToObject(root, "payload_off", "OFF");
    cJSON_AddStringToObject(root, "state_on", "ON");
    cJSON_AddStringToObject(root, "state_off", "OFF");
    cJSON_AddStringToObject(root, "icon", "mdi:led-strip");

    cJSON* device = cJSON_CreateObject();
    if (device) {
        cJSON_AddStringToObject(device, "identifiers", unique_id.c_str());
        cJSON_AddStringToObject(device, "name", ("LEDBrain " + cfg.network.hostname).c_str());
        cJSON_AddStringToObject(device, "model", "LEDBrain");
        cJSON_AddStringToObject(device, "manufacturer", "LEDBrain");
        cJSON_AddItemToObject(root, "device", device);
    }

    char* json_str = cJSON_PrintUnformatted(root);
    if (!json_str) {
        cJSON_Delete(root);
        return false;
    }

    int msg_id = esp_mqtt_client_publish(client, discovery_topic.c_str(), json_str, 0, 1, true);
    cJSON_free(json_str);
    cJSON_Delete(root);

    if (msg_id < 0) {
        ESP_LOGE(TAG, "Nie udało się opublikować discovery message");
        return false;
    }

    ESP_LOGI(TAG, "Opublikowano HA discovery: %s", discovery_topic.c_str());
    return true;
}

bool mqtt_test_connection() {
    if (!client || s_base_topic.empty()) {
        return false;
    }

    std::string test_topic = s_base_topic + "/test";
    const char* test_payload = "test";
    int msg_id = esp_mqtt_client_publish(client, test_topic.c_str(), test_payload, 0, 0, false);

    if (msg_id < 0) {
        ESP_LOGE(TAG, "Test publikacji nie powiódł się");
        return false;
    }

    ESP_LOGI(TAG, "Test publikacji wysłany do %s", test_topic.c_str());
    return true;
}
