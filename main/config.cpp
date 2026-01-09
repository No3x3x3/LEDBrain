#include "config.hpp"
#include "cJSON.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <algorithm>
#include <string>
#include <utility>

static constexpr const char* TAG = "config";
static constexpr const char* NVS_NAMESPACE = "appcfg";
static constexpr const char* KEY_BLOB = "config";
static constexpr uint32_t CURRENT_SCHEMA = 8;

extern const uint8_t _binary_config_json_start[] asm("_binary_config_json_start");
extern const uint8_t _binary_config_json_end[] asm("_binary_config_json_end");

namespace {

void decode_led_engine(AppConfig& cfg, cJSON* root);
void encode_led_engine(const AppConfig& cfg, cJSON* root);
void decode_wled_effects(AppConfig& cfg, cJSON* root);
void encode_wled_effects(const AppConfig& cfg, cJSON* root);
void decode_virtual_segments(AppConfig& cfg, cJSON* root);
void encode_virtual_segments(const AppConfig& cfg, cJSON* root);

void prune_legacy_defaults(AppConfig& cfg) {
  // Drop the old default "Biurko"/"Desk" segment that was bundled in config.json
  if (cfg.led_engine.segments.size() == 1) {
    const auto& seg = cfg.led_engine.segments.front();
    if ((seg.id == "segment-1") &&
        (seg.name == "Biurko" || seg.name == "Desk" || seg.name.empty()) &&
        seg.start_index == 0) {
      cfg.led_engine.segments.clear();
      cfg.led_engine.effects.assignments.clear();
    }
  }
}

void prune_effect_assignments(AppConfig& cfg) {
  if (cfg.led_engine.effects.assignments.empty()) {
    return;
  }
  std::vector<std::string> valid;
  valid.reserve(cfg.led_engine.segments.size());
  for (const auto& seg : cfg.led_engine.segments) {
    if (!seg.id.empty()) valid.push_back(seg.id);
  }
  auto& assigns = cfg.led_engine.effects.assignments;
  assigns.erase(std::remove_if(assigns.begin(), assigns.end(), [&](const EffectAssignment& asg) {
                  return std::find(valid.begin(), valid.end(), asg.segment_id) == valid.end();
                }),
                assigns.end());
}

void prune_wled_effect_bindings(AppConfig& cfg) {
  if (cfg.wled_effects.bindings.empty()) {
    return;
  }
  std::vector<std::string> valid_devices;
  valid_devices.reserve(cfg.wled_devices.size());
  for (const auto& dev : cfg.wled_devices) {
    if (!dev.id.empty()) {
      valid_devices.push_back(dev.id);
    }
  }
  auto& bindings = cfg.wled_effects.bindings;
  bindings.erase(std::remove_if(bindings.begin(), bindings.end(), [&](const WledEffectBinding& bind) {
                   if (bind.device_id.empty()) {
                     return true;
                   }
                   return std::find(valid_devices.begin(), valid_devices.end(), bind.device_id) == valid_devices.end();
                 }),
                 bindings.end());
}

void dedupe_wled_effects(AppConfig& cfg) {
  if (cfg.wled_effects.bindings.empty()) {
    return;
  }
  std::vector<WledEffectBinding> unique;
  std::vector<std::string> keys;
  for (const auto& bind : cfg.wled_effects.bindings) {
    if (bind.device_id.empty()) {
      continue;
    }
    const std::string key = bind.device_id + "#" + std::to_string(bind.segment_index);
    if (std::find(keys.begin(), keys.end(), key) != keys.end()) {
      continue;
    }
    keys.push_back(key);
    unique.push_back(bind);
  }
  cfg.wled_effects.bindings.swap(unique);
}

void dedupe_wled_config(AppConfig& cfg) {
  if (cfg.wled_devices.empty()) {
    return;
  }
  std::vector<WledDeviceConfig> deduped;
  auto key_of = [](const WledDeviceConfig& dev) {
    if (!dev.address.empty()) return dev.address;
    return dev.id;
  };
  for (const auto& dev : cfg.wled_devices) {
    const auto key = key_of(dev);
    bool exists = false;
    for (const auto& kept : deduped) {
      const auto kept_key = key_of(kept);
      if (!key.empty() && !kept_key.empty() && key == kept_key) {
        exists = true;
        break;
      }
    }
    if (!exists) {
      deduped.push_back(dev);
    }
  }
  cfg.wled_devices.swap(deduped);
}

void decode_wled_devices(AppConfig& cfg, cJSON* root) {
  if (!root) {
    return;
  }
  cJSON* arr = cJSON_GetObjectItem(root, "wled_devices");
  if (!arr) {
    return;
  }
  cfg.wled_devices.clear();
  if (!cJSON_IsArray(arr)) {
    return;
  }
  cJSON* entry = nullptr;
  cJSON_ArrayForEach(entry, arr) {
    if (!cJSON_IsObject(entry)) {
      continue;
    }
    WledDeviceConfig dev{};
    if (cJSON* id = cJSON_GetObjectItem(entry, "id"); cJSON_IsString(id)) dev.id = id->valuestring;
    if (cJSON* name = cJSON_GetObjectItem(entry, "name"); cJSON_IsString(name)) dev.name = name->valuestring;
    if (cJSON* addr = cJSON_GetObjectItem(entry, "address"); cJSON_IsString(addr)) dev.address = addr->valuestring;
    if (cJSON* leds = cJSON_GetObjectItem(entry, "leds"); cJSON_IsNumber(leds)) {
      dev.leds = static_cast<uint16_t>(std::max(0, static_cast<int>(leds->valuedouble)));
    }
    if (cJSON* segments = cJSON_GetObjectItem(entry, "segments"); cJSON_IsNumber(segments)) {
      int seg_val = static_cast<int>(segments->valuedouble);
      dev.segments = static_cast<uint16_t>(seg_val <= 0 ? 1 : seg_val);
    }
    if (cJSON* active = cJSON_GetObjectItem(entry, "active"); cJSON_IsBool(active)) {
      dev.active = cJSON_IsTrue(active);
    }
    if (cJSON* autod = cJSON_GetObjectItem(entry, "auto_discovered"); cJSON_IsBool(autod)) {
      dev.auto_discovered = cJSON_IsTrue(autod);
    }
    if (dev.id.empty()) {
      if (!dev.address.empty()) {
        dev.id = dev.address;
      } else {
        continue;
      }
    }
    if (dev.name.empty()) {
      dev.name = dev.id;
    }
    cfg.wled_devices.push_back(std::move(dev));
  }
}

void encode_wled_devices(const AppConfig& cfg, cJSON* root) {
  if (!root) {
    return;
  }
  cJSON* arr = cJSON_AddArrayToObject(root, "wled_devices");
  if (!arr) {
    return;
  }
  for (const auto& dev : cfg.wled_devices) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj) {
      continue;
    }
    cJSON_AddStringToObject(obj, "id", dev.id.c_str());
    cJSON_AddStringToObject(obj, "name", dev.name.c_str());
    cJSON_AddStringToObject(obj, "address", dev.address.c_str());
    cJSON_AddNumberToObject(obj, "leds", dev.leds);
    cJSON_AddNumberToObject(obj, "segments", dev.segments);
    cJSON_AddBoolToObject(obj, "active", dev.active);
    cJSON_AddBoolToObject(obj, "auto_discovered", dev.auto_discovered);
    cJSON_AddItemToArray(arr, obj);
  }
}

