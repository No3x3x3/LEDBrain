#include "cJSON.h"
#include "esp_timer.h"
#include "web_setup.hpp"
#include "esp_http_server.h"
#include "esp_log.h"
#include "eth_init.hpp"
#include "config.hpp"
#include "mdns.h"
#include "wled_discovery.hpp"
#include "led_engine.hpp"
#include "led_engine/pinout.hpp"
#include "led_engine/audio_pipeline.hpp"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "ota.hpp"
#include <string>
#include <algorithm>
#include <cctype>

extern const unsigned char index_html_start[] asm("_binary_index_html_start");
extern const unsigned char index_html_end[]   asm("_binary_index_html_end");
extern const unsigned char app_js_start[] asm("_binary_app_js_start");
extern const unsigned char app_js_end[]   asm("_binary_app_js_end");
extern const unsigned char style_css_start[] asm("_binary_style_css_start");
extern const unsigned char style_css_end[]   asm("_binary_style_css_end");
extern const unsigned char lang_pl_json_start[] asm("_binary_lang_pl_json_start");
extern const unsigned char lang_pl_json_end[]   asm("_binary_lang_pl_json_end");
extern const unsigned char lang_en_json_start[] asm("_binary_lang_en_json_start");
extern const unsigned char lang_en_json_end[]   asm("_binary_lang_en_json_end");

static const char* TAG="web";

static esp_err_t send_mem(httpd_req_t* req, const unsigned char* begin, const unsigned char* end, const char* type){
  httpd_resp_set_type(req, type);
  return httpd_resp_send(req, (const char*)begin, end-begin);
}

static AppConfig* s_cfg = nullptr;
static LedEngineRuntime* s_led_runtime = nullptr;

static esp_err_t root_get(httpd_req_t* req){ return send_mem(req, index_html_start, index_html_end, "text/html"); }
static esp_err_t js_get(httpd_req_t* req){ return send_mem(req, app_js_start, app_js_end, "application/javascript"); }
static esp_err_t css_get(httpd_req_t* req){ return send_mem(req, style_css_start, style_css_end, "text/css"); }
static esp_err_t pl_get(httpd_req_t* req){ return send_mem(req, lang_pl_json_start, lang_pl_json_end, "application/json"); }
static esp_err_t en_get(httpd_req_t* req){ return send_mem(req, lang_en_json_start, lang_en_json_end, "application/json"); }

static esp_err_t api_get_config(httpd_req_t* req){
  // return current configuration snapshot from NVS
  std::string json = config_to_json(*s_cfg);
  httpd_resp_set_type(req,"application/json");
  return httpd_resp_send(req, json.data(), json.size());
}

static std::string read_body(httpd_req_t* req){
  int total = req->content_len; std::string body; body.resize(total);
  int off=0; while(off<total){ int r = httpd_req_recv(req, &body[off], total-off); if (r<=0) break; off+=r; }
  body.resize(off); return body;
}

static std::string trim_copy(const std::string& value) {
  auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
  auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
  if (begin >= end) {
    return {};
  }
  return std::string(begin, end);
}

static esp_err_t api_save_network(httpd_req_t* req){
  auto body = read_body(req);
  cJSON* root = cJSON_ParseWithLength(body.c_str(), body.size());
  if (!root) return httpd_resp_send_500(req);
  cJSON* net = cJSON_GetObjectItem(root,"network");
  if (cJSON_IsObject(net)){
    cJSON* dh = cJSON_GetObjectItem(net,"use_dhcp");
    cJSON* hn = cJSON_GetObjectItem(net,"hostname");
    cJSON* ip = cJSON_GetObjectItem(net,"static_ip");
    cJSON* sn = cJSON_GetObjectItem(net,"netmask");
    cJSON* gw = cJSON_GetObjectItem(net,"gateway");
    cJSON* dns= cJSON_GetObjectItem(net,"dns");
    if (cJSON_IsBool(dh)) s_cfg->network.use_dhcp = cJSON_IsTrue(dh);
    if (cJSON_IsString(hn)) s_cfg->network.hostname = hn->valuestring;
    if (cJSON_IsString(ip)) s_cfg->network.static_ip = ip->valuestring;
    if (cJSON_IsString(sn)) {
      s_cfg->network.netmask = sn->valuestring;
    } else if (cJSON* legacy = cJSON_GetObjectItem(net, "subnet"); cJSON_IsString(legacy)) {
      s_cfg->network.netmask = legacy->valuestring;
    }
    if (cJSON_IsString(gw)) s_cfg->network.gateway = gw->valuestring;
    if (cJSON_IsString(dns)) s_cfg->network.dns = dns->valuestring;
    if (config_save(*s_cfg) != ESP_OK) {
      cJSON_Delete(root);
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to persist network config");
    }
  }
  cJSON_Delete(root);
  httpd_resp_sendstr(req,"OK");
  return ESP_OK;
}

