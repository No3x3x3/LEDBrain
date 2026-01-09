# Plan implementacji: Smart WiFi AP/STA z automatycznym przełączaniem Ethernet↔WiFi

## Cel

Zaimplementować inteligentny system sieciowy:
- ✅ **Bez Ethernet** → ESP32-C6 uruchamia WiFi (AP jeśli brak sieci, STA jeśli jest zapisana)
- ✅ **Z Ethernet** → ESP32-P4 używa Ethernet, WiFi C6 wyłączony
- ✅ **AP Mode** → Lista wykrytych sieci, wybór, hasło, zapis
- ✅ **Automatyczne przełączanie** → Jeśli Ethernet zostanie podłączony, powrót do Ethernet

## Architektura

```
ESP32-P4 (HOST)
├─ Ethernet → Główny interfejs (priorytet 1)
└─ PPP Client → ESP32-C6 (priorytet 2, tylko jeśli brak Ethernet)

ESP32-C6 (COPROCESSOR)
├─ WiFi AP → "LEDBrain-Setup-C6" (jeśli brak zapisanej sieci)
├─ WiFi STA → Połączenie z siecią (jeśli jest zapisana)
├─ PPP Server → Komunikacja z ESP32-P4
└─ Protokół UART → Komunikacja kontrolna (konfiguracja WiFi)
```

## Etapy implementacji

### Etap 1: Firmware ESP32-C6 - Smart AP/STA Mode ⏳

**Plik:** `esp32c6_firmware/main/main.c`

**Zmiany:**
1. ✅ Sprawdzanie NVS przy starcie - czy są zapisane dane WiFi
2. ⬜ Funkcja `wifi_load_from_nvs()` - wczytanie konfiguracji WiFi z NVS
3. ⬜ Funkcja `wifi_save_to_nvs()` - zapis konfiguracji WiFi do NVS
4. ⬜ Funkcja `wifi_start_ap_mode()` - uruchomienie AP mode
5. ⬜ Funkcja `wifi_start_sta_mode()` - uruchomienie STA mode (z AP w tle)
6. ⬜ Funkcja `wifi_start_apsta_mode()` - tryb AP+STA
7. ⬜ Obsługa eventów WiFi - AP start/stop, STA connect/disconnect
8. ⬜ Skanowanie WiFi - funkcja do wykrywania dostępnych sieci

**Status:** W toku

### Etap 2: Protokół komunikacji ESP32-P4 ↔ ESP32-C6 ⏸️

**Cel:** ESP32-P4 musi móc komunikować się z ESP32-C6 aby:
- Skanować sieci WiFi
- Konfigurować WiFi STA
- Sprawdzać status WiFi

**Opcje:**
- **A. Dodatkowy UART** (najprostsze)
  - Użyj UART2 dla komunikacji kontrolnej
  - UART1 dla PPP (już używany)
  - Prosty protokół tekstowy: `CMD:SCAN`, `CMD:CONNECT|SSID:xxx|PASS:yyy`
  
- **B. HTTP przez PPP** (najbardziej elastyczne)
  - ESP32-C6 uruchamia web server (opcjonalnie)
  - ESP32-P4 wysyła HTTP requesty przez PPP
  - RESTful API: `GET /api/wifi/scan`, `POST /api/wifi/connect`

- **C. Kontrolny kanał w eppp_link** (najbardziej zaawansowane)
  - Rozszerz `eppp_link` o kanał kontrolny
  - Wymaga modyfikacji `eppp_link`

**Rekomendacja:** Opcja A (dodatkowy UART) - najprostsze do implementacji

### Etap 3: Funkcje WiFi w ESP32-P4 ⏸️

**Plik:** `main/wifi_c6.cpp`, `main/wifi_c6.hpp`

**Zmiany:**
1. ⬜ `wifi_c6_scan()` - skanowanie sieci WiFi przez ESP32-C6
2. ⬜ `wifi_c6_configure_sta()` - konfiguracja WiFi STA na ESP32-C6
3. ⬜ `wifi_c6_get_scan_results()` - pobranie wyników skanowania
4. ⬜ `wifi_c6_ap_start()` - rzeczywista implementacja (komunikacja z ESP32-C6)
5. ⬜ Protokół UART - wysyłanie/odbieranie komend

**Status:** Oczekuje na Etap 2

### Etap 4: Monitoring Ethernet w ESP32-P4 ⏸️

**Plik:** `main/main.cpp`, `main/eth_init.cpp`

**Zmiany:**
1. ⬜ Task monitorujący Ethernet - sprawdzanie co N sekund
2. ⬜ Event handler - `ETHERNET_EVENT_CONNECTED` / `ETHERNET_EVENT_DISCONNECTED`
3. ⬜ Logika przełączania:
   - Ethernet disconnect → uruchom WiFi C6
   - Ethernet connect → zatrzymaj WiFi C6, użyj Ethernet
