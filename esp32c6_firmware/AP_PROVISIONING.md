# WiFi AP Provisioning dla ESP32-C6 - Plan implementacji

## Problem

Obecny firmware ESP32-C6 **tylko próbuje połączyć się jako STA** (Station) z prekonfigurowanym SSID/hasłem w `menuconfig`. To powoduje problem:

**Jak skonfigurować WiFi bez Ethernet?**
- ESP32-P4 nie ma WiFi → nie może uruchomić AP
- ESP32-C6 ma WiFi, ale nie uruchamia AP → użytkownik nie może się połączyć
- Bez AP → brak dostępu do web interface → brak konfiguracji WiFi
- **Błędne koło!** 🔄

## Rozwiązanie: Smart AP/STA Mode

ESP32-C6 powinien działać w trybie **AP+STA** (Access Point + Station):

### 1. Start jako AP (jeśli nie ma zapisanych danych WiFi)

**Logika:**
```
Jeśli NVS nie zawiera zapisanych danych WiFi:
  1. Uruchom WiFi AP (Access Point)
     - SSID: "LEDBrain-Setup-C6" (lub podobny)
     - Hasło: "ledbrain123" (lub puste dla open)
     - IP: 192.168.4.1
  2. Uruchom web server na ESP32-C6 (opcjonalnie, prosty)
  3. Uruchom PPP server (ESP32-P4 może się połączyć przez AP)
  4. Czekaj na konfigurację WiFi przez użytkownika
```

### 2. Po otrzymaniu konfiguracji WiFi

**Logika:**
```
Gdy użytkownik skonfiguruje WiFi (przez ESP32-P4 lub bezpośrednio):
  1. Zapisz SSID/hasło w NVS
  2. Przełącz WiFi na tryb STA (połącz się z siecią)
  3. Jeśli połączenie udane:
     - Zatrzymaj AP (opcjonalnie - można zostawić dla łatwiejszego dostępu)
     - Włącz NAPT (przekierowanie z PPP do WiFi)
  4. Jeśli połączenie nieudane:
     - Wróć do AP mode
     - Wyświetl błąd
```

### 3. Tryb mieszany (AP+STA)

**Najlepsze rozwiązanie:**
```
1. Uruchom WiFi w trybie AP+STA:
   - AP: dla konfiguracji (192.168.4.1)
   - STA: dla normalnej pracy (jeśli skonfigurowane)

2. ESP32-P4 może łączyć się przez:
   - AP ESP32-C6 → konfiguracja WiFi
   - STA ESP32-C6 → normalna praca (przez PPP)

3. Po połączeniu STA → włącz NAPT dla Internetu
```

## Implementacja - Wymagane zmiany

### 1. Firmware ESP32-C6 (`esp32c6_firmware/main/main.c`)

#### A. Sprawdzenie NVS przy starcie

```c
// Sprawdź czy są zapisane dane WiFi w NVS
nvs_handle_t nvs_handle;
nvs_open("wifi_cfg", NVS_READONLY, &nvs_handle);

size_t required_size = sizeof(wifi_config_t);
wifi_config_t wifi_config;

esp_err_t err = nvs_get_blob(nvs_handle, "sta_config", &wifi_config, &required_size);
if (err != ESP_OK) {
    // Brak zapisanych danych → uruchom AP
    ESP_LOGI(TAG, "No saved WiFi config, starting AP mode for provisioning");
    wifi_start_ap_mode();
} else {
    // Są zapisane dane → uruchom STA
    ESP_LOGI(TAG, "Found saved WiFi config, starting STA mode");
    wifi_start_sta_mode(&wifi_config);
}
nvs_close(nvs_handle);
```

#### B. Funkcja AP Mode

```c
static esp_err_t wifi_start_ap_mode(void)
{
    // Utwórz AP network interface
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    
    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = "LEDBrain-Setup-C6",
            .password = "ledbrain123",
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = 4,
            .channel = 1,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));  // AP+STA
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi AP started: SSID=LEDBrain-Setup-C6, IP=192.168.4.1");
    return ESP_OK;
}
```

#### C. Funkcja STA Mode (z AP w tle)

```c
static esp_err_t wifi_start_sta_mode(wifi_config_t *sta_config)
{
    // Uruchom AP (dla łatwiejszego dostępu)
    wifi_start_ap_mode();
    
    // Skonfiguruj STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));  // AP+STA
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    ESP_LOGI(TAG, "WiFi STA started, connecting to: %s", sta_config->sta.ssid);
    return ESP_OK;
}
```

#### D. Komunikacja z ESP32-P4 przez UART (opcjonalnie)

Aby ESP32-P4 mógł skonfigurować WiFi na ESP32-C6, potrzebny jest protokół komunikacji przez UART:

```c
// Oprócz PPP, użyj dodatkowego kanału UART dla komend konfiguracyjnych
// Lub użyj prostego protokołu przez PPP (HTTP przez PPP)

// Przykład: prosty protokół komend przez UART
typedef struct {
    uint8_t cmd;        // 0=GET_STATUS, 1=SET_WIFI, 2=SCAN, etc.
    uint8_t data_len;
    uint8_t data[256];
} wifi_ctrl_cmd_t;
```

### 2. Firmware ESP32-P4 (`main/wifi_c6.cpp`)

#### A. Implementacja rzeczywistej funkcji `wifi_c6_ap_start()`

