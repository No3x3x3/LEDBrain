#include "led_engine/rmt_driver.hpp"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/gpio.h"
#include <algorithm>
#include <cstring>
#include <stddef.h>

#ifndef __containerof
#define __containerof(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

namespace {

static const char* TAG = "rmt-driver";

// WS2812 timing (in nanoseconds, converted to RMT ticks)
// T0H: 300ns, T0L: 900ns (0 bit)
// T1H: 900ns, T1L: 300ns (1 bit)
// RESET: >50us

// SK6812 timing (similar but slightly different)
// T0H: 300ns, T0L: 900ns (0 bit)
// T1H: 600ns, T1L: 600ns (1 bit)
// RESET: >80us

struct ws2812_encoder_t {
    rmt_encoder_t base;
    rmt_encoder_t* bytes_encoder;
    rmt_encoder_t* copy_encoder;
    int state;
    rmt_symbol_word_t reset_symbol;
};

size_t ws2812_encode(rmt_encoder_t* encoder, rmt_channel_handle_t channel, const void* primary_data, size_t data_size, rmt_encode_state_t* ret_state) {
    ws2812_encoder_t* ws2812 = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = ws2812->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = ws2812->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    switch (ws2812->state) {
        case 0: // send RGB data
            encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                ws2812->state = 1; // switch to sending reset code
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state = static_cast<rmt_encode_state_t>(state | RMT_ENCODING_MEM_FULL);
                goto out;
            }
            // fall-through
        case 1: // send reset code
            encoded_symbols += copy_encoder->encode(copy_encoder, channel, &ws2812->reset_symbol,
                                                    sizeof(ws2812->reset_symbol), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                ws2812->state = 0; // back to sending RGB data
                state = static_cast<rmt_encode_state_t>(state | RMT_ENCODING_COMPLETE);
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state = static_cast<rmt_encode_state_t>(state | RMT_ENCODING_MEM_FULL);
                goto out;
            }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

esp_err_t ws2812_del(rmt_encoder_t* encoder) {
    ws2812_encoder_t* ws2812 = __containerof(encoder, ws2812_encoder_t, base);
    rmt_del_encoder(ws2812->bytes_encoder);
    rmt_del_encoder(ws2812->copy_encoder);
    free(ws2812);
    return ESP_OK;
}

esp_err_t ws2812_reset(rmt_encoder_t* encoder) {
    ws2812_encoder_t* ws2812 = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encoder_reset(ws2812->bytes_encoder);
    rmt_encoder_reset(ws2812->copy_encoder);
    ws2812->state = 0;
    return ESP_OK;
}

}  // namespace

struct RmtDriverSegment {
    rmt_channel_handle_t channel;
    rmt_encoder_handle_t encoder;
    int gpio;
    uint8_t rmt_channel;
    std::string chipset;
    std::string color_order;
    bool initialized;
    std::vector<uint8_t> buffer;
};

static std::vector<RmtDriverSegment> s_segments;
static std::mutex s_mutex;

static rmt_tx_channel_config_t make_channel_config(int gpio) {
    rmt_tx_channel_config_t tx_chan_config = {};
    tx_chan_config.gpio_num = static_cast<gpio_num_t>(gpio);
    tx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_chan_config.resolution_hz = 10'000'000;  // 10MHz = 100ns per tick
    tx_chan_config.mem_block_symbols = 64;
    tx_chan_config.trans_queue_depth = 4;
    tx_chan_config.flags.with_dma = false;  // Can enable if needed
    tx_chan_config.flags.invert_out = false;
    return tx_chan_config;
}

static rmt_bytes_encoder_config_t make_bytes_encoder_config(const std::string& chipset) {
    rmt_bytes_encoder_config_t bytes_encoder_config = {};
    
    // WS2812 timing: T0H=300ns, T0L=900ns, T1H=900ns, T1L=300ns
    // SK6812 timing: T0H=300ns, T0L=900ns, T1H=600ns, T1L=600ns
    if (chipset.find("sk6812") != std::string::npos || chipset.find("SK6812") != std::string::npos) {
        bytes_encoder_config.bit0 = {
            .duration0 = 3,  // 300ns @ 10MHz
            .level0 = 1,
            .duration1 = 9,  // 900ns @ 10MHz
            .level1 = 0,
        };
        bytes_encoder_config.bit1 = {
            .duration0 = 6,  // 600ns @ 10MHz
            .level0 = 1,
            .duration1 = 6,  // 600ns @ 10MHz
            .level1 = 0,
        };
    } else {
        // WS2812 default
        bytes_encoder_config.bit0 = {
            .duration0 = 3,  // 300ns @ 10MHz
            .level0 = 1,
            .duration1 = 9,  // 900ns @ 10MHz
            .level1 = 0,
        };
        bytes_encoder_config.bit1 = {
            .duration0 = 9,  // 900ns @ 10MHz
            .level0 = 1,
            .duration1 = 3,  // 300ns @ 10MHz
            .level1 = 0,
        };
    }
    bytes_encoder_config.flags.msb_first = true;
    return bytes_encoder_config;
}

static esp_err_t create_ws2812_encoder(rmt_encoder_handle_t* ret_encoder, const std::string& chipset) {
    ws2812_encoder_t* ws2812_encoder = (ws2812_encoder_t*)calloc(1, sizeof(ws2812_encoder_t));
    if (ws2812_encoder == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    ws2812_encoder->base.encode = ws2812_encode;
    ws2812_encoder->base.del = ws2812_del;
    ws2812_encoder->base.reset = ws2812_reset;

    rmt_bytes_encoder_config_t bytes_encoder_config = make_bytes_encoder_config(chipset);
    esp_err_t err = rmt_new_bytes_encoder(&bytes_encoder_config, &ws2812_encoder->bytes_encoder);
    if (err != ESP_OK) {
        free(ws2812_encoder);
        return err;
    }

    rmt_copy_encoder_config_t copy_encoder_config = {};
    err = rmt_new_copy_encoder(&copy_encoder_config, &ws2812_encoder->copy_encoder);
    if (err != ESP_OK) {
        rmt_del_encoder(ws2812_encoder->bytes_encoder);
        free(ws2812_encoder);
        return err;
    }

    // Reset code: low for >50us (WS2812) or >80us (SK6812)
    uint16_t reset_ticks = (chipset.find("sk6812") != std::string::npos || chipset.find("SK6812") != std::string::npos) ? 800 : 500;
    ws2812_encoder->reset_symbol = (rmt_symbol_word_t){
        .duration0 = reset_ticks,
        .level0 = 0,
        .duration1 = 0,
        .level1 = 0,
    };

    *ret_encoder = &ws2812_encoder->base;
    return ESP_OK;
}

esp_err_t rmt_driver_init_segment(const LedSegmentConfig& seg) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    // Check if already initialized
    auto it = std::find_if(s_segments.begin(), s_segments.end(),
                          [&](const RmtDriverSegment& s) { return s.gpio == seg.gpio && s.rmt_channel == seg.rmt_channel; });
    if (it != s_segments.end() && it->initialized) {
        ESP_LOGW(TAG, "Segment %s already initialized on GPIO %d", seg.id.c_str(), seg.gpio);
        return ESP_OK;
    }

    RmtDriverSegment driver_seg{};
    driver_seg.gpio = seg.gpio;
    driver_seg.rmt_channel = seg.rmt_channel;
    driver_seg.chipset = seg.chipset.empty() ? "ws2812b" : seg.chipset;
    driver_seg.color_order = seg.color_order.empty() ? "GRB" : seg.color_order;
    driver_seg.initialized = false;

    // Create RMT channel (ESP-IDF 5.x doesn't use channel numbers, each channel is independent)
    rmt_tx_channel_config_t tx_chan_config = make_channel_config(seg.gpio);
    esp_err_t err = rmt_new_tx_channel(&tx_chan_config, &driver_seg.channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT channel for GPIO %d: %s", seg.gpio, esp_err_to_name(err));
        return err;
    }

    // Create encoder
    err = create_ws2812_encoder(&driver_seg.encoder, driver_seg.chipset);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create encoder for GPIO %d: %s", seg.gpio, esp_err_to_name(err));
        rmt_del_channel(driver_seg.channel);
        return err;
    }

