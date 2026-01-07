# LEDBrain v0.1.1 - Update Release

## 🎉 Update Release

This update brings improved audio processing performance and bug fixes to LEDBrain - your powerful ESP32-P4 based LED controller combining WLED and LEDFx functionality.

## 🚀 What's New

### ⚡ Performance Improvements
- **Enhanced Audio Processing**: Integrated ESP-DSP library for optimized FFT operations
- **Better CPU Utilization**: Improved SIMD/Xai optimizations for ESP32-P4
- **Faster Audio Reactivity**: More efficient frequency analysis and beat detection

### 🐛 Bug Fixes
- Fixed DSP function naming compatibility
- Resolved variable redeclaration issues in audio processing
- Fixed web server configuration compatibility with ESP-IDF 5.5.2
- Improved build system compatibility

### 📦 Dependencies
- Added ESP-DSP library for advanced signal processing
- Updated component dependencies

## ✨ Najważniejsze Funkcjonalności

### 🎨 Dual Effect Engine - Podwójny Silnik Efektów
**Najfajniejsza funkcja!** LEDBrain łączy w sobie dwa potężne silniki efektów:

- **WLED Effects (30+ efektów)**: Klasyczne efekty wizualne
  - Rainbow, Fire, Meteor, Scanner, Energy Flow
  - Beat Pulse, Beat Bars, Beat Scatter
  - Fireworks, Rain, Pacifica, Ripple
  - I wiele więcej!

- **LEDFx Effects (10+ efektów)**: Zaawansowane efekty audio-reaktywne
  - Energy Flow, Waves, Plasma, Matrix
  - Wszystkie z pełną reaktywnością na muzykę!

**Możesz przełączać się między silnikami w locie przez interfejs web!**

### 🎵 Audio Reactivity - Reaktywność Audio
**Najbardziej imponująca funkcja!** Pełna integracja z Snapcast:

- **Real-time Audio Analysis**: Analiza audio w czasie rzeczywistym
- **Frequency Band Control**: Kontrola pasm częstotliwości (Bass, Mids, Treble)
- **Beat Detection**: Automatyczne wykrywanie rytmu
- **Custom Frequency Ranges**: Własne zakresy częstotliwości
- **Optimized FFT Processing**: Zoptymalizowane przetwarzanie FFT dzięki ESP-DSP

**Efekty LEDFx reagują na każdy beat i częstotliwość muzyki!**

### 💡 LED Control - Kontrola LED
- **Physical Strips**: Bezpośrednia kontrola pasków WS2812/SK6812
- **Matrix Support**: Pełne wsparcie dla matryc LED z rotacją i lustrzaniem
- **Power Management**: Automatyczne zarządzanie mocą
- **Gamma Correction**: Korekcja gamma dla dokładności kolorów

### 🌐 Network & Integration - Sieć i Integracja
- **WLED Integration**: Wysyłanie efektów do zdalnych urządzeń WLED przez DDP
- **Ethernet Support**: Stabilne połączenie przez Ethernet
- **mDNS Discovery**: Dostęp przez `http://ledbrain.local`
- **MQTT Home Assistant**: Pełna integracja z Home Assistant
- **OTA Updates**: Aktualizacje przez internet lub upload pliku

### 🖥️ Modern Web Interface - Nowoczesny Interfejs Web
- **Responsive Design**: Działa na desktop, tablet i telefon
- **Multi-language**: Polski i Angielski
- **Real-time Configuration**: Zmiany zapisują się automatycznie
- **Intuitive Controls**: Łatwy w użyciu interfejs

## 🎯 Najfajniejsze Funkcje

### 1. 🎨 Synchronizacja Multi-Device
Kontroluj wiele urządzeń WLED jednocześnie! Dodaj urządzenia przez auto-discovery lub ręcznie, i synchronizuj efekty na wszystkich urządzeniach w czasie rzeczywistym.

### 2. 🎵 Audio Visualization
Połącz LEDBrain z Snapcast i ciesz się wizualizacją muzyki w czasie rzeczywistym. Efekty LEDFx reagują na każdy beat, bas i częstotliwość!

### 3. 🌈 40+ Efektów
Masz do dyspozycji ponad 40 różnych efektów - od klasycznych efektów WLED po zaawansowane audio-reaktywne efekty LEDFx.

### 4. 🎛️ Zaawansowana Konfiguracja
Każdy efekt ma szczegółowe ustawienia:
- Kolory (Primary, Secondary, Tertiary)
- Gradienty i palety
- Jasność, intensywność, prędkość
- Audio reactivity z wyborem pasma częstotliwości
- Zaawansowane opcje (gamma, blend mode, layers)

### 5. 🔄 OTA Updates
Aktualizuj firmware bez podłączania kabla USB! Przez interfejs web możesz:
- Sprawdzić dostępne aktualizacje z GitHub
- Wgrać plik .bin bezpośrednio
- Automatyczna aktualizacja z URL

### 6. 🏠 Home Assistant Integration
Pełna integracja z Home Assistant przez MQTT:
- Auto-discovery urządzeń
- Kontrola przez HA
- Automatyzacje i sceny

## 📦 Installation - Instalacja

### Flash via ESP-IDF
```bash
idf.py set-target esp32p4
idf.py -p COM5 flash
```

### Flash via Flash Download Tool
1. Download `ledbrain-v0.1.1-esp32p4.bin`
2. Use ESP32 Flash Download Tool
3. Flash at offset `0x20000` (OTA partition)

**Note**: This build uses OTA partition (0x20000) instead of factory partition due to size.

## 🔄 Upgrade from v0.1.0

If you're upgrading from v0.1.0:
1. Download `ledbrain-v0.1.1-esp32p4.bin`
2. Use OTA update feature in web interface
3. Or flash directly to OTA partition at offset `0x20000`

## 📋 Requirements

- ESP32-P4 development board (tested on JC-ESP32P4-M3-DEV)
- Ethernet PHY (LAN8720, IP101, or DP83848)
- ESP-IDF 5.5.0+ for building from source
- ESP-DSP library (included in components/)

## 🐛 Known Issues

- Application size requires OTA partition (0x20000) instead of factory partition
- Some advanced settings may need page refresh to apply

## 🔄 What's Next

- More WLED and LEDFx effects
- Improved effect catalog filtering
- Enhanced audio reactivity profiles
- Matrix layout editor
- Scene scheduling
- More audio processing optimizations

## 🙏 Acknowledgments

- Inspired by [WLED](https://github.com/Aircoookie/WLED) and [LEDFx](https://github.com/LedFx/LedFx)
- Built with ESP-IDF framework
- Audio processing powered by ESP-DSP library

## 📚 Documentation

See [README.md](../README.md) and [docs/README.md](../docs/README.md) for detailed documentation.

---

**Enjoy your LEDBrain! 🎉✨**