4. ⬜ Priorytet interfejsów - Ethernet > WiFi

**Status:** Oczekuje na Etap 1-2

### Etap 5: Endpointy WiFi w Web Interface ⏸️

**Plik:** `main/web_setup.cpp`

**Zmiany:**
1. ⬜ Odkomentowanie endpointów WiFi (linie 881-883)
2. ⬜ Implementacja `api_wifi_scan()` - zwraca listę sieci WiFi
3. ⬜ Implementacja `api_wifi_connect()` - łączy się z wybraną siecią
4. ⬜ UI w `app.js` - wybór sieci, pole hasła, przycisk połączenia

**Status:** Oczekuje na Etap 3

## Szczegółowy plan: Etap 1 - Firmware ESP32-C6

### 1.1 Funkcje NVS (zapis/odczyt)

```c
// Wczytaj konfigurację WiFi z NVS
static esp_err_t wifi_load_from_nvs(wifi_config_t *sta_config) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    size_t required_size = sizeof(wifi_config_t);
    err = nvs_get_blob(nvs_handle, NVS_KEY_STA_CONFIG, sta_config, &required_size);
    nvs_close(nvs_handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved WiFi config in NVS");
        return ESP_ERR_NOT_FOUND;
    }
    
    return err;
}

// Zapisz konfigurację WiFi do NVS
static esp_err_t wifi_save_to_nvs(const wifi_config_t *sta_config) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    err = nvs_set_blob(nvs_handle, NVS_KEY_STA_CONFIG, sta_config, sizeof(wifi_config_t));
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi config saved to NVS: SSID=%s", sta_config->sta.ssid);
    }
    
    return err;
}
```

### 1.2 Funkcja AP Mode

```c
static esp_err_t wifi_start_ap_mode(void) {
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    if (!ap_netif) {
        ESP_LOGE(TAG, "Failed to create AP netif");
        return ESP_FAIL;
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
    return ESP_OK;
}
```

### 1.3 Funkcja STA Mode (z AP w tle)

```c
static esp_err_t wifi_start_sta_mode(const wifi_config_t *sta_config, bool keep_ap) {
    // Uruchom AP jeśli keep_ap == true (dla łatwiejszego dostępu)
    if (keep_ap) {
        wifi_start_ap_mode();
    } else {
        esp_netif_create_default_wifi_sta();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    if (!keep_ap) {
        ESP_ERROR_CHECK(esp_wifi_connect());
    } else {
        // W trybie AP+STA, STA łączy się automatycznie
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    
    ESP_LOGI(TAG, "WiFi STA started, connecting to: %s", sta_config->sta.ssid);
    return ESP_OK;
}
```

### 1.4 Główna logika w `app_main()`

```c
void app_main(void) {
    // ... inicjalizacja NVS ...
    
    wifi_config_t saved_sta_config;
    esp_err_t load_err = wifi_load_from_nvs(&saved_sta_config);
    
    bool wifi_sta_configured = (load_err == ESP_OK);
    bool start_ap = false;
    
    if (!wifi_sta_configured) {
        ESP_LOGI(TAG, "No saved WiFi config, starting AP mode for provisioning");
        start_ap = true;
    } else {
        ESP_LOGI(TAG, "Found saved WiFi config, starting STA mode: %s", saved_sta_config.sta.ssid);
        // Sprawdź czy SSID nie jest pusty
        if (strlen((char*)saved_sta_config.sta.ssid) == 0) {
            start_ap = true;
            wifi_sta_configured = false;
        }
    }
    
    // Inicjalizacja WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    s_wifi_event_group = xEventGroupCreate();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Rejestracja event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                                        &wifi_event_handler, NULL, NULL));
    
    esp_err_t wifi_ret;
    if (start_ap) {
        // Tryb AP (dla konfiguracji)
        wifi_ret = wifi_start_ap_mode();
    } else {
        // Tryb STA (z AP w tle dla łatwiejszego dostępu)
        wifi_ret = wifi_start_sta_mode(&saved_sta_config, true);  // keep_ap=true
    }
    
    // ... reszta kodu (PPP server, NAPT) ...
}
```

## Następne kroki

1. **Zaimplementować Etap 1** - Smart AP/STA mode w ESP32-C6
2. **Zaimplementować protokół komunikacji** - UART lub HTTP przez PPP
3. **Zaimplementować funkcje w ESP32-P4** - skanowanie, konfiguracja WiFi
4. **Dodać monitoring Ethernet** - automatyczne przełączanie
5. **Zaimplementować endpointy web** - UI dla konfiguracji WiFi

---

**Status ogólny:** 🔄 W toku - Etap 1 w implementacji
