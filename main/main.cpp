#include "config.hpp"
#include "ddp_tx.hpp"
#include "eth_init.hpp"
#include "led_engine.hpp"
#include "mqtt_ha.hpp"
#include "web_setup.hpp"
#include "wled_discovery.hpp"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern bool mdns_start(const char* hostname);
extern void heartbeat_task(void*);

static const char* TAG = "main";
static LedEngineRuntime s_led_engine;

extern "C" void app_main(void) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(config_service_init());

  AppConfig cfg{};
  ESP_ERROR_CHECK(config_load(cfg));

  const esp_err_t led_init = s_led_engine.init(cfg.led_engine);
  if (led_init != ESP_OK) {
    ESP_LOGW(TAG, "LED engine init partial: %s", esp_err_to_name(led_init));
  }
  s_led_engine.set_enabled(cfg.autostart);

  ESP_LOGI(TAG, "LEDBrain ESP32-P4 baseline, IDF %s", IDF_VER);

  const esp_err_t eth_status = ethernet_start(cfg.network);
  if (eth_status != ESP_OK) {
    ESP_LOGE(TAG, "Ethernet init failed: %s", esp_err_to_name(eth_status));
  } else if (!mdns_start(cfg.network.hostname.c_str())) {
    ESP_LOGW(TAG, "mDNS init failed");
  }

  wled_discovery_start(cfg);
  start_web_server(cfg, &s_led_engine);
  wled_discovery_trigger_scan();

  if (cfg.mqtt.configured && !cfg.mqtt.host.empty()) {
    mqtt_start(cfg);
  } else {
    ESP_LOGW(TAG, "MQTT nie skonfigurowane - ustaw w GUI (http://%s.local)", cfg.network.hostname.c_str());
  }

  ddp_tx_init(cfg.mqtt);

  xTaskCreatePinnedToCore(heartbeat_task, "heartbeat", 4096, nullptr, 5, nullptr, 0);
}
