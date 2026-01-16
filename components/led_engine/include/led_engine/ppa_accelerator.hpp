#pragma once

#include "esp_err.h"
#include <cstdint>
#include <vector>

#ifdef CONFIG_IDF_TARGET_ESP32P4
#include "driver/ppa.h"

// PPA (Pixel Processing Accelerator) wrapper for ESP32-P4
// Provides hardware-accelerated pixel operations:
// - Blending (alpha blending, color mixing)
// - Fill (solid color fill)
// - Scale-Rotate-Mirror (for matrix effects)

namespace ppa_accel {

// Initialize PPA client for blending operations
esp_err_t init_blend_client();

// Initialize PPA client for fill operations
esp_err_t init_fill_client();

// Deinitialize PPA clients
void deinit();

// Blend two RGB buffers with alpha blending (hardware accelerated)
// dst = src * alpha + dst * (1 - alpha)
// All buffers must be RGB888 format (3 bytes per pixel)
// Returns ESP_OK on success
esp_err_t blend_rgb(const uint8_t* src, const uint8_t* dst, uint8_t* out,
                    uint16_t width, uint16_t height, float alpha);

// Blend two RGB buffers with per-pixel alpha (hardware accelerated)
// dst = src * src_alpha + dst * (1 - src_alpha)
// src_alpha_buffer: per-pixel alpha values (0-255)
// Returns ESP_OK on success
esp_err_t blend_rgb_per_pixel_alpha(const uint8_t* src, const uint8_t* dst, uint8_t* out,
                                    const uint8_t* src_alpha_buffer,
                                    uint16_t width, uint16_t height);

// Fill RGB buffer with solid color (hardware accelerated)
// Returns ESP_OK on success
esp_err_t fill_rgb(uint8_t* buffer, uint16_t width, uint16_t height,
                   uint8_t r, uint8_t g, uint8_t b);

// Check if PPA is available (ESP32-P4 only)
bool is_available();

}  // namespace ppa_accel

#else

// Stub implementation for non-ESP32-P4 platforms
namespace ppa_accel {

inline esp_err_t init_blend_client() { return ESP_ERR_NOT_SUPPORTED; }
inline esp_err_t init_fill_client() { return ESP_ERR_NOT_SUPPORTED; }
inline void deinit() {}
inline esp_err_t blend_rgb(const uint8_t* src, const uint8_t* dst, uint8_t* out,
                          uint16_t, uint16_t, float) { return ESP_ERR_NOT_SUPPORTED; }
inline esp_err_t blend_rgb_per_pixel_alpha(const uint8_t*, const uint8_t*, uint8_t*,
                                          const uint8_t*, uint16_t, uint16_t) { 
  return ESP_ERR_NOT_SUPPORTED; 
}
inline esp_err_t fill_rgb(uint8_t*, uint16_t, uint16_t, uint8_t, uint8_t, uint8_t) { 
  return ESP_ERR_NOT_SUPPORTED; 
}
inline bool is_available() { return false; }

}  // namespace ppa_accel

#endif  // CONFIG_IDF_TARGET_ESP32P4
