/*
 * ESP32-C6 WiFi Coprocessor Firmware
 * Provides WiFi connectivity to ESP32-P4 via PPP over UART
 * 
 * This firmware runs on ESP32-C6 and acts as a PPP server,
 * allowing ESP32-P4 (which doesn't have WiFi) to access the Internet
 * through ESP32-C6's WiFi connection.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "eppp_link.h"
#include "esp_netif.h"
#include "lwip/ip_addr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include <stdlib.h>
#include <cJSON.h>

static const char *TAG = "ledbrain_c6";
static const char *NVS_NAMESPACE = "wifi_cfg";
static const char *NVS_KEY_STA_CONFIG = "sta_config";

/* MAC address formatting macros */
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

/* Event group to signal WiFi connection status */
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;
static esp_netif_t *s_ap_netif = NULL;
static esp_netif_t *s_sta_netif = NULL;
static httpd_handle_t s_httpd = NULL;

// Control UART configuration (UART1 for control commands)
// Note: UART0 is used for PPP (GPIO18/17), so UART1 uses different pins
#define CTRL_UART_PORT UART_NUM_1
#define CTRL_UART_TX 19  // ESP32-C6 TX -> ESP32-P4 RX (UART2, GPIO19)
#define CTRL_UART_RX 20  // ESP32-P4 TX -> ESP32-C6 RX (UART2, GPIO20)
#define CTRL_UART_BAUD 115200
#define CTRL_UART_BUF_SIZE 1024
#define CTRL_UART_QUEUE_SIZE 10

#ifndef WIFI_MAX_RETRY
#define WIFI_MAX_RETRY CONFIG_ESP_MAXIMUM_RETRY
#endif

/* WiFi event handler */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "WiFi AP started");
                break;
            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "WiFi AP stopped");
                break;
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d",
                         MAC2STR(event->mac), event->aid);
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
                ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d",
                         MAC2STR(event->mac), event->aid);
                break;
            }
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi station started, connecting to AP...");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                if (s_retry_num < WIFI_MAX_RETRY) {
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGI(TAG, "Retry to connect to AP (attempt %d/%d)", s_retry_num, WIFI_MAX_RETRY);
                } else {
                    if (s_wifi_event_group) {
                        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                    }
                    ESP_LOGE(TAG, "Failed to connect to WiFi after %d attempts", WIFI_MAX_RETRY);
                }
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        if (s_wifi_event_group) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

/* Load WiFi configuration from NVS */
static esp_err_t wifi_load_from_nvs(wifi_config_t *sta_config)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }

    size_t required_size = sizeof(wifi_config_t);
    err = nvs_get_blob(nvs_handle, NVS_KEY_STA_CONFIG, sta_config, &required_size);
    nvs_close(nvs_handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved WiFi config in NVS");
        return ESP_ERR_NOT_FOUND;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read WiFi config from NVS: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Loaded WiFi config from NVS: SSID=%s", sta_config->sta.ssid);
    return ESP_OK;
}

/* Save WiFi configuration to NVS */
static esp_err_t wifi_save_to_nvs(const wifi_config_t *sta_config)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs_handle, NVS_KEY_STA_CONFIG, sta_config, sizeof(wifi_config_t));
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi config saved to NVS: SSID=%s", sta_config->sta.ssid);
    } else {
        ESP_LOGE(TAG, "Failed to save WiFi config to NVS: %s", esp_err_to_name(err));
    }

    return err;
}

/* Forward declarations */
static esp_err_t wifi_start_sta_mode(wifi_config_t *sta_config, bool keep_ap);
static void stop_web_server(void);
static httpd_handle_t start_web_server(void);

