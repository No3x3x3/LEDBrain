#include "led_engine/rmt_driver.hpp"
#include "led_engine/chipset_info.hpp"
#include "led_engine/color_processing.hpp"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_sync.h"
#include "driver/gpio.h"
#include <algorithm>
#include <cstring>
#include <stddef.h>
#include <cmath>
#include <vector>

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
    bool supports_rgbw;
    uint8_t bytes_per_pixel;
    bool initialized;
    std::vector<uint8_t> buffer;
};

static std::vector<RmtDriverSegment> s_segments;
static std::mutex s_mutex;
static rmt_sync_manager_handle_t s_sync_manager = nullptr;
static bool s_parallel_mode_enabled = false;

static rmt_tx_channel_config_t make_channel_config(int gpio, bool enable_dma) {
    rmt_tx_channel_config_t tx_chan_config = {};
    tx_chan_config.gpio_num = static_cast<gpio_num_t>(gpio);
    tx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_chan_config.resolution_hz = 10'000'000;  // 10MHz = 100ns per tick
    tx_chan_config.mem_block_symbols = 64;
    tx_chan_config.trans_queue_depth = 4;
    tx_chan_config.flags.with_dma = enable_dma;  // Use DMA if enabled in config
    tx_chan_config.flags.invert_out = false;
    return tx_chan_config;
}

static rmt_bytes_encoder_config_t make_bytes_encoder_config(const ChipsetInfo* info) {
    rmt_bytes_encoder_config_t bytes_encoder_config = {};
    
    if (info->uses_spi) {
        // SPI-based chipsets (APA102, SK9822) use different protocol
        // For now, return default - SPI support will be added separately
        bytes_encoder_config.bit0 = {
            .duration0 = 3,
            .level0 = 1,
            .duration1 = 9,
            .level1 = 0,
        };
        bytes_encoder_config.bit1 = {
            .duration0 = 9,
            .level0 = 1,
            .duration1 = 3,
            .level1 = 0,
        };
    } else {
        // RMT-based chipsets use timing from chipset info
        bytes_encoder_config.bit0 = {
            .duration0 = info->timing.t0h_ticks,
            .level0 = 1,
            .duration1 = info->timing.t0l_ticks,
            .level1 = 0,
        };
        bytes_encoder_config.bit1 = {
            .duration0 = info->timing.t1h_ticks,
            .level0 = 1,
            .duration1 = info->timing.t1l_ticks,
            .level1 = 0,
        };
    }
    bytes_encoder_config.flags.msb_first = true;
    return bytes_encoder_config;
}