bool decode_json(AppConfig& cfg, const char* json, size_t len) {
  cJSON* root = cJSON_ParseWithLength(json, len);
  if (!root) {
    ESP_LOGW(TAG, "Failed to parse config JSON");
    return false;
  }

  if (cJSON* lang = cJSON_GetObjectItem(root, "lang"); cJSON_IsString(lang)) {
    cfg.lang = lang->valuestring;
  }

  if (cJSON* autostart = cJSON_GetObjectItem(root, "autostart"); cJSON_IsBool(autostart)) {
    cfg.autostart = cJSON_IsTrue(autostart);
  }

  if (cJSON* net = cJSON_GetObjectItem(root, "network"); cJSON_IsObject(net)) {
    if (cJSON* hn = cJSON_GetObjectItem(net, "hostname"); cJSON_IsString(hn)) cfg.network.hostname = hn->valuestring;
    if (cJSON* dh = cJSON_GetObjectItem(net, "use_dhcp"); cJSON_IsBool(dh)) cfg.network.use_dhcp = cJSON_IsTrue(dh);
    if (cJSON* ip = cJSON_GetObjectItem(net, "static_ip"); cJSON_IsString(ip)) cfg.network.static_ip = ip->valuestring;
    if (cJSON* nm = cJSON_GetObjectItem(net, "netmask"); cJSON_IsString(nm)) {
      cfg.network.netmask = nm->valuestring;
    } else if (cJSON* sn = cJSON_GetObjectItem(net, "subnet"); cJSON_IsString(sn)) {
      cfg.network.netmask = sn->valuestring;
    }
    if (cJSON* gw = cJSON_GetObjectItem(net, "gateway"); cJSON_IsString(gw)) cfg.network.gateway = gw->valuestring;
    if (cJSON* dns = cJSON_GetObjectItem(net, "dns"); cJSON_IsString(dns)) cfg.network.dns = dns->valuestring;
    if (cJSON* wifi_ssid = cJSON_GetObjectItem(net, "wifi_ssid"); cJSON_IsString(wifi_ssid)) cfg.network.wifi_ssid = wifi_ssid->valuestring;
    if (cJSON* wifi_password = cJSON_GetObjectItem(net, "wifi_password"); cJSON_IsString(wifi_password)) cfg.network.wifi_password = wifi_password->valuestring;
    if (cJSON* wifi_enabled = cJSON_GetObjectItem(net, "wifi_enabled"); cJSON_IsBool(wifi_enabled)) cfg.network.wifi_enabled = cJSON_IsTrue(wifi_enabled);
  }

  if (cJSON* mq = cJSON_GetObjectItem(root, "mqtt"); cJSON_IsObject(mq)) {
    if (cJSON* cfgd = cJSON_GetObjectItem(mq, "configured"); cJSON_IsBool(cfgd)) cfg.mqtt.configured = cJSON_IsTrue(cfgd);
    if (cJSON* host = cJSON_GetObjectItem(mq, "host"); cJSON_IsString(host)) cfg.mqtt.host = host->valuestring;
    if (cJSON* port = cJSON_GetObjectItem(mq, "port"); cJSON_IsNumber(port)) cfg.mqtt.port = static_cast<uint16_t>(port->valuedouble);
    if (cJSON* user = cJSON_GetObjectItem(mq, "username"); cJSON_IsString(user)) cfg.mqtt.username = user->valuestring;
    if (cJSON* pass = cJSON_GetObjectItem(mq, "password"); cJSON_IsString(pass)) cfg.mqtt.password = pass->valuestring;
    if (cJSON* ddp_target = cJSON_GetObjectItem(mq, "ddp_target"); cJSON_IsString(ddp_target)) cfg.mqtt.ddp_target = ddp_target->valuestring;
    if (cJSON* ddp_port = cJSON_GetObjectItem(mq, "ddp_port"); cJSON_IsNumber(ddp_port)) cfg.mqtt.ddp_port = static_cast<uint16_t>(ddp_port->valuedouble);
  }

  decode_wled_devices(cfg, root);
  if (cJSON* led = cJSON_GetObjectItem(root, "led_engine"); cJSON_IsObject(led)) {
    decode_led_engine(cfg, led);
  }
  decode_wled_effects(cfg, root);
  decode_virtual_segments(cfg, root);

  if (cJSON* schema = cJSON_GetObjectItem(root, "schema_version"); cJSON_IsNumber(schema)) {
    cfg.schema_version = static_cast<uint32_t>(schema->valuedouble);
  }

  prune_legacy_defaults(cfg);
  prune_effect_assignments(cfg);
  dedupe_wled_config(cfg);
  prune_wled_effect_bindings(cfg);
  dedupe_wled_effects(cfg);

  cJSON_Delete(root);
  return true;
}

std::string encode_json(const AppConfig& cfg) {
  cJSON* root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "lang", cfg.lang.c_str());
  cJSON_AddBoolToObject(root, "autostart", cfg.autostart);
  cJSON_AddNumberToObject(root, "schema_version", CURRENT_SCHEMA);

  cJSON* net = cJSON_AddObjectToObject(root, "network");
  cJSON_AddStringToObject(net, "hostname", cfg.network.hostname.c_str());
  cJSON_AddBoolToObject(net, "use_dhcp", cfg.network.use_dhcp);
  cJSON_AddStringToObject(net, "static_ip", cfg.network.static_ip.c_str());
  cJSON_AddStringToObject(net, "netmask", cfg.network.netmask.c_str());
  cJSON_AddStringToObject(net, "gateway", cfg.network.gateway.c_str());
  cJSON_AddStringToObject(net, "dns", cfg.network.dns.c_str());
  cJSON_AddStringToObject(net, "wifi_ssid", cfg.network.wifi_ssid.c_str());
  cJSON_AddStringToObject(net, "wifi_password", cfg.network.wifi_password.c_str());
  cJSON_AddBoolToObject(net, "wifi_enabled", cfg.network.wifi_enabled);

  cJSON* mq = cJSON_AddObjectToObject(root, "mqtt");
  cJSON_AddBoolToObject(mq, "configured", cfg.mqtt.configured);
  cJSON_AddStringToObject(mq, "host", cfg.mqtt.host.c_str());
  cJSON_AddNumberToObject(mq, "port", cfg.mqtt.port);
  cJSON_AddStringToObject(mq, "username", cfg.mqtt.username.c_str());
  cJSON_AddStringToObject(mq, "password", cfg.mqtt.password.c_str());
  cJSON_AddStringToObject(mq, "ddp_target", cfg.mqtt.ddp_target.c_str());
  cJSON_AddNumberToObject(mq, "ddp_port", cfg.mqtt.ddp_port);

  encode_wled_devices(cfg, root);
  encode_wled_effects(cfg, root);
  encode_virtual_segments(cfg, root);
  encode_led_engine(cfg, root);

  char* txt = cJSON_PrintUnformatted(root);
  std::string out = txt ? txt : "{}";
  if (txt) {
    cJSON_free(txt);
  }
  cJSON_Delete(root);
  return out;
}

std::string led_driver_to_string(LedDriverType type) {
  switch (type) {
    case LedDriverType::EspRmt:
      return "esp_rmt";
    case LedDriverType::NeoPixelBus:
      return "neopixelbus";
    case LedDriverType::External:
      return "external";
  }
  return "esp_rmt";
}

LedDriverType led_driver_from_string(const char* value) {
  if (!value) {
    return LedDriverType::EspRmt;
  }
  std::string v = value;
  if (v == "neopixelbus") return LedDriverType::NeoPixelBus;
  if (v == "external") return LedDriverType::External;
  return LedDriverType::EspRmt;
}

