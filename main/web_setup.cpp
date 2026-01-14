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
#include "wled_effects.hpp"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "ota.hpp"
#include "mqtt_ha.hpp"
#include "snapclient_light.hpp"
#include "wifi_c6_ctrl.hpp"
#include "wifi_c6.hpp"
#include "ddp_tx.hpp"
#include "temperature_monitor.hpp"
#include "esp_wifi_types.h"
#include "esp_netif.h"
#include "lwip/netif.h"
#include <string>
#include <algorithm>
#include <cctype>
#include <vector>

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
  // Disable caching for static files to ensure updates are visible
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  httpd_resp_set_hdr(req, "Pragma", "no-cache");
  httpd_resp_set_hdr(req, "Expires", "0");
  return httpd_resp_send(req, (const char*)begin, end-begin);
}

static AppConfig* s_cfg = nullptr;
static LedEngineRuntime* s_led_runtime = nullptr;
static WledEffectsRuntime* s_wled_fx_runtime = nullptr;

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

// WiFi scan via ESP32-C6 coprocessor control protocol
static esp_err_t api_wifi_scan(httpd_req_t* req) {
  if (!wifi_c6_ctrl_is_available()) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "WiFi control not available");
  }

  std::vector<WifiScanResult> results;
  esp_err_t ret = wifi_c6_ctrl_scan(results);
  
  if (ret != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "WiFi scan failed");
  }

  cJSON *root = cJSON_CreateObject();
  cJSON *networks = cJSON_CreateArray();
  
  for (const auto& net : results) {
    cJSON *net_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(net_obj, "ssid", net.ssid.c_str());
    cJSON_AddNumberToObject(net_obj, "rssi", net.rssi);
    cJSON_AddNumberToObject(net_obj, "channel", net.channel);
    cJSON_AddBoolToObject(net_obj, "secure", net.authmode != WIFI_AUTH_OPEN);
    cJSON_AddItemToArray(networks, net_obj);
  }
  
  cJSON_AddItemToObject(root, "networks", networks);
  cJSON_AddNumberToObject(root, "count", results.size());
  
  char *json_str = cJSON_Print(root);
  httpd_resp_set_type(req, "application/json");
  esp_err_t http_ret = httpd_resp_send(req, json_str, strlen(json_str));
  free(json_str);
  cJSON_Delete(root);
  
  return http_ret;
}

