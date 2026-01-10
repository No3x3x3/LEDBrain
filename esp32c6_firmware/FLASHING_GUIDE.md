# Przewodnik wgrywania firmware na ESP32-C6 dla płytki JC-ESP32P4-M3-DEV

## 📋 Wprowadzenie

Płytka **JC-ESP32P4-M3-DEV** zawiera dwa mikrokontrolery:
- **ESP32-P4** - główny procesor (programowany przez port USB-C głównej płytki)
- **ESP32-C6** - współprocesor WiFi (wymaga osobnego programowania)

ESP32-P4 **nie ma wbudowanego WiFi**, dlatego potrzebuje ESP32-C6 jako współprocesora WiFi.

## 🔌 Opcje połączenia z ESP32-C6

Płytka JC-ESP32P4-M3-DEV może mieć różne konfiguracje. Sprawdź dokumentację płytki i schematy w `docs/hardware/`.

### Opcja 1: Port USB 1.1 OTG Full-Speed (Type-C) dla ESP32-C6 ✅

Płytka **JC-ESP32P4-M3-DEV** ma **3 porty USB**:

1. **USB 2.0 OTG High-Speed (Type-C)** - dla ESP32-P4 (główny procesor)
   - Służy do zasilania, programowania ESP32-P4 i debugowania
   
2. **USB 1.1 OTG Full-Speed (Type-C)** - dla ESP32-C6 (współprocesor WiFi) ⭐
   - **To jest port dla ESP32-C6!**
   - Ma wbudowany konwerter USB-UART
   - Służy do programowania i debugowania ESP32-C6
   
3. **USB-A** - dla urządzeń peryferyjnych

**Jak zidentyfikować port ESP32-C6:**
- Znajdź port USB 1.1 OTG Full-Speed (Type-C) na płytce
- To będzie **środkowy port USB-C** lub port oznaczony jako "C6" / "WiFi Coprocessor"
- **Podłącz kabel USB-C** do tego portu
- **Zidentyfikuj port COM** w systemie:
  - **Windows**: Otwórz Device Manager → Porty (COM i LPT) → szukaj "USB Serial" lub "CH340"
  - **Linux**: `ls /dev/ttyUSB*` lub `ls /dev/ttyACM*`
  - **macOS**: `ls /dev/cu.*` lub `ls /dev/tty.*`

### Opcja 2: Przełącznik/switch do wyboru programowania

Niektóre płytki mają przełącznik do wyboru, który mikrokontroler programować:

1. **Ustaw przełącznik** w pozycji "C6" lub "WiFi Coprocessor"
2. **Użyj głównego portu USB-C** płytki
3. Port COM będzie reprezentował ESP32-C6 zamiast ESP32-P4

### Opcja 3: Programowanie przez ESP32-P4 (zaawansowane)

Jeśli płytka nie ma osobnego portu, może być możliwość programowania ESP32-C6 przez ESP32-P4:

1. Najpierw wgraj specjalny firmware "bootloader/bridge" na ESP32-P4
2. ESP32-P4 będzie działał jako most między USB a ESP32-C6
3. Wymaga dodatkowej konfiguracji (sprawdź dokumentację płytki)

### Opcja 4: Zewnętrzny programator UART

Jeśli masz dostęp do pinów ESP32-C6 (boot, reset, TX, RX, GND, VCC):

1. **Podłącz zewnętrzny programator USB-to-UART** (np. CP2102, CH340, FT232):
   - **GND** → GND płytki
   - **VCC** → 3.3V (nie 5V!)
   - **TX** → RX ESP32-C6 (GPIO17)
   - **RX** → TX ESP32-C6 (GPIO18)
   - **DTR** → Boot pin ESP32-C6 (opcjonalnie, dla auto-reset)
   - **RTS** → Reset pin ESP32-C6 (opcjonalnie)

2. **Użyj portu COM** programatora

## 📝 Krok po kroku: Wgrywanie firmware na ESP32-C6

### Krok 1: Przygotowanie środowiska