std::string audio_source_to_string(AudioSourceType type) {
  switch (type) {
    case AudioSourceType::None:
      return "none";
    case AudioSourceType::Snapcast:
      return "snapcast";
    case AudioSourceType::LineInput:
      return "line_in";
  }
  return "none";
}

AudioSourceType audio_source_from_string(const char* value) {
  if (!value) {
    return AudioSourceType::None;
  }
  std::string v = value;
  if (v == "snapcast") return AudioSourceType::Snapcast;
  if (v == "line_in" || v == "linein") return AudioSourceType::LineInput;
  return AudioSourceType::None;
}

void decode_matrix(LedMatrixConfig& cfg, cJSON* obj) {
  if (!cJSON_IsObject(obj)) {
    return;
  }
  if (cJSON* w = cJSON_GetObjectItem(obj, "width"); cJSON_IsNumber(w)) {
    cfg.width = static_cast<uint16_t>(std::max(0, static_cast<int>(w->valuedouble)));
  }
  if (cJSON* h = cJSON_GetObjectItem(obj, "height"); cJSON_IsNumber(h)) {
    cfg.height = static_cast<uint16_t>(std::max(0, static_cast<int>(h->valuedouble)));
  }
  if (cJSON* serp = cJSON_GetObjectItem(obj, "serpentine"); cJSON_IsBool(serp)) {
    cfg.serpentine = cJSON_IsTrue(serp);
  }
  if (cJSON* vert = cJSON_GetObjectItem(obj, "vertical"); cJSON_IsBool(vert)) {
    cfg.vertical = cJSON_IsTrue(vert);
  }
}

cJSON* encode_matrix(const LedMatrixConfig& cfg) {
  cJSON* obj = cJSON_CreateObject();
  if (!obj) {
    return nullptr;
  }
  cJSON_AddNumberToObject(obj, "width", cfg.width);
  cJSON_AddNumberToObject(obj, "height", cfg.height);
  cJSON_AddBoolToObject(obj, "serpentine", cfg.serpentine);
  cJSON_AddBoolToObject(obj, "vertical", cfg.vertical);
  return obj;
}

void decode_segment_audio(LedSegmentAudioConfig& cfg, cJSON* obj) {
  if (!cJSON_IsObject(obj)) {
    return;
  }
  if (cJSON* split = cJSON_GetObjectItem(obj, "stereo_split"); cJSON_IsBool(split)) {
    cfg.stereo_split = cJSON_IsTrue(split);
  }
  if (cJSON* gl = cJSON_GetObjectItem(obj, "gain_left"); cJSON_IsNumber(gl)) {
    cfg.gain_left = static_cast<float>(gl->valuedouble);
  }
  if (cJSON* gr = cJSON_GetObjectItem(obj, "gain_right"); cJSON_IsNumber(gr)) {
    cfg.gain_right = static_cast<float>(gr->valuedouble);
  }
  if (cJSON* sens = cJSON_GetObjectItem(obj, "sensitivity"); cJSON_IsNumber(sens)) {
    cfg.sensitivity = static_cast<float>(sens->valuedouble);
  }
  if (cJSON* thr = cJSON_GetObjectItem(obj, "threshold"); cJSON_IsNumber(thr)) {
    cfg.threshold = static_cast<float>(thr->valuedouble);
  }
  if (cJSON* sm = cJSON_GetObjectItem(obj, "smoothing"); cJSON_IsNumber(sm)) {
    cfg.smoothing = static_cast<float>(sm->valuedouble);
  }
  if (cJSON* map = cJSON_GetObjectItem(obj, "per_led_sensitivity"); cJSON_IsArray(map)) {
    cfg.per_led_sensitivity.clear();
    cJSON* node = nullptr;
    cJSON_ArrayForEach(node, map) {
      if (cJSON_IsNumber(node)) {
        cfg.per_led_sensitivity.push_back(static_cast<float>(node->valuedouble));
      }
    }
  }
}

cJSON* encode_segment_audio(const LedSegmentAudioConfig& cfg) {
  cJSON* obj = cJSON_CreateObject();
  if (!obj) {
    return nullptr;
  }
  cJSON_AddBoolToObject(obj, "stereo_split", cfg.stereo_split);
  cJSON_AddNumberToObject(obj, "gain_left", cfg.gain_left);
  cJSON_AddNumberToObject(obj, "gain_right", cfg.gain_right);
  cJSON_AddNumberToObject(obj, "sensitivity", cfg.sensitivity);
  cJSON_AddNumberToObject(obj, "threshold", cfg.threshold);
  cJSON_AddNumberToObject(obj, "smoothing", cfg.smoothing);
  if (!cfg.per_led_sensitivity.empty()) {
    cJSON* arr = cJSON_AddArrayToObject(obj, "per_led_sensitivity");
    if (arr) {
      for (float value : cfg.per_led_sensitivity) {
        cJSON_AddItemToArray(arr, cJSON_CreateNumber(value));
      }
    }
  }
  return obj;
}