static esp_err_t create_led_encoder(rmt_encoder_handle_t* ret_encoder, const ChipsetInfo* chipset_info) {
    ws2812_encoder_t* encoder = (ws2812_encoder_t*)calloc(1, sizeof(ws2812_encoder_t));
    if (encoder == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    encoder->base.encode = ws2812_encode;
    encoder->base.del = ws2812_del;
    encoder->base.reset = ws2812_reset;

    rmt_bytes_encoder_config_t bytes_encoder_config = make_bytes_encoder_config(chipset_info);
    esp_err_t err = rmt_new_bytes_encoder(&bytes_encoder_config, &encoder->bytes_encoder);
    if (err != ESP_OK) {
        free(encoder);
        return err;
    }

    rmt_copy_encoder_config_t copy_encoder_config = {};
    err = rmt_new_copy_encoder(&copy_encoder_config, &encoder->copy_encoder);
    if (err != ESP_OK) {
        rmt_del_encoder(encoder->bytes_encoder);
        free(encoder);
        return err;
    }

    // Reset code from chipset timing
    encoder->reset_symbol = (rmt_symbol_word_t){
        .duration0 = chipset_info->timing.reset_ticks,
        .level0 = 0,
        .duration1 = 0,
        .level1 = 0,
    };

    *ret_encoder = &encoder->base;
    return ESP_OK;
}

esp_err_t rmt_driver_init_segment(const LedSegmentConfig& seg, bool enable_dma) {
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
    
    // Get chipset info
    const ChipsetInfo* chipset_info = get_chipset_info(driver_seg.chipset);
    driver_seg.supports_rgbw = chipset_info->supports_rgbw;
    driver_seg.bytes_per_pixel = chipset_info->supports_rgbw ? 4 : 3;
    
    // Use chipset default color order if not specified
    driver_seg.color_order = seg.color_order.empty() ? chipset_info->default_color_order : seg.color_order;
    driver_seg.initialized = false;

    // Create RMT channel (ESP-IDF 5.x doesn't use channel numbers, each channel is independent)
    rmt_tx_channel_config_t tx_chan_config = make_channel_config(seg.gpio, enable_dma);
    esp_err_t err = rmt_new_tx_channel(&tx_chan_config, &driver_seg.channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT channel for GPIO %d: %s", seg.gpio, esp_err_to_name(err));
        return err;
    }

    // Create encoder with chipset-specific timing
    err = create_led_encoder(&driver_seg.encoder, chipset_info);
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

    // Allocate buffer for full segment (RGB or RGBW)
    driver_seg.buffer.resize(seg.led_count * driver_seg.bytes_per_pixel);
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
    // Minimize mutex lock time - only for lookup
    RmtDriverSegment* driver_seg = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = std::find_if(s_segments.begin(), s_segments.end(),
                              [&](const RmtDriverSegment& s) { return s.gpio == seg.gpio && s.rmt_channel == seg.rmt_channel; });
        if (it == s_segments.end() || !it->initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        driver_seg = &(*it);  // Store pointer, mutex released after this block
    }
    
    // Input is always RGB (3 bytes per pixel)
    const uint8_t input_bytes_per_pixel = 3;
    if (rgb.size() < (start + length) * input_bytes_per_pixel) {
        return ESP_ERR_INVALID_SIZE;
    }

    // Ensure buffer is large enough for full segment (RGB or RGBW)
    const size_t buffer_size = seg.led_count * driver_seg->bytes_per_pixel;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (driver_seg->buffer.size() < buffer_size) {
            driver_seg->buffer.resize(buffer_size, 0);
        }
    }

    // Process pixels with color processing pipeline (outside mutex for performance)
    const size_t pixel_count = std::min(length, seg.led_count - start);
    const uint8_t* src = rgb.data() + start * input_bytes_per_pixel;
    
    // Get gamma values from segment config (default to 2.2 if not set)
    float gamma_color = seg.gamma_color > 0.0f ? seg.gamma_color : 2.2f;
    float gamma_brightness = seg.gamma_brightness > 0.0f ? seg.gamma_brightness : 2.2f;
    bool apply_gamma_flag = seg.apply_gamma;
    
    // Process pixels to temporary buffer (no mutex needed)
    std::vector<uint8_t> temp_buffer(pixel_count * driver_seg->bytes_per_pixel);
    for (size_t i = 0; i < pixel_count; ++i) {
        process_pixel(src + i * input_bytes_per_pixel, 
                     temp_buffer.data() + i * driver_seg->bytes_per_pixel,
                     driver_seg->color_order,
                     driver_seg->bytes_per_pixel,
                     gamma_color,
                     gamma_brightness,
                     apply_gamma_flag);
    }
    
    // Copy to segment buffer and send (minimal mutex time)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        // Verify segment still exists and is initialized
        auto it = std::find_if(s_segments.begin(), s_segments.end(),
                              [&](const RmtDriverSegment& s) { return s.gpio == seg.gpio && s.rmt_channel == seg.rmt_channel; });
        if (it == s_segments.end() || !it->initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        
        // Copy processed pixels to segment buffer
        std::memcpy(it->buffer.data() + start * it->bytes_per_pixel, 
                   temp_buffer.data(), 
                   temp_buffer.size());
    }
    
    // Send via RMT (non-blocking, doesn't need mutex)
    rmt_transmit_config_t tx_config = {};
    tx_config.loop_count = 0;
    tx_config.flags.eot_level = 0;
    
    // Need to get channel/encoder pointer (minimal lock)
    rmt_channel_handle_t channel;
    rmt_encoder_handle_t encoder;
    uint8_t* buffer_ptr;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = std::find_if(s_segments.begin(), s_segments.end(),
                              [&](const RmtDriverSegment& s) { return s.gpio == seg.gpio && s.rmt_channel == seg.rmt_channel; });
        if (it == s_segments.end() || !it->initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        channel = it->channel;
        encoder = it->encoder;
        buffer_ptr = it->buffer.data();
    }

    esp_err_t err = rmt_transmit(channel, encoder, buffer_ptr, buffer_size, &tx_config);
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
    
    // Delete sync manager if exists
    if (s_sync_manager) {
        rmt_del_sync_manager(s_sync_manager);
        s_sync_manager = nullptr;
        s_parallel_mode_enabled = false;
    }
    
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

// Initialize parallel IO mode - creates sync manager for simultaneous transmission
esp_err_t rmt_driver_init_parallel_mode(const std::vector<const LedSegmentConfig*>& segments) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (s_sync_manager) {
        ESP_LOGW(TAG, "Parallel mode already initialized");
        return ESP_OK;
    }
    
    if (segments.empty() || segments.size() > 4) {
        ESP_LOGE(TAG, "Parallel mode requires 1-4 segments (ESP32-P4 has 4 TX channels)");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Collect channels for sync manager
    std::vector<rmt_channel_handle_t> channels;
    for (const auto* seg : segments) {
        auto it = std::find_if(s_segments.begin(), s_segments.end(),
                              [&](const RmtDriverSegment& s) { 
                                  return s.gpio == seg->gpio && s.rmt_channel == seg->rmt_channel; 
                              });
        if (it == s_segments.end() || !it->initialized) {
            ESP_LOGE(TAG, "Segment GPIO %d not initialized for parallel mode", seg->gpio);
            return ESP_ERR_INVALID_STATE;
        }
        channels.push_back(it->channel);
    }
    
    // Create sync manager
    rmt_sync_manager_config_t sync_cfg = {};
    sync_cfg.tx_channel_array = channels.data();
    sync_cfg.array_size = channels.size();
    
    esp_err_t err = rmt_new_sync_manager(&sync_cfg, &s_sync_manager);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT sync manager: %s", esp_err_to_name(err));
        return err;
    }
    
    s_parallel_mode_enabled = true;
    ESP_LOGI(TAG, "Parallel IO mode initialized with %zu channels", channels.size());
    return ESP_OK;
}

