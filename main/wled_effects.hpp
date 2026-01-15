#pragma once

#include "config.hpp"
#include "led_engine.hpp"
#include "wled_discovery.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>

class WledEffectsRuntime {
 public:
  esp_err_t start(AppConfig* cfg, LedEngineRuntime* led_runtime);
  void update_config(const AppConfig& cfg);
  void stop();

 private:
  static void task_entry(void* arg);
  void task_loop();
  bool should_run() const;
  bool resolve_device(const std::string& device_id,
                      const std::vector<WledDeviceConfig>& devices,
                      const std::vector<WledDeviceStatus>& status,
                      WledDeviceConfig& out,
                      std::string& ip) const;
  bool render_and_send(const WledEffectBinding& binding,
                       const WledDeviceConfig& device,
                       const std::string& ip,
                       float time_s,
                       uint32_t frame_idx,  // Frame counter for caching/logging only
                       uint8_t global_brightness,
                       uint16_t fps);
  std::vector<uint8_t> render_frame(const WledEffectBinding& binding,
                                    uint16_t led_count,
                                    uint32_t frame_idx,
                                    uint8_t global_brightness,
                                    uint16_t fps,
                                    const LedLayoutConfig& layout = LedLayoutConfig{});
  float apply_envelope(const std::string& key, float input, uint16_t fps, uint16_t attack_ms, uint16_t release_ms);
  std::string envelope_key(const WledEffectBinding& binding) const;
  uint8_t next_seq();  // Get next DDP sequence (1-15, cycling)

  AppConfig* cfg_ref_{nullptr};
  LedEngineRuntime* led_runtime_{nullptr};
  std::mutex mutex_;
  WledEffectsConfig effects_{};
  std::vector<WledDeviceConfig> devices_{};
  TaskHandle_t task_{nullptr};
  bool running_{false};
  uint8_t seq_{1};  // DDP sequence must be 1-15 (0 is reserved)
  std::unordered_map<std::string, float> envelope_state_;
  std::unordered_set<std::string> active_ddp_devices_;  // Track devices with active DDP mode
  
  // Performance optimizations for multiple devices/segments
  struct CachedAddrInfo {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    uint64_t cached_at_us;
    bool valid;
  };
  std::unordered_map<std::string, CachedAddrInfo> ddp_addr_cache_;  // IP -> cached addrinfo
  static constexpr uint64_t DNS_CACHE_TTL_US = 30'000'000ULL;  // 30 seconds
  
  // Frame cache for same effect (same effect + same LED count = reuse frame)
  struct FrameCacheKey {
    std::string effect_name;
    uint16_t led_count;
    uint32_t frame_idx;
    bool operator==(const FrameCacheKey& other) const {
      return effect_name == other.effect_name && led_count == other.led_count && frame_idx == other.frame_idx;
    }
  };
  struct FrameCacheKeyHash {
    size_t operator()(const FrameCacheKey& k) const {
      return std::hash<std::string>{}(k.effect_name) ^ (std::hash<uint16_t>{}(k.led_count) << 1) ^ (std::hash<uint32_t>{}(k.frame_idx) << 2);
    }
  };
  std::unordered_map<FrameCacheKey, std::vector<uint8_t>, FrameCacheKeyHash> frame_cache_;
};
