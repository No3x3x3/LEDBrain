#include "led_engine/pinout.hpp"
#include <algorithm>

const std::vector<LedPinInfo>& led_pin_catalog() {
  static const std::vector<LedPinInfo> pins = {
      // Header pins advertised on JC-ESP32P4-M3 (LED-capable only)
      {1, "GPIO1", "Header", true, true, false, false, "Preferred short runs"},
      {2, "GPIO2", "Header", true, true, false, false, nullptr},
      {3, "GPIO3", "U0RXD/strap", true, true, true, true, "Console/strapping"},
      {4, "GPIO4", "Header", true, true, false, false, nullptr},
      {5, "GPIO5", "Header", true, true, false, false, nullptr},
      {20, "GPIO20", "Header", true, true, false, false, nullptr},
      {32, "GPIO32", "Header", true, true, false, false, nullptr},
      {33, "GPIO33", "Header", true, true, false, false, nullptr},
      // Header pins we refuse for LED (no RMT or reserved)
      {45, "GPIO45", "USB D-", false, false, true, false, "USB PHY"},
      {46, "GPIO46", "USB D+", false, false, true, false, "USB PHY"},
      {47, "GPIO47", "RTC", false, false, false, true, "RTC"},
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
