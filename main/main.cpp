#include "config.hpp"
#include "ddp_tx.hpp"
#include "eth_init.hpp"
#include "wifi_c6.hpp"  // WiFi via ESP32-C6 coprocessor (PPP over UART)
#include "wifi_c6_ctrl.hpp"  // Control protocol for ESP32-C6
#include "sd_card.hpp"  // SD card support for 64GB microSD (FAT32/exFAT)
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

static void network_monitor_task(void* arg);

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

  // Initialize SD card (64GB microSD for data storage)
  const esp_err_t sd_status = sd_card_init();
  if (sd_status == ESP_OK) {
    ESP_LOGI(TAG, "SD card initialized: %s (Total: %.2f GB, Free: %.2f GB)",
      sd_card_get_mount_point().c_str(),
      sd_card_get_total_size() / (1024.0 * 1024.0 * 1024.0),
      sd_card_get_free_size() / (1024.0 * 1024.0 * 1024.0));
    ESP_LOGI(TAG, "SD card filesystem: %s", sd_card_get_filesystem_type().c_str());
  } else {
    ESP_LOGW(TAG, "SD card initialization failed: %s (continuing without SD card)", esp_err_to_name(sd_status));
    ESP_LOGI(TAG, "Note: Critical config and firmware remain on flash (SPIFFS)");
  }

  const esp_err_t eth_status = ethernet_start(s_cfg.network);
  bool eth_connected = false;
  if (eth_status == ESP_OK) {
    // Wait a bit for Ethernet to connect
    vTaskDelay(pdMS_TO_TICKS(2000));
    eth_connected = ethernet_is_connected();
    if (eth_connected) {
      ESP_LOGI(TAG, "Ethernet connected: %s", ethernet_get_ip().c_str());
      if (!mdns_start(s_cfg.network.hostname.c_str())) {
        ESP_LOGW(TAG, "mDNS init failed");
      }
    } else {
      ESP_LOGW(TAG, "Ethernet not connected");
    }
  } else {
    ESP_LOGE(TAG, "Ethernet init failed: %s", esp_err_to_name(eth_status));
  }

  // WiFi via ESP32-C6 coprocessor (PPP over UART)
  // Initialize WiFi connection through ESP32-C6 if Ethernet is not available
  if (!eth_connected) {
    ESP_LOGI(TAG, "Ethernet not connected, initializing WiFi via ESP32-C6...");
    const esp_err_t wifi_status = wifi_c6_init();
    if (wifi_status == ESP_OK) {
      // Initialize control UART for WiFi configuration
      wifi_c6_ctrl_init();
      
      // Try to connect to saved WiFi network if available
      if (s_cfg.network.wifi_enabled && !s_cfg.network.wifi_ssid.empty()) {
        ESP_LOGI(TAG, "Connecting to WiFi network: %s", s_cfg.network.wifi_ssid.c_str());
        wifi_c6_sta_start(s_cfg.network.wifi_ssid, s_cfg.network.wifi_password);
      } else {
        ESP_LOGI(TAG, "No WiFi credentials configured. ESP32-C6 is in AP mode for provisioning.");
        ESP_LOGI(TAG, "Connect to 'LEDBrain-Setup-C6' (password: ledbrain123) to configure WiFi.");
      }
      
      // Wait a bit for WiFi connection
      vTaskDelay(pdMS_TO_TICKS(3000));
      
      if (wifi_c6_is_connected()) {
        ESP_LOGI(TAG, "WiFi connected via ESP32-C6: %s (IP: %s)", 
          wifi_c6_get_ssid().c_str(), wifi_c6_get_ip().c_str());
        if (!mdns_start(s_cfg.network.hostname.c_str())) {
          ESP_LOGW(TAG, "mDNS init failed");
        }
      } else {
        ESP_LOGW(TAG, "WiFi connection pending - ESP32-C6 is in AP mode or connecting...");
      }
    } else {
      ESP_LOGW(TAG, "WiFi C6 initialization failed: %s (continuing without WiFi)", esp_err_to_name(wifi_status));
    }
  }

  wled_discovery_start(s_cfg);
  if (s_cfg.led_engine.audio.source == AudioSourceType::Snapcast && s_cfg.led_engine.audio.snapcast.enabled) {
    snapclient_light_start(s_cfg.led_engine.audio.snapcast);
  }
  s_wled_fx.start(&s_cfg, &s_led_engine);
  start_web_server(s_cfg, &s_led_engine, &s_wled_fx);
  wled_discovery_trigger_scan();

  if (s_cfg.mqtt.configured && !s_cfg.mqtt.host.empty()) {
    mqtt_start(s_cfg, &s_led_engine);
  } else {
    ESP_LOGW(TAG, "MQTT nie skonfigurowane - ustaw w GUI (http://%s.local)", s_cfg.network.hostname.c_str());
  }

  ddp_tx_init(s_cfg.mqtt);

  // Core 0: System and network tasks (heartbeat, discovery, OTA, web server, MQTT, mDNS)
  // Core 1: Real-time tasks (LED effects rendering, audio processing)
  xTaskCreatePinnedToCore(heartbeat_task, "heartbeat", 4096, nullptr, 5, nullptr, 0);
  
  // Network monitoring task - automatically switch between Ethernet and WiFi
  xTaskCreatePinnedToCore(network_monitor_task, "net_monitor", 4096, nullptr, 3, nullptr, 0);
}

