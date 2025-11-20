#pragma once
#include <string>
#include <vector>

struct LedPinInfo {
  int gpio{-1};
  const char* label{nullptr};
  const char* function{nullptr};
  bool supports_rmt{false};
  bool supports_dma{false};
  bool reserved{false};
  bool strapping{false};
  const char* note{nullptr};
};

const std::vector<LedPinInfo>& led_pin_catalog();
const LedPinInfo* led_pin_find(int gpio);
bool led_pin_is_allowed(int gpio);
