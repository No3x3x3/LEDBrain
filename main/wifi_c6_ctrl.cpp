#include "wifi_c6_ctrl.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <cstdlib>

static const char* TAG = "wifi_c6_ctrl";

namespace {
  // UART2 for control communication (separate from UART1 used for PPP)
  // ESP32-P4: GPIO4 (TX) -> ESP32-C6: GPIO20 (RX)
  // ESP32-C6: GPIO19 (TX) -> ESP32-P4: GPIO5 (RX)
  constexpr uart_port_t CTRL_UART_PORT = UART_NUM_2;
  constexpr int CTRL_UART_TX = 4;   // ESP32-P4 TX -> ESP32-C6 RX (UART2, GPIO20)
  constexpr int CTRL_UART_RX = 5;   // ESP32-C6 TX -> ESP32-P4 RX (UART2, GPIO19)
  constexpr int CTRL_UART_BAUD = 115200;  // Lower baudrate for control (reliability over speed)
  constexpr int CTRL_UART_BUF_SIZE = 1024;
  constexpr int CTRL_UART_QUEUE_SIZE = 10;
  
  bool s_initialized = false;
  QueueHandle_t s_uart_queue = nullptr;
  
  // Protocol constants
  constexpr const char* CMD_SCAN = "SCAN";
  constexpr const char* CMD_CONNECT = "CONNECT";
  constexpr const char* CMD_STATUS = "STATUS";
  constexpr const char* RESP_OK = "OK";
  constexpr const char* RESP_ERROR = "ERROR";
  constexpr int CMD_TIMEOUT_MS = 5000;
}

esp_err_t wifi_c6_ctrl_init() {
  if (s_initialized) {
    ESP_LOGW(TAG, "Control UART already initialized");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Initializing WiFi C6 control UART (UART%d, TX: GPIO%d, RX: GPIO%d, Baud: %d)",
           CTRL_UART_PORT, CTRL_UART_TX, CTRL_UART_RX, CTRL_UART_BAUD);

  // Configure UART
  uart_config_t uart_config = {};
  uart_config.baud_rate = CTRL_UART_BAUD;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.source_clk = UART_SCLK_DEFAULT;

  esp_err_t ret = uart_driver_install(CTRL_UART_PORT, CTRL_UART_BUF_SIZE, CTRL_UART_BUF_SIZE,
                                      CTRL_UART_QUEUE_SIZE, &s_uart_queue, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = uart_param_config(CTRL_UART_PORT, &uart_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(ret));
    uart_driver_delete(CTRL_UART_PORT);
    return ret;
  }

  ret = uart_set_pin(CTRL_UART_PORT, CTRL_UART_TX, CTRL_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
    uart_driver_delete(CTRL_UART_PORT);
    return ret;
  }

  s_initialized = true;
  ESP_LOGI(TAG, "WiFi C6 control UART initialized successfully");
  return ESP_OK;
}

void wifi_c6_ctrl_deinit() {
  if (!s_initialized) {
    return;
  }

  ESP_LOGI(TAG, "Deinitializing WiFi C6 control UART");
  uart_driver_delete(CTRL_UART_PORT);
  s_uart_queue = nullptr;
  s_initialized = false;
}

bool wifi_c6_ctrl_is_available() {
  return s_initialized;
}

