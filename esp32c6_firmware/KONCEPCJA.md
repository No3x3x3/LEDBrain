# Koncepcja Firmware ESP32-C6 dla LEDBrain

## Dlaczego osobny firmware?

ESP32-P4 **nie ma wbudowanego WiFi**. Płytka JC-ESP32P4-M3-DEV używa **ESP32-C6 jako współprocesora WiFi**.

## Jak to działa?

```
┌─────────────────┐         UART (PPP)         ┌─────────────────┐
│   ESP32-P4      │  <───────────────────────> │   ESP32-C6      │
│   (HOST)        │  TX: GPIO32, RX: GPIO33    │   (SLAVE)       │
│                 │  Baud: 921600              │                 │
│  - LEDBrain     │                            │  - WiFi STA     │
│  - Ethernet     │                            │  - PPP Server   │
│  - PPP Client   │                            │  - NAPT Router  │
│                 │                            │                 │
│  IP: 192.168.   │                            │  WiFi IP: DHCP  │
│  11.2           │                            │  PPP IP: 192.   │
│                 │                            │  168.11.1       │
└─────────────────┘                            └─────────────────┘
       │                                              │
       │                                              │
       └─────────────── Ethernet ────────────────────┘
                                   WiFi
                                    │
                                    ▼
                              ┌─────────┐
                              │ Router  │
                              │ Internet│
                              └─────────┘
```

## Dwa osobne firmware:

### 1. Firmware ESP32-P4 (główny projekt LEDBrain)
- **Lokalizacja**: `main/` (główny projekt)
- **Rola**: Główna aplikacja LEDBrain
- **Komunikacja**: Działa jako **PPP Client**, łączy się z ESP32-C6
- **Plik**: `main/wifi_c6.cpp` - obsługuje połączenie PPP z ESP32-C6

### 2. Firmware ESP32-C6 (współprocesor WiFi)
- **Lokalizacja**: `esp32c6_firmware/` (osobny projekt)
- **Rola**: WiFi gateway dla ESP32-P4
- **Funkcje**:
  - Łączy się z WiFi jako Station (STA)
  - Działa jako **PPP Server** na UART
  - Włącza **NAPT (Network Address Port Translation)**
  - Przekierowuje ruch sieciowy z PPP do WiFi

## Komunikacja UART

- **ESP32-P4 TX (GPIO32)** → **ESP32-C6 RX (GPIO17)**
- **ESP32-C6 TX (GPIO18)** → **ESP32-P4 RX (GPIO33)**
- **Baudrate**: 921600 (do 3Mbps wspierane)
- **Protokół**: PPP (Point-to-Point Protocol)

## Przykład przepływu danych:

1. ESP32-P4 chce wysłać pakiet HTTP do `google.com`
2. Pakiet trafia przez interfejs PPP (UART) do ESP32-C6
3. ESP32-C6 odbiera pakiet przez UART (PPP)
4. ESP32-C6 wysyła pakiet przez WiFi do routera
5. Router przekazuje do Internetu
6. Odpowiedź wraca tą samą drogą (WiFi → ESP32-C6 → PPP → UART → ESP32-P4)

## Instalacja:

### Krok 1: Zainstaluj firmware na ESP32-C6

```bash
cd esp32c6_firmware
idf.py set-target esp32c6
idf.py menuconfig  # Skonfiguruj WiFi SSID i hasło
idf.py build
idf.py -p COMX flash  # Gdzie COMX to port ESP32-C6
```

### Krok 2: ESP32-P4 automatycznie się połączy

Firmware ESP32-P4 (główny projekt) automatycznie:
1. Sprawdza czy Ethernet jest dostępny
2. Jeśli nie - próbuje połączyć się z ESP32-C6 przez UART
3. Uruchamia PPP Client i łączy się z ESP32-C6

## Zalety tego podejścia:

✅ **Modularność**: ESP32-C6 działa niezależnie, można go zaktualizować osobno
✅ **Wydajność**: WiFi nie obciąża ESP32-P4
✅ **Elastyczność**: Można użyć ESP32-C6 z innymi mikrokontrolerami
✅ **Separacja**: Problemy z WiFi nie wpływają na główną aplikację

## Konfiguracja pinów:

Piny muszą być **zweryfikowane** w schemacie płytki `docs/hardware/schematics/`.

**Obecne ustawienia (do weryfikacji):**
- ESP32-P4: TX=GPIO32, RX=GPIO33 (UART1)
- ESP32-C6: TX=GPIO18, RX=GPIO17 (UART0)

**Ważne**: GPIO9 na ESP32-P4 jest oznaczony jako "C6_IO9" w pinout.cpp - może być używany do komunikacji z ESP32-C6.