static esp_err_t api_wifi_connect(httpd_req_t* req) {
  if (!wifi_c6_ctrl_is_available()) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "WiFi control not available");
  }

  char content[512];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
  }
  content[ret] = '\0';

  cJSON *json = cJSON_Parse(content);
  if (!json) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
  }

  cJSON *ssid_json = cJSON_GetObjectItem(json, "ssid");
  cJSON *password_json = cJSON_GetObjectItem(json, "password");

  if (!ssid_json || !cJSON_IsString(ssid_json)) {
    cJSON_Delete(json);
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid SSID");
  }

  std::string ssid = cJSON_GetStringValue(ssid_json);
  std::string password = password_json && cJSON_IsString(password_json) 
                         ? cJSON_GetStringValue(password_json) 
                         : "";

  cJSON_Delete(json);

  ESP_LOGI("web", "WiFi connect request: SSID=%s", ssid.c_str());
  
  esp_err_t wifi_ret = wifi_c6_sta_start(ssid, password);
  
  cJSON *response = cJSON_CreateObject();
  if (wifi_ret == ESP_OK) {
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "WiFi connection initiated");
    
    // Save to config
    if (s_cfg) {
      s_cfg->network.wifi_enabled = true;
      s_cfg->network.wifi_ssid = ssid;
      s_cfg->network.wifi_password = password;
      config_save(*s_cfg);
    }
  } else {
    cJSON_AddBoolToObject(response, "success", false);
    cJSON_AddStringToObject(response, "message", "Failed to initiate WiFi connection");
  }
  
  char *json_str = cJSON_Print(response);
  httpd_resp_set_type(req, "application/json");
  esp_err_t http_ret = httpd_resp_send(req, json_str, strlen(json_str));
  free(json_str);
  cJSON_Delete(response);
  
  return http_ret;
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
    if (s_wled_fx_runtime) {
      s_wled_fx_runtime->update_config(*s_cfg);
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
  // IP - check Ethernet first, then WiFi STA, then WiFi AP
  char ip[16]="0.0.0.0";
  esp_netif_ip_info_t info;
  extern esp_netif_t *netif;
  bool has_eth_ip = false;
  bool eth_connected = false;
  if (netif && esp_netif_get_ip_info(netif, &info) == ESP_OK && info.ip.addr != 0) {
    snprintf(ip,sizeof(ip), IPSTR, IP2STR(&info.ip));
    has_eth_ip = true;
    eth_connected = true;
  }
  
  // Check WiFi IP via ESP32-C6 coprocessor (PPP)
  if (!has_eth_ip) {
    std::string wifi_ip = wifi_c6_get_ip();
    if (!wifi_ip.empty()) {
      strncpy(ip, wifi_ip.c_str(), sizeof(ip) - 1);
      ip[sizeof(ip) - 1] = '\0';
    }
  }
  
  cJSON_AddStringToObject(root,"ip", ip);
  
  // WiFi status via ESP32-C6
  bool wifi_connected = wifi_c6_is_connected();
  std::string wifi_ssid = wifi_c6_get_ssid();
  bool wifi_ap_mode = !wifi_connected && !wifi_ssid.empty() && wifi_ssid.find("Setup") != std::string::npos;
  
  cJSON_AddBoolToObject(root,"eth_connected", eth_connected);
  cJSON_AddBoolToObject(root,"wifi_ap_active", wifi_ap_mode);
  cJSON_AddBoolToObject(root,"wifi_sta_connected", wifi_connected);
  
  // Network traffic statistics
  // Get DDP TX statistics
  static uint64_t last_stats_time_us = 0;
  static uint64_t last_tx_bytes = 0;
  static uint64_t last_rx_bytes = 0;
  uint64_t current_time_us = esp_timer_get_time();
  double tx_rate = 0.0;
  double rx_rate = 0.0;
  
  uint64_t current_tx_bytes = 0;
  uint64_t current_rx_bytes = 0;
  ddp_get_stats(&current_tx_bytes, &current_rx_bytes);
  
  if (last_stats_time_us > 0) {
    uint64_t time_delta_us = current_time_us - last_stats_time_us;
    if (time_delta_us > 0 && time_delta_us < 10'000'000ULL) {  // Only if less than 10 seconds
      // Calculate rate in bytes per second
      double time_delta_s = static_cast<double>(time_delta_us) / 1'000'000.0;
      if (time_delta_s > 0) {
        uint64_t tx_delta = current_tx_bytes - last_tx_bytes;
        uint64_t rx_delta = current_rx_bytes - last_rx_bytes;
        tx_rate = static_cast<double>(tx_delta) / time_delta_s;
        rx_rate = static_cast<double>(rx_delta) / time_delta_s;
      }
    }
  }
  
  last_stats_time_us = current_time_us;
  last_tx_bytes = current_tx_bytes;
  last_rx_bytes = current_rx_bytes;
  
  cJSON_AddNumberToObject(root, "network_tx_rate", tx_rate);
  cJSON_AddNumberToObject(root, "network_rx_rate", rx_rate);
  
  // CPU temperature (TSENS)
  float cpu_temp = 0.0f;
  esp_err_t temp_err = temperature_monitor_get_cpu_temp(&cpu_temp);
  if (temp_err == ESP_OK) {
    cJSON_AddNumberToObject(root, "cpu_temp_celsius", cpu_temp);
  } else {
    // Always include the field, use -1 to indicate unavailable
    cJSON_AddNumberToObject(root, "cpu_temp_celsius", -1.0);
  }
  
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
      cJSON_AddNumberToObject(led, "brightness", st.global_brightness);
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

static esp_err_t api_wled_delete(httpd_req_t* req) {
  auto body = read_body(req);
  if (body.empty()) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty payload");
  }
  cJSON* root = cJSON_ParseWithLength(body.c_str(), body.size());
  if (!root) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
  }
  std::string device_id;
  std::string device_address;
  if (cJSON* id = cJSON_GetObjectItem(root, "id"); cJSON_IsString(id)) {
    device_id = trim_copy(id->valuestring);
  }
  if (cJSON* addr = cJSON_GetObjectItem(root, "address"); cJSON_IsString(addr)) {
    device_address = trim_copy(addr->valuestring);
  }
  cJSON_Delete(root);

  if (device_id.empty() && device_address.empty()) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "id or address required");
  }

  auto& devices = s_cfg->wled_devices;
  auto it = std::remove_if(devices.begin(), devices.end(), [&](const WledDeviceConfig& cfg) {
    if (!device_id.empty() && !cfg.id.empty() && cfg.id == device_id) {
      return true;
    }
    if (!device_address.empty() && !cfg.address.empty() && cfg.address == device_address) {
      return true;
    }
    // Also match by id if address matches
    if (!device_address.empty() && !cfg.id.empty() && cfg.id == device_address) {
      return true;
    }
    return false;
  });
  
  if (it != devices.end()) {
    devices.erase(it, devices.end());
    // Also remove from WLED effects bindings
    if (s_cfg) {
      auto& bindings = s_cfg->wled_effects.bindings;
      bindings.erase(
        std::remove_if(bindings.begin(), bindings.end(), [&](const WledEffectBinding& b) {
          return (!device_id.empty() && b.device_id == device_id) ||
                 (!device_address.empty() && b.device_id == device_address);
        }),
        bindings.end()
      );
    }
    if (config_save(*s_cfg) != ESP_OK) {
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "persist failed");
    }
    if (s_wled_fx_runtime) {
      s_wled_fx_runtime->update_config(*s_cfg);
    }
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
  }
  
  return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "device not found");
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
  if (s_wled_fx_runtime) {
    s_wled_fx_runtime->update_config(*s_cfg);
  }
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
  if (s_wled_fx_runtime) {
    s_wled_fx_runtime->update_config(*s_cfg);
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
  uint8_t brightness = s_cfg ? s_cfg->led_engine.global_brightness : 255;
  if (s_led_runtime) {
    brightness = s_led_runtime->brightness();
  }
  cJSON_AddBoolToObject(root, "enabled", enabled);
  cJSON_AddBoolToObject(root, "autostart", s_cfg ? s_cfg->autostart : false);
  cJSON_AddNumberToObject(root, "brightness", brightness);
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

static esp_err_t api_wled_effects_save(httpd_req_t* req) {
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
  if (s_wled_fx_runtime) {
    s_wled_fx_runtime->update_config(*s_cfg);
  }
  httpd_resp_sendstr(req, "OK");
  return ESP_OK;
}

static esp_err_t api_led_state_post(httpd_req_t* req) {
  auto body = read_body(req);
  cJSON* root = cJSON_ParseWithLength(body.c_str(), body.size());
  if (!root) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
  }
  bool value = s_led_runtime ? s_led_runtime->enabled() : (s_cfg ? s_cfg->autostart : false);
  bool has_enabled = false;
  if (cJSON* enabled = cJSON_GetObjectItem(root, "enabled"); cJSON_IsBool(enabled)) {
    value = cJSON_IsTrue(enabled);
    has_enabled = true;
  }
  bool has_brightness = false;
  uint8_t brightness = s_cfg ? s_cfg->led_engine.global_brightness : 255;
  if (cJSON* bri = cJSON_GetObjectItem(root, "brightness"); cJSON_IsNumber(bri)) {
    const int val = static_cast<int>(bri->valuedouble);
    brightness = static_cast<uint8_t>(std::clamp(val, 0, 255));
    has_brightness = true;
  }
  if (s_led_runtime) {
    if (has_enabled) {
      s_led_runtime->set_enabled(value);
      // Stop snapclient when effects are stopped to prevent unnecessary audio processing
      // Start snapclient when effects are enabled and audio source is Snapcast
      if (s_cfg && s_cfg->led_engine.audio.source == AudioSourceType::Snapcast) {
        if (!value) {
          snapclient_light_stop();
        } else if (s_cfg->led_engine.audio.snapcast.enabled) {
          snapclient_light_start(s_cfg->led_engine.audio.snapcast);
        }
      }
    }
    if (has_brightness) {
      s_led_runtime->set_brightness(brightness);
    }
  }
  bool remember = false;
  if (cJSON* rem = cJSON_GetObjectItem(root, "remember"); cJSON_IsBool(rem)) {
    remember = cJSON_IsTrue(rem);
  }
  cJSON_Delete(root);
  if (s_cfg) {
    if (has_brightness) {
      s_cfg->led_engine.global_brightness = brightness;
    }
    if (remember) {
      s_cfg->autostart = has_enabled ? value : s_cfg->autostart;
    }
    if (remember || has_brightness) {
      if (config_save(*s_cfg) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to store led state");
      }
    }
  }
  httpd_resp_sendstr(req, "OK");
  return ESP_OK;
}

