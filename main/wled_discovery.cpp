#include "wled_discovery.hpp"
#include "config.hpp"
#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_netif_ip_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "mdns.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static const char* TAG = "wled_scan";
static constexpr uint32_t WLED_SCAN_TIMEOUT_MS = 2000;
static constexpr uint32_t WLED_SCAN_MAX_RESULTS = 20;
static constexpr uint64_t WLED_OFFLINE_AFTER_MS = 120000;

namespace {

AppConfig* s_cfg = nullptr;
SemaphoreHandle_t s_lock = nullptr;
TaskHandle_t s_task = nullptr;
std::vector<WledDeviceStatus> s_status;

struct HttpInfo {
  std::string name;
  std::string version;
  uint16_t leds{0};
  uint16_t segments{1};
  bool looks_like_wled{false};
};

uint64_t now_ms() {
  return esp_timer_get_time() / 1000ULL;
}

std::string lowercase_copy(const char* text) {
  if (!text) {
    return {};
  }
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return out;
}

bool string_contains_ci(const char* haystack, const char* needle) {
  if (!haystack || !needle) {
    return false;
  }
  auto lower_haystack = lowercase_copy(haystack);
  auto lower_needle = lowercase_copy(needle);
  return lower_haystack.find(lower_needle) != std::string::npos;
}

bool looks_like_wled_result(const mdns_result_t& result) {
  if (string_contains_ci(result.instance_name, "wled") || string_contains_ci(result.hostname, "wled")) {
    return true;
  }
  for (size_t i = 0; i < result.txt_count; ++i) {
    const auto* txt = &result.txt[i];
    const char* key = txt->key ? txt->key : "";
    const char* value = txt->value ? txt->value : "";
    const auto key_lower = lowercase_copy(key);
    const auto value_lower = lowercase_copy(value);
    if (key_lower == "wled") {
      return true;
    }
    if (key_lower == "md" && value_lower.find("wled") != std::string::npos) {
      return true;
    }
    if (key_lower == "id" && value_lower.rfind("wled", 0) == 0) {
      return true;
    }
  }
  return false;
}

std::string ip_to_string(const mdns_ip_addr_t* addr) {
  if (!addr) {
    return {};
  }
  if (addr->addr.type == ESP_IPADDR_TYPE_V4) {
    char buf[16];
    snprintf(buf, sizeof(buf), IPSTR, IP2STR(&(addr->addr.u_addr.ip4)));
    return buf;
  }
  return {};
}

bool fetch_info_http(const std::string& ip, uint16_t port, HttpInfo& info) {
  if (ip.empty()) {
    return false;
  }
  const uint16_t effective_port = port == 0 ? 80 : port;
  char url[96];
  snprintf(url, sizeof(url), "http://%s:%u/json/info", ip.c_str(), effective_port);
  esp_http_client_config_t cfg = {};
  cfg.url = url;
  cfg.timeout_ms = 1500;
  cfg.skip_cert_common_name_check = true;

  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (!client) {
    return false;
  }
  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    esp_http_client_cleanup(client);
    return false;
  }

  std::string body;
  const int content_length = esp_http_client_fetch_headers(client);
  if (content_length > 0) {
    body.reserve(content_length);
  }