void decode_led_segments(LedHardwareConfig& hw, cJSON* obj) {
  if (!cJSON_IsObject(obj)) {
    return;
  }
  if (cJSON* driver = cJSON_GetObjectItem(obj, "driver"); cJSON_IsString(driver)) {
    hw.driver = led_driver_from_string(driver->valuestring);
  }
  if (cJSON* fps = cJSON_GetObjectItem(obj, "max_fps"); cJSON_IsNumber(fps)) {
    hw.max_fps = static_cast<uint16_t>(std::max(1, static_cast<int>(fps->valuedouble)));
  }
  if (cJSON* limit = cJSON_GetObjectItem(obj, "global_current_limit_ma"); cJSON_IsNumber(limit)) {
    hw.global_current_limit_ma = static_cast<uint32_t>(std::max(0, static_cast<int>(limit->valuedouble)));
  }
  if (cJSON* bri = cJSON_GetObjectItem(obj, "global_brightness"); cJSON_IsNumber(bri)) {
    const int val = static_cast<int>(bri->valuedouble);
    hw.global_brightness = static_cast<uint8_t>(std::clamp(val, 0, 255));
  }
  if (cJSON* voltage = cJSON_GetObjectItem(obj, "power_supply_voltage"); cJSON_IsNumber(voltage)) {
    hw.power_supply_voltage = static_cast<float>(voltage->valuedouble);
  }
  if (cJSON* watts = cJSON_GetObjectItem(obj, "power_supply_watts"); cJSON_IsNumber(watts)) {
    hw.power_supply_watts = static_cast<float>(watts->valuedouble);
  }
  if (cJSON* auto_limit = cJSON_GetObjectItem(obj, "auto_power_limit"); cJSON_IsBool(auto_limit)) {
    hw.auto_power_limit = cJSON_IsTrue(auto_limit);
  }
  if (hw.power_supply_voltage <= 0.0f) {
    hw.power_supply_voltage = 5.0f;
  }
  if (hw.power_supply_watts < 0.0f) {
    hw.power_supply_watts = 0.0f;
  }
  if (hw.auto_power_limit && hw.power_supply_voltage > 0.0f && hw.power_supply_watts > 0.0f) {
    const float limit = (hw.power_supply_watts * 1000.0f) / hw.power_supply_voltage;
    hw.global_current_limit_ma = static_cast<uint32_t>(std::max(0.0f, limit));
  }
  if (cJSON* outputs = cJSON_GetObjectItem(obj, "parallel_outputs"); cJSON_IsNumber(outputs)) {
    hw.parallel_outputs = static_cast<uint8_t>(std::max(1, static_cast<int>(outputs->valuedouble)));
  }
  if (cJSON* dma = cJSON_GetObjectItem(obj, "enable_dma"); cJSON_IsBool(dma)) {
    hw.enable_dma = cJSON_IsTrue(dma);
  }

  hw.segments.clear();
  if (cJSON* arr = cJSON_GetObjectItem(obj, "segments"); cJSON_IsArray(arr)) {
    cJSON* entry = nullptr;
    cJSON_ArrayForEach(entry, arr) {
      if (!cJSON_IsObject(entry)) {
        continue;
      }
      LedSegmentConfig seg{};
      if (cJSON* id = cJSON_GetObjectItem(entry, "id"); cJSON_IsString(id)) seg.id = id->valuestring;
      if (cJSON* name = cJSON_GetObjectItem(entry, "name"); cJSON_IsString(name)) seg.name = name->valuestring;
      if (cJSON* start = cJSON_GetObjectItem(entry, "start_index"); cJSON_IsNumber(start)) {
        seg.start_index = static_cast<uint16_t>(std::max(0, static_cast<int>(start->valuedouble)));
      }
      if (cJSON* order = cJSON_GetObjectItem(entry, "render_order"); cJSON_IsNumber(order)) {
        seg.render_order = static_cast<uint16_t>(std::max(0, static_cast<int>(order->valuedouble)));
      }
      if (cJSON* cnt = cJSON_GetObjectItem(entry, "led_count"); cJSON_IsNumber(cnt)) {
        seg.led_count = static_cast<uint16_t>(std::max(0, static_cast<int>(cnt->valuedouble)));
      }
      if (cJSON* gpio = cJSON_GetObjectItem(entry, "gpio"); cJSON_IsNumber(gpio)) {
        seg.gpio = static_cast<int>(gpio->valuedouble);
      }
      if (cJSON* ch = cJSON_GetObjectItem(entry, "rmt_channel"); cJSON_IsNumber(ch)) {
        seg.rmt_channel = static_cast<uint8_t>(std::max(0, static_cast<int>(ch->valuedouble)));
      }
      if (cJSON* chipset = cJSON_GetObjectItem(entry, "chipset"); cJSON_IsString(chipset)) seg.chipset = chipset->valuestring;
      if (cJSON* order = cJSON_GetObjectItem(entry, "color_order"); cJSON_IsString(order)) seg.color_order = order->valuestring;
      if (cJSON* src = cJSON_GetObjectItem(entry, "effect_source"); cJSON_IsString(src)) {
        seg.effect_source = src->valuestring;
      }
      if (cJSON* ena = cJSON_GetObjectItem(entry, "enabled"); cJSON_IsBool(ena)) seg.enabled = cJSON_IsTrue(ena);
      if (cJSON* rev = cJSON_GetObjectItem(entry, "reverse"); cJSON_IsBool(rev)) seg.reverse = cJSON_IsTrue(rev);
      if (cJSON* mir = cJSON_GetObjectItem(entry, "mirror"); cJSON_IsBool(mir)) seg.mirror = cJSON_IsTrue(mir);
      if (cJSON* gamma_col = cJSON_GetObjectItem(entry, "gamma_color"); cJSON_IsNumber(gamma_col)) {
        seg.gamma_color = static_cast<float>(gamma_col->valuedouble);
      }
      if (cJSON* gamma_br = cJSON_GetObjectItem(entry, "gamma_brightness"); cJSON_IsNumber(gamma_br)) {
        seg.gamma_brightness = static_cast<float>(gamma_br->valuedouble);
      }
      if (cJSON* apply_gamma = cJSON_GetObjectItem(entry, "apply_gamma"); cJSON_IsBool(apply_gamma)) {
        seg.apply_gamma = cJSON_IsTrue(apply_gamma);
      }
      if (cJSON* matrix_en = cJSON_GetObjectItem(entry, "matrix_enabled"); cJSON_IsBool(matrix_en)) {
        seg.matrix_enabled = cJSON_IsTrue(matrix_en);
      }
      if (cJSON* matrix = cJSON_GetObjectItem(entry, "matrix"); cJSON_IsObject(matrix)) {
        decode_matrix(seg.matrix, matrix);
      }
      if (cJSON* limit_ma = cJSON_GetObjectItem(entry, "power_limit_ma"); cJSON_IsNumber(limit_ma)) {
        seg.power_limit_ma = static_cast<uint16_t>(std::max(0, static_cast<int>(limit_ma->valuedouble)));
      }
      if (cJSON* audio = cJSON_GetObjectItem(entry, "audio"); cJSON_IsObject(audio)) {
        decode_segment_audio(seg.audio, audio);
      }
      hw.segments.push_back(std::move(seg));
    }
    // Fallback render order if not provided
    for (size_t i = 0; i < hw.segments.size(); ++i) {
      if (hw.segments[i].render_order == 0 && i > 0) {
        hw.segments[i].render_order = static_cast<uint16_t>(i);
      }
    }
  }
}

void encode_led_segments(const LedHardwareConfig& hw, cJSON* obj) {
  if (!obj) {
    return;
  }
  const std::string driver = led_driver_to_string(hw.driver);
  cJSON_AddStringToObject(obj, "driver", driver.c_str());
  cJSON_AddNumberToObject(obj, "max_fps", hw.max_fps);
  cJSON_AddNumberToObject(obj, "global_current_limit_ma", hw.global_current_limit_ma);
  cJSON_AddNumberToObject(obj, "global_brightness", hw.global_brightness);
  cJSON_AddNumberToObject(obj, "power_supply_voltage", hw.power_supply_voltage);
  cJSON_AddNumberToObject(obj, "power_supply_watts", hw.power_supply_watts);
  cJSON_AddBoolToObject(obj, "auto_power_limit", hw.auto_power_limit);
  cJSON_AddNumberToObject(obj, "parallel_outputs", hw.parallel_outputs);
  cJSON_AddBoolToObject(obj, "enable_dma", hw.enable_dma);

  cJSON* arr = cJSON_AddArrayToObject(obj, "segments");
  if (!arr) {
    return;
  }
  for (const auto& seg : hw.segments) {
    cJSON* s = cJSON_CreateObject();
    if (!s) {
      continue;
    }
    cJSON_AddStringToObject(s, "id", seg.id.c_str());
    cJSON_AddStringToObject(s, "name", seg.name.c_str());
    cJSON_AddNumberToObject(s, "start_index", seg.start_index);
    cJSON_AddNumberToObject(s, "render_order", seg.render_order);
    cJSON_AddNumberToObject(s, "led_count", seg.led_count);
    cJSON_AddNumberToObject(s, "gpio", seg.gpio);
    cJSON_AddNumberToObject(s, "rmt_channel", seg.rmt_channel);
    cJSON_AddStringToObject(s, "chipset", seg.chipset.c_str());
    cJSON_AddStringToObject(s, "color_order", seg.color_order.c_str());
    cJSON_AddStringToObject(s, "effect_source", seg.effect_source.c_str());
    cJSON_AddBoolToObject(s, "enabled", seg.enabled);
    cJSON_AddBoolToObject(s, "reverse", seg.reverse);
    cJSON_AddBoolToObject(s, "mirror", seg.mirror);
    cJSON_AddBoolToObject(s, "matrix_enabled", seg.matrix_enabled);
    if (cJSON* matrix = encode_matrix(seg.matrix)) {
      cJSON_AddItemToObject(s, "matrix", matrix);
    }
    cJSON_AddNumberToObject(s, "power_limit_ma", seg.power_limit_ma);
    cJSON_AddNumberToObject(s, "gamma_color", seg.gamma_color);
    cJSON_AddNumberToObject(s, "gamma_brightness", seg.gamma_brightness);
    cJSON_AddBoolToObject(s, "apply_gamma", seg.apply_gamma);
    if (cJSON* audio = encode_segment_audio(seg.audio)) {
      cJSON_AddItemToObject(s, "audio", audio);
    }
    cJSON_AddItemToArray(arr, s);
  }
}