static esp_err_t api_audio_state(httpd_req_t* req) {
  auto body = read_body(req);
  if (body.empty()) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty payload");
  }
  cJSON* root = cJSON_ParseWithLength(body.c_str(), body.size());
  if (!root) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
  }
  bool enabled = true;
  if (cJSON* en = cJSON_GetObjectItem(root, "enabled"); cJSON_IsBool(en)) {
    enabled = cJSON_IsTrue(en);
  }
  cJSON_Delete(root);
  
  // Set audio running state independently from source configuration
  const esp_err_t err = led_audio_set_running(enabled);
  if (err != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "failed to set audio state");
  }
  
  // Also stop/start snapclient if source is Snapcast
  if (s_cfg && s_cfg->led_engine.audio.source == AudioSourceType::Snapcast) {
    if (!enabled) {
      snapclient_light_stop();
    } else if (s_cfg->led_engine.audio.snapcast.enabled) {
      snapclient_light_start(s_cfg->led_engine.audio.snapcast);
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

static esp_err_t api_ota_upload(httpd_req_t* req) {
  // Check if OTA is already in progress
  if (ota_in_progress()) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA already in progress");
  }
  
  // Get Content-Type header
  size_t content_type_len = httpd_req_get_hdr_value_len(req, "Content-Type");
  if (content_type_len == 0) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing Content-Type");
  }
  
  std::string content_type;
  content_type.resize(content_type_len);
  if (httpd_req_get_hdr_value_str(req, "Content-Type", &content_type[0], content_type_len + 1) != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Content-Type");
  }
  
  // Check if it's multipart/form-data
  if (content_type.find("multipart/form-data") == std::string::npos) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected multipart/form-data");
  }
  
  // Extract boundary
  std::string boundary;
  size_t boundary_pos = content_type.find("boundary=");
  if (boundary_pos != std::string::npos) {
    boundary = content_type.substr(boundary_pos + 9);
    // Remove quotes if present
    if (boundary.front() == '"' && boundary.back() == '"') {
      boundary = boundary.substr(1, boundary.length() - 2);
    }
  } else {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing boundary");
  }
  
  // Start OTA upload
  esp_err_t err = ota_start_upload();
  if (err != ESP_OK) {
    if (err == ESP_ERR_INVALID_STATE) {
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA already in progress");
    }
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to start OTA");
  }
  
  // Read body in chunks and parse multipart
  const size_t chunk_size = 4096;
  std::vector<char> buffer(chunk_size);
  std::string boundary_marker = "--" + boundary;
  std::string boundary_end = boundary_marker + "--";
  std::string accumulated_data;
  bool in_file_data = false;
  size_t total_received = 0;
  const size_t content_length = req->content_len;
  bool connection_closed = false;
  int consecutive_timeouts = 0;
  const int max_timeouts = 100; // Allow up to 100 consecutive timeouts (about 50 seconds with default timeout)
  
  ESP_LOGI(TAG, "OTA upload: content_length=%zu", content_length);
  
  // Read until we have all data or connection closes
  // If content_length is 0, we read until we find the end boundary
  while (!connection_closed && (content_length == 0 || total_received < content_length)) {
    int recv_len = httpd_req_recv(req, buffer.data(), chunk_size);
    if (recv_len <= 0) {
      if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
        consecutive_timeouts++;
        if (consecutive_timeouts > max_timeouts) {
          ESP_LOGE(TAG, "OTA upload: too many timeouts");
          ota_abort_upload();
          return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload timeout");
        }
        // Check if we have end boundary in accumulated data
        if (in_file_data && accumulated_data.find(boundary_end) != std::string::npos) {
          connection_closed = true;
          break;
        }
        continue;
      }
      // Connection closed or error
      if (recv_len == 0 || recv_len == HTTPD_SOCK_ERR_FAIL) {
        connection_closed = true;
        ESP_LOGI(TAG, "OTA upload: connection closed, total_received=%zu", total_received);
        break;
      }
      ESP_LOGE(TAG, "OTA upload: recv failed, error=%d", recv_len);
      ota_abort_upload();
      return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
    }
    
    consecutive_timeouts = 0; // Reset timeout counter on successful receive
    total_received += recv_len;
    accumulated_data.append(buffer.data(), recv_len);
    
    // Process accumulated data
    while (!accumulated_data.empty()) {
      if (!in_file_data) {
        // Look for start of file data (after boundary and headers)
        size_t boundary_start = accumulated_data.find(boundary_marker);
        if (boundary_start == std::string::npos) {
          // Haven't found boundary yet, keep accumulating
          if (accumulated_data.length() > 4096) {
            // Too much data without boundary, something's wrong
            ESP_LOGE(TAG, "OTA upload: boundary not found after %zu bytes", accumulated_data.length());
            ota_abort_upload();
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid multipart data");
          }
          break;
        }
        
        // Skip boundary and headers
        size_t headers_end = accumulated_data.find("\r\n\r\n", boundary_start);
        if (headers_end == std::string::npos) {
          // Headers not complete yet
          break;
        }
        
        // Start of file data
        size_t data_start = headers_end + 4;
        accumulated_data = accumulated_data.substr(data_start);
        in_file_data = true;
        ESP_LOGI(TAG, "OTA upload: found file data start");
      }
      
      if (in_file_data) {
        // Look for end boundary (both regular and final)
        size_t boundary_pos = accumulated_data.find(boundary_marker);
        if (boundary_pos != std::string::npos) {
          // Write data before boundary
          std::string file_data = accumulated_data.substr(0, boundary_pos);
          // Remove trailing \r\n before boundary
          while (file_data.length() >= 2 && file_data.substr(file_data.length() - 2) == "\r\n") {
            file_data = file_data.substr(0, file_data.length() - 2);
          }
          if (!file_data.empty()) {
            err = ota_write_data(file_data.data(), file_data.length());
            if (err != ESP_OK) {
              ESP_LOGE(TAG, "OTA upload: write failed");
              ota_abort_upload();
              return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            }
          }
          // Check if this is the final boundary (boundary-- instead of just boundary)
          std::string remaining = accumulated_data.substr(boundary_pos);
          if (remaining.find(boundary_end) == 0) {
            connection_closed = true;
            ESP_LOGI(TAG, "OTA upload: found final boundary");
          }
          // Done with file data
          accumulated_data.clear();
          in_file_data = false;
          break;
        } else {
          // Check if we have enough data to write (keep some for boundary check)
          // Keep more buffer to handle boundary detection better
          size_t reserve_size = boundary_marker.length() + 200;
          if (accumulated_data.length() > reserve_size) {
            size_t write_len = accumulated_data.length() - reserve_size;
            err = ota_write_data(accumulated_data.data(), write_len);
            if (err != ESP_OK) {
              ESP_LOGE(TAG, "OTA upload: write failed");
              ota_abort_upload();
              return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            }
            accumulated_data = accumulated_data.substr(write_len);
          } else {
            // Not enough data, wait for more
            break;
          }
        }
      }
    }
    
    // Log progress periodically
    if (content_length > 0) {
      size_t progress_step = content_length / 10;
      if (progress_step > 0 && (total_received % progress_step == 0 || total_received == content_length)) {
        ESP_LOGI(TAG, "OTA upload progress: %zu / %zu bytes (%.1f%%)", 
                 total_received, content_length, (100.0f * total_received) / content_length);
      }
    } else if (total_received > 0 && (total_received % (1024 * 100) == 0)) {
      // Log every 100KB when content_length is unknown
      ESP_LOGI(TAG, "OTA upload progress: %zu bytes received", total_received);
    }
  }
  
  // Write any remaining data
  if (in_file_data && !accumulated_data.empty()) {
    // Remove trailing boundary if present
    size_t boundary_pos = accumulated_data.find(boundary_marker);
    if (boundary_pos != std::string::npos) {
      std::string file_data = accumulated_data.substr(0, boundary_pos);
      while (file_data.length() >= 2 && file_data.substr(file_data.length() - 2) == "\r\n") {
        file_data = file_data.substr(0, file_data.length() - 2);
      }
      if (!file_data.empty()) {
        err = ota_write_data(file_data.data(), file_data.length());
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "OTA upload: final write failed");
          ota_abort_upload();
          return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
        }
      }
    } else {
      // Write all remaining data (no boundary found, might be end of stream)
      err = ota_write_data(accumulated_data.data(), accumulated_data.length());
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA upload: final write failed");
        ota_abort_upload();
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
      }
    }
  }
  
  ESP_LOGI(TAG, "OTA upload: finished reading, total_received=%zu", total_received);
  
  // Finish OTA upload (this will validate and reboot)
  err = ota_finish_upload();
  if (err != ESP_OK) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA finish failed");
  }
  
  httpd_resp_sendstr(req, "OTA upload completed");
  return ESP_OK;
}

