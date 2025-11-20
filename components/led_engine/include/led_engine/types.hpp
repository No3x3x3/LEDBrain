#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum class LedDriverType {
  EspRmt,
  NeoPixelBus,
  External,
};

enum class AudioSourceType {
  None,
  Snapcast,
  LineInput,
};

struct LedMatrixConfig {
  uint16_t width{0};
  uint16_t height{0};
  bool serpentine{true};
  bool vertical{false};
};

struct LedSegmentAudioConfig {
  bool stereo_split{false};
  float gain_left{1.0f};
  float gain_right{1.0f};
  float sensitivity{0.85f};
  float threshold{0.05f};
  float smoothing{0.65f};
  std::vector<float> per_led_sensitivity{};
};

struct LedSegmentConfig {
  std::string id{"segment-1"};
  std::string name{"Segment"};
  uint16_t start_index{0};
  uint16_t led_count{120};
  int gpio{-1};
  uint8_t rmt_channel{0};
  std::string chipset{"ws2812b"};
  std::string color_order{"GRB"};
  bool enabled{true};
  bool reverse{false};
  bool mirror{false};
  bool matrix_enabled{false};
  LedMatrixConfig matrix{};
  uint16_t power_limit_ma{0};
  LedSegmentAudioConfig audio{};
};

struct SnapcastConfig {
  bool enabled{false};
  std::string host{"snapcast.local"};
  uint16_t port{1704};
  uint16_t latency_ms{60};
  bool prefer_udp{true};
};

struct AudioConfig {
  AudioSourceType source{AudioSourceType::None};
  uint32_t sample_rate{48000};
  uint16_t frame_ms{20};
  uint16_t fft_size{1024};
  bool stereo{true};
  float sensitivity{1.0f};
  SnapcastConfig snapcast{};
};

struct EffectAssignment {
  std::string segment_id{"segment-1"};
  std::string engine{"wled"};
  std::string effect{"Solid"};
  std::string preset{};
  bool audio_link{false};
  std::string audio_profile{"default"};
  uint8_t brightness{255};
  uint8_t intensity{128};
  uint8_t speed{128};
  std::string audio_mode{"spectrum"};
};

struct EffectsConfig {
  std::string default_engine{"wled"};
  std::vector<EffectAssignment> assignments{};
};

struct LedHardwareConfig {
  LedDriverType driver{LedDriverType::EspRmt};
  uint16_t max_fps{120};
  uint32_t global_current_limit_ma{6000};
  float power_supply_voltage{5.0f};
  float power_supply_watts{0.0f};
  bool auto_power_limit{false};
  uint8_t parallel_outputs{4};
  bool dedicate_core{true};
  bool enable_dma{true};
  std::vector<LedSegmentConfig> segments{};
  AudioConfig audio{};
  EffectsConfig effects{};
};