/* HTTP Server handlers */
static const char* html_page = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1'>"
"<title>LEDBrain WiFi Setup</title>"
"<style>"
"body{font-family:Arial,sans-serif;max-width:600px;margin:50px auto;padding:20px;background:#f5f5f5;}"
"h1{color:#333;text-align:center;}"
".container{background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}"
".panel{padding:12px;border:1px solid #cfe2ff;background:#e7f1ff;border-radius:8px;margin:10px 0;}"
".panel h2{margin:0 0 8px 0;font-size:18px;color:#084298;}"
".panel p{margin:6px 0;color:#084298;}"
".panel a{display:inline-block;text-decoration:none;background:#0d6efd;color:#fff;padding:10px 14px;border-radius:6px;}"
".panel a:hover{background:#0b5ed7;}"
"button{width:100%;padding:12px;margin:10px 0;background:#007bff;color:white;border:none;border-radius:5px;font-size:16px;cursor:pointer;}"
"button:hover{background:#0056b3;}"
"button:disabled{background:#ccc;cursor:not-allowed;}"
"input[type='text'],input[type='password']{width:100%;padding:10px;margin:10px 0;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;}"
"select{width:100%;padding:10px;margin:10px 0;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;}"
".network-item{padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:5px;cursor:pointer;background:#f9f9f9;}"
".network-item:hover{background:#e9e9e9;}"
".network-item.selected{background:#007bff;color:white;}"
"#status{margin:20px 0;padding:10px;border-radius:5px;}"
".status-ok{background:#d4edda;color:#155724;border:1px solid #c3e6cb;}"
".status-error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb;}"
".status-info{background:#d1ecf1;color:#0c5460;border:1px solid #bee5eb;}"
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>LEDBrain WiFi Setup</h1>"
"<div class='panel'>"
"<h2>Panel LEDBrain (ESP32-P4)</h2>"
"<p>Jeśli jesteś podłączony do sieci <b>LEDBrain-Setup-C6</b>, panel LEDBrain jest pod adresem:</p>"
"<p><b>http://192.168.11.2/</b></p>"
"<a href='/ledbrain'>Otwórz panel LEDBrain</a>"
"</div>"
"<div id='status' class='status-info' style='display:none;'></div>"
"<button onclick='scanWiFi()'>Skanuj sieci WiFi</button>"
"<div id='networks'></div>"
"<div id='form' style='display:none;'>"
"<h3>Hasło WiFi:</h3>"
"<input type='password' id='password' placeholder='Wpisz hasło'>"
"<button onclick='connectWiFi()'>Połącz</button>"
"</div>"
"</div>"
"<script>"
"let selectedSSID='';"
"function showStatus(msg,type){"
"const el=document.getElementById('status');"
"el.textContent=msg;"
"el.className='status-'+type;"
"el.style.display='block';"
"}"
"function scanWiFi(){"
"showStatus('Skanowanie...','info');"
"fetch('/api/wifi/scan')"
".then(r=>r.json())"
".then(data=>{"
"if(data.error){showStatus('Błąd: '+data.error,'error');return;}"
"const div=document.getElementById('networks');"
"div.innerHTML='';"
"if(data.networks.length===0){showStatus('Nie znaleziono sieci','info');return;}"
"data.networks.forEach(net=>{"
"const item=document.createElement('div');"
"item.className='network-item';"
"item.innerHTML=net.ssid+' <span style=\"float:right;\">'+net.rssi+' dBm</span>';"
"item.onclick=()=>{"
"document.querySelectorAll('.network-item').forEach(i=>i.classList.remove('selected'));"
"item.classList.add('selected');"
"selectedSSID=net.ssid;"
"document.getElementById('form').style.display='block';"
"};"
"div.appendChild(item);"
"});"
"showStatus('Znaleziono '+data.networks.length+' sieci','ok');"
"})"
".catch(e=>{showStatus('Błąd skanowania: '+e,'error');});"
"}"
"function connectWiFi(){"
"const pwd=document.getElementById('password').value;"
"if(!selectedSSID){showStatus('Wybierz sieć','error');return;}"
"if(!pwd){showStatus('Wpisz hasło','error');return;}"
"showStatus('Łączenie...','info');"
"fetch('/api/wifi/connect',{"
"method:'POST',"
"headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({ssid:selectedSSID,password:pwd})"
"})"
".then(r=>r.json())"
".then(data=>{"
"if(data.success){"
"showStatus('Połączono! Przekierowywanie...','ok');"
"setTimeout(()=>window.location.reload(),2000);"
"}else{"
"showStatus('Błąd: '+(data.error||'Nieznany błąd'),'error');"
"}"
"})"
".catch(e=>{showStatus('Błąd: '+e,'error');});"
"}"
"</script>"
"</body>"
"</html>";