static esp_err_t api_mqtt_test(httpd_req_t* req) {
  if (!mqtt_handle()) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "MQTT not connected");
  }
  bool success = mqtt_test_connection();
  if (success) {
    httpd_resp_sendstr(req, "OK");
  } else {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Test failed");
  }
  return ESP_OK;
}

static esp_err_t api_mqtt_sync(httpd_req_t* req) {
  if (!mqtt_handle()) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "MQTT not connected");
  }
  if (!s_cfg) {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Config not available");
  }
  bool success = mqtt_publish_ha_discovery(*s_cfg);
  if (success) {
    httpd_resp_sendstr(req, "OK");
  } else {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Sync failed");
  }
  return ESP_OK;
}

void start_web_server(AppConfig& cfg, LedEngineRuntime* runtime, WledEffectsRuntime* wled_runtime){
  s_cfg = &cfg;
  s_led_runtime = runtime;
  s_wled_fx_runtime = wled_runtime;
  httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
  server_config.uri_match_fn = httpd_uri_match_wildcard;
  server_config.max_uri_handlers = 30;
  // Increase timeouts for large OTA uploads
  // keep_alive_timeout is not available in this ESP-IDF version
  server_config.recv_wait_timeout = 10;   // 10 seconds
  server_config.send_wait_timeout = 10;   // 10 seconds
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
  httpd_uri_t u_wled_fx = { .uri="/api/wled/effects", .method=HTTP_POST, .handler=api_wled_effects_save, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_wled_fx);
  httpd_uri_t u_wled_delete = { .uri="/api/wled/delete", .method=HTTP_POST, .handler=api_wled_delete, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_wled_delete);
  httpd_uri_t u_led_pins = { .uri="/api/led/pins", .method=HTTP_GET, .handler=api_led_pins, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_led_pins);
  
  // WiFi API endpoints - ESP32-C6 coprocessor integration
  httpd_uri_t u_wifi_scan = { .uri="/api/wifi/scan", .method=HTTP_GET, .handler=api_wifi_scan, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_wifi_scan);
  httpd_uri_t u_wifi_connect = { .uri="/api/wifi/connect", .method=HTTP_POST, .handler=api_wifi_connect, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_wifi_connect);
  httpd_uri_t u_led_save = { .uri="/api/led/save", .method=HTTP_POST, .handler=api_save_led, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_led_save);
  httpd_uri_t u_led_state_get = { .uri="/api/led/state", .method=HTTP_GET, .handler=api_led_state_get, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_led_state_get);
  httpd_uri_t u_led_state_post = { .uri="/api/led/state", .method=HTTP_POST, .handler=api_led_state_post, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_led_state_post);
  httpd_uri_t u_ota = { .uri="/api/ota/update", .method=HTTP_POST, .handler=api_ota, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_ota);
  httpd_uri_t u_mqtt_test = { .uri="/api/mqtt/test", .method=HTTP_POST, .handler=api_mqtt_test, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_mqtt_test);
  httpd_uri_t u_mqtt_sync = { .uri="/api/mqtt/sync", .method=HTTP_POST, .handler=api_mqtt_sync, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_mqtt_sync);
  httpd_uri_t u_audio_state = { .uri="/api/audio/state", .method=HTTP_POST, .handler=api_audio_state, .user_ctx=NULL };
  httpd_register_uri_handler(server, &u_audio_state);

  ESP_LOGI(TAG,"Web server started");
}