static esp_err_t api_save_mqtt(httpd_req_t* req){
  auto body = read_body(req);
  cJSON* root = cJSON_ParseWithLength(body.c_str(), body.size());
  if (!root) return httpd_resp_send_500(req);
  cJSON* mq = cJSON_GetObjectItem(root,"mqtt");
  if (cJSON_IsObject(mq)){
    bool enabled = s_cfg->mqtt.configured;
    cJSON* en = cJSON_GetObjectItem(mq,"enabled");
    if (cJSON_IsBool(en)) {
      enabled = cJSON_IsTrue(en);
    }
    cJSON* ho=cJSON_GetObjectItem(mq,"host");
    cJSON* pt=cJSON_GetObjectItem(mq,"port");
    cJSON* un=cJSON_GetObjectItem(mq,"username");
    cJSON* pw=cJSON_GetObjectItem(mq,"password");
    cJSON* dt=cJSON_GetObjectItem(mq,"ddp_target");
    cJSON* dp=cJSON_GetObjectItem(mq,"ddp_port");
    if (cJSON_IsString(ho)) s_cfg->mqtt.host = ho->valuestring;
    if (cJSON_IsNumber(pt)) s_cfg->mqtt.port = pt->valueint;
    if (cJSON_IsString(un)) s_cfg->mqtt.username = un->valuestring;
    if (cJSON_IsString(pw)) s_cfg->mqtt.password = pw->valuestring;
    if (cJSON_IsString(dt)) s_cfg->mqtt.ddp_target = dt->valuestring;
    if (cJSON_IsNumber(dp)) s_cfg->mqtt.ddp_port = dp->valueint;
    s_cfg->mqtt.configured = enabled || (!cJSON_IsBool(en) && !s_cfg->mqtt.host.empty());
    if (config_save(*s_cfg) != ESP_OK) {
      cJSON_Delete(root);
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to persist mqtt config");
    }
  }
  cJSON_Delete(root);
  httpd_resp_sendstr(req,"OK");
  return ESP_OK;
}

static esp_err_t api_save_system(httpd_req_t* req) {
  auto body = read_body(req);
  cJSON* root = cJSON_ParseWithLength(body.c_str(), body.size());
  if (!root) return httpd_resp_send_500(req);
  bool updated = false;
  if (cJSON* lang = cJSON_GetObjectItem(root, "lang"); cJSON_IsString(lang)) {
    s_cfg->lang = lang->valuestring;
    updated = true;
  }
  if (cJSON* autostart = cJSON_GetObjectItem(root, "autostart"); cJSON_IsBool(autostart)) {
    s_cfg->autostart = cJSON_IsTrue(autostart);
    updated = true;
  }
  cJSON_Delete(root);
  if (updated) {
    if (config_save(*s_cfg) != ESP_OK) {
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to persist system config");
    }
  }
  httpd_resp_sendstr(req, "OK");
  return ESP_OK;
}

static esp_err_t api_factory_reset(httpd_req_t* req) {
  config_reset_defaults(*s_cfg);
  if (config_save(*s_cfg) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to reset config");
  }
  httpd_resp_sendstr(req, "OK");
  vTaskDelay(pdMS_TO_TICKS(800));
  esp_restart();
  return ESP_OK;
}

