#pragma once

#include <string>
#include "esp_err.h"
#include "esp_netif.h"

// WiFi via ESP32-C6 coprocessor using eppp_link (PPP over UART)
// ESP32-P4 (HOST) <--UART--> ESP32-C6 (SLAVE/Co-processor)
// ESP32-C6 acts as WiFi gateway providing network connectivity to ESP32-P4

namespace {
  // UART configuration for ESP32-C6 communication
  // Based on pinout.cpp: GPIO9 is "C6_IO9" - likely used for ESP32-C6 communication
  // ESP32-P4 (HOST/Client) <--UART--> ESP32-C6 (SLAVE/Server/Co-processor)
  // Typical configuration for JC-ESP32P4-M3-DEV:
  // Note: GPIO32 and GPIO33 are on header JP1-19 and JP1-21, available for use
  // GPIO9 is marked as "C6_IO9" - might be control/handshake, but UART typically needs TX/RX
  constexpr int WIFI_C6_UART_PORT = 1;  // UART1 (UART0 is console on GPIO3)
  constexpr int WIFI_C6_UART_TX = 32;   // TX pin from ESP32-P4 to ESP32-C6 RX (JP1-19)
  constexpr int WIFI_C6_UART_RX = 33;   // RX pin from ESP32-C6 TX to ESP32-P4 (JP1-21)
  constexpr int WIFI_C6_UART_BAUD = 921600;  // High-speed UART (up to 3Mbps supported)
  // Alternative configuration if GPIO9 is used:
  // constexpr int WIFI_C6_UART_TX = 9;   // If C6_IO9 is TX
  // constexpr int WIFI_C6_UART_RX = 33;  // Or other pin
}

// Initialize WiFi via ESP32-C6 coprocessor
// Returns ESP_OK on success, error code otherwise
esp_err_t wifi_c6_init();

// Deinitialize WiFi
void wifi_c6_deinit();

// Check if WiFi is connected
bool wifi_c6_is_connected();

// Get WiFi IP address
std::string wifi_c6_get_ip();

// Get WiFi SSID
std::string wifi_c6_get_ssid();

// Get WiFi RSSI (signal strength)
int wifi_c6_get_rssi();

// Start WiFi station mode (connect to existing network)
esp_err_t wifi_c6_sta_start(const std::string& ssid, const std::string& password);

// Start WiFi AP mode (create access point)
esp_err_t wifi_c6_ap_start(const std::string& ssid, const std::string& password);

// Stop WiFi station mode
esp_err_t wifi_c6_sta_stop();

// Stop WiFi AP mode
esp_err_t wifi_c6_ap_stop();

// WiFi scan and configuration functions (via control protocol)
#include "wifi_c6_ctrl.hpp"