// Render to multiple segments in parallel (simultaneous transmission)
esp_err_t rmt_driver_render_parallel(const std::vector<ParallelRenderRequest>& requests) {
    if (!s_parallel_mode_enabled || !s_sync_manager) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (requests.empty() || requests.size() > 4) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Prepare all channels first (outside sync)
    std::vector<rmt_channel_handle_t> channels;
    std::vector<rmt_encoder_handle_t> encoders;
    std::vector<uint8_t*> buffers;
    std::vector<size_t> buffer_sizes;
    
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        for (const auto& req : requests) {
            auto it = std::find_if(s_segments.begin(), s_segments.end(),
                                  [&](const RmtDriverSegment& s) { 
                                      return s.gpio == req.segment->gpio && 
                                             s.rmt_channel == req.segment->rmt_channel; 
                                  });
            if (it == s_segments.end() || !it->initialized) {
                ESP_LOGE(TAG, "Segment GPIO %d not found for parallel render", req.segment->gpio);
                return ESP_ERR_INVALID_STATE;
            }
            
            // Process pixels (same as single render)
            const size_t pixel_count = std::min(req.length, req.segment->led_count - req.start);
            const uint8_t input_bytes_per_pixel = 3;
            const uint8_t* src = req.rgb.data() + req.start * input_bytes_per_pixel;
            
            float gamma_color = req.segment->gamma_color > 0.0f ? req.segment->gamma_color : 2.2f;
            float gamma_brightness = req.segment->gamma_brightness > 0.0f ? req.segment->gamma_brightness : 2.2f;
            bool apply_gamma_flag = req.segment->apply_gamma;
            
            const size_t buffer_size = req.segment->led_count * it->bytes_per_pixel;
            if (it->buffer.size() < buffer_size) {
                it->buffer.resize(buffer_size, 0);
            }
            
            // Process pixels to segment buffer
            for (size_t i = 0; i < pixel_count; ++i) {
                process_pixel(src + i * input_bytes_per_pixel,
                             it->buffer.data() + (req.start + i) * it->bytes_per_pixel,
                             it->color_order,
                             it->bytes_per_pixel,
                             gamma_color,
                             gamma_brightness,
                             apply_gamma_flag);
            }
            
            channels.push_back(it->channel);
            encoders.push_back(it->encoder);
            buffers.push_back(it->buffer.data());
            buffer_sizes.push_back(buffer_size);
        }
    }
    
    // Reset sync manager before parallel transmission
    esp_err_t err = rmt_sync_reset(s_sync_manager);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Sync reset failed: %s", esp_err_to_name(err));
    }
    
    // Transmit on all channels (they will sync automatically)
    rmt_transmit_config_t tx_config = {};
    tx_config.loop_count = 0;
    tx_config.flags.eot_level = 0;
    
    for (size_t i = 0; i < channels.size(); ++i) {
        err = rmt_transmit(channels[i], encoders[i], buffers[i], buffer_sizes[i], &tx_config);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Parallel RMT transmit failed for channel %zu: %s", i, esp_err_to_name(err));
            // Continue with other channels
        }
    }
    
    return ESP_OK;
}