static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const char* http_status_text(int status) {
    switch (status) {
        case 200: return "200 OK";
        case 302: return "302 Found";
        case 400: return "400 Bad Request";
        case 404: return "404 Not Found";
        case 500: return "500 Internal Server Error";
        default: return "502 Bad Gateway";
    }
}

static esp_err_t proxy_to_p4(httpd_req_t *req, const char *path_override) {
    const char *path = path_override ? path_override : req->uri;
    char url[192];
    snprintf(url, sizeof(url), "http://192.168.11.2%s", path);

    esp_http_client_method_t method = HTTP_METHOD_GET;
    if (req->method == HTTP_POST) {
        method = HTTP_METHOD_POST;
    }

    esp_http_client_config_t cfg = {};
    cfg.url = url;
    cfg.method = method;
    cfg.timeout_ms = 4000;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        httpd_resp_set_status(req, http_status_text(500));
        httpd_resp_send(req, "Proxy init failed", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    int content_len = req->content_len;
    std::string body;
    if (content_len > 0 && content_len < 64 * 1024) {
        body.resize(content_len);
        int received = 0;
        while (received < content_len) {
            int r = httpd_req_recv(req, &body[received], content_len - received);
            if (r <= 0) {
                esp_http_client_cleanup(client);
                httpd_resp_set_status(req, http_status_text(400));
                httpd_resp_send(req, "Failed to read request body", HTTPD_RESP_USE_STRLEN);
                return ESP_FAIL;
            }
            received += r;
        }
    }

    esp_err_t err = esp_http_client_open(client, content_len);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        httpd_resp_set_status(req, http_status_text(502));
        httpd_resp_send(req, "Proxy connect failed", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    if (content_len > 0) {
        esp_http_client_write(client, body.data(), content_len);
    }

    int fetch_len = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    httpd_resp_set_status(req, http_status_text(status));

    char content_type[64] = {};
    if (esp_http_client_get_resp_header(client, "Content-Type", content_type, sizeof(content_type)) == ESP_OK) {
        httpd_resp_set_type(req, content_type);
    }

    char buffer[512];
    int read_len = 0;
    int remaining = fetch_len;
    while (true) {
        read_len = esp_http_client_read(client, buffer, sizeof(buffer));
        if (read_len <= 0) {
            break;
        }
        httpd_resp_send_chunk(req, buffer, read_len);
        if (remaining > 0) {
            remaining -= read_len;
        }
    }
    httpd_resp_send_chunk(req, NULL, 0);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ESP_OK;
}

static esp_err_t ledbrain_redirect_handler(httpd_req_t *req) {
    const char *uri = req->uri;
    const char *suffix = "";
    if (strncmp(uri, "/ledbrain", 9) == 0) {
        suffix = uri + 9;
        if (suffix[0] == '\0') {
            suffix = "/";
        }
    }
    return proxy_to_p4(req, suffix);
}

static esp_err_t ledbrain_assets_handler(httpd_req_t *req) {
    if (strncmp(req->uri, "/api/wifi/", 10) == 0) {
        httpd_resp_set_status(req, http_status_text(404));
        httpd_resp_send(req, "Not found", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    return proxy_to_p4(req, nullptr);
}

static esp_err_t api_wifi_scan_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
    };
    
    esp_wifi_scan_start(&scan_config, true);
    
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    cJSON *root = cJSON_CreateObject();
    cJSON *networks = cJSON_CreateArray();
    
    if (ap_count > 0) {
        wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
        if (ap_records) {
            esp_wifi_scan_get_ap_records(&ap_count, ap_records);
            
            for (uint16_t i = 0; i < ap_count; i++) {
                cJSON *net = cJSON_CreateObject();
                cJSON_AddStringToObject(net, "ssid", (char*)ap_records[i].ssid);
                cJSON_AddNumberToObject(net, "rssi", ap_records[i].rssi);
                cJSON_AddNumberToObject(net, "authmode", ap_records[i].authmode);
                cJSON_AddNumberToObject(net, "channel", ap_records[i].primary);
                cJSON_AddItemToArray(networks, net);
            }
            
            free(ap_records);
        }
    }
    
    cJSON_AddItemToObject(root, "networks", networks);
    
    char *json_str = cJSON_Print(root);
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    free(json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}

static esp_err_t api_wifi_connect_handler(httpd_req_t *req) {
    char content[512];
    size_t recv_size = sizeof(content);
    
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        httpd_resp_set_status(req, HTTPD_400);
        httpd_resp_send(req, "{\"success\":false,\"error\":\"No data\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    content[ret] = '\0';
    
    cJSON *root = cJSON_Parse(content);
    if (!root) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, HTTPD_400);
        httpd_resp_send(req, "{\"success\":false,\"error\":\"Invalid JSON\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    cJSON *ssid_json = cJSON_GetObjectItem(root, "ssid");
    cJSON *password_json = cJSON_GetObjectItem(root, "password");
    
    if (!cJSON_IsString(ssid_json) || !cJSON_IsString(password_json)) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, HTTPD_400);
        httpd_resp_send(req, "{\"success\":false,\"error\":\"Missing ssid or password\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    
    const char *ssid = cJSON_GetStringValue(ssid_json);
    const char *password = cJSON_GetStringValue(password_json);
    
    // Configure WiFi STA
    wifi_config_t sta_config = {0};
    strncpy((char*)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid) - 1);
    strncpy((char*)sta_config.sta.password, password, sizeof(sta_config.sta.password) - 1);
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    
    // Save to NVS
    wifi_save_to_nvs(&sta_config);
    
    // Stop web server before switching to STA mode
    stop_web_server();
    
    // Start STA mode
    esp_err_t wifi_ret = wifi_start_sta_mode(&sta_config, false);  // don't keep AP
    
    httpd_resp_set_type(req, "application/json");
    
    cJSON *response = cJSON_CreateObject();
    if (wifi_ret == ESP_OK) {
        cJSON_AddBoolToObject(response, "success", true);
        ESP_LOGI(TAG, "WiFi configured: SSID=%s", ssid);
    } else {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "error", "Failed to configure WiFi");
    }
    
    char *response_str = cJSON_Print(response);
    httpd_resp_send(req, response_str, HTTPD_RESP_USE_STRLEN);
    free(response_str);
    cJSON_Delete(response);
    cJSON_Delete(root);
    
    return ESP_OK;
}

static httpd_handle_t start_web_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 8;
    config.uri_match_fn = httpd_uri_match_wildcard;
    
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
        };
        httpd_register_uri_handler(server, &root);

        httpd_uri_t ledbrain = {
            .uri = "/ledbrain",
            .method = HTTP_GET,
            .handler = ledbrain_redirect_handler,
        };
        httpd_register_uri_handler(server, &ledbrain);

        httpd_uri_t ledbrain_wildcard = {
            .uri = "/ledbrain/*",
            .method = HTTP_GET,
            .handler = ledbrain_redirect_handler,
        };
        httpd_register_uri_handler(server, &ledbrain_wildcard);
        
        httpd_uri_t scan = {
            .uri = "/api/wifi/scan",
            .method = HTTP_GET,
            .handler = api_wifi_scan_handler,
        };
        httpd_register_uri_handler(server, &scan);
        
        httpd_uri_t connect = {
            .uri = "/api/wifi/connect",
            .method = HTTP_POST,
            .handler = api_wifi_connect_handler,
        };
        httpd_register_uri_handler(server, &connect);

        httpd_uri_t api_proxy = {
            .uri = "/api/*",
            .method = HTTP_GET,
            .handler = ledbrain_assets_handler,
        };
        httpd_register_uri_handler(server, &api_proxy);
        httpd_uri_t api_proxy_post = {
            .uri = "/api/*",
            .method = HTTP_POST,
            .handler = ledbrain_assets_handler,
        };
        httpd_register_uri_handler(server, &api_proxy_post);

        httpd_uri_t lang_proxy = {
            .uri = "/lang/*",
            .method = HTTP_GET,
            .handler = ledbrain_assets_handler,
        };
        httpd_register_uri_handler(server, &lang_proxy);

        httpd_uri_t app_js_proxy = {
            .uri = "/app.js",
            .method = HTTP_GET,
            .handler = ledbrain_assets_handler,
        };
        httpd_register_uri_handler(server, &app_js_proxy);

        httpd_uri_t css_proxy = {
            .uri = "/style.css",
            .method = HTTP_GET,
            .handler = ledbrain_assets_handler,
        };
        httpd_register_uri_handler(server, &css_proxy);
        
        ESP_LOGI(TAG, "Web server started on http://192.168.4.1");
    }
    
    return server;
}

