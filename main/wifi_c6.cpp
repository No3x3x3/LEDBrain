#include "wifi_c6.hpp"
#include "wifi_c6_ctrl.hpp"
#include "eppp_link.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "lwip/ip_addr.h"
#include <cstring>

static const char* TAG = "wifi_c6";

namespace {
  bool s_initialized = false;
  bool s_connected = false;
  esp_netif_t* s_netif = nullptr;
  std::string s_current_ssid;
  std::string s_current_ip;
  int s_rssi = 0;
}

esp_err_t wifi_c6_init() {
  if (s_initialized) {
    ESP_LOGW(TAG, "WiFi C6 already initialized");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Initializing WiFi via ESP32-C6 coprocessor (UART)...");

  // Configure eppp_link as CLIENT (ESP32-P4 connects to ESP32-C6)
  // ESP32-C6 will act as SERVER providing WiFi connectivity
  eppp_config_t config = EPPP_DEFAULT_CLIENT_CONFIG();
  config.transport = EPPP_TRANSPORT_UART;
  config.uart.port = WIFI_C6_UART_PORT;
  config.uart.baud = WIFI_C6_UART_BAUD;
  config.uart.tx_io = WIFI_C6_UART_TX;
  config.uart.rx_io = WIFI_C6_UART_RX;
  config.uart.queue_size = 32;  // Larger queue for WiFi data
  config.uart.rx_buffer_size = 2048;  // Larger buffer for WiFi packets
  config.uart.rts_io = -1;  // No hardware flow control
  config.uart.cts_io = -1;
  config.uart.flow_control = 0;
  
  // PPP configuration (already set by EPPP_DEFAULT_CLIENT_CONFIG)
  // ESP32-P4 (client) IP: 192.168.11.2
  // ESP32-C6 (server) IP: 192.168.11.1
  // Note: EPPP_DEFAULT_CLIENT_CONFIG already sets correct IPs
  config.ppp.netif_prio = 10;  // Lower priority than Ethernet
  config.ppp.netif_description = "pppos";

  // Task configuration
  config.task.run_task = true;
  config.task.stack_size = 8192;  // Larger stack for WiFi data
  config.task.priority = 5;

  ESP_LOGI(TAG, "Connecting to ESP32-C6 via UART%d (TX: GPIO%d, RX: GPIO%d, Baud: %d)",
    WIFI_C6_UART_PORT, WIFI_C6_UART_TX, WIFI_C6_UART_RX, WIFI_C6_UART_BAUD);

  // Connect to ESP32-C6 (PPP client mode)
  s_netif = eppp_connect(&config);
  if (s_netif == nullptr) {
    ESP_LOGE(TAG, "Failed to connect to ESP32-C6 coprocessor");
    return ESP_FAIL;
  }

  s_initialized = true;
  ESP_LOGI(TAG, "WiFi C6 initialized successfully");
  ESP_LOGI(TAG, "Waiting for ESP32-C6 to establish WiFi connection...");

  // Note: Actual WiFi connection (SSID, password) should be configured
  // on the ESP32-C6 side. This module only handles the PPP link.
  // The ESP32-C6 coprocessor should have its own firmware managing WiFi.

  return ESP_OK;
}

void wifi_c6_deinit() {
  if (!s_initialized) {
    return;
  }

  ESP_LOGI(TAG, "Deinitializing WiFi C6...");

  if (s_netif != nullptr) {
    eppp_close(s_netif);
    s_netif = nullptr;
  }

  s_initialized = false;
  s_connected = false;
  s_current_ssid.clear();
  s_current_ip.clear();
  s_rssi = 0;

  ESP_LOGI(TAG, "WiFi C6 deinitialized");
}

bool wifi_c6_is_connected() {
  if (!s_initialized || s_netif == nullptr) {
    return false;
  }

  // Check if network interface is up and has IP address
  esp_netif_ip_info_t ip_info;
  esp_err_t ret = esp_netif_get_ip_info(s_netif, &ip_info);
  if (ret == ESP_OK && ip_info.ip.addr != 0) {
    s_connected = true;
    // Update IP address string
    char ip_str[16];
    esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
    s_current_ip = ip_str;
    return true;
  }

  s_connected = false;
  return false;
}

std::string wifi_c6_get_ip() {
  if (!wifi_c6_is_connected()) {
    return "";
  }
  return s_current_ip;
}

std::string wifi_c6_get_ssid() {
  // Try to get SSID from control protocol if available
  if (wifi_c6_ctrl_is_available()) {
    std::string ssid, ip;
    int rssi;
    if (wifi_c6_ctrl_get_status(ssid, ip, rssi) == ESP_OK && !ssid.empty()) {
      s_current_ssid = ssid;
      return ssid;
    }
  }
  // Fallback to cached SSID
  return s_current_ssid.empty() ? "ESP32-C6" : s_current_ssid;
}

int wifi_c6_get_rssi() {
  // RSSI is managed by ESP32-C6, not directly accessible via PPP
  // This would require additional communication protocol with ESP32-C6
  return s_rssi;
}

esp_err_t wifi_c6_sta_start(const std::string& ssid, const std::string& password) {
  if (!s_initialized) {
    ESP_LOGE(TAG, "WiFi C6 not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  // Initialize control UART if not already done
  if (!wifi_c6_ctrl_is_available()) {
    esp_err_t ret = wifi_c6_ctrl_init();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize control UART: %s", esp_err_to_name(ret));
      return ret;
    }
  }

  // Send connect command to ESP32-C6
  ESP_LOGI(TAG, "Sending WiFi connect command to ESP32-C6: SSID=%s", ssid.c_str());
  esp_err_t ret = wifi_c6_ctrl_connect(ssid, password);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to send connect command: %s", esp_err_to_name(ret));
    return ret;
  }

  s_current_ssid = ssid;
  
  // Wait for connection (ESP32-C6 will connect in background)
  vTaskDelay(pdMS_TO_TICKS(5000));
  
  if (wifi_c6_is_connected()) {
    ESP_LOGI(TAG, "WiFi connected via ESP32-C6");
    return ESP_OK;
  }
  
  ESP_LOGW(TAG, "WiFi connection pending - ESP32-C6 is connecting...");
  return ESP_OK;  // Return OK even if not connected yet - connection is in progress
}

esp_err_t wifi_c6_ap_start(const std::string& ssid, const std::string& password) {
  if (!s_initialized) {
    ESP_LOGE(TAG, "WiFi C6 not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  // WiFi AP configuration should be done on ESP32-C6 side
  ESP_LOGW(TAG, "WiFi AP start: AP configuration should be done on ESP32-C6 firmware");
  ESP_LOGI(TAG, "Requested AP SSID: %s", ssid.c_str());
  
  s_current_ssid = ssid;
  
  return ESP_OK;
}

esp_err_t wifi_c6_sta_stop() {
  // WiFi STA stop should be done on ESP32-C6 side
  s_current_ssid.clear();
  s_connected = false;
  return ESP_OK;
}

esp_err_t wifi_c6_ap_stop() {
  // WiFi AP stop should be done on ESP32-C6 side
  s_current_ssid.clear();
  return ESP_OK;
}

