#include "led_engine/pinout.hpp"
#include <algorithm>

const std::vector<LedPinInfo>& led_pin_catalog() {
  static const std::vector<LedPinInfo> pins = {
      // Header pins available on JC-ESP32P4-M3-DEV Expand IO header (JP1)
      // Based on schematic: 2_EXPAND_IO&BAT.png
      {1, "GPIO1", "Header JP1-8", true, true, false, false, "Expand IO header"},
      {2, "GPIO2", "Header JP1-9", true, true, false, false, "Expand IO header"},
      {3, "GPIO3", "Header JP1-10 (U0RXD/strap)", true, true, true, true, "Console/strapping - use with caution"},
      {4, "GPIO4", "Header JP1-11", true, true, false, false, "Expand IO header"},
      {5, "GPIO5", "Header JP1-12", true, true, false, false, "Expand IO header"},
      {9, "GPIO9", "Header JP1-24 (C6_IO9)", true, true, false, false, "Expand IO header"},
      {20, "GPIO20", "Header JP1-17", true, true, false, false, "Expand IO header"},
      {32, "GPIO32", "Header JP1-19", true, true, false, false, "Expand IO header"},
      {33, "GPIO33", "Header JP1-21", true, true, false, false, "Expand IO header"},
      // Pins on header but reserved/not suitable for LED
      {45, "GPIO45", "Header JP1-14 (USB D-)", false, false, true, false, "USB PHY - reserved"},
      {46, "GPIO46", "Header JP1-13 (USB D+)", false, false, true, false, "USB PHY - reserved"},
      {47, "GPIO47", "Header JP1-18 (RTC)", false, false, false, true, "RTC - reserved"},
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
