#pragma once
#include <cstdint>
#include "led_engine/types.hpp"

// Convert matrix coordinates (x, y) to linear LED index
// Supports serpentine (zigzag) and vertical layouts
uint16_t matrix_coords_to_index(uint16_t x, uint16_t y, const LedMatrixConfig& matrix);

// Convert linear LED index to matrix coordinates (x, y)
void matrix_index_to_coords(uint16_t index, uint16_t* x_out, uint16_t* y_out, const LedMatrixConfig& matrix);

// Check if matrix configuration is valid
bool matrix_config_valid(const LedMatrixConfig& matrix);

// Get total LED count for matrix
uint16_t matrix_total_leds(const LedMatrixConfig& matrix);

