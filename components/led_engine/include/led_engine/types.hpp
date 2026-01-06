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
  uint16_t render_order{0};
  int gpio{-1};
  uint8_t rmt_channel{0};
  std::string chipset{"ws2812b"};
  std::string color_order{"GRB"};
  std::string effect_source{"local"};
  bool enabled{true};
  bool reverse{false};
  bool mirror{false};
  bool matrix_enabled{false};
  LedMatrixConfig matrix{};
  uint16_t power_limit_ma{0};
  LedSegmentAudioConfig audio{};
  // Color processing
  float gamma_color{2.2f};
  float gamma_brightness{2.2f};
  bool apply_gamma{true};
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
  std::string direction{"forward"};
  float scatter{0.0f};
  uint16_t fade_in{0};
  uint16_t fade_out{0};
  std::string color1{"#ffffff"};
  std::string color2{"#ff6600"};
  std::string color3{"#0033ff"};
  std::string palette{};
  std::string gradient{};
  uint8_t brightness_override{0};
  float gamma_color{2.2f};
  float gamma_brightness{2.2f};
  std::string blend_mode{"normal"};
  uint8_t layers{1};
  std::string reactive_mode{"full"};
  float band_gain_low{1.0f};
  float band_gain_mid{1.0f};
  float band_gain_high{1.0f};
  float amplitude_scale{1.0f};
  float brightness_compress{0.0f};
  bool beat_response{false};
  uint16_t attack_ms{25};
  uint16_t release_ms{120};
  std::string scene_preset{};
  std::string scene_schedule{};
  bool beat_shuffle{false};
  // Custom frequency range for audio reactivity (0 = use default bands)
  float freq_min{0.0f};  // Minimum frequency in Hz (0 = use reactive_mode)
  float freq_max{0.0f};  // Maximum frequency in Hz (0 = use reactive_mode)
  // Selected frequency bands for audio reactivity
  // Empty = use reactive_mode, otherwise use selected bands
  std::vector<std::string> selected_bands{};  // e.g. ["bass", "mid_low", "treble_high", "energy", "beat"]
};

struct VirtualSegmentMember {
  std::string type{"physical"};  // physical | wled
  std::string id{};              // segment id or wled device id
  uint16_t segment_index{0};
  uint16_t start{0};
  uint16_t length{0};
};

struct VirtualSegmentConfig {
  std::string id{"vseg-1"};
  std::string name{"Virtual"};
  std::vector<VirtualSegmentMember> members{};
  EffectAssignment effect{};  // effect applied to the whole virtual group
  bool enabled{true};
};

struct EffectsConfig {
  std::string default_engine{"wled"};
  std::vector<EffectAssignment> assignments{};
};

struct LedHardwareConfig {
  LedDriverType driver{LedDriverType::EspRmt};
  uint16_t max_fps{120};
  uint32_t global_current_limit_ma{6000};
  uint8_t global_brightness{255};
  float power_supply_voltage{5.0f};
  float power_supply_watts{0.0f};
  bool auto_power_limit{false};
  uint8_t parallel_outputs{4};
  bool enable_dma{true};
  std::vector<LedSegmentConfig> segments{};
  std::vector<VirtualSegmentConfig> virtual_segments{};
  AudioConfig audio{};
  EffectsConfig effects{};
};
