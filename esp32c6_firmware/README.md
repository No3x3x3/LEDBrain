# ESP32-C6 WiFi Coprocessor Firmware

Ten projekt zawiera firmware dla ESP32-C6, który działa jako współprocesor WiFi dla ESP32-P4.

## Konfiguracja

ESP32-C6 działa jako:
- **WiFi Station (STA)**: Łączy się z siecią WiFi
- **PPPoS Server**: Udostępnia połączenie WiFi przez PPP do ESP32-P4
- **NAPT (Network Address Port Translation)**: Przekierowuje ruch sieciowy

## Komunikacja

- **Transport**: UART
- **Protokół**: PPP over UART
- **IP ESP32-C6 (Server)**: 192.168.11.1
- **IP ESP32-P4 (Client)**: 192.168.11.2

## Piny UART (do dostosowania według schematu)

- **ESP32-C6 TX**: GPIO18 (domyślnie) → ESP32-P4 RX: GPIO33
- **ESP32-C6 RX**: GPIO17 (domyślnie) → ESP32-P4 TX: GPIO32
- **Baudrate**: 921600

## Budowanie i flash

```bash
cd esp32c6_firmware
idf.py set-target esp32c6
idf.py menuconfig  # Skonfiguruj WiFi SSID i hasło
idf.py build
idf.py -p PORT flash
```

## Konfiguracja WiFi

1. Uruchom `idf.py menuconfig`
2. Przejdź do "Example Configuration"
3. Ustaw `ESP_WIFI_SSID` i `ESP_WIFI_PASSWORD`
4. Zapisz i zbuduj

## Funkcjonalność

Po uruchomieniu ESP32-C6:
1. Łączy się z siecią WiFi (STA mode)
2. Uruchamia serwer PPP na UART
3. Włącza NAPT, aby przekierowywać ruch z PPP do WiFi
4. ESP32-P4 może teraz korzystać z Internetu przez ESP32-C6