// Network monitoring task - automatically switch between Ethernet and WiFi
static void network_monitor_task(void* arg) {
  bool eth_was_connected = false;
  bool wifi_was_active = false;
  
  ESP_LOGI(TAG, "Network monitor task started");
  
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(5000));  // Check every 5 seconds
    
    bool eth_connected = ethernet_is_connected();
    
    if (eth_connected && !eth_was_connected) {
      // Ethernet just connected - disable WiFi C6
      ESP_LOGI(TAG, "Ethernet connected, disabling WiFi C6");
      if (wifi_was_active) {
        wifi_c6_deinit();
        wifi_c6_ctrl_deinit();
        wifi_was_active = false;
      }
      eth_was_connected = true;
      
      // Restart mDNS with Ethernet
      if (!mdns_start(s_cfg.network.hostname.c_str())) {
        ESP_LOGW(TAG, "mDNS restart failed");
      }
    } else if (!eth_connected && eth_was_connected) {
      // Ethernet just disconnected - enable WiFi C6
      ESP_LOGI(TAG, "Ethernet disconnected, enabling WiFi C6");
      eth_was_connected = false;
      
      if (!wifi_was_active) {
        esp_err_t wifi_status = wifi_c6_init();
        if (wifi_status == ESP_OK) {
          wifi_c6_ctrl_init();
          wifi_was_active = true;
          
          // Try to connect to saved WiFi network
          if (s_cfg.network.wifi_enabled && !s_cfg.network.wifi_ssid.empty()) {
            ESP_LOGI(TAG, "Connecting to saved WiFi: %s", s_cfg.network.wifi_ssid.c_str());
            wifi_c6_sta_start(s_cfg.network.wifi_ssid, s_cfg.network.wifi_password);
          }
          
          // Wait a bit and check connection
          vTaskDelay(pdMS_TO_TICKS(3000));
          if (wifi_c6_is_connected()) {
            ESP_LOGI(TAG, "WiFi connected via ESP32-C6");
            if (!mdns_start(s_cfg.network.hostname.c_str())) {
              ESP_LOGW(TAG, "mDNS restart failed");
            }
          }
        }
      }
    } else if (!eth_connected && !wifi_was_active) {
      // No Ethernet, no WiFi - try to initialize WiFi
      esp_err_t wifi_status = wifi_c6_init();
      if (wifi_status == ESP_OK) {
        wifi_c6_ctrl_init();
        wifi_was_active = true;
        
        if (s_cfg.network.wifi_enabled && !s_cfg.network.wifi_ssid.empty()) {
          wifi_c6_sta_start(s_cfg.network.wifi_ssid, s_cfg.network.wifi_password);
        }
      }
    }
  }
}