```cpp
esp_err_t wifi_c6_ap_start(const std::string& ssid, const std::string& password) {
  if (!s_initialized) {
    ESP_LOGE(TAG, "WiFi C6 not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  // TODO: Wyślij komendę do ESP32-C6 przez UART
  // aby uruchomić AP mode
  // Obecnie to jest placeholder
  
  // Opcja 1: Komunikacja przez dodatkowy kanał UART
  // Opcja 2: HTTP przez PPP (gdy PPP jest aktywne)
  // Opcja 3: Protokół kontrolny w eppp_link
  
  ESP_LOGW(TAG, "WiFi AP start: Requires ESP32-C6 firmware support");
  return ESP_ERR_NOT_SUPPORTED;
}
```

#### B. Funkcja konfiguracji WiFi na ESP32-C6

```cpp
esp_err_t wifi_c6_configure_sta(const std::string& ssid, const std::string& password) {
  // TODO: Wyślij komendę do ESP32-C6 aby skonfigurować WiFi STA
  // Przez UART lub przez HTTP przez PPP
  
  // Po skonfigurowaniu, ESP32-C6 powinien:
  // 1. Zapisać SSID/hasło w NVS
  // 2. Przełączyć się na tryb STA
  // 3. Połączyć się z siecią
  // 4. Włączyć NAPT
  
  return ESP_OK;
}
```

### 3. Web Interface (ESP32-P4)

#### A. Odkomentowanie endpointów WiFi

W `main/web_setup.cpp`:
```cpp
// Odkomentuj:
httpd_uri_t u_wifi_scan = { .uri="/api/wifi/scan", .method=HTTP_GET, .handler=api_wifi_scan, .user_ctx=NULL };
httpd_uri_t u_wifi_connect = { .uri="/api/wifi/connect", .method=HTTP_POST, .handler=api_wifi_connect, .user_ctx=NULL };
```

#### B. Implementacja `api_wifi_scan()` - przez ESP32-C6

```cpp
static esp_err_t api_wifi_scan(httpd_req_t* req) {
  // Wyślij komendę SCAN do ESP32-C6 przez UART
  // ESP32-C6 wykona skanowanie WiFi
  // Zwróć wyniki przez JSON
  // ...
}
```

#### C. Implementacja `api_wifi_connect()` - przez ESP32-C6

```cpp
static esp_err_t api_wifi_connect(httpd_req_t* req) {
  // Parsuj SSID i hasło z JSON
  // Wyślij komendę SET_WIFI do ESP32-C6
  // ESP32-C6 skonfiguruje WiFi STA
  // Zwróć status
  // ...
}
```

## Alternatywne rozwiązanie: Web Server na ESP32-C6

**Jeśli komunikacja przez UART jest zbyt skomplikowana:**

1. **ESP32-C6 uruchamia własny web server** (prosty, minimalny)
2. **Użytkownik łączy się z AP ESP32-C6** (np. `LEDBrain-Setup-C6`)
3. **Konfiguruje WiFi przez web interface na ESP32-C6**
4. **ESP32-C6 zapisuje konfigurację i przełącza się na STA**
5. **ESP32-P4 łączy się przez PPP z ESP32-C6**

**Zalety:**
- Prostsze (nie wymaga protokołu przez UART)
- Niezależne od ESP32-P4
- Może działać nawet gdy ESP32-P4 nie jest jeszcze skonfigurowane

**Wady:**
- Dwa osobne web interfejsy (ESP32-C6 i ESP32-P4)
- Trzeba utrzymywać dodatkowy kod na ESP32-C6

## Proponowane rozwiązanie (najprostsze)

### Faza 1: Smart AP Mode w ESP32-C6

1. **ESP32-C6 startuje jako AP** jeśli nie ma zapisanych danych WiFi
2. **ESP32-P4 łączy się z AP ESP32-C6 przez PPP**
3. **ESP32-P4 ma dostęp do web interface** (przez PPP)
4. **Użytkownik konfiguruje WiFi** przez web interface ESP32-P4
5. **ESP32-P4 wysyła komendę do ESP32-C6** (prosty protokół przez UART)
6. **ESP32-C6 konfiguruje WiFi STA i przełącza się**

### Faza 2: Protokół komunikacji ESP32-P4 ↔ ESP32-C6

**Opcja A: Dodatkowy UART** (najprostsze)
- Użyj innego UART niż PPP (np. UART2)
- Prosty protokół tekstowy: `CMD:SET_WIFI|SSID:xxx|PASS:yyy`

**Opcja B: Kontrolny kanał w eppp_link**
- Rozszerz `eppp_link` o kanał kontrolny
- Wymaga modyfikacji `eppp_link`

**Opcja C: HTTP przez PPP** (najbardziej elastyczne)
- ESP32-C6 uruchamia web server
- ESP32-P4 wysyła HTTP requesty przez PPP
- RESTful API: `POST /api/wifi/configure`

## Zalecany plan działania

1. ✅ **Zrozum problem** (zrobione)
2. ⬜ **Zmodyfikuj firmware ESP32-C6** - Smart AP mode
3. ⬜ **Dodaj protokół komunikacji** - UART lub HTTP przez PPP
4. ⬜ **Zaimplementuj funkcje w ESP32-P4** - `wifi_c6_configure_sta()`
5. ⬜ **Odkomentuj endpointy WiFi** w web_setup.cpp
6. ⬜ **Przetestuj** pełny flow: AP → konfiguracja → STA → PPP → Internet

---

**Status:** 📋 Plan - do implementacji
