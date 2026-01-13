# Rozwiązywanie problemów z zewnętrznym programatorem TTL-USB

## Problem: Programator nie pokazuje się w Menedżerze urządzeń

### Krok 1: Podstawowa diagnostyka sprzętowa

#### 1.1. Sprawdź połączenie USB
- ✅ **Odłącz i podłącz ponownie** kabel USB do komputera
- ✅ **Spróbuj innego portu USB** (najlepiej USB 2.0, unikaj hubów USB)
- ✅ **Spróbuj innego kabla USB** (niektóre kable są tylko do ładowania, bez transmisji danych)
- ✅ **Sprawdź czy dioda LED na programatorze świeci** (jeśli ma)

#### 1.2. Sprawdź zasilanie
- ⚠️ **WAŻNE**: Wczoraj działało po podłączeniu dodatkowego zasilania USB do płytki
- ✅ **Podłącz dodatkowe zasilanie USB do płytki** (jak wczoraj)
- ✅ **Sprawdź czy programator ma własne zasilanie** (niektóre wymagają zewnętrznego zasilania)
- ✅ **Nie podłączaj VCC z programatora do ESP32-C6** jeśli płytka ma już zasilanie

### Krok 2: Diagnostyka w Windows

#### 2.1. Sprawdź Menedżer urządzeń
1. Naciśnij `Win + X` → wybierz **"Menedżer urządzeń"**
2. Sprawdź sekcje:
   - **"Porty (COM i LPT)"** - szukaj "USB Serial Port", "CH340", "CP210x", "FTDI"
   - **"Inne urządzenia"** - jeśli widzisz urządzenie z żółtym trójkątem ⚠️, to brak sterowników
   - **"Uniwersalne kontrolery magistrali serialnej USB"** - może być tam

#### 2.2. Sprawdź wszystkie urządzenia USB
1. W Menedżerze urządzeń: **Widok → Pokaż ukryte urządzenia**
2. Sprawdź czy programator nie jest na liście jako "ukryty" lub "nieznany"

#### 2.3. Sprawdź w PowerShell
```powershell
# Lista wszystkich portów COM
[System.IO.Ports.SerialPort]::getportnames()

# Lista urządzeń USB (wymaga Get-PnpDevice)
Get-PnpDevice | Where-Object {$_.Class -eq "USB" -or $_.Class -eq "Ports"} | Format-Table -AutoSize
```

### Krok 3: Problem ze sterownikami

#### 3.1. Zidentyfikuj typ programatora
Sprawdź na programatorze lub w dokumentacji:
- **CH340/CH341** - najpopularniejszy, tani
- **CP2102/CP2104** - Silicon Labs
- **FT232/FT2232** - FTDI (droższe, ale bardziej niezawodne)
- **PL2303** - Prolific

#### 3.2. Zainstaluj/odinstaluj sterowniki

**Dla CH340:**
1. Pobierz: http://www.wch-ic.com/downloads/CH341SER_EXE.html
2. Zainstaluj jako administrator
3. **Odłącz i podłącz ponownie** programator

**Dla CP2102/CP2104:**
1. Pobierz: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
2. Zainstaluj jako administrator
3. **Odłącz i podłącz ponownie** programator

**Dla FTDI:**
1. Pobierz: https://ftdichip.com/drivers/vcp-drivers/
2. Zainstaluj jako administrator
3. **Odłącz i podłącz ponownie** programator

#### 3.3. Odinstaluj i zainstaluj ponownie sterowniki
1. W Menedżerze urządzeń: **Kliknij prawym na urządzenie → Odinstaluj urządzenie**
2. Zaznacz **"Usuń oprogramowanie sterownika dla tego urządzenia"**
3. **Odłącz programator**
4. **Zainstaluj sterowniki ponownie**
5. **Podłącz programator**

### Krok 4: Sprawdź czy programator działa

#### 4.1. Test na innym komputerze
- Podłącz programator do **innego komputera**
- Jeśli działa → problem z Windows/sterownikami na Twoim komputerze
- Jeśli nie działa → problem z programatorem

#### 4.2. Test z innym urządzeniem
- Podłącz programator do **innego urządzenia** (np. Arduino)
- Jeśli działa → problem z ESP32-C6/połączeniami
- Jeśli nie działa → problem z programatorem

### Krok 5: Sprawdź połączenia z ESP32-C6

#### 5.1. Weryfikacja połączeń
```
Programator TTL-USB    →    ESP32-C6
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
GND                    →    GND (wspólna masa)
VCC (3.3V!)           →    NIE PODŁĄCZAJ jeśli płytka ma zasilanie USB!
TX                     →    GPIO16 (UART0 RX ESP32-C6)
RX                     →    GPIO17 (UART0 TX ESP32-C6)
DTR (opcjonalnie)      →    GPIO9 (BOOT)
RTS (opcjonalnie)      →    EN (RESET)
```

**⚠️ WAŻNE:**
- **NIE podłączaj VCC z programatora** jeśli płytka ma już zasilanie USB
- **Użyj TYLKO 3.3V** jeśli musisz podłączyć VCC (5V uszkodzi ESP32-C6!)
- **GND musi być wspólny** między programatorem a płytką

#### 5.2. Sprawdź czy piny są poprawne
- Sprawdź schemat płytki w `docs/hardware/schematics/`
- Upewnij się, że piny UART0 ESP32-C6 są dostępne na płytce
- Niektóre płytki mają piny oznaczone inaczej

### Krok 6: Diagnostyka zaawansowana