static esp_err_t api_info(httpd_req_t* req){
  // proste info JSON
  cJSON* root = cJSON_CreateObject();
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_app_desc_t desc{};
  if (esp_ota_get_partition_description(running, &desc) == ESP_OK) {
    cJSON_AddStringToObject(root, "fw_name", desc.project_name);
    cJSON_AddStringToObject(root, "fw_version", desc.version);
    cJSON_AddStringToObject(root, "fw_build_date", desc.date);
    cJSON_AddStringToObject(root, "fw_build_time", desc.time);
  }
  cJSON_AddStringToObject(root, "fw", "LEDBrain");
  cJSON_AddStringToObject(root, "idf", IDF_VER);
  cJSON_AddStringToObject(root,"hostname", s_cfg->network.hostname.c_str());
  cJSON_AddStringToObject(root, "ota_state", ota_state_string());
  if (ota_last_error() != ESP_OK) {
    cJSON_AddStringToObject(root, "ota_error", esp_err_to_name(ota_last_error()));
  }
  // IP
  char ip[16]="0.0.0.0";
  esp_netif_ip_info_t info;
  extern esp_netif_t *netif;
  if (esp_netif_get_ip_info(netif, &info) == ESP_OK) {
    snprintf(ip,sizeof(ip), IPSTR, IP2STR(&info.ip));
  }
  cJSON_AddStringToObject(root,"ip", ip);
  // uptime
  uint64_t us = esp_timer_get_time();
  cJSON_AddNumberToObject(root,"uptime_s", (int)(us/1000000ULL));
  // MQTT
  cJSON_AddBoolToObject(root,"mqtt_configured", s_cfg->mqtt.configured);
  cJSON_AddStringToObject(root,"ddp_target", s_cfg->mqtt.ddp_target.c_str());
  if (s_led_runtime) {
    const LedEngineStatus st = s_led_runtime->status();
    cJSON* led = cJSON_AddObjectToObject(root, "led_engine");
    if (led) {
      cJSON_AddBoolToObject(led, "initialized", st.initialized);
      cJSON_AddBoolToObject(led, "enabled", st.enabled);
      cJSON_AddNumberToObject(led, "target_fps", st.target_fps);
      cJSON_AddNumberToObject(led, "segments", static_cast<double>(st.segment_count));
      cJSON_AddNumberToObject(led, "current_ma", st.global_current_ma);
      AudioDiagnostics diag = led_audio_get_diagnostics();
      cJSON* audio = cJSON_AddObjectToObject(led, "audio");
      if (audio) {
        cJSON_AddBoolToObject(audio, "running", diag.running);
        cJSON_AddBoolToObject(audio, "stereo", diag.stereo);
        cJSON_AddNumberToObject(audio, "sample_rate", diag.sample_rate);
        cJSON_AddStringToObject(audio,
                                "source",
                                diag.source == AudioSourceType::Snapcast
                                    ? "snapcast"
                                    : (diag.source == AudioSourceType::LineInput ? "line_in" : "none"));
      }
    }
  }
  char* txt = cJSON_PrintUnformatted(root);
  httpd_resp_set_type(req,"application/json");
  httpd_resp_sendstr(req, txt);
  cJSON_free(txt); cJSON_Delete(root);
  return ESP_OK;
}

static esp_err_t api_reboot(httpd_req_t* req){
  httpd_resp_sendstr(req,"OK");
  vTaskDelay(pdMS_TO_TICKS(1000));
  esp_restart();
  return ESP_OK;
}