  char buffer[256];
  while (true) {
    int read = esp_http_client_read(client, buffer, sizeof(buffer));
    if (read <= 0) {
      break;
    }
    body.append(buffer, read);
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  if (body.empty()) {
    return false;
  }

  cJSON* root = cJSON_ParseWithLength(body.c_str(), body.size());
  if (!root) {
    return false;
  }

  if (cJSON* ver = cJSON_GetObjectItem(root, "ver"); cJSON_IsString(ver)) {
    info.version = ver->valuestring;
    if (!info.version.empty()) {
      info.looks_like_wled = true;
    }
  }
  if (cJSON* name = cJSON_GetObjectItem(root, "name"); cJSON_IsString(name)) {
    info.name = name->valuestring;
  }
  if (cJSON* leds = cJSON_GetObjectItem(root, "leds"); cJSON_IsObject(leds)) {
    if (cJSON* count = cJSON_GetObjectItem(leds, "count"); cJSON_IsNumber(count)) {
      int value = static_cast<int>(count->valuedouble);
      if (value > 0) {
        info.leds = static_cast<uint16_t>(value);
      }
    }
    if (cJSON* seglc = cJSON_GetObjectItem(leds, "seglc"); cJSON_IsNumber(seglc)) {
      int seg_val = static_cast<int>(seglc->valuedouble);
      if (seg_val > 0) {
        info.segments = static_cast<uint16_t>(seg_val);
      }
    }
  }
  if (cJSON* brand = cJSON_GetObjectItem(root, "brand"); cJSON_IsString(brand)) {
    if (string_contains_ci(brand->valuestring, "wled")) {
      info.looks_like_wled = true;
    }
  }
  if (!info.looks_like_wled) {
    if (cJSON* vid = cJSON_GetObjectItem(root, "vid"); cJSON_IsNumber(vid)) {
      info.looks_like_wled = true;
    }
  }

  cJSON_Delete(root);
  return true;
}

WledDeviceStatus make_status_from_result(const mdns_result_t& result) {
  WledDeviceStatus status{};
  status.online = true;
  status.last_seen_ms = now_ms();
  status.config.auto_discovered = true;
  status.config.address = result.hostname ? result.hostname : "";

  // prefer IPv4 address if available
  for (mdns_ip_addr_t* addr = result.addr; addr; addr = addr->next) {
    std::string ip = ip_to_string(addr);
    if (!ip.empty()) {
      status.ip = ip;
      status.config.address = ip;
      break;
    }
  }

  if (result.instance_name) {
    status.config.name = result.instance_name;
  }

  for (size_t i = 0; i < result.txt_count; ++i) {
    const auto* txt = &result.txt[i];
    const char* value = txt->value ? txt->value : "";
    if (strcmp(txt->key, "id") == 0 && value) {
      status.config.id = value;
    } else if (strcmp(txt->key, "mac") == 0 && value && status.config.id.empty()) {
      status.config.id = value;
    } else if (strcmp(txt->key, "leds") == 0 && value) {
      status.config.leds = static_cast<uint16_t>(atoi(value));
    } else if (strcmp(txt->key, "name") == 0 && value && status.config.name.empty()) {
      status.config.name = value;
    } else if (strcmp(txt->key, "ver") == 0 && value) {
      status.version = value;
    }
  }

  if (status.config.id.empty()) {
    status.config.id = status.config.address;
  }
  if (status.config.name.empty()) {
    status.config.name = status.config.id;
  }

  HttpInfo info{};
  if (fetch_info_http(status.ip.empty() ? status.config.address : status.ip, result.port, info)) {
    if (!info.name.empty()) status.config.name = info.name;
    if (!info.version.empty()) status.version = info.version;
    if (info.leds > 0) status.config.leds = info.leds;
    if (info.segments > 0) status.config.segments = info.segments;
    status.http_verified = info.looks_like_wled;
  }
  // Ensure ID is stable across different service entries for the same host/IP
  if (!status.config.id.empty() && status.config.address == status.config.id && !info.version.empty()) {
    status.config.id = status.config.address;
  }

  return status;
}

void collect_service_results(const char* service, bool filter_candidates, std::vector<WledDeviceStatus>& discovered) {
  mdns_result_t* results = nullptr;
  esp_err_t err = mdns_query_ptr(service, "_tcp", WLED_SCAN_TIMEOUT_MS, WLED_SCAN_MAX_RESULTS, &results);
  if (err == ESP_ERR_NOT_FOUND) {
    return;
  }
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "mDNS query for %s failed: %s", service, esp_err_to_name(err));
    return;
  }
  size_t added = 0;
  for (mdns_result_t* res = results; res; res = res->next) {
    bool mdns_candidate = looks_like_wled_result(*res);
    WledDeviceStatus status = make_status_from_result(*res);
    status.http_verified = status.http_verified || mdns_candidate;
    if (filter_candidates && !mdns_candidate && !status.http_verified) {
      continue;
    }
    if (status.config.address.empty() && status.ip.empty()) {
      continue;
    }
    discovered.push_back(std::move(status));
    ++added;
  }
  if (added > 0) {
    ESP_LOGI(TAG, "mDNS %s returned %u candidate(s)", service, static_cast<unsigned>(added));
  }
  mdns_query_results_free(results);
}

void refresh_online_state_locked() {
  const uint64_t now = now_ms();
  for (auto& st : s_status) {
    if (st.last_seen_ms == 0) {
      st.online = false;
      continue;
    }
    st.online = (now - st.last_seen_ms) < WLED_OFFLINE_AFTER_MS;
  }
}

bool upsert_config_locked(const WledDeviceStatus& status) {
  if (!s_cfg) {
    return false;
  }
  auto same_device = [&](const WledDeviceConfig& cfg) {
    if (!status.config.id.empty() && cfg.id == status.config.id) {
      return true;
    }
    if (!status.config.address.empty() && !cfg.address.empty() && cfg.address == status.config.address) {
      return true;
    }
    return false;
  };
  auto it = std::find_if(s_cfg->wled_devices.begin(), s_cfg->wled_devices.end(), same_device);
  if (it == s_cfg->wled_devices.end()) {
    s_cfg->wled_devices.push_back(status.config);
    return true;
  }
  bool changed = false;
  if (!status.config.address.empty() && it->address != status.config.address) {
    it->address = status.config.address;
    changed = true;
  }
  if (status.config.leds > 0 && it->leds != status.config.leds) {
    it->leds = status.config.leds;
    changed = true;
  }
  if (status.config.segments > 0 && it->segments != status.config.segments) {
    it->segments = status.config.segments;
    changed = true;
  }
  if (it->auto_discovered && !status.config.name.empty() && it->name != status.config.name) {
    it->name = status.config.name;
    changed = true;
  }
  it->auto_discovered = it->auto_discovered || status.config.auto_discovered;
  return changed;
}