static void stop_web_server(void) {
    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
}

/* Start WiFi AP mode */
static esp_err_t wifi_start_ap_mode(void)
{
    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (!s_ap_netif) {
            ESP_LOGE(TAG, "Failed to create AP netif");
            return ESP_FAIL;
        }
    }

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "LEDBrain-Setup-C6",
            .password = "ledbrain123",
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = 4,
            .channel = 1,
            .ssid_hidden = 0,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP started: SSID=LEDBrain-Setup-C6, IP=192.168.4.1");
    ESP_LOGI(TAG, "Password: ledbrain123");
    
    // Start web server for WiFi configuration
    if (!s_httpd) {
        s_httpd = start_web_server();
    }
    
    return ESP_OK;
}

/* Start WiFi STA mode (with optional AP in background) */
static esp_err_t wifi_start_sta_mode(wifi_config_t *sta_config, bool keep_ap)
{
    if (keep_ap) {
        // Start AP mode for easier access
        wifi_start_ap_mode();
    } else {
        // STA only mode
        if (!s_sta_netif) {
            s_sta_netif = esp_netif_create_default_wifi_sta();
        }
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, (wifi_config_t*)sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    if (keep_ap) {
        // In AP+STA mode, STA connects automatically after start
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP_ERROR_CHECK(esp_wifi_connect());
    } else {
        ESP_ERROR_CHECK(esp_wifi_connect());
    }

    ESP_LOGI(TAG, "WiFi STA started, connecting to: %s", sta_config->sta.ssid);
    return ESP_OK;
}

/* Initialize control UART for ESP32-P4 communication */
static esp_err_t ctrl_uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = CTRL_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(CTRL_UART_PORT, CTRL_UART_BUF_SIZE, CTRL_UART_BUF_SIZE,
                                         CTRL_UART_QUEUE_SIZE, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CTRL_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(CTRL_UART_PORT, CTRL_UART_TX, CTRL_UART_RX,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "Control UART initialized (UART%d, TX: GPIO%d, RX: GPIO%d)",
             CTRL_UART_PORT, CTRL_UART_TX, CTRL_UART_RX);
    return ESP_OK;
}

/* Handle SCAN command */
static void handle_scan_command(void)
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
    };

    esp_wifi_scan_start(&scan_config, true);

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    if (ap_count == 0) {
        uart_write_bytes(CTRL_UART_PORT, "OK|\r\n", 5);
        return;
    }

    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_records) {
        uart_write_bytes(CTRL_UART_PORT, "ERROR|MEMORY\r\n", 14);
        return;
    }

    esp_wifi_scan_get_ap_records(&ap_count, ap_records);

    // Format: OK|SSID1|RSSI1|AUTH1|CH1|SSID2|RSSI2|AUTH2|CH2|...
    char response[2048] = "OK|";
    int pos = 3;

    for (uint16_t i = 0; i < ap_count && pos < sizeof(response) - 50; i++) {
        int len = snprintf(response + pos, sizeof(response) - pos, "%s|%d|%d|%d|",
                          ap_records[i].ssid, ap_records[i].rssi,
                          ap_records[i].authmode, ap_records[i].primary);
        if (len > 0) {
            pos += len;
        }
    }

    strcat(response, "\r\n");
    uart_write_bytes(CTRL_UART_PORT, response, strlen(response));
    free(ap_records);
}