Upewnij się, że masz zainstalowane:
- ESP-IDF v5.5.0 lub nowsze
- Python 3.8+
- Sterowniki USB-to-Serial (CH340, CP2102, FTDI)

### Krok 2: Aktywacja środowiska ESP-IDF

**Windows PowerShell:**
```powershell
& C:\Espressif\frameworks\esp-idf-v5.5.2\export.ps1
```

**Linux/macOS:**
```bash
. $HOME/esp/esp-idf/export.sh
```

### Krok 3: Przejdź do katalogu firmware ESP32-C6

```bash
cd esp32c6_firmware
```

### Krok 4: Ustaw target na ESP32-C6

```bash
idf.py set-target esp32c6
```

### Krok 5: Konfiguracja WiFi (WYMAGANE!)

**Przed wgraniem firmware musisz skonfigurować SSID i hasło WiFi:**

```bash
idf.py menuconfig
```

Nawiguj:
1. Przejdź do **"LEDBrain C6 Coprocessor Configuration"**
2. Ustaw **`ESP_WIFI_SSID`** - nazwa Twojej sieci WiFi
3. Ustaw **`ESP_WIFI_PASSWORD`** - hasło do sieci WiFi
4. Opcjonalnie: dostosuj piny UART (domyślnie: TX=GPIO18, RX=GPIO17)
5. Opcjonalnie: zmień baudrate (domyślnie: 921600)
6. Zapisz konfigurację: **Save** → **Exit**

### Krok 6: Kompilacja firmware

```bash
idf.py build
```

Powinno zakończyć się bez błędów.

### Krok 7: Znajdź port COM ESP32-C6

**Windows:**
```powershell
# Lista wszystkich portów COM
[System.IO.Ports.SerialPort]::getportnames()

# Lub w Device Manager
# Start → Device Manager → Ports (COM & LPT)
```

**Linux:**
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

**macOS:**
```bash
ls /dev/cu.* /dev/tty.*
```

**Typowe nazwy portów:**
- Windows: `COM3`, `COM4`, `COM5`...
- Linux: `/dev/ttyUSB0`, `/dev/ttyACM0`...
- macOS: `/dev/cu.usbserial-*`, `/dev/cu.usbmodem*`

### Krok 8: Wgraj firmware

**Windows:**
```bash
idf.py -p COM3 flash monitor
```
(Zamień `COM3` na właściwy port)

