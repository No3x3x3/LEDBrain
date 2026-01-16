#pragma once

#include "led_engine/types.hpp"
#include "esp_err.h"
#include <vector>
#include <mutex>

// Request for parallel rendering
struct ParallelRenderRequest {
    const LedSegmentConfig* segment;
    const std::vector<uint8_t>& rgb;
    size_t start;
    size_t length;
};

// Initialize RMT driver for a segment
esp_err_t rmt_driver_init_segment(const LedSegmentConfig& seg, bool enable_dma);

// Render RGB data to segment via RMT
esp_err_t rmt_driver_render(const LedSegmentConfig& seg, const std::vector<uint8_t>& rgb, size_t start, size_t length);

// Initialize parallel IO mode - creates sync manager for simultaneous transmission
// segments: vector of segment configs to sync (1-4 segments, ESP32-P4 has 4 TX channels)
esp_err_t rmt_driver_init_parallel_mode(const std::vector<const LedSegmentConfig*>& segments);

// Render to multiple segments in parallel (simultaneous transmission)
// All segments in requests must be initialized and part of the sync manager
esp_err_t rmt_driver_render_parallel(const std::vector<ParallelRenderRequest>& requests);

// Deinitialize RMT driver for a segment
esp_err_t rmt_driver_deinit_segment(int gpio, uint8_t rmt_channel);

// Deinitialize all RMT drivers
void rmt_driver_deinit_all();