/* Handle CONNECT command */
static void handle_connect_command(const char *cmd)
{
    // Format: CONNECT|SSID:xxx|PASS:yyy
    char ssid[33] = {0};
    char password[65] = {0};

    const char *ssid_start = strstr(cmd, "SSID:");
    const char *pass_start = strstr(cmd, "PASS:");

    if (!ssid_start || !pass_start) {
        uart_write_bytes(CTRL_UART_PORT, "ERROR|INVALID_FORMAT\r\n", 22);
        return;
    }

    ssid_start += 5;  // Skip "SSID:"
    pass_start += 5;  // Skip "PASS:"

    // Extract SSID (until | or end)
    const char *ssid_end = strchr(ssid_start, '|');
    int ssid_len = ssid_end ? (ssid_end - ssid_start) : strlen(ssid_start);
    if (ssid_len > 32) ssid_len = 32;
    strncpy(ssid, ssid_start, ssid_len);
    ssid[ssid_len] = '\0';

    // Extract password (until | or end)
    const char *pass_end = strchr(pass_start, '|');
    int pass_len = pass_end ? (pass_end - pass_start) : strlen(pass_start);
    if (pass_len > 64) pass_len = 64;
    strncpy(password, pass_start, pass_len);
    password[pass_len] = '\0';

    // Configure WiFi STA
    wifi_config_t sta_config = {0};
    strncpy((char*)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid) - 1);
    strncpy((char*)sta_config.sta.password, password, sizeof(sta_config.sta.password) - 1);
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    // Save to NVS
    wifi_save_to_nvs(&sta_config);

    // Start STA mode
    esp_err_t ret = wifi_start_sta_mode(&sta_config, true);  // keep_ap=true
    if (ret == ESP_OK) {
        uart_write_bytes(CTRL_UART_PORT, "OK|CONNECTING\r\n", 16);
    } else {
        uart_write_bytes(CTRL_UART_PORT, "ERROR|CONFIG_FAILED\r\n", 22);
    }
}

