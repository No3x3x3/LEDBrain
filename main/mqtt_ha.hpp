#pragma once
#include "mqtt_client.h"
#include "config.hpp"
#include "led_engine.hpp"

// Zwraca wskaźnik na aktywnego klienta MQTT
esp_mqtt_client_handle_t mqtt_handle();

// Uruchamia klienta MQTT na podstawie konfiguracji z AppConfig
bool mqtt_start(const AppConfig &cfg, LedEngineRuntime* runtime);

// Zatrzymuje klienta MQTT (opcjonalne)
void mqtt_stop();

// Publikuje stan (np. ON/OFF) do HA
void mqtt_publish_state(bool enabled);

// Publikuje Home Assistant discovery message
bool mqtt_publish_ha_discovery(const AppConfig& cfg);

// Testuje połączenie MQTT (publikuje test message)
bool mqtt_test_connection();