**Linux/macOS:**
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```
(Zamień `/dev/ttyUSB0` na właściwy port)

### Krok 9: Weryfikacja

Po wgraniu powinieneś zobaczyć w monitorze:

```
LEDBrain ESP32-C6 WiFi Coprocessor Firmware
ESP-IDF version: v5.5.2
Initializing WiFi Station...
Connecting to WiFi SSID: [Twoja sieć]
Got IP address: [IP z DHCP]
Initializing PPP server over UART...
Starting PPP server on UART0 (TX: GPIO18, RX: GPIO17, Baud: 921600)
PPP server started successfully
Enabling NAPT for Internet sharing...
NAPT enabled - ESP32-P4 can now access Internet via ESP32-C6
ESP32-C6 WiFi coprocessor ready
Waiting for ESP32-P4 to connect via PPP...
```

## 🔧 Rozwiązywanie problemów

### Problem: Nie widzę portu COM dla ESP32-C6

**Rozwiązania:**
1. Sprawdź, czy kabel USB jest podłączony
2. Sprawdź sterowniki USB-to-Serial (zainstaluj CH340, CP2102 lub FTDI)
3. Spróbuj innego kabla USB
4. Sprawdź Device Manager (Windows) czy widzi urządzenie
5. Sprawdź schemat płytki, czy ESP32-C6 ma osobny port USB

### Problem: `Permission denied` (Linux/macOS)

**Rozwiązanie:**
```bash
sudo chmod 666 /dev/ttyUSB0
# Lub dodaj użytkownika do grupy dialout
sudo usermod -a -G dialout $USER
# Następnie wyloguj się i zaloguj ponownie
```

### Problem: ESP32-C6 nie łączy się z WiFi

**Sprawdź:**
1. Czy SSID i hasło są poprawne w `menuconfig`
2. Czy sieć WiFi jest dostępna (zasięg sygnału)
3. Czy sieć używa WPA2/WPA3 (obsługiwane)
4. Sprawdź logi w monitorze serial - zobaczysz dokładny błąd

### Problem: ESP32-P4 nie łączy się z ESP32-C6

**Sprawdź:**
1. Czy piny UART są poprawne:
   - ESP32-P4 TX (GPIO32) → ESP32-C6 RX (GPIO17)
   - ESP32-C6 TX (GPIO18) → ESP32-P4 RX (GPIO33)
2. Czy baudrate jest taki sam (921600) na obu stronach
3. Czy ESP32-C6 działa (sprawdź logi)
4. Czy ESP32-P4 próbuje połączyć się (sprawdź logi ESP32-P4)

### Problem: Boot mode nie uruchamia się automatycznie

**Rozwiązanie:**
Ręczne przejście w tryb bootloader:
1. Przytrzymaj przycisk **BOOT** (lub **IO0**) na ESP32-C6
2. Naciśnij i zwolnij przycisk **RESET**
3. Zwolnij przycisk **BOOT**
4. ESP32-C6 jest teraz w trybie bootloader
5. Spróbuj wgrać firmware ponownie

**Alternatywa - użyj flash z ręcznym resetem:**
```bash
idf.py -p COM3 flash --before default_reset --after hard_reset
```

## 📚 Dodatkowe informacje

### Aktualizacja firmware ESP32-C6

Możesz zaktualizować firmware ESP32-C6 w dowolnym momencie:

```bash
cd esp32c6_firmware
idf.py set-target esp32c6
idf.py menuconfig  # Opcjonalnie - zmień konfigurację
idf.py build
idf.py -p COM3 flash monitor
```

**Uwaga:** Podczas aktualizacji ESP32-C6, ESP32-P4 **nie będzie miał dostępu do WiFi** aż ESP32-C6 nie zakończy aktualizacji.

### Sprawdzanie aktualnej konfiguracji

Możesz sprawdzić, jakie ustawienia są zapisane w firmware:

```bash
idf.py menuconfig
# Przejdź do "LEDBrain C6 Coprocessor Configuration"
# Zobaczysz aktualne wartości
```

### Weryfikacja komunikacji PPP

Po wgraniu firmware na oba urządzenia (ESP32-P4 i ESP32-C6):

1. **ESP32-C6** powinien logować:
   ```
   PPP server started successfully
   Waiting for ESP32-P4 to connect via PPP...
   ```

2. **ESP32-P4** powinien logować (w `main.cpp`):
   ```
   Initializing WiFi via ESP32-C6 coprocessor (UART)...
   Connecting to ESP32-C6 via UART1 (TX: GPIO32, RX: GPIO33, Baud: 921600)
   WiFi C6 initialized successfully
   WiFi connected via ESP32-C6
   ```

3. **Sprawdź IP na ESP32-P4:**
   ```
   WiFi IP: 192.168.11.2  (PPP client IP)
   ```

4. **Sprawdź IP na ESP32-C6:**
   ```
   WiFi IP: [IP z routera]  (WiFi STA IP)
   PPP IP: 192.168.11.1  (PPP server IP)
   ```

## 🔄 Workflow: Pierwsza instalacja

1. **Wgraj firmware na ESP32-C6** (ten przewodnik)
2. **Wgraj firmware na ESP32-P4** (główny projekt LEDBrain)
3. **Uruchom oba urządzenia**
4. **ESP32-P4 automatycznie połączy się z ESP32-C6** (jeśli Ethernet nie jest dostępny)
5. **ESP32-C6 zapewni WiFi** dla ESP32-P4

## 📞 Wsparcie

Jeśli masz problemy:
1. Sprawdź schematy płytki w `docs/hardware/schematics/`
2. Sprawdź dokumentację płytki JC-ESP32P4-M3-DEV
3. Sprawdź logi serial na obu urządzeniach
4. Zweryfikuj konfigurację pinów UART

---

**Powodzenia! 🚀**