/* Handle STATUS command */
static void handle_status_command(void)
{
    char response[256];
    wifi_ap_record_t ap_info;
    esp_netif_ip_info_t ip_info;

    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK && ap_info.rssi != 0) {
        // STA connected
        if (s_sta_netif && esp_netif_get_ip_info(s_sta_netif, &ip_info) == ESP_OK) {
            snprintf(response, sizeof(response), "OK|SSID:%s|IP:" IPSTR "|RSSI:%d\r\n",
                     ap_info.ssid, IP2STR(&ip_info.ip), ap_info.rssi);
        } else {
            snprintf(response, sizeof(response), "OK|SSID:%s|IP:0.0.0.0|RSSI:%d\r\n",
                     ap_info.ssid, ap_info.rssi);
        }
    } else {
        // AP mode or not connected
        snprintf(response, sizeof(response), "OK|SSID:LEDBrain-Setup-C6|IP:192.168.4.1|RSSI:0\r\n");
    }

    uart_write_bytes(CTRL_UART_PORT, response, strlen(response));
}

/* Control UART command handler task */
static void ctrl_uart_task(void *pvParameters)
{
    uint8_t *data = (uint8_t *)malloc(CTRL_UART_BUF_SIZE);
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate buffer for control UART");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Control UART task started");

    while (1) {
        int len = uart_read_bytes(CTRL_UART_PORT, data, CTRL_UART_BUF_SIZE - 1, portMAX_DELAY);
        if (len > 0) {
            data[len] = '\0';

            // Remove trailing \r\n
            while (len > 0 && (data[len - 1] == '\r' || data[len - 1] == '\n')) {
                data[--len] = '\0';
            }

            ESP_LOGI(TAG, "Received command: %s", data);

            if (strncmp((char*)data, "SCAN", 4) == 0) {
                handle_scan_command();
            } else if (strncmp((char*)data, "CONNECT", 7) == 0) {
                handle_connect_command((char*)data);
            } else if (strncmp((char*)data, "STATUS", 6) == 0) {
                handle_status_command();
            } else {
                uart_write_bytes(CTRL_UART_PORT, "ERROR|UNKNOWN_CMD\r\n", 19);
            }
        }
    }

    free(data);
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "LEDBrain ESP32-C6 WiFi Coprocessor Firmware");
    ESP_LOGI(TAG, "ESP-IDF version: %s", IDF_VER);

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize WiFi - Smart AP/STA mode */
    ESP_LOGI(TAG, "Initializing WiFi...");
    
    wifi_config_t saved_sta_config = {0};
    esp_err_t load_err = wifi_load_from_nvs(&saved_sta_config);
    
    bool wifi_sta_configured = (load_err == ESP_OK && strlen((char*)saved_sta_config.sta.ssid) > 0);
    bool start_ap = !wifi_sta_configured;
    
    if (start_ap) {
        ESP_LOGI(TAG, "No saved WiFi config, starting AP mode for provisioning");
        ESP_LOGI(TAG, "Connect to 'LEDBrain-Setup-C6' (password: ledbrain123) to configure WiFi");
    } else {
        ESP_LOGI(TAG, "Found saved WiFi config, starting STA mode: %s", saved_sta_config.sta.ssid);
    }
    
    // Initialize WiFi subsystem
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    s_wifi_event_group = xEventGroupCreate();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));
    
    esp_err_t wifi_ret;
    if (start_ap) {
        // Start AP mode for provisioning
        wifi_ret = wifi_start_ap_mode();
    } else {
        // Start STA mode (with AP in background for easier access)
        wifi_ret = wifi_start_sta_mode(&saved_sta_config, true);  // keep_ap=true
        
        // Wait for WiFi connection
        if (wifi_ret == ESP_OK && s_wifi_event_group) {
            EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                                   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                                   pdFALSE,
                                                   pdFALSE,
                                                   pdMS_TO_TICKS(15000));  // 15s timeout
            
            if (bits & WIFI_CONNECTED_BIT) {
                ESP_LOGI(TAG, "WiFi connected successfully");
                wifi_ret = ESP_OK;
            } else if (bits & WIFI_FAIL_BIT) {
                ESP_LOGW(TAG, "WiFi connection failed, AP mode still available");
                wifi_ret = ESP_FAIL;
            } else {
                ESP_LOGW(TAG, "WiFi connection timeout, continuing anyway");
                wifi_ret = ESP_FAIL;
            }
        }
    }
    
    if (wifi_ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi initialization had issues: %s", esp_err_to_name(wifi_ret));
        ESP_LOGI(TAG, "Note: PPP server will still start, but NAPT won't work without WiFi STA connection");
    }

    /* Initialize PPP server over UART */
    ESP_LOGI(TAG, "Initializing PPP server over UART...");
    
    eppp_config_t config = EPPP_DEFAULT_SERVER_CONFIG();
    
    /* Configure UART transport for ESP32-C6 to ESP32-P4 communication */
    config.transport = EPPP_TRANSPORT_UART;
    config.uart.port = 0;  // UART0 on ESP32-C6 (can be changed via menuconfig)
    config.uart.baud = CONFIG_EXAMPLE_UART_BAUDRATE;  // High-speed UART (default: 921600)
    config.uart.tx_io = CONFIG_EXAMPLE_UART_TX_PIN;   // ESP32-C6 TX -> ESP32-P4 RX (default: GPIO18)
    config.uart.rx_io = CONFIG_EXAMPLE_UART_RX_PIN;   // ESP32-P4 TX -> ESP32-C6 RX (default: GPIO17)
    config.uart.queue_size = 32;
    config.uart.rx_buffer_size = 2048;
    config.uart.rts_io = -1;  // No flow control
    config.uart.cts_io = -1;
    config.uart.flow_control = 0;

    /* PPP configuration */
    // ESP32-C6 (server) IP: 192.168.11.1 (already set by EPPP_DEFAULT_SERVER_CONFIG)
    // ESP32-P4 (client) IP: 192.168.11.2 (already set by EPPP_DEFAULT_SERVER_CONFIG)
    config.ppp.netif_prio = 10;
    config.ppp.netif_description = "pppos-server";

    /* Task configuration */
    config.task.run_task = true;
    config.task.stack_size = 8192;
    config.task.priority = 5;

    ESP_LOGI(TAG, "Starting PPP server on UART%d (TX: GPIO%d, RX: GPIO%d, Baud: %d)",
             config.uart.port, config.uart.tx_io, config.uart.rx_io, config.uart.baud);

    esp_netif_t *ppp_netif = eppp_listen(&config);
    if (ppp_netif == NULL) {
        ESP_LOGE(TAG, "Failed to setup PPP server");
        return;
    }

    ESP_LOGI(TAG, "PPP server started successfully");

    /* Enable NAPT (Network Address Port Translation) */
    /* This allows ESP32-P4 to access Internet through ESP32-C6's WiFi */
    /* Only enable if WiFi STA is actually connected */
    bool wifi_sta_connected = false;
    if (s_wifi_event_group) {
        EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
        wifi_sta_connected = (bits & WIFI_CONNECTED_BIT) != 0;
    }
    
    if (wifi_sta_connected && !start_ap) {
        ESP_LOGI(TAG, "WiFi STA connected, enabling NAPT for Internet sharing...");
        esp_err_t napt_ret = esp_netif_napt_enable(ppp_netif);
        if (napt_ret == ESP_OK) {
            ESP_LOGI(TAG, "NAPT enabled - ESP32-P4 can now access Internet via ESP32-C6");
        } else {
            ESP_LOGW(TAG, "NAPT enable failed: %s (continuing anyway)", esp_err_to_name(napt_ret));
        }
    } else {
        // In AP-only or STA-not-connected mode, enable NAPT on PPP so AP clients
        // can still reach the ESP32-P4 web UI through the PPP link.
        ESP_LOGI(TAG, "WiFi not connected, enabling NAPT for AP->PPP access...");
        esp_err_t napt_ret = esp_netif_napt_enable(ppp_netif);
        if (napt_ret == ESP_OK) {
            ESP_LOGI(TAG, "NAPT enabled - AP clients can reach ESP32-P4 at 192.168.11.2");
        } else {
            ESP_LOGW(TAG, "NAPT enable failed: %s (AP clients may not reach ESP32-P4)", esp_err_to_name(napt_ret));
        }
    }

    ESP_LOGI(TAG, "ESP32-C6 WiFi coprocessor ready");
    ESP_LOGI(TAG, "Waiting for ESP32-P4 to connect via PPP...");

    /* Initialize control UART for ESP32-P4 communication */
    ctrl_uart_init();
    xTaskCreate(ctrl_uart_task, "ctrl_uart", 4096, NULL, 5, NULL);

    /* Main loop - monitor WiFi status and re-enable NAPT when connected */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        /* Check WiFi status periodically */
        wifi_ap_record_t ap_info;
        
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK && ap_info.rssi != 0) {
            ESP_LOGI(TAG, "WiFi status: Connected to %s (RSSI: %d dBm)",
                     ap_info.ssid, ap_info.rssi);
            
            // Check if NAPT is enabled, enable it if not
            // Note: This is a simplified check - actual NAPT status check requires more work
            // For now, we'll just log the status
        } else if (!start_ap) {
            // STA mode but not connected
            ESP_LOGW(TAG, "WiFi status: STA mode but not connected");
        } else {
            // AP mode
            ESP_LOGI(TAG, "WiFi status: AP mode active (SSID: LEDBrain-Setup-C6)");
        }
    }
}

