#include "led_engine/color_processing.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

// Precomputed gamma tables for common gamma values
const uint8_t gamma_table_22[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6,
    6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11, 11, 12,
    12, 13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19,
    20, 20, 21, 22, 22, 23, 23, 24, 25, 25, 26, 26, 27, 28, 28, 29,
    30, 30, 31, 32, 33, 33, 34, 35, 35, 36, 37, 38, 39, 39, 40, 41,
    42, 43, 43, 44, 45, 46, 47, 48, 49, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
    73, 74, 75, 76, 77, 78, 79, 81, 82, 83, 84, 85, 87, 88, 89, 90,
    91, 93, 94, 95, 97, 98, 99, 100, 102, 103, 105, 106, 107, 109, 110, 111,
    113, 114, 116, 117, 119, 120, 121, 123, 124, 126, 127, 129, 130, 132, 133, 135,
    137, 138, 140, 141, 143, 145, 146, 148, 149, 151, 153, 154, 156, 158, 159, 161,
    163, 165, 166, 168, 170, 172, 173, 175, 177, 179, 181, 182, 184, 186, 188, 190,
    192, 194, 196, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221,
    223, 225, 227, 229, 231, 234, 236, 238, 240, 242, 244, 246, 248, 251, 253, 255
};

const uint8_t gamma_table_24[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9,
    9, 10, 10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 14, 15, 15,
    16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 22, 22, 23, 23, 24,
    24, 25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33, 34, 35,
    35, 36, 37, 38, 39, 39, 40, 41, 42, 43, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 53, 54, 55, 56, 57, 58, 59, 60, 62, 63, 64,
    65, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78, 80, 81, 82,
    83, 85, 86, 87, 88, 90, 91, 92, 94, 95, 96, 98, 99, 100, 102, 103,
    105, 106, 108, 109, 111, 112, 114, 115, 117, 118, 120, 121, 123, 124, 126, 127,
    129, 131, 132, 134, 136, 137, 139, 141, 142, 144, 146, 148, 149, 151, 153, 155,
    156, 158, 160, 162, 164, 166, 167, 169, 171, 173, 175, 177, 179, 181, 183, 185,
    187, 189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 210, 212, 214, 216, 218,
    220, 223, 225, 227, 229, 232, 234, 236, 239, 241, 243, 246, 248, 250, 253, 255
};

const uint8_t gamma_table_28[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
    2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5,
    5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10,
    10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
    17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
    25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
    37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
    51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
    69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
    90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
    115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
    144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
    177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
    215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255
};

uint8_t apply_gamma(uint8_t value, float gamma) {
    if (gamma <= 0.0f || value == 0) return 0;
    if (value == 255) return 255;
    
    // Use lookup table for common gamma values
    if (std::abs(gamma - 2.2f) < 0.01f) {
        return gamma_table_22[value];
    }
    if (std::abs(gamma - 2.4f) < 0.01f) {
        return gamma_table_24[value];
    }
    if (std::abs(gamma - 2.8f) < 0.01f) {
        return gamma_table_28[value];
    }
    
    // Calculate gamma correction
    float normalized = static_cast<float>(value) / 255.0f;
    float corrected = std::pow(normalized, gamma);
    return static_cast<uint8_t>(std::round(corrected * 255.0f));
}

void apply_gamma_pixel(uint8_t* pixel, uint8_t bytes_per_pixel, float gamma_color, float gamma_brightness) {
    if (bytes_per_pixel == 3) {
        // RGB
        pixel[0] = apply_gamma(pixel[0], gamma_color);
        pixel[1] = apply_gamma(pixel[1], gamma_color);
        pixel[2] = apply_gamma(pixel[2], gamma_color);
    } else if (bytes_per_pixel == 4) {
        // RGBW - apply gamma to RGB, brightness gamma to W
        pixel[0] = apply_gamma(pixel[0], gamma_color);
        pixel[1] = apply_gamma(pixel[1], gamma_color);
        pixel[2] = apply_gamma(pixel[2], gamma_color);
        pixel[3] = apply_gamma(pixel[3], gamma_brightness);
    }
}

void rgb_to_rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t* rgbw_out) {
    // White extraction algorithm - find minimum of RGB as white component
    uint8_t w = std::min({r, g, b});
    
    // Subtract white from RGB
    rgbw_out[0] = (r > w) ? (r - w) : 0;
    rgbw_out[1] = (g > w) ? (g - w) : 0;
    rgbw_out[2] = (b > w) ? (b - w) : 0;
    rgbw_out[3] = w;
}

