#include "led_engine/framebuffer.hpp"
#include "esp_log.h"
#include <cstring>
#include <unordered_map>

namespace framebuffer {

static const char* TAG = "framebuffer";
static FramebufferManager s_manager;

std::shared_ptr<Framebuffer> FramebufferManager::get_framebuffer(
    const std::string& segment_id, uint16_t width, uint16_t height) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = framebuffers_.find(segment_id);
    if (it != framebuffers_.end()) {
        // Check if dimensions match
        if (it->second.width == width && it->second.height == height) {
            return it->second.fb;
        } else {
            // Dimensions changed - recreate
            ESP_LOGI(TAG, "Recreating framebuffer for %s: %ux%u -> %ux%u",
                     segment_id.c_str(), it->second.width, it->second.height, width, height);
            framebuffers_.erase(it);
        }
    }
    
    // Create new framebuffer
    auto fb = std::make_shared<Framebuffer>(width, height);
    framebuffers_[segment_id] = {fb, width, height};
    
    ESP_LOGI(TAG, "Created framebuffer for %s: %ux%u (%u bytes)",
             segment_id.c_str(), width, height, width * height * 3);
    
    return fb;
}

void FramebufferManager::release_framebuffer(const std::string& segment_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = framebuffers_.find(segment_id);
    if (it != framebuffers_.end()) {
        // shared_ptr will automatically clean up when ref count reaches 0
        // We keep the entry until all references are released
        // For now, we'll keep it - actual cleanup happens when ref count is 0
        ESP_LOGD(TAG, "Release requested for framebuffer %s", segment_id.c_str());
    }
}

void FramebufferManager::clear_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [id, entry] : framebuffers_) {
        if (entry.fb) {
            entry.fb->clear();
        }
    }
    
    ESP_LOGI(TAG, "Cleared all framebuffers");
}

size_t FramebufferManager::get_framebuffer_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return framebuffers_.size();
}

size_t FramebufferManager::get_total_memory_bytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t total = 0;
    for (const auto& [id, entry] : framebuffers_) {
        total += entry.width * entry.height * 3;
    }
    return total;
}

FramebufferManager& get_manager() {
    return s_manager;
}

}  // namespace framebuffer