void merge_status_locked(const std::vector<WledDeviceStatus>& discovered, bool& config_changed) {
  config_changed = false;
  for (const auto& device : discovered) {
    auto matches = [&](const WledDeviceStatus& st) {
      if (!device.config.id.empty() && st.config.id == device.config.id) {
        return true;
      }
      if (!device.ip.empty() && !st.ip.empty() && st.ip == device.ip) {
        return true;
      }
      if (!device.config.address.empty() && !st.config.address.empty() &&
          st.config.address == device.config.address) {
        return true;
      }
      return false;
    };
    auto it = std::find_if(s_status.begin(), s_status.end(), matches);
    if (it == s_status.end()) {
      s_status.push_back(device);
    } else {
      *it = device;
    }
    bool changed = upsert_config_locked(device);
    config_changed = config_changed || changed;
  }
  refresh_online_state_locked();

  // Drop duplicates on same address/id/ip, keep the newest entries
  std::vector<WledDeviceStatus> deduped;
  std::vector<std::string> seen;
  auto key_of = [](const WledDeviceStatus& st) -> std::string {
    if (!st.config.address.empty()) return st.config.address;
    if (!st.ip.empty()) return st.ip;
    return st.config.id;
  };
  for (auto it = s_status.rbegin(); it != s_status.rend(); ++it) {
    auto key = key_of(*it);
    if (key.empty()) {
      deduped.push_back(*it);
      continue;
    }
    if (std::find(seen.begin(), seen.end(), key) != seen.end()) {
      continue;
    }
    seen.push_back(key);
    deduped.push_back(*it);
  }
  std::reverse(deduped.begin(), deduped.end());
  s_status.swap(deduped);
}

void wled_scan_once() {
  if (!s_cfg) {
    return;
  }
  std::vector<WledDeviceStatus> discovered;
  collect_service_results("_wled", false, discovered);
  collect_service_results("_http", true, discovered);

  bool config_changed = false;
  if (s_lock) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    merge_status_locked(discovered, config_changed);
    xSemaphoreGive(s_lock);
  }
  if (config_changed) {
    config_save(*s_cfg);
  }
}

void discovery_task(void*) {
  ESP_LOGI(TAG, "WLED discovery task started");
  while (true) {
    wled_scan_once();
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(60000));
  }
}

}  // namespace

void wled_discovery_start(AppConfig& cfg) {
  s_cfg = &cfg;
  if (!s_lock) {
    s_lock = xSemaphoreCreateMutex();
  }
  if (s_lock) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.clear();
    for (const auto& stored : cfg.wled_devices) {
      WledDeviceStatus status{};
      status.config = stored;
      status.config.auto_discovered = stored.auto_discovered;
      status.online = false;
      status.last_seen_ms = 0;
      s_status.push_back(status);
    }
    xSemaphoreGive(s_lock);
  }
  if (!s_task) {
    xTaskCreatePinnedToCore(discovery_task, "wled_scan", 4096, nullptr, 4, &s_task, 0);
  }
}

void wled_discovery_trigger_scan() {
  if (s_task) {
    xTaskNotifyGive(s_task);
  }
}

std::vector<WledDeviceStatus> wled_discovery_status() {
  std::vector<WledDeviceStatus> copy;
  if (s_lock) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    copy = s_status;
    xSemaphoreGive(s_lock);
  }
  return copy;
}

void wled_discovery_register_manual(const WledDeviceConfig& cfg) {
  WledDeviceStatus status{};
  status.config = cfg;
  status.config.auto_discovered = false;
  if (status.config.id.empty()) {
    if (!status.config.address.empty()) {
      status.config.id = status.config.address;
    } else if (!status.config.name.empty()) {
      status.config.id = status.config.name;
    } else {
      status.config.id = "wled-manual";
    }
  }
  if (status.config.name.empty()) {
    status.config.name = status.config.id;
  }
  if (status.config.segments == 0) {
    status.config.segments = 1;
  }
  status.online = false;
  status.last_seen_ms = 0;
  status.http_verified = false;

  if (!s_lock) {
    s_lock = xSemaphoreCreateMutex();
  }
  if (!s_lock) {
    return;
  }

  xSemaphoreTake(s_lock, portMAX_DELAY);
  auto it = std::find_if(s_status.begin(), s_status.end(),
                         [&](const WledDeviceStatus& st) { return st.config.id == status.config.id; });
  if (it == s_status.end()) {
    s_status.push_back(status);
  } else {
    *it = status;
  }
  xSemaphoreGive(s_lock);
}