// Send command and wait for response
static esp_err_t send_command(const char* cmd, std::string& response, int timeout_ms = CMD_TIMEOUT_MS) {
  if (!s_initialized) {
    ESP_LOGE(TAG, "Control UART not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  // Clear RX buffer
  uart_flush(CTRL_UART_PORT);

  // Send command
  std::string cmd_str = std::string(cmd) + "\r\n";
  int len = uart_write_bytes(CTRL_UART_PORT, cmd_str.c_str(), cmd_str.length());
  if (len < 0) {
    ESP_LOGE(TAG, "Failed to send command: %s", cmd);
    return ESP_FAIL;
  }

  ESP_LOGD(TAG, "Sent command: %s", cmd);

  // Wait for response
  uint8_t* data = (uint8_t*)malloc(CTRL_UART_BUF_SIZE);
  if (!data) {
    ESP_LOGE(TAG, "Failed to allocate buffer");
    return ESP_ERR_NO_MEM;
  }

  int total_len = 0;
  int64_t start_time = esp_timer_get_time() / 1000;  // Convert to ms

  while ((esp_timer_get_time() / 1000 - start_time) < timeout_ms) {
    int len = uart_read_bytes(CTRL_UART_PORT, data + total_len, CTRL_UART_BUF_SIZE - total_len - 1, 100);
    if (len > 0) {
      total_len += len;
      data[total_len] = '\0';

      // Check if we have a complete response (ends with \r\n)
      if (total_len >= 2 && data[total_len - 2] == '\r' && data[total_len - 1] == '\n') {
        break;
      }
    } else if (len == 0 && total_len > 0) {
      // No more data, but we have something - might be complete
      break;
    }
  }

  if (total_len == 0) {
    ESP_LOGE(TAG, "No response received for command: %s", cmd);
    free(data);
    return ESP_ERR_TIMEOUT;
  }

  response = std::string((char*)data, total_len);
  // Remove trailing \r\n
  while (!response.empty() && (response.back() == '\r' || response.back() == '\n')) {
    response.pop_back();
  }

  free(data);
  ESP_LOGD(TAG, "Received response: %s", response.c_str());
  return ESP_OK;
}

esp_err_t wifi_c6_ctrl_scan(std::vector<WifiScanResult>& results) {
  results.clear();

  std::string response;
  esp_err_t ret = send_command(CMD_SCAN, response);
  if (ret != ESP_OK) {
    return ret;
  }

  // Parse response: OK|SSID1|RSSI1|AUTH1|CH1|SSID2|RSSI2|AUTH2|CH2|...
  if (response.find(RESP_OK) != 0) {
    ESP_LOGE(TAG, "Scan failed: %s", response.c_str());
    return ESP_FAIL;
  }

  // Remove "OK|" prefix
  std::string data = response.substr(strlen(RESP_OK) + 1);
  
  // Parse comma-separated or pipe-separated values
  std::istringstream iss(data);
  std::string token;
  WifiScanResult current;
  int field = 0;

  while (std::getline(iss, token, '|')) {
    if (token.empty()) continue;

    switch (field % 4) {
      case 0:  // SSID
        current.ssid = token;
        break;
      case 1:  // RSSI
        current.rssi = std::stoi(token);
        break;
      case 2:  // Auth mode
        current.authmode = std::stoi(token);
        break;
      case 3:  // Channel
        current.channel = std::stoi(token);
        results.push_back(current);
        current = WifiScanResult{};
        break;
    }
    field++;
  }

  ESP_LOGI(TAG, "Scan completed: found %d networks", results.size());
  return ESP_OK;
}

esp_err_t wifi_c6_ctrl_connect(const std::string& ssid, const std::string& password) {
  // Format: CONNECT|SSID:xxx|PASS:yyy
  std::string cmd = std::string(CMD_CONNECT) + "|SSID:" + ssid + "|PASS:" + password;

  std::string response;
  esp_err_t ret = send_command(cmd.c_str(), response, 10000);  // 10s timeout for connection
  if (ret != ESP_OK) {
    return ret;
  }

  if (response.find(RESP_OK) == 0) {
    ESP_LOGI(TAG, "WiFi connection initiated: %s", ssid.c_str());
    return ESP_OK;
  } else {
    ESP_LOGE(TAG, "WiFi connection failed: %s", response.c_str());
    return ESP_FAIL;
  }
}

esp_err_t wifi_c6_ctrl_get_status(std::string& ssid, std::string& ip, int& rssi) {
  std::string response;
  esp_err_t ret = send_command(CMD_STATUS, response);
  if (ret != ESP_OK) {
    return ret;
  }

  // Parse response: OK|SSID:xxx|IP:yyy|RSSI:zzz
  if (response.find(RESP_OK) != 0) {
    ESP_LOGE(TAG, "Status query failed: %s", response.c_str());
    return ESP_FAIL;
  }

  // Remove "OK|" prefix
  std::string data = response.substr(strlen(RESP_OK) + 1);
  
  std::istringstream iss(data);
  std::string token;
  
  while (std::getline(iss, token, '|')) {
    if (token.find("SSID:") == 0) {
      ssid = token.substr(5);
    } else if (token.find("IP:") == 0) {
      ip = token.substr(3);
    } else if (token.find("RSSI:") == 0) {
      rssi = std::stoi(token.substr(5));
    }
  }

  return ESP_OK;
}
