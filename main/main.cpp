#include "config.hpp"
#include "ddp_tx.hpp"
#include "eth_init.hpp"
#include "led_engine.hpp"
#include "mqtt_ha.hpp"
#include "web_setup.hpp"
#include "wled_discovery.hpp"
#include "wled_effects.hpp"
#include "snapclient_light.hpp"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern bool mdns_start(const char* hostname);
extern void heartbeat_task(void*);

static const char* TAG = "main";
static LedEngineRuntime s_led_engine;
static AppConfig s_cfg{};
static WledEffectsRuntime s_wled_fx;

extern "C" void app_main(void) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(config_service_init());

  ESP_ERROR_CHECK(config_load(s_cfg));

  const esp_err_t led_init = s_led_engine.init(s_cfg.led_engine);
  if (led_init != ESP_OK) {
    ESP_LOGW(TAG, "LED engine init partial: %s", esp_err_to_name(led_init));
  }
  s_led_engine.set_enabled(s_cfg.autostart);

  ESP_LOGI(TAG, "LEDBrain ESP32-P4 baseline, IDF %s", IDF_VER);

  const esp_err_t eth_status = ethernet_start(s_cfg.network);
  if (eth_status != ESP_OK) {
    ESP_LOGE(TAG, "Ethernet init failed: %s", esp_err_to_name(eth_status));
  } else if (!mdns_start(s_cfg.network.hostname.c_str())) {
    ESP_LOGW(TAG, "mDNS init failed");
  }

  wled_discovery_start(s_cfg);
  if (s_cfg.led_engine.audio.source == AudioSourceType::Snapcast && s_cfg.led_engine.audio.snapcast.enabled) {
    snapclient_light_start(s_cfg.led_engine.audio.snapcast);
  }
  s_wled_fx.start(&s_cfg, &s_led_engine);
  start_web_server(s_cfg, &s_led_engine, &s_wled_fx);
  wled_discovery_trigger_scan();

  if (s_cfg.mqtt.configured && !s_cfg.mqtt.host.empty()) {
    mqtt_start(s_cfg);
  } else {
    ESP_LOGW(TAG, "MQTT nie skonfigurowane - ustaw w GUI (http://%s.local)", s_cfg.network.hostname.c_str());
  }

  ddp_tx_init(s_cfg.mqtt);

  xTaskCreatePinnedToCore(heartbeat_task, "heartbeat", 4096, nullptr, 5, nullptr, 0);
}