void decode_audio_config(AudioConfig& audio, cJSON* obj) {
  if (!cJSON_IsObject(obj)) {
    return;
  }
  if (cJSON* source = cJSON_GetObjectItem(obj, "source"); cJSON_IsString(source)) {
    audio.source = audio_source_from_string(source->valuestring);
  }
  if (cJSON* sr = cJSON_GetObjectItem(obj, "sample_rate"); cJSON_IsNumber(sr)) {
    audio.sample_rate = static_cast<uint32_t>(std::max(8000, static_cast<int>(sr->valuedouble)));
  }
  if (cJSON* frame = cJSON_GetObjectItem(obj, "frame_ms"); cJSON_IsNumber(frame)) {
    audio.frame_ms = static_cast<uint16_t>(std::max(5, static_cast<int>(frame->valuedouble)));
  }
  if (cJSON* fft = cJSON_GetObjectItem(obj, "fft_size"); cJSON_IsNumber(fft)) {
    audio.fft_size = static_cast<uint16_t>(std::max(64, static_cast<int>(fft->valuedouble)));
  }
  if (cJSON* stereo = cJSON_GetObjectItem(obj, "stereo"); cJSON_IsBool(stereo)) {
    audio.stereo = cJSON_IsTrue(stereo);
  }
  if (cJSON* sens = cJSON_GetObjectItem(obj, "sensitivity"); cJSON_IsNumber(sens)) {
    audio.sensitivity = static_cast<float>(sens->valuedouble);
  }
  if (cJSON* snapcast = cJSON_GetObjectItem(obj, "snapcast"); cJSON_IsObject(snapcast)) {
    if (cJSON* en = cJSON_GetObjectItem(snapcast, "enabled"); cJSON_IsBool(en)) {
      audio.snapcast.enabled = cJSON_IsTrue(en);
    }
    if (cJSON* host = cJSON_GetObjectItem(snapcast, "host"); cJSON_IsString(host)) {
      audio.snapcast.host = host->valuestring;
    }
    if (cJSON* port = cJSON_GetObjectItem(snapcast, "port"); cJSON_IsNumber(port)) {
      audio.snapcast.port = static_cast<uint16_t>(std::max(0, static_cast<int>(port->valuedouble)));
    }
    if (cJSON* latency = cJSON_GetObjectItem(snapcast, "latency_ms"); cJSON_IsNumber(latency)) {
      audio.snapcast.latency_ms =
          static_cast<uint16_t>(std::max(0, static_cast<int>(latency->valuedouble)));
    }
    if (cJSON* proto = cJSON_GetObjectItem(snapcast, "prefer_udp"); cJSON_IsBool(proto)) {
      audio.snapcast.prefer_udp = cJSON_IsTrue(proto);
    }
  }
}

void encode_audio_config(const AudioConfig& audio, cJSON* obj) {
  if (!obj) {
    return;
  }
  const std::string source = audio_source_to_string(audio.source);
  cJSON_AddStringToObject(obj, "source", source.c_str());
  cJSON_AddNumberToObject(obj, "sample_rate", audio.sample_rate);
  cJSON_AddNumberToObject(obj, "frame_ms", audio.frame_ms);
  cJSON_AddNumberToObject(obj, "fft_size", audio.fft_size);
  cJSON_AddBoolToObject(obj, "stereo", audio.stereo);
  cJSON_AddNumberToObject(obj, "sensitivity", audio.sensitivity);
  cJSON* snap = cJSON_AddObjectToObject(obj, "snapcast");
  if (snap) {
    cJSON_AddBoolToObject(snap, "enabled", audio.snapcast.enabled);
    cJSON_AddStringToObject(snap, "host", audio.snapcast.host.c_str());
    cJSON_AddNumberToObject(snap, "port", audio.snapcast.port);
    cJSON_AddNumberToObject(snap, "latency_ms", audio.snapcast.latency_ms);
    cJSON_AddBoolToObject(snap, "prefer_udp", audio.snapcast.prefer_udp);
  }
}