static cJSON* wled_device_to_json(const WledDeviceStatus& dev) {
  cJSON* obj = cJSON_CreateObject();
  if (!obj) {
    return nullptr;
  }
  cJSON_AddStringToObject(obj, "id", dev.config.id.c_str());
  cJSON_AddStringToObject(obj, "name", dev.config.name.c_str());
  cJSON_AddStringToObject(obj, "address", dev.config.address.c_str());
  cJSON_AddNumberToObject(obj, "leds", dev.config.leds);
  cJSON_AddNumberToObject(obj, "segments", dev.config.segments);
  cJSON_AddBoolToObject(obj, "active", dev.config.active);
  cJSON_AddBoolToObject(obj, "auto_discovered", dev.config.auto_discovered);
  cJSON_AddBoolToObject(obj, "online", dev.online);
  cJSON_AddStringToObject(obj, "ip", dev.ip.c_str());
  cJSON_AddStringToObject(obj, "version", dev.version.c_str());
  cJSON_AddBoolToObject(obj, "http_verified", dev.http_verified);
  uint64_t now_ms = esp_timer_get_time() / 1000ULL;
  uint64_t age_ms = 0;
  if (dev.last_seen_ms > 0 && now_ms >= dev.last_seen_ms) {
    age_ms = now_ms - dev.last_seen_ms;
  }
  cJSON_AddNumberToObject(obj, "last_seen_ms", static_cast<double>(dev.last_seen_ms));
  cJSON_AddNumberToObject(obj, "age_ms", static_cast<double>(age_ms));
  return obj;
}

static esp_err_t api_wled_list(httpd_req_t* req) {
  auto devices = wled_discovery_status();
  cJSON* root = cJSON_CreateObject();
  cJSON* arr = cJSON_AddArrayToObject(root, "devices");
  for (const auto& dev : devices) {
    cJSON* obj = wled_device_to_json(dev);
    if (obj) {
      cJSON_AddItemToArray(arr, obj);
    }
  }
  char* txt = cJSON_PrintUnformatted(root);
  if (!txt) {
    cJSON_Delete(root);
    return httpd_resp_send_500(req);
  }
  httpd_resp_set_type(req,"application/json");
  httpd_resp_sendstr(req, txt);
  cJSON_free(txt);
  cJSON_Delete(root);
  return ESP_OK;
}

static esp_err_t api_wled_rescan(httpd_req_t* req) {
  wled_discovery_trigger_scan();
  httpd_resp_sendstr(req,"OK");
  return ESP_OK;
}

static esp_err_t api_wled_add(httpd_req_t* req) {
  auto body = read_body(req);
  if (body.empty()) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty payload");
  }
  cJSON* root = cJSON_ParseWithLength(body.c_str(), body.size());
  if (!root) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
  }
  std::string name;
  std::string address;
  uint16_t leds = 0;
  uint16_t segments = 1;
  if (cJSON* n = cJSON_GetObjectItem(root, "name"); cJSON_IsString(n)) {
    name = trim_copy(n->valuestring);
  }
  if (cJSON* addr = cJSON_GetObjectItem(root, "address"); cJSON_IsString(addr)) {
    address = trim_copy(addr->valuestring);
  }
  if (cJSON* l = cJSON_GetObjectItem(root, "leds"); cJSON_IsNumber(l)) {
    int raw = l->valueint;
    if (raw > 0) {
      leds = static_cast<uint16_t>(raw);
    }
  }
  if (cJSON* s = cJSON_GetObjectItem(root, "segments"); cJSON_IsNumber(s)) {
    int raw = s->valueint;
    if (raw > 0) {
      segments = static_cast<uint16_t>(raw);
    }
  }
  cJSON_Delete(root);

  if (address.empty()) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "address required");
  }
  WledDeviceConfig entry{};
  entry.address = address;
  entry.name = !name.empty() ? name : address;
  entry.id = address;
  entry.leds = leds;
  entry.segments = segments == 0 ? 1 : segments;
  entry.active = true;
  entry.auto_discovered = false;

  auto& devices = s_cfg->wled_devices;
  auto it = std::find_if(devices.begin(), devices.end(), [&](const WledDeviceConfig& cfg) {
    return (!cfg.id.empty() && cfg.id == entry.id) ||
           (!cfg.address.empty() && cfg.address == entry.address) ||
           (!cfg.name.empty() && cfg.name == entry.name);
  });
  if (it != devices.end()) {
    it->name = entry.name;
    it->address = entry.address;
    it->leds = entry.leds;
    it->segments = entry.segments;
    it->active = true;
    it->auto_discovered = false;
    if (it->id.empty()) {
      it->id = entry.id;
    }
  } else {
    devices.push_back(entry);
  }
  if (config_save(*s_cfg) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "persist failed");
  }
  wled_discovery_register_manual(entry);
  wled_discovery_trigger_scan();
  httpd_resp_sendstr(req, "OK");
  return ESP_OK;
}