#### 6.1. Sprawdź logi Windows
1. Otwórz **Podgląd zdarzeń** (`Win + R` → `eventvwr.msc`)
2. Przejdź do **"Dzienniki systemu"**
3. Podłącz programator
4. Sprawdź czy pojawiają się błędy związane z USB

#### 6.2. Sprawdź w Device Manager z podłączeniem
1. **Otwórz Menedżer urządzeń**
2. **Podłącz programator** (podczas gdy Menedżer jest otwarty)
3. **Odśwież widok** (`F5`)
4. Sprawdź czy pojawia się nowe urządzenie (nawet z błędem)

#### 6.3. Sprawdź czy Windows widzi urządzenie USB
```powershell
# W PowerShell jako administrator
Get-PnpDevice | Where-Object {$_.FriendlyName -like "*USB*" -or $_.FriendlyName -like "*Serial*" -or $_.FriendlyName -like "*CH340*" -or $_.FriendlyName -like "*CP210*" -or $_.FriendlyName -like "*FTDI*"} | Format-Table -AutoSize
```

### Krok 7: Rozwiązania specyficzne

#### 7.1. Problem: Programator działał wczoraj, dzisiaj nie
**Możliwe przyczyny:**
- ✅ **Zasilanie** - wczoraj działało z dodatkowym zasilaniem USB
- ✅ **Port USB** - spróbuj innego portu
- ✅ **Kabel USB** - może być uszkodzony
- ✅ **Sterowniki** - Windows mógł je zaktualizować/zmienić
- ✅ **Programator** - mógł się uszkodzić

**Rozwiązanie:**
1. **Podłącz dodatkowe zasilanie USB do płytki** (jak wczoraj)
2. **Spróbuj innego portu USB**
3. **Spróbuj innego kabla USB**
4. **Odinstaluj i zainstaluj sterowniki ponownie**

#### 7.2. Problem: Programator nie ma zasilania
**Objawy:**
- Brak diody LED na programatorze
- Nie pojawia się w Menedżerze urządzeń

**Rozwiązanie:**
- Sprawdź czy programator wymaga zewnętrznego zasilania
- Sprawdź czy kabel USB dostarcza zasilanie (niektóre kable są tylko do danych)
- Spróbuj kabla USB z zasilaniem

#### 7.3. Problem: Konflikt z innym urządzeniem
**Objawy:**
- Programator pojawia się, ale z błędem
- Port COM jest zajęty

**Rozwiązanie:**
```powershell
# Sprawdź czy port COM jest używany
Get-PnpDevice | Where-Object {$_.Class -eq "Ports"} | Format-Table -AutoSize

# Zamknij inne programy używające portu COM (Arduino IDE, Putty, itp.)
```

### Krok 8: Test programatora bez ESP32-C6

#### 8.1. Test z zwarcie TX-RX
1. **Odłącz programator od ESP32-C6**
2. **Zewrzyj TX i RX** na programatorze (krótkie spięcie)
3. **Podłącz programator do komputera**
4. **Otwórz terminal serial** (np. Putty, Arduino Serial Monitor)
5. **Wyślij znak** - powinien wrócić (loopback test)
6. Jeśli działa → programator jest OK, problem z połączeniem do ESP32-C6
7. Jeśli nie działa → problem z programatorem

### Krok 9: Alternatywne rozwiązania

#### 9.1. Użyj portu USB na płytce (jeśli dostępny)
- Sprawdź czy płytka JC-ESP32P4-M3-DEV ma port USB dla ESP32-C6
- Zobacz `FLASHING_GUIDE.md` - Opcja 1: Port USB 1.1 OTG Full-Speed

#### 9.2. Użyj innego programatora
- Jeśli masz dostęp do innego programatora (CP2102, FTDI), spróbuj go
- Różne programatory mogą działać lepiej na różnych systemach

#### 9.3. Programowanie przez ESP32-P4 (zaawansowane)
- Jeśli płytka to obsługuje, możesz programować ESP32-C6 przez ESP32-P4
- Wymaga specjalnego firmware na ESP32-P4

### Krok 10: Sprawdź czy programator nie jest uszkodzony

**Objawy uszkodzonego programatora:**
- ❌ Nie świeci dioda LED (jeśli ma)
- ❌ Nie pojawia się w Menedżerze urządzeń na żadnym komputerze
- ❌ Pojawia się jako "nieznane urządzenie" z błędem
- ❌ Fizyczne uszkodzenia (spalone elementy, uszkodzone piny)

**Test:**
- Podłącz do innego komputera
- Jeśli nie działa na żadnym → programator jest uszkodzony

## 🔧 Szybka lista kontrolna

Przed szukaniem dalej, sprawdź:

- [ ] Programator podłączony do komputera
- [ ] Kabel USB działa (spróbuj innego)
- [ ] Port USB działa (spróbuj innego)
- [ ] Sterowniki zainstalowane (sprawdź Menedżer urządzeń)
- [ ] Dodatkowe zasilanie USB podłączone do płytki (jak wczoraj)
- [ ] GND wspólny między programatorem a płytką
- [ ] VCC NIE podłączony (jeśli płytka ma zasilanie)
- [ ] TX/RX podłączone poprawnie (TX programatora → RX ESP32-C6, RX programatora → TX ESP32-C6)
- [ ] Programator działa na innym komputerze (test)

## 📞 Jeśli nic nie pomaga

1. **Sprawdź dokumentację programatora** - może mieć specjalne wymagania
2. **Sprawdź forum ESP32** - może ktoś miał podobny problem
3. **Spróbuj innego programatora** - może Twój jest uszkodzony
4. **Użyj portu USB na płytce** - jeśli płytka ma wbudowany port USB dla ESP32-C6

---

**Powodzenia! 🚀**