bool decode_effect_assignment(EffectAssignment& assign, cJSON* entry, bool allow_segment_id) {
  if (!cJSON_IsObject(entry)) {
    return false;
  }
  if (allow_segment_id) {
    if (cJSON* seg = cJSON_GetObjectItem(entry, "segment_id"); cJSON_IsString(seg)) {
      assign.segment_id = seg->valuestring;
    }
  }
  if (cJSON* engine = cJSON_GetObjectItem(entry, "engine"); cJSON_IsString(engine)) {
    assign.engine = engine->valuestring;
  }
  if (cJSON* effect = cJSON_GetObjectItem(entry, "effect"); cJSON_IsString(effect)) {
    assign.effect = effect->valuestring;
  }
  if (cJSON* preset = cJSON_GetObjectItem(entry, "preset"); cJSON_IsString(preset)) {
    assign.preset = preset->valuestring;
  }
  if (cJSON* audio = cJSON_GetObjectItem(entry, "audio_link"); cJSON_IsBool(audio)) {
    assign.audio_link = cJSON_IsTrue(audio);
  }
  if (cJSON* profile = cJSON_GetObjectItem(entry, "audio_profile"); cJSON_IsString(profile)) {
    assign.audio_profile = profile->valuestring;
  }
  if (cJSON* bri = cJSON_GetObjectItem(entry, "brightness"); cJSON_IsNumber(bri)) {
    int value = static_cast<int>(bri->valuedouble);
    assign.brightness = static_cast<uint8_t>(std::clamp(value, 0, 255));
  }
  if (cJSON* inten = cJSON_GetObjectItem(entry, "intensity"); cJSON_IsNumber(inten)) {
    int value = static_cast<int>(inten->valuedouble);
    assign.intensity = static_cast<uint8_t>(std::clamp(value, 0, 255));
  }
  if (cJSON* spd = cJSON_GetObjectItem(entry, "speed"); cJSON_IsNumber(spd)) {
    int value = static_cast<int>(spd->valuedouble);
    assign.speed = static_cast<uint8_t>(std::clamp(value, 0, 255));
  }
  if (cJSON* mode = cJSON_GetObjectItem(entry, "audio_mode"); cJSON_IsString(mode)) {
    assign.audio_mode = mode->valuestring;
  }
  if (cJSON* dir = cJSON_GetObjectItem(entry, "direction"); cJSON_IsString(dir)) {
    assign.direction = dir->valuestring;
  }
  if (cJSON* scatter = cJSON_GetObjectItem(entry, "scatter"); cJSON_IsNumber(scatter)) {
    assign.scatter = static_cast<float>(scatter->valuedouble);
  }
  if (cJSON* fade_in = cJSON_GetObjectItem(entry, "fade_in"); cJSON_IsNumber(fade_in)) {
    assign.fade_in = static_cast<uint16_t>(std::max(0, static_cast<int>(fade_in->valuedouble)));
  }
  if (cJSON* fade_out = cJSON_GetObjectItem(entry, "fade_out"); cJSON_IsNumber(fade_out)) {
    assign.fade_out = static_cast<uint16_t>(std::max(0, static_cast<int>(fade_out->valuedouble)));
  }
  if (cJSON* c1 = cJSON_GetObjectItem(entry, "color1"); cJSON_IsString(c1)) assign.color1 = c1->valuestring;
  if (cJSON* c2 = cJSON_GetObjectItem(entry, "color2"); cJSON_IsString(c2)) assign.color2 = c2->valuestring;
  if (cJSON* c3 = cJSON_GetObjectItem(entry, "color3"); cJSON_IsString(c3)) assign.color3 = c3->valuestring;
  if (cJSON* pal = cJSON_GetObjectItem(entry, "palette"); cJSON_IsString(pal)) assign.palette = pal->valuestring;
  if (cJSON* grad = cJSON_GetObjectItem(entry, "gradient"); cJSON_IsString(grad)) assign.gradient = grad->valuestring;
  if (cJSON* bri_o = cJSON_GetObjectItem(entry, "brightness_override"); cJSON_IsNumber(bri_o)) {
    int value = static_cast<int>(bri_o->valuedouble);
    assign.brightness_override = static_cast<uint8_t>(std::clamp(value, 0, 255));
  }
  if (cJSON* gcol = cJSON_GetObjectItem(entry, "gamma_color"); cJSON_IsNumber(gcol)) {
    assign.gamma_color = static_cast<float>(gcol->valuedouble);
  }
  if (cJSON* gbr = cJSON_GetObjectItem(entry, "gamma_brightness"); cJSON_IsNumber(gbr)) {
    assign.gamma_brightness = static_cast<float>(gbr->valuedouble);
  }
  if (cJSON* blend = cJSON_GetObjectItem(entry, "blend_mode"); cJSON_IsString(blend)) {
    assign.blend_mode = blend->valuestring;
  }
  if (cJSON* layers = cJSON_GetObjectItem(entry, "layers"); cJSON_IsNumber(layers)) {
    int value = static_cast<int>(layers->valuedouble);
    assign.layers = static_cast<uint8_t>(std::clamp(value, 1, 8));
  }
  if (cJSON* reactive = cJSON_GetObjectItem(entry, "reactive_mode"); cJSON_IsString(reactive)) {
    assign.reactive_mode = reactive->valuestring;
  }
  if (cJSON* low = cJSON_GetObjectItem(entry, "band_gain_low"); cJSON_IsNumber(low)) {
    assign.band_gain_low = static_cast<float>(low->valuedouble);
  }
  if (cJSON* mid = cJSON_GetObjectItem(entry, "band_gain_mid"); cJSON_IsNumber(mid)) {
    assign.band_gain_mid = static_cast<float>(mid->valuedouble);
  }
  if (cJSON* high = cJSON_GetObjectItem(entry, "band_gain_high"); cJSON_IsNumber(high)) {
    assign.band_gain_high = static_cast<float>(high->valuedouble);
  }
  if (cJSON* amp = cJSON_GetObjectItem(entry, "amplitude_scale"); cJSON_IsNumber(amp)) {
    assign.amplitude_scale = static_cast<float>(amp->valuedouble);
  }
  if (cJSON* comp = cJSON_GetObjectItem(entry, "brightness_compress"); cJSON_IsNumber(comp)) {
    assign.brightness_compress = static_cast<float>(comp->valuedouble);
  }
  if (cJSON* beat = cJSON_GetObjectItem(entry, "beat_response"); cJSON_IsBool(beat)) {
    assign.beat_response = cJSON_IsTrue(beat);
  }
  if (cJSON* attack = cJSON_GetObjectItem(entry, "attack_ms"); cJSON_IsNumber(attack)) {
    assign.attack_ms = static_cast<uint16_t>(std::max(0, static_cast<int>(attack->valuedouble)));
  }
  if (cJSON* release = cJSON_GetObjectItem(entry, "release_ms"); cJSON_IsNumber(release)) {
    assign.release_ms = static_cast<uint16_t>(std::max(0, static_cast<int>(release->valuedouble)));
  }
  if (cJSON* scene = cJSON_GetObjectItem(entry, "scene_preset"); cJSON_IsString(scene)) {
    assign.scene_preset = scene->valuestring;
  }
  if (cJSON* sched = cJSON_GetObjectItem(entry, "scene_schedule"); cJSON_IsString(sched)) {
    assign.scene_schedule = sched->valuestring;
  }
  if (cJSON* shuffle = cJSON_GetObjectItem(entry, "beat_shuffle"); cJSON_IsBool(shuffle)) {
    assign.beat_shuffle = cJSON_IsTrue(shuffle);
  }
  if (cJSON* freq_min = cJSON_GetObjectItem(entry, "freq_min"); cJSON_IsNumber(freq_min)) {
    assign.freq_min = static_cast<float>(std::max(0.0, freq_min->valuedouble));
  }
  if (cJSON* freq_max = cJSON_GetObjectItem(entry, "freq_max"); cJSON_IsNumber(freq_max)) {
    assign.freq_max = static_cast<float>(std::max(0.0, freq_max->valuedouble));
  }
  if (cJSON* bands = cJSON_GetObjectItem(entry, "selected_bands"); cJSON_IsArray(bands)) {
    assign.selected_bands.clear();
    cJSON* band_item = nullptr;
    cJSON_ArrayForEach(band_item, bands) {
      if (cJSON_IsString(band_item)) {
        assign.selected_bands.push_back(band_item->valuestring);
      }
    }
  }
  return true;
}