void hsv_to_rgb(float h, float s, float v, uint8_t* rgb_out) {
    h = std::fmod(h, 360.0f);
    if (h < 0) h += 360.0f;
    
    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    
    float r = 0, g = 0, b = 0;
    
    if (h < 60) {
        r = c; g = x; b = 0;
    } else if (h < 120) {
        r = x; g = c; b = 0;
    } else if (h < 180) {
        r = 0; g = c; b = x;
    } else if (h < 240) {
        r = 0; g = x; b = c;
    } else if (h < 300) {
        r = x; g = 0; b = c;
    } else {
        r = c; g = 0; b = x;
    }
    
    rgb_out[0] = static_cast<uint8_t>((r + m) * 255.0f);
    rgb_out[1] = static_cast<uint8_t>((g + m) * 255.0f);
    rgb_out[2] = static_cast<uint8_t>((b + m) * 255.0f);
}

void rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b, float* h_out, float* s_out, float* v_out) {
    float rf = r / 255.0f;
    float gf = g / 255.0f;
    float bf = b / 255.0f;
    
    float max = std::max({rf, gf, bf});
    float min = std::min({rf, gf, bf});
    float delta = max - min;
    
    *v_out = max;
    
    if (delta == 0) {
        *h_out = 0;
        *s_out = 0;
        return;
    }
    
    *s_out = delta / max;
    
    float h = 0;
    if (max == rf) {
        h = 60.0f * std::fmod((gf - bf) / delta, 6.0f);
    } else if (max == gf) {
        h = 60.0f * (((bf - rf) / delta) + 2.0f);
    } else {
        h = 60.0f * (((rf - gf) / delta) + 4.0f);
    }
    
    if (h < 0) h += 360.0f;
    *h_out = h;
}

void convert_color_order(const uint8_t* src, uint8_t* dst, const std::string& order, uint8_t bytes_per_pixel) {
    if (bytes_per_pixel == 3) {
        // RGB
        uint8_t r = src[0], g = src[1], b = src[2];
        
        if (order == "GRB" || order == "grb") {
            dst[0] = g; dst[1] = r; dst[2] = b;
        } else if (order == "RGB" || order == "rgb") {
            dst[0] = r; dst[1] = g; dst[2] = b;
        } else if (order == "BRG" || order == "brg") {
            dst[0] = b; dst[1] = r; dst[2] = g;
        } else if (order == "RBG" || order == "rbg") {
            dst[0] = r; dst[1] = b; dst[2] = g;
        } else if (order == "GBR" || order == "gbr") {
            dst[0] = g; dst[1] = b; dst[2] = r;
        } else if (order == "BGR" || order == "bgr") {
            dst[0] = b; dst[1] = g; dst[2] = r;
        } else {
            // Default to GRB
            dst[0] = g; dst[1] = r; dst[2] = b;
        }
    } else if (bytes_per_pixel == 4) {
        // RGBW
        uint8_t r = src[0], g = src[1], b = src[2], w = src[3];
        
        if (order == "GRBW" || order == "grbw") {
            dst[0] = g; dst[1] = r; dst[2] = b; dst[3] = w;
        } else if (order == "RGBW" || order == "rgbw") {
            dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = w;
        } else if (order == "BRGW" || order == "brgw") {
            dst[0] = b; dst[1] = r; dst[2] = g; dst[3] = w;
        } else if (order == "RBGW" || order == "rbgw") {
            dst[0] = r; dst[1] = b; dst[2] = g; dst[3] = w;
        } else if (order == "GBRW" || order == "gbrw") {
            dst[0] = g; dst[1] = b; dst[2] = r; dst[3] = w;
        } else if (order == "BGRW" || order == "bgrw") {
            dst[0] = b; dst[1] = g; dst[2] = r; dst[3] = w;
        } else if (order == "WRGB" || order == "wrgb") {
            dst[0] = w; dst[1] = r; dst[2] = g; dst[3] = b;
        } else if (order == "WGRB" || order == "wgrb") {
            dst[0] = w; dst[1] = g; dst[2] = r; dst[3] = b;
        } else {
            // Default to GRBW
            dst[0] = g; dst[1] = r; dst[2] = b; dst[3] = w;
        }
    }
}

void process_pixel(const uint8_t* src_rgb, uint8_t* dst, const std::string& color_order, 
                   uint8_t bytes_per_pixel, float gamma_color, float gamma_brightness, bool apply_gamma_flag) {
    uint8_t temp[4] = {src_rgb[0], src_rgb[1], src_rgb[2], 0};
    
    // Convert RGB to RGBW if needed
    if (bytes_per_pixel == 4) {
        rgb_to_rgbw(temp[0], temp[1], temp[2], temp);
    }
    
    // Apply gamma correction
    if (apply_gamma_flag) {
        apply_gamma_pixel(temp, bytes_per_pixel, gamma_color, gamma_brightness);
    }
    
    // Convert color order
    convert_color_order(temp, dst, color_order, bytes_per_pixel);
}