static esp_err_t api_led_pins(httpd_req_t* req) {
  cJSON* arr = cJSON_CreateArray();
  for (const auto& pin : led_pin_catalog()) {
    cJSON* obj = cJSON_CreateObject();
    if (!obj) {
      continue;
    }
    cJSON_AddNumberToObject(obj, "gpio", pin.gpio);
    cJSON_AddStringToObject(obj, "label", pin.label ? pin.label : "");
    cJSON_AddStringToObject(obj, "function", pin.function ? pin.function : "");
    cJSON_AddBoolToObject(obj, "supports_rmt", pin.supports_rmt);
    cJSON_AddBoolToObject(obj, "supports_dma", pin.supports_dma);
    cJSON_AddBoolToObject(obj, "reserved", pin.reserved);
    cJSON_AddBoolToObject(obj, "strapping", pin.strapping);
    cJSON_AddStringToObject(obj, "note", pin.note ? pin.note : "");
    cJSON_AddBoolToObject(obj, "allowed", led_pin_is_allowed(pin.gpio));
    cJSON_AddItemToArray(arr, obj);
  }
  char* txt = cJSON_PrintUnformatted(arr);
  if (!txt) {
    cJSON_Delete(arr);
    return httpd_resp_send_500(req);
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, txt);
  cJSON_free(txt);
  cJSON_Delete(arr);
  return ESP_OK;
}

static esp_err_t api_save_led(httpd_req_t* req) {
  auto body = read_body(req);
  if (body.empty()) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty payload");
  }
  if (config_apply_json(*s_cfg, body.c_str(), body.size()) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
  }
  if (config_save(*s_cfg) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "persist failed");
  }
  if (s_led_runtime) {
    s_led_runtime->update_config(s_cfg->led_engine);
  }
  httpd_resp_sendstr(req, "OK");
  return ESP_OK;
}

static esp_err_t api_led_state_get(httpd_req_t* req) {
  cJSON* root = cJSON_CreateObject();
  if (!root) {
    return httpd_resp_send_500(req);
  }
  const bool enabled = s_led_runtime ? s_led_runtime->enabled() : (s_cfg ? s_cfg->autostart : false);
  cJSON_AddBoolToObject(root, "enabled", enabled);
  cJSON_AddBoolToObject(root, "autostart", s_cfg ? s_cfg->autostart : false);
  char* txt = cJSON_PrintUnformatted(root);
  if (!txt) {
    cJSON_Delete(root);
    return httpd_resp_send_500(req);
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, txt);
  cJSON_free(txt);
  cJSON_Delete(root);
  return ESP_OK;
}

static esp_err_t api_led_state_post(httpd_req_t* req) {
  auto body = read_body(req);
  cJSON* root = cJSON_ParseWithLength(body.c_str(), body.size());
  if (!root) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
  }
  cJSON* enabled = cJSON_GetObjectItem(root, "enabled");
  if (!cJSON_IsBool(enabled)) {
    cJSON_Delete(root);
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "enabled missing");
  }
  const bool value = cJSON_IsTrue(enabled);
  if (s_led_runtime) {
    s_led_runtime->set_enabled(value);
  }
  bool remember = false;
  if (cJSON* rem = cJSON_GetObjectItem(root, "remember"); cJSON_IsBool(rem)) {
    remember = cJSON_IsTrue(rem);
  }
  cJSON_Delete(root);
  if (remember && s_cfg) {
    s_cfg->autostart = value;
    if (config_save(*s_cfg) != ESP_OK) {
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to store autostart");
    }
  }
  httpd_resp_sendstr(req, "OK");
  return ESP_OK;
}