cJSON* encode_effect_assignment(const EffectAssignment& assign, bool include_segment_id) {
  cJSON* a = cJSON_CreateObject();
  if (!a) {
    return nullptr;
  }
  if (include_segment_id) {
    cJSON_AddStringToObject(a, "segment_id", assign.segment_id.c_str());
  }
  cJSON_AddStringToObject(a, "engine", assign.engine.c_str());
  cJSON_AddStringToObject(a, "effect", assign.effect.c_str());
  cJSON_AddStringToObject(a, "preset", assign.preset.c_str());
  cJSON_AddBoolToObject(a, "audio_link", assign.audio_link);
  cJSON_AddStringToObject(a, "audio_profile", assign.audio_profile.c_str());
  cJSON_AddNumberToObject(a, "brightness", assign.brightness);
  cJSON_AddNumberToObject(a, "intensity", assign.intensity);
  cJSON_AddNumberToObject(a, "speed", assign.speed);
  cJSON_AddStringToObject(a, "audio_mode", assign.audio_mode.c_str());
  cJSON_AddStringToObject(a, "direction", assign.direction.c_str());
  cJSON_AddNumberToObject(a, "scatter", assign.scatter);
  cJSON_AddNumberToObject(a, "fade_in", assign.fade_in);
  cJSON_AddNumberToObject(a, "fade_out", assign.fade_out);
  cJSON_AddStringToObject(a, "color1", assign.color1.c_str());
  cJSON_AddStringToObject(a, "color2", assign.color2.c_str());
  cJSON_AddStringToObject(a, "color3", assign.color3.c_str());
  cJSON_AddStringToObject(a, "palette", assign.palette.c_str());
  cJSON_AddStringToObject(a, "gradient", assign.gradient.c_str());
  cJSON_AddNumberToObject(a, "brightness_override", assign.brightness_override);
  cJSON_AddNumberToObject(a, "gamma_color", assign.gamma_color);
  cJSON_AddNumberToObject(a, "gamma_brightness", assign.gamma_brightness);
  cJSON_AddStringToObject(a, "blend_mode", assign.blend_mode.c_str());
  cJSON_AddNumberToObject(a, "layers", assign.layers);
  cJSON_AddStringToObject(a, "reactive_mode", assign.reactive_mode.c_str());
  cJSON_AddNumberToObject(a, "band_gain_low", assign.band_gain_low);
  cJSON_AddNumberToObject(a, "band_gain_mid", assign.band_gain_mid);
  cJSON_AddNumberToObject(a, "band_gain_high", assign.band_gain_high);
  cJSON_AddNumberToObject(a, "amplitude_scale", assign.amplitude_scale);
  cJSON_AddNumberToObject(a, "brightness_compress", assign.brightness_compress);
  cJSON_AddBoolToObject(a, "beat_response", assign.beat_response);
  cJSON_AddNumberToObject(a, "attack_ms", assign.attack_ms);
  cJSON_AddNumberToObject(a, "release_ms", assign.release_ms);
  cJSON_AddStringToObject(a, "scene_preset", assign.scene_preset.c_str());
  cJSON_AddStringToObject(a, "scene_schedule", assign.scene_schedule.c_str());
  cJSON_AddBoolToObject(a, "beat_shuffle", assign.beat_shuffle);
  cJSON_AddNumberToObject(a, "freq_min", assign.freq_min);
  cJSON_AddNumberToObject(a, "freq_max", assign.freq_max);
  if (!assign.selected_bands.empty()) {
    cJSON* bands_arr = cJSON_AddArrayToObject(a, "selected_bands");
    if (bands_arr) {
      for (const auto& band : assign.selected_bands) {
        cJSON_AddItemToArray(bands_arr, cJSON_CreateString(band.c_str()));
      }
    }
  }
  return a;
}

void decode_effects(EffectsConfig& effects, cJSON* obj) {
  if (!cJSON_IsObject(obj)) {
    return;
  }
  if (cJSON* def = cJSON_GetObjectItem(obj, "default_engine"); cJSON_IsString(def)) {
    effects.default_engine = def->valuestring;
  }
  effects.assignments.clear();
  if (cJSON* arr = cJSON_GetObjectItem(obj, "assignments"); cJSON_IsArray(arr)) {
    cJSON* entry = nullptr;
    cJSON_ArrayForEach(entry, arr) {
      EffectAssignment assign{};
      if (decode_effect_assignment(assign, entry, true)) {
        effects.assignments.push_back(std::move(assign));
      }
    }
  }
}

void encode_effects(const EffectsConfig& effects, cJSON* obj) {
  if (!obj) {
    return;
  }
  cJSON_AddStringToObject(obj, "default_engine", effects.default_engine.c_str());
  cJSON* arr = cJSON_AddArrayToObject(obj, "assignments");
  if (!arr) {
    return;
  }
  for (const auto& assign : effects.assignments) {
    cJSON* a = encode_effect_assignment(assign, true);
    if (a) {
      cJSON_AddItemToArray(arr, a);
    }
  }
}

void decode_wled_effects(AppConfig& cfg, cJSON* root) {
  if (!root) {
    return;
  }
  cJSON* obj = cJSON_GetObjectItem(root, "wled_effects");
  if (!cJSON_IsObject(obj)) {
    return;
  }
  if (cJSON* fps = cJSON_GetObjectItem(obj, "target_fps"); cJSON_IsNumber(fps)) {
    int value = static_cast<int>(fps->valuedouble);
    cfg.wled_effects.target_fps = static_cast<uint16_t>(std::clamp(value, 1, 240));
  }
  if (cJSON* arr = cJSON_GetObjectItem(obj, "bindings"); cJSON_IsArray(arr)) {
    cfg.wled_effects.bindings.clear();
    cJSON* entry = nullptr;
    cJSON_ArrayForEach(entry, arr) {
      if (!cJSON_IsObject(entry)) {
        continue;
      }
      WledEffectBinding bind{};
      if (cJSON* id = cJSON_GetObjectItem(entry, "device_id"); cJSON_IsString(id)) {
        bind.device_id = id->valuestring;
      }
      if (cJSON* seg = cJSON_GetObjectItem(entry, "segment_index"); cJSON_IsNumber(seg)) {
        bind.segment_index = static_cast<uint16_t>(std::max(0, static_cast<int>(seg->valuedouble)));
      } else if (cJSON* seg_alt = cJSON_GetObjectItem(entry, "segment"); cJSON_IsNumber(seg_alt)) {
        bind.segment_index = static_cast<uint16_t>(std::max(0, static_cast<int>(seg_alt->valuedouble)));
      }
      if (cJSON* ena = cJSON_GetObjectItem(entry, "enabled"); cJSON_IsBool(ena)) {
        bind.enabled = cJSON_IsTrue(ena);
      }
      if (cJSON* ddp = cJSON_GetObjectItem(entry, "ddp"); cJSON_IsBool(ddp)) {
        bind.ddp = cJSON_IsTrue(ddp);
      }
      if (cJSON* ch = cJSON_GetObjectItem(entry, "audio_channel"); cJSON_IsString(ch)) {
        bind.audio_channel = ch->valuestring;
      }
      if (cJSON* fx = cJSON_GetObjectItem(entry, "effect")) {
        decode_effect_assignment(bind.effect, fx, false);
      } else {
        decode_effect_assignment(bind.effect, entry, false);
      }
      if (!bind.device_id.empty()) {
        cfg.wled_effects.bindings.push_back(std::move(bind));
      }
    }
  }
  if (cfg.wled_effects.target_fps == 0) {
    cfg.wled_effects.target_fps = 60;
  }
}

void encode_wled_effects(const AppConfig& cfg, cJSON* root) {
  if (!root) {
    return;
  }
  cJSON* obj = cJSON_AddObjectToObject(root, "wled_effects");
  if (!obj) {
    return;
  }
  cJSON_AddNumberToObject(obj, "target_fps", cfg.wled_effects.target_fps);
  cJSON* arr = cJSON_AddArrayToObject(obj, "bindings");
  if (!arr) {
    return;
  }
  for (const auto& bind : cfg.wled_effects.bindings) {
    cJSON* b = cJSON_CreateObject();
    if (!b) {
      continue;
    }
    cJSON_AddStringToObject(b, "device_id", bind.device_id.c_str());
    cJSON_AddNumberToObject(b, "segment_index", bind.segment_index);
    cJSON_AddBoolToObject(b, "enabled", bind.enabled);
    cJSON_AddBoolToObject(b, "ddp", bind.ddp);
    cJSON_AddStringToObject(b, "audio_channel", bind.audio_channel.c_str());
    if (cJSON* fx = encode_effect_assignment(bind.effect, false)) {
      cJSON_AddItemToObject(b, "effect", fx);
    }
    cJSON_AddItemToArray(arr, b);
  }
}

void decode_led_engine(AppConfig& cfg, cJSON* root) {
  if (!cJSON_IsObject(root)) {
    return;
  }
  decode_led_segments(cfg.led_engine, root);
  if (cJSON* audio = cJSON_GetObjectItem(root, "audio"); cJSON_IsObject(audio)) {
    decode_audio_config(cfg.led_engine.audio, audio);
  }
  if (cJSON* effects = cJSON_GetObjectItem(root, "effects"); cJSON_IsObject(effects)) {
    decode_effects(cfg.led_engine.effects, effects);
  }
}

