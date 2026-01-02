#pragma once

#include "led_engine/types.hpp"
#include "esp_err.h"
#include <vector>
#include <mutex>

// Initialize RMT driver for a segment
esp_err_t rmt_driver_init_segment(const LedSegmentConfig& seg);

// Render RGB data to segment via RMT
esp_err_t rmt_driver_render(const LedSegmentConfig& seg, const std::vector<uint8_t>& rgb, size_t start, size_t length);

// Deinitialize RMT driver for a segment
esp_err_t rmt_driver_deinit_segment(int gpio, uint8_t rmt_channel);

// Deinitialize all RMT drivers
void rmt_driver_deinit_all();