static esp_err_t api_ota(httpd_req_t* req) {
  std::string url = kDefaultOtaUrl;
  auto body = read_body(req);
  if (!body.empty()) {
    cJSON* root = cJSON_ParseWithLength(body.c_str(), body.size());
    if (root) {
      if (cJSON* u = cJSON_GetObjectItem(root, "url"); cJSON_IsString(u) && u->valuestring) {
        url = u->valuestring;
      }
      cJSON_Delete(root);
    }
  }
  const esp_err_t st = ota_trigger_update(url.c_str());
  if (st == ESP_ERR_INVALID_STATE) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA already in progress");
  }
  if (st != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to start OTA");
  }
  httpd_resp_sendstr(req, "OTA started");
  return ESP_OK;
}

void start_web_server(AppConfig& cfg, LedEngineRuntime* runtime){
  s_cfg = &cfg;
  s_led_runtime = runtime;
  httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
  server_config.uri_match_fn = httpd_uri_match_wildcard;
  server_config.max_uri_handlers = 20;
  httpd_handle_t server = nullptr;
  ESP_ERROR_CHECK(httpd_start(&server, &server_config));

  httpd_uri_t u_root = { .uri="/", .method=HTTP_GET, .handler=root_get, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_root);
  httpd_uri_t u_js = { .uri="/app.js", .method=HTTP_GET, .handler=js_get, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_js);
  httpd_uri_t u_css = { .uri="/style.css", .method=HTTP_GET, .handler=css_get, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_css);
  httpd_uri_t u_pl = { .uri="/lang/pl.json", .method=HTTP_GET, .handler=pl_get, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_pl);
  httpd_uri_t u_en = { .uri="/lang/en.json", .method=HTTP_GET, .handler=en_get, .user_ctx=NULL };

  httpd_register_uri_handler(server, &u_en);
  httpd_uri_t u_getcfg = { .uri="/api/get_config", .method=HTTP_GET, .handler=api_get_config, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_getcfg);
  httpd_uri_t u_save_net = { .uri="/api/save_network", .method=HTTP_POST, .handler=api_save_network, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_save_net);
  httpd_uri_t u_save_mqtt = { .uri="/api/save_mqtt", .method=HTTP_POST, .handler=api_save_mqtt, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_save_mqtt);
  httpd_uri_t u_save_system = { .uri="/api/save_system", .method=HTTP_POST, .handler=api_save_system, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_save_system);
  httpd_uri_t u_factory_reset = { .uri="/api/factory_reset", .method=HTTP_POST, .handler=api_factory_reset, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_factory_reset);
  httpd_uri_t u_info = { .uri="/api/info", .method=HTTP_GET, .handler=api_info, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_info);
  httpd_uri_t u_reboot = { .uri="/api/reboot", .method=HTTP_POST, .handler=api_reboot, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_reboot);
  httpd_uri_t u_wled_list = { .uri="/api/wled/list", .method=HTTP_GET, .handler=api_wled_list, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_wled_list);
  httpd_uri_t u_wled_add = { .uri="/api/wled/add", .method=HTTP_POST, .handler=api_wled_add, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_wled_add);
  httpd_uri_t u_wled_rescan = { .uri="/api/wled/rescan", .method=HTTP_POST, .handler=api_wled_rescan, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_wled_rescan);
  httpd_uri_t u_led_pins = { .uri="/api/led/pins", .method=HTTP_GET, .handler=api_led_pins, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_led_pins);
  httpd_uri_t u_led_save = { .uri="/api/led/save", .method=HTTP_POST, .handler=api_save_led, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_led_save);
  httpd_uri_t u_led_state_get = { .uri="/api/led/state", .method=HTTP_GET, .handler=api_led_state_get, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_led_state_get);
  httpd_uri_t u_led_state_post = { .uri="/api/led/state", .method=HTTP_POST, .handler=api_led_state_post, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_led_state_post);
  httpd_uri_t u_ota = { .uri="/api/ota/update", .method=HTTP_POST, .handler=api_ota, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_ota);

  ESP_LOGI(TAG,"Web server started");
}
