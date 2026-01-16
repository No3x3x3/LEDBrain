#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <cstring>

// Framebuffer for LED effects - allows multi-pass rendering
// Provides off-screen rendering buffer for effects that need multiple passes
// or intermediate rendering steps

namespace framebuffer {

struct Framebuffer {
    std::vector<uint8_t> data;  // RGB888 format (3 bytes per pixel)
    uint16_t width;
    uint16_t height;
    uint32_t size_bytes;
    
    Framebuffer(uint16_t w, uint16_t h) 
        : width(w), height(h), size_bytes(w * h * 3) {
        data.resize(size_bytes, 0);
    }
    
    // Get pixel at (x, y) - returns pointer to RGB bytes
    uint8_t* pixel(uint16_t x, uint16_t y) {
        if (x >= width || y >= height) return nullptr;
        return data.data() + (y * width + x) * 3;
    }
    
    const uint8_t* pixel(uint16_t x, uint16_t y) const {
        if (x >= width || y >= height) return nullptr;
        return data.data() + (y * width + x) * 3;
    }
    
    // Clear framebuffer (fill with black)
    void clear() {
        std::memset(data.data(), 0, size_bytes);
    }
    
    // Clear with color
    void clear(uint8_t r, uint8_t g, uint8_t b) {
        for (size_t i = 0; i < data.size(); i += 3) {
            data[i] = r;
            data[i + 1] = g;
            data[i + 2] = b;
        }
    }
    
    // Copy from another framebuffer
    void copy_from(const Framebuffer& other) {
        if (width == other.width && height == other.height) {
            std::memcpy(data.data(), other.data.data(), size_bytes);
        }
    }
    
    // Copy to RGB buffer (for rendering to LED)
    void copy_to(std::vector<uint8_t>& out) const {
        out.resize(size_bytes);
        std::memcpy(out.data(), data.data(), size_bytes);
    }
};

// Framebuffer manager - manages multiple framebuffers for multi-pass effects
class FramebufferManager {
public:
    // Get or create framebuffer for segment
    // Returns shared_ptr to framebuffer (thread-safe)
    std::shared_ptr<Framebuffer> get_framebuffer(const std::string& segment_id, 
                                                  uint16_t width, uint16_t height);
    
    // Release framebuffer (decrements ref count)
    void release_framebuffer(const std::string& segment_id);
    
    // Clear all framebuffers
    void clear_all();
    
    // Get statistics
    size_t get_framebuffer_count() const;
    size_t get_total_memory_bytes() const;
    
private:
    struct FramebufferEntry {
        std::shared_ptr<Framebuffer> fb;
        uint16_t width;
        uint16_t height;
    };
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, FramebufferEntry> framebuffers_;
};

// Global framebuffer manager instance
FramebufferManager& get_manager();

}  // namespace framebuffer