    // Enable channel
    err = rmt_enable(driver_seg.channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RMT channel for GPIO %d: %s", seg.gpio, esp_err_to_name(err));
        rmt_del_encoder(driver_seg.encoder);
        rmt_del_channel(driver_seg.channel);
        return err;
    }

    driver_seg.buffer.resize(seg.led_count * 3);
    driver_seg.initialized = true;

    if (it != s_segments.end()) {
        *it = driver_seg;
    } else {
        s_segments.push_back(driver_seg);
    }

    ESP_LOGI(TAG, "RMT driver initialized: GPIO %d, chipset %s, color_order %s, LEDs %u",
             seg.gpio, driver_seg.chipset.c_str(), driver_seg.color_order.c_str(), seg.led_count);
    return ESP_OK;
}

esp_err_t rmt_driver_render(const LedSegmentConfig& seg, const std::vector<uint8_t>& rgb, size_t start, size_t length) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    auto it = std::find_if(s_segments.begin(), s_segments.end(),
                          [&](const RmtDriverSegment& s) { return s.gpio == seg.gpio && s.rmt_channel == seg.rmt_channel; });
    if (it == s_segments.end() || !it->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (rgb.size() < (start + length) * 3) {
        return ESP_ERR_INVALID_SIZE;
    }

    // Ensure buffer is large enough for full segment
    if (it->buffer.size() < seg.led_count * 3) {
        it->buffer.resize(seg.led_count * 3, 0);
    }

    // Apply color order conversion and copy to buffer
    const size_t pixel_count = std::min(length, seg.led_count - start);
    const uint8_t* src = rgb.data() + start * 3;
    uint8_t* dst = it->buffer.data() + start * 3;

    // Convert color order if needed
    const std::string& order = it->color_order;
    for (size_t i = 0; i < pixel_count; ++i) {
        const uint8_t r = src[i * 3 + 0];
        const uint8_t g = src[i * 3 + 1];
        const uint8_t b = src[i * 3 + 2];
        
        if (order == "GRB" || order == "grb") {
            dst[i * 3 + 0] = g;
            dst[i * 3 + 1] = r;
            dst[i * 3 + 2] = b;
        } else if (order == "RGB" || order == "rgb") {
            dst[i * 3 + 0] = r;
            dst[i * 3 + 1] = g;
            dst[i * 3 + 2] = b;
        } else if (order == "BRG" || order == "brg") {
            dst[i * 3 + 0] = b;
            dst[i * 3 + 1] = r;
            dst[i * 3 + 2] = g;
        } else if (order == "RBG" || order == "rbg") {
            dst[i * 3 + 0] = r;
            dst[i * 3 + 1] = b;
            dst[i * 3 + 2] = g;
        } else if (order == "GBR" || order == "gbr") {
            dst[i * 3 + 0] = g;
            dst[i * 3 + 1] = b;
            dst[i * 3 + 2] = r;
        } else if (order == "BGR" || order == "bgr") {
            dst[i * 3 + 0] = b;
            dst[i * 3 + 1] = g;
            dst[i * 3 + 2] = r;
        } else {
            // Default to GRB (WS2812)
            dst[i * 3 + 0] = g;
            dst[i * 3 + 1] = r;
            dst[i * 3 + 2] = b;
        }
    }

    // Send via RMT - send full segment buffer for proper reset timing
    rmt_transmit_config_t tx_config = {};
    tx_config.loop_count = 0;
    tx_config.flags.eot_level = 0;

    esp_err_t err = rmt_transmit(it->channel, it->encoder, it->buffer.data(), seg.led_count * 3, &tx_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "RMT transmit failed for GPIO %d: %s", seg.gpio, esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t rmt_driver_deinit_segment(int gpio, uint8_t rmt_channel) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    auto it = std::find_if(s_segments.begin(), s_segments.end(),
                          [&](const RmtDriverSegment& s) { return s.gpio == gpio && s.rmt_channel == rmt_channel; });
    if (it == s_segments.end() || !it->initialized) {
        return ESP_OK;
    }

    rmt_disable(it->channel);
    rmt_del_encoder(it->encoder);
    rmt_del_channel(it->channel);
    it->initialized = false;

    ESP_LOGI(TAG, "RMT driver deinitialized: GPIO %d", gpio);
    return ESP_OK;
}

void rmt_driver_deinit_all() {
    std::lock_guard<std::mutex> lock(s_mutex);
    for (auto& seg : s_segments) {
        if (seg.initialized) {
            rmt_disable(seg.channel);
            rmt_del_encoder(seg.encoder);
            rmt_del_channel(seg.channel);
            seg.initialized = false;
        }
    }
    s_segments.clear();
}

