#ifdef CONFIG_IDF_TARGET_ESP32P4

#include "led_engine/ppa_accelerator.hpp"
#include "esp_log.h"
#include <cstring>
#include <algorithm>

namespace ppa_accel {

static const char* TAG = "ppa-accel";
static ppa_client_handle_t s_blend_client = nullptr;
static ppa_client_handle_t s_fill_client = nullptr;
static bool s_initialized = false;

esp_err_t init_blend_client() {
    if (s_blend_client) {
        return ESP_OK;  // Already initialized
    }
    
    ppa_client_config_t client_cfg = {};
    client_cfg.oper_type = PPA_OPERATION_BLEND;
    client_cfg.max_pending_trans_num = 4;
    client_cfg.data_burst_length = PPA_DATA_BURST_LENGTH_128;
    
    esp_err_t err = ppa_register_client(&client_cfg, &s_blend_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register PPA blend client: %s", esp_err_to_name(err));
        return err;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "PPA blend client initialized");
    return ESP_OK;
}

esp_err_t init_fill_client() {
    if (s_fill_client) {
        return ESP_OK;  // Already initialized
    }
    
    ppa_client_config_t client_cfg = {};
    client_cfg.oper_type = PPA_OPERATION_FILL;
    client_cfg.max_pending_trans_num = 4;
    client_cfg.data_burst_length = PPA_DATA_BURST_LENGTH_128;
    
    esp_err_t err = ppa_register_client(&client_cfg, &s_fill_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register PPA fill client: %s", esp_err_to_name(err));
        return err;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "PPA fill client initialized");
    return ESP_OK;
}

void deinit() {
    if (s_blend_client) {
        ppa_unregister_client(s_blend_client);
        s_blend_client = nullptr;
    }
    if (s_fill_client) {
        ppa_unregister_client(s_fill_client);
        s_fill_client = nullptr;
    }
    s_initialized = false;
    ESP_LOGI(TAG, "PPA clients deinitialized");
}

esp_err_t blend_rgb(const uint8_t* src, const uint8_t* dst, uint8_t* out,
                    uint16_t width, uint16_t height, float alpha) {
    if (!s_blend_client) {
        esp_err_t err = init_blend_client();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    // Clamp alpha to 0.0-1.0
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    
    // PPA requires buffers to be aligned and in specific format
    // For simplicity, we'll use RGB888 format
    // Note: PPA works on blocks, so we need to configure input/output blocks
    
    ppa_blend_oper_config_t blend_cfg = {};
    
    // Foreground (source) block
    blend_cfg.fg_pic_blk.pic_buffer = const_cast<uint8_t*>(src);
    blend_cfg.fg_pic_blk.pic_width = width;
    blend_cfg.fg_pic_blk.pic_height = height;
    blend_cfg.fg_pic_blk.blk_offset_x = 0;
    blend_cfg.fg_pic_blk.blk_offset_y = 0;
    blend_cfg.fg_pic_blk.blk_width = width;
    blend_cfg.fg_pic_blk.blk_height = height;
    blend_cfg.fg_pic_blk.color_mode = PPA_BLEND_COLOR_MODE_RGB888;
    
    // Background (destination) block
    blend_cfg.bg_pic_blk.pic_buffer = const_cast<uint8_t*>(dst);
    blend_cfg.bg_pic_blk.pic_width = width;
    blend_cfg.bg_pic_blk.pic_height = height;
    blend_cfg.bg_pic_blk.blk_offset_x = 0;
    blend_cfg.bg_pic_blk.blk_offset_y = 0;
    blend_cfg.bg_pic_blk.blk_width = width;
    blend_cfg.bg_pic_blk.blk_height = height;
    blend_cfg.bg_pic_blk.color_mode = PPA_BLEND_COLOR_MODE_RGB888;
    
    // Output block
    blend_cfg.out_pic_blk.pic_buffer = out;
    blend_cfg.out_pic_blk.pic_width = width;
    blend_cfg.out_pic_blk.pic_height = height;
    blend_cfg.out_pic_blk.blk_offset_x = 0;
    blend_cfg.out_pic_blk.blk_offset_y = 0;
    blend_cfg.out_pic_blk.blk_width = width;
    blend_cfg.out_pic_blk.blk_height = height;
    blend_cfg.out_pic_blk.color_mode = PPA_BLEND_COLOR_MODE_RGB888;
    
    // Alpha configuration - use fixed alpha value
    blend_cfg.alpha_mode = PPA_ALPHA_SCALE;
    blend_cfg.alpha_scale = static_cast<uint8_t>(alpha * 255.0f);
    
    // No color keying
    blend_cfg.color_key_enable = false;
    
    // Blocking mode for simplicity
    esp_err_t err = ppa_do_blend(s_blend_client, &blend_cfg, PPA_TRANS_MODE_BLOCKING);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "PPA blend failed: %s", esp_err_to_name(err));
        return err;
    }
    
