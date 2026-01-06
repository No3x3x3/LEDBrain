#pragma once
#include <cstdint>
#include <vector>
#include <string>

// Gamma correction lookup table (256 entries)
extern const uint8_t gamma_table_22[256];
extern const uint8_t gamma_table_24[256];
extern const uint8_t gamma_table_28[256];

// Apply gamma correction to a single value
uint8_t apply_gamma(uint8_t value, float gamma);

// Apply gamma correction to RGB/RGBW pixel
void apply_gamma_pixel(uint8_t* pixel, uint8_t bytes_per_pixel, float gamma_color, float gamma_brightness);

// Convert RGB to RGBW (white extraction)
void rgb_to_rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t* rgbw_out);

// Convert HSV to RGB
void hsv_to_rgb(float h, float s, float v, uint8_t* rgb_out);

// Convert RGB to HSV
void rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b, float* h_out, float* s_out, float* v_out);

// Convert color order (supports RGB and RGBW)
void convert_color_order(const uint8_t* src, uint8_t* dst, const std::string& order, uint8_t bytes_per_pixel);

// Apply color processing pipeline (gamma, color order, RGBW conversion)
void process_pixel(const uint8_t* src_rgb, uint8_t* dst, const std::string& color_order, 
                   uint8_t bytes_per_pixel, float gamma_color, float gamma_brightness, bool apply_gamma);

