#include "led_engine/chipset_info.hpp"
#include <algorithm>
#include <cstring>

namespace {

// Chipset timing definitions (all at 10MHz = 100ns per tick)
const ChipsetInfo s_chipsets[] = {
    // WS2811 - same timing as WS2812B
    {ChipsetType::WS2811, "ws2811", false, false,
     {3, 9, 9, 3, 500}, "GRB"},
    
    // WS2812B - most common
    {ChipsetType::WS2812B, "ws2812b", false, false,
     {3, 9, 9, 3, 500}, "GRB"},
    
    // WS2813 - improved WS2812 with backup data line
    {ChipsetType::WS2813, "ws2813", false, false,
     {3, 9, 9, 3, 500}, "GRB"},
    
    // WS2815 - 12V version
    {ChipsetType::WS2815, "ws2815", false, false,
     {3, 9, 9, 3, 500}, "GRB"},
    
    // SK6812 - RGB only
    {ChipsetType::SK6812, "sk6812", false, false,
     {3, 9, 6, 6, 800}, "GRB"},
    
    // SK6812 RGBW - 4-channel
    {ChipsetType::SK6812_RGBW, "sk6812_rgbw", true, false,
     {3, 9, 6, 6, 800}, "GRBW"},
    
    // SK9822 - SPI-based, uses different protocol
    {ChipsetType::SK9822, "sk9822", false, true,
     {0, 0, 0, 0, 0}, "RGB"},  // SPI timing handled separately
    
    // APA102 (DotStar) - SPI-based
    {ChipsetType::APA102, "apa102", false, true,
     {0, 0, 0, 0, 0}, "RGB"},  // SPI timing handled separately
    
    // TM1814 - RGBW variant
    {ChipsetType::TM1814, "tm1814", true, false,
     {3, 9, 9, 3, 500}, "GRBW"},
    
    // TM1829 - RGBW variant
    {ChipsetType::TM1829, "tm1829", true, false,
     {3, 9, 9, 3, 500}, "GRBW"},
    
    // TM1914 - RGBW variant
    {ChipsetType::TM1914, "tm1914", true, false,
     {3, 9, 9, 3, 500}, "GRBW"},
};

const size_t s_chipset_count = sizeof(s_chipsets) / sizeof(s_chipsets[0]);

}  // namespace

const ChipsetInfo* get_chipset_info(const std::string& chipset_name) {
  std::string lower_name = chipset_name;
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
  
  for (size_t i = 0; i < s_chipset_count; ++i) {
    if (lower_name == s_chipsets[i].name) {
      return &s_chipsets[i];
    }
  }
  
  // Default to WS2812B if not found
  return &s_chipsets[1];  // WS2812B
}

bool chipset_supports_rgbw(const std::string& chipset_name) {
  const ChipsetInfo* info = get_chipset_info(chipset_name);
  return info->supports_rgbw;
}

uint8_t chipset_bytes_per_pixel(const std::string& chipset_name) {
  const ChipsetInfo* info = get_chipset_info(chipset_name);
  return info->supports_rgbw ? 4 : 3;
}