    return ESP_OK;
}

esp_err_t blend_rgb_per_pixel_alpha(const uint8_t* src, const uint8_t* dst, uint8_t* out,
                                    const uint8_t* src_alpha_buffer,
                                    uint16_t width, uint16_t height) {
    // PPA doesn't directly support per-pixel alpha in blend mode
    // We'll need to fall back to software blending for this
    // TODO: Could use PPA for chunks with same alpha, but complexity may not be worth it
    ESP_LOGW(TAG, "Per-pixel alpha blending not fully supported by PPA, using software fallback");
    
    // Software fallback
    for (uint32_t i = 0; i < static_cast<uint32_t>(width) * height; ++i) {
        const float alpha = src_alpha_buffer[i] / 255.0f;
        const float inv_alpha = 1.0f - alpha;
        out[i * 3 + 0] = static_cast<uint8_t>(src[i * 3 + 0] * alpha + dst[i * 3 + 0] * inv_alpha);
        out[i * 3 + 1] = static_cast<uint8_t>(src[i * 3 + 1] * alpha + dst[i * 3 + 1] * inv_alpha);
        out[i * 3 + 2] = static_cast<uint8_t>(src[i * 3 + 2] * alpha + dst[i * 3 + 2] * inv_alpha);
    }
    
    return ESP_OK;
}

esp_err_t fill_rgb(uint8_t* buffer, uint16_t width, uint16_t height,
                   uint8_t r, uint8_t g, uint8_t b) {
    if (!s_fill_client) {
        esp_err_t err = init_fill_client();
        if (err != ESP_OK) {
            return err;
        }
    }
    
    ppa_fill_oper_config_t fill_cfg = {};
    
    // Output block
    fill_cfg.out_pic_blk.pic_buffer = buffer;
    fill_cfg.out_pic_blk.pic_width = width;
    fill_cfg.out_pic_blk.pic_height = height;
    fill_cfg.out_pic_blk.blk_offset_x = 0;
    fill_cfg.out_pic_blk.blk_offset_y = 0;
    fill_cfg.out_pic_blk.blk_width = width;
    fill_cfg.out_pic_blk.blk_height = height;
    fill_cfg.out_pic_blk.color_mode = PPA_FILL_COLOR_MODE_RGB888;
    
    // Fill color (ARGB format, but we only use RGB)
    fill_cfg.fill_color_mode = PPA_FILL_COLOR_MODE_RGB888;
    fill_cfg.fill_color.rgb888.r = r;
    fill_cfg.fill_color.rgb888.g = g;
    fill_cfg.fill_color.rgb888.b = b;
    
    // Blocking mode
    esp_err_t err = ppa_do_fill(s_fill_client, &fill_cfg, PPA_TRANS_MODE_BLOCKING);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "PPA fill failed: %s", esp_err_to_name(err));
        return err;
    }
    
    return ESP_OK;
}

bool is_available() {
    return s_initialized && (s_blend_client != nullptr || s_fill_client != nullptr);
}

}  // namespace ppa_accel

#endif  // CONFIG_IDF_TARGET_ESP32P4
