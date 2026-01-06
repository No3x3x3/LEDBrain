#include "led_engine/matrix_utils.hpp"
#include <algorithm>

uint16_t matrix_coords_to_index(uint16_t x, uint16_t y, const LedMatrixConfig& matrix) {
    if (!matrix_config_valid(matrix)) {
        return 0;
    }
    
    if (matrix.vertical) {
        // Vertical layout: columns go up/down
        if (matrix.serpentine && (x % 2 == 1)) {
            // Odd columns: reverse direction
            return x * matrix.height + (matrix.height - 1 - y);
        } else {
            // Even columns: normal direction
            return x * matrix.height + y;
        }
    } else {
        // Horizontal layout: rows go left/right
        if (matrix.serpentine && (y % 2 == 1)) {
            // Odd rows: reverse direction
            return y * matrix.width + (matrix.width - 1 - x);
        } else {
            // Even rows: normal direction
            return y * matrix.width + x;
        }
    }
}

void matrix_index_to_coords(uint16_t index, uint16_t* x_out, uint16_t* y_out, const LedMatrixConfig& matrix) {
    if (!matrix_config_valid(matrix)) {
        *x_out = 0;
        *y_out = 0;
        return;
    }
    
    if (matrix.vertical) {
        // Vertical layout
        *x_out = index / matrix.height;
        uint16_t y_in_col = index % matrix.height;
        
        if (matrix.serpentine && (*x_out % 2 == 1)) {
            // Odd columns: reverse
            *y_out = matrix.height - 1 - y_in_col;
        } else {
            *y_out = y_in_col;
        }
    } else {
        // Horizontal layout
        *y_out = index / matrix.width;
        uint16_t x_in_row = index % matrix.width;
        
        if (matrix.serpentine && (*y_out % 2 == 1)) {
            // Odd rows: reverse
            *x_out = matrix.width - 1 - x_in_row;
        } else {
            *x_out = x_in_row;
        }
    }
}

bool matrix_config_valid(const LedMatrixConfig& matrix) {
    return matrix.width > 0 && matrix.height > 0;
}

uint16_t matrix_total_leds(const LedMatrixConfig& matrix) {
    if (!matrix_config_valid(matrix)) {
        return 0;
    }
    return matrix.width * matrix.height;
}

