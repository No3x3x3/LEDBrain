#include "led_engine/pinout.hpp"
#include <algorithm>

const std::vector<LedPinInfo>& led_pin_catalog() {
  static const std::vector<LedPinInfo> pins = {
      {1, "GPIO1", "RMT0", true, true, false, false, "Preferred for short strips"},
      {2, "GPIO2", "RMT0", true, true, false, false, nullptr},
      {3, "GPIO3", "UART0", true, true, true, true, "Console/strapping"},
      {4, "GPIO4", "RMT1", true, true, false, false, nullptr},
      {5, "GPIO5", "RMT1", true, true, false, false, nullptr},
      {6, "GPIO6", "SPI", false, false, true, false, "PSRAM"},
      {7, "GPIO7", "SPI", false, false, true, false, "PSRAM"},
      {8, "GPIO8", "SPI", false, false, true, false, "Flash"},
      {9, "GPIO9", "SPI", false, false, true, false, "Flash"},
      {10, "GPIO10", "RMT2", true, true, false, false, nullptr},
      {11, "GPIO11", "RMT2", true, true, false, false, nullptr},
      {12, "GPIO12", "ADC2", true, true, false, true, "Boot strapping"},
      {13, "GPIO13", "ADC2", true, true, false, false, nullptr},
      {14, "GPIO14", "RMT3", true, true, false, false, nullptr},
      {15, "GPIO15", "RMT3", true, true, false, false, nullptr},
      {16, "GPIO16", "I2S", true, true, false, false, nullptr},
      {17, "GPIO17", "I2S", true, true, false, false, nullptr},
      {18, "GPIO18", "Ethernet CLK", false, false, true, false, "ETH"},
      {19, "GPIO19", "Ethernet", false, false, true, false, "ETH"},
      {20, "GPIO20", "Ethernet", false, false, true, false, "ETH"},
      {21, "GPIO21", "Ethernet", false, false, true, false, "ETH"},
      {33, "GPIO33", "RMT4", true, true, false, false, nullptr},
      {34, "GPIO34", "RMT4", true, true, false, false, nullptr},
      {35, "GPIO35", "RMT5", true, true, false, false, nullptr},
      {36, "GPIO36", "RMT5", true, true, false, false, nullptr},
      {37, "GPIO37", "USB", false, false, true, false, "USB PHY"},
      {38, "GPIO38", "USB", false, false, true, false, "USB PHY"},
      {39, "GPIO39", "RMT6", true, true, false, false, nullptr},
      {40, "GPIO40", "RMT6", true, true, false, false, nullptr},
      {41, "GPIO41", "RMT7", true, true, false, false, nullptr},
      {42, "GPIO42", "RMT7", true, true, false, false, nullptr},
      {43, "GPIO43", "SPI", false, false, true, false, nullptr},
      {44, "GPIO44", "SPI", false, false, true, false, nullptr},
      {45, "GPIO45", "USB D-", false, false, true, false, "USB"},
      {46, "GPIO46", "USB D+", false, false, true, false, "USB"},
      {47, "GPIO47", "RTC", false, false, false, true, nullptr},
      {48, "GPIO48", "RTC", false, false, false, true, nullptr},
  };
  return pins;
}

const LedPinInfo* led_pin_find(int gpio) {
  const auto& pins = led_pin_catalog();
  auto it = std::find_if(pins.begin(), pins.end(), [gpio](const LedPinInfo& info) { return info.gpio == gpio; });
  if (it == pins.end()) {
    return nullptr;
  }
  return &(*it);
}

bool led_pin_is_allowed(int gpio) {
  const auto* info = led_pin_find(gpio);
  if (!info) {
    return false;
  }
  if (info->reserved || info->strapping) {
    return false;
  }
  return info->supports_rmt;
}
