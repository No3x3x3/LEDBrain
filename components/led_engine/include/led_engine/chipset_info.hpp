#pragma once
#include <cstdint>
#include <string>

enum class ChipsetType {
  WS2811,
  WS2812B,
  WS2813,
  WS2815,
  SK6812,
  SK6812_RGBW,
  SK9822,
  APA102,
  TM1814,
  TM1829,
  TM1914,
};

struct ChipsetTiming {
  uint8_t t0h_ticks;  // T0H in ticks @ 10MHz (100ns per tick)
  uint8_t t0l_ticks;  // T0L in ticks
  uint8_t t1h_ticks;  // T1H in ticks
  uint8_t t1l_ticks;  // T1L in ticks
  uint16_t reset_ticks;  // Reset duration in ticks
};

struct ChipsetInfo {
  ChipsetType type;
  const char* name;
  bool supports_rgbw;  // 4-channel support
  bool uses_spi;  // SPI-based (APA102, SK9822) vs RMT-based
  ChipsetTiming timing;
  const char* default_color_order;
};

// Get chipset info from string name
const ChipsetInfo* get_chipset_info(const std::string& chipset_name);

// Check if chipset supports RGBW
bool chipset_supports_rgbw(const std::string& chipset_name);

// Get bytes per pixel (3 for RGB, 4 for RGBW)
uint8_t chipset_bytes_per_pixel(const std::string& chipset_name);