void encode_led_engine(const AppConfig& cfg, cJSON* root) {
  cJSON* engine = cJSON_AddObjectToObject(root, "led_engine");
  if (!engine) {
    return;
  }
  encode_led_segments(cfg.led_engine, engine);
  cJSON* audio = cJSON_AddObjectToObject(engine, "audio");
  if (audio) {
    encode_audio_config(cfg.led_engine.audio, audio);
  }
  cJSON* effects = cJSON_AddObjectToObject(engine, "effects");
  if (effects) {
    encode_effects(cfg.led_engine.effects, effects);
  }
}

void decode_virtual_segments(AppConfig& cfg, cJSON* root) {
  if (!root) return;
  cJSON* arr = cJSON_GetObjectItem(root, "virtual_segments");
  if (!cJSON_IsArray(arr)) {
    return;
  }
  cfg.virtual_segments.clear();
  cJSON* entry = nullptr;
  cJSON_ArrayForEach(entry, arr) {
    if (!cJSON_IsObject(entry)) continue;
    VirtualSegmentConfig seg{};
    if (cJSON* id = cJSON_GetObjectItem(entry, "id"); cJSON_IsString(id)) seg.id = id->valuestring;
    if (cJSON* name = cJSON_GetObjectItem(entry, "name"); cJSON_IsString(name)) seg.name = name->valuestring;
    if (cJSON* enabled = cJSON_GetObjectItem(entry, "enabled"); cJSON_IsBool(enabled)) seg.enabled = cJSON_IsTrue(enabled);
    if (cJSON* fx = cJSON_GetObjectItem(entry, "effect")) {
      decode_effect_assignment(seg.effect, fx, false);
    }
    if (cJSON* members = cJSON_GetObjectItem(entry, "members"); cJSON_IsArray(members)) {
      cJSON* m = nullptr;
      cJSON_ArrayForEach(m, members) {
        if (!cJSON_IsObject(m)) continue;
        VirtualSegmentMember mem{};
        if (cJSON* type = cJSON_GetObjectItem(m, "type"); cJSON_IsString(type)) mem.type = type->valuestring;
        if (cJSON* ref = cJSON_GetObjectItem(m, "id"); cJSON_IsString(ref)) mem.id = ref->valuestring;
        if (cJSON* seg_idx = cJSON_GetObjectItem(m, "segment_index"); cJSON_IsNumber(seg_idx)) {
          mem.segment_index = static_cast<uint16_t>(std::max(0, static_cast<int>(seg_idx->valuedouble)));
        }
        if (cJSON* start = cJSON_GetObjectItem(m, "start"); cJSON_IsNumber(start)) {
          mem.start = static_cast<uint16_t>(std::max(0, static_cast<int>(start->valuedouble)));
        }
        if (cJSON* len = cJSON_GetObjectItem(m, "length"); cJSON_IsNumber(len)) {
          mem.length = static_cast<uint16_t>(std::max(0, static_cast<int>(len->valuedouble)));
        }
        seg.members.push_back(mem);
      }
    }
    if (seg.id.empty()) {
      continue;
    }
    cfg.virtual_segments.push_back(std::move(seg));
  }
}

void encode_virtual_segments(const AppConfig& cfg, cJSON* root) {
  if (!root) return;
  cJSON* arr = cJSON_AddArrayToObject(root, "virtual_segments");
  if (!arr) return;
  for (const auto& seg : cfg.virtual_segments) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj) continue;
    cJSON_AddStringToObject(obj, "id", seg.id.c_str());
    cJSON_AddStringToObject(obj, "name", seg.name.c_str());
    cJSON_AddBoolToObject(obj, "enabled", seg.enabled);
    if (cJSON* fx = encode_effect_assignment(seg.effect, false)) {
      cJSON_AddItemToObject(obj, "effect", fx);
    }
    cJSON* members = cJSON_AddArrayToObject(obj, "members");
    if (members) {
      for (const auto& mem : seg.members) {
        cJSON* m = cJSON_CreateObject();
        if (!m) continue;
        cJSON_AddStringToObject(m, "type", mem.type.c_str());
        cJSON_AddStringToObject(m, "id", mem.id.c_str());
        cJSON_AddNumberToObject(m, "segment_index", mem.segment_index);
        cJSON_AddNumberToObject(m, "start", mem.start);
        cJSON_AddNumberToObject(m, "length", mem.length);
        cJSON_AddItemToArray(members, m);
      }
    }
    cJSON_AddItemToArray(arr, obj);
  }
}

}  // namespace

esp_err_t config_service_init() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS needs erase, resetting");
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
  }
  return err;
}

void config_reset_defaults(AppConfig& cfg) {
  const char* begin = reinterpret_cast<const char*>(_binary_config_json_start);
  const char* end = reinterpret_cast<const char*>(_binary_config_json_end);
  cfg = AppConfig{};
  decode_json(cfg, begin, end - begin);
  cfg.schema_version = CURRENT_SCHEMA;
}

esp_err_t config_load(AppConfig& cfg) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGI(TAG, "Config not found, using defaults");
    config_reset_defaults(cfg);
    return ESP_OK;
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
    return err;
  }

  size_t required = 0;
  err = nvs_get_blob(handle, KEY_BLOB, nullptr, &required);
  if (err == ESP_ERR_NVS_NOT_FOUND || required == 0) {
    ESP_LOGW(TAG, "Config blob empty, using defaults");
    config_reset_defaults(cfg);
    nvs_close(handle);
    return ESP_OK;
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_get_blob failed: %s", esp_err_to_name(err));
    nvs_close(handle);
    return err;
  }

  std::string blob;
  blob.resize(required);
  err = nvs_get_blob(handle, KEY_BLOB, blob.data(), &required);
  nvs_close(handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_get_blob read failed: %s", esp_err_to_name(err));
    return err;
  }

  decode_json(cfg, blob.data(), blob.size());

  if (cfg.schema_version < CURRENT_SCHEMA) {
    ESP_LOGI(TAG, "Schema upgrade %u -> %u", cfg.schema_version, CURRENT_SCHEMA);
    cfg.schema_version = CURRENT_SCHEMA;
    ESP_ERROR_CHECK(config_save(cfg));
  } else if (cfg.schema_version > CURRENT_SCHEMA) {
    ESP_LOGW(TAG, "Schema downgrade detected (%u > %u), resetting defaults", cfg.schema_version, CURRENT_SCHEMA);
    config_reset_defaults(cfg);
    ESP_ERROR_CHECK(config_save(cfg));
  }

  return ESP_OK;
}

esp_err_t config_save(const AppConfig& cfg) {
  std::string blob = encode_json(cfg);

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open write failed: %s", esp_err_to_name(err));
    return err;
  }

  err = nvs_set_blob(handle, KEY_BLOB, blob.data(), blob.size());
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  } else {
    ESP_LOGE(TAG, "nvs_set_blob failed: %s", esp_err_to_name(err));
  }
  nvs_close(handle);
  return err;
}

esp_err_t config_apply_json(AppConfig& cfg, const char* data, size_t len) {
  if (!data || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  return decode_json(cfg, data, len) ? ESP_OK : ESP_FAIL;
}

std::string config_to_json(const AppConfig& cfg) {
  return encode_json(cfg);
}
