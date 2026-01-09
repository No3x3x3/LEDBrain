#pragma once

#include <string>
#include <vector>
#include "esp_err.h"

// Control protocol for ESP32-P4 ↔ ESP32-C6 communication
// Uses UART2 for control commands (separate from UART1 used for PPP)

struct WifiScanResult {
    std::string ssid;
    int rssi;
    uint8_t authmode;
    uint8_t channel;
};

// Initialize control UART communication
esp_err_t wifi_c6_ctrl_init();

// Deinitialize control UART
void wifi_c6_ctrl_deinit();

// Scan for WiFi networks
esp_err_t wifi_c6_ctrl_scan(std::vector<WifiScanResult>& results);

// Connect to WiFi network
esp_err_t wifi_c6_ctrl_connect(const std::string& ssid, const std::string& password);

// Get WiFi status
esp_err_t wifi_c6_ctrl_get_status(std::string& ssid, std::string& ip, int& rssi);

// Check if control channel is available
bool wifi_c6_ctrl_is_available();
