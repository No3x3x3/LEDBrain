# LEDBrain v0.1.0 - Early Release

## 🎉 First Public Release

This is an early release of LEDBrain - a powerful ESP32-P4 based LED controller combining WLED and LEDFx functionality.

## ✨ Features

### Core Functionality
- ✅ Dual effect engine (WLED + LEDFx)
- ✅ Physical LED strip control (WS2812/SK6812)
- ✅ WLED device integration via DDP
- ✅ Audio reactivity with Snapcast
- ✅ Modern web interface (Polish/English)
- ✅ MQTT Home Assistant integration
- ✅ OTA firmware updates
- ✅ Ethernet support

### Effect Support
- **30+ WLED effects**: Rainbow, Fire, Meteor, Scanner, Energy Flow, and more
- **10+ LEDFx effects**: Waves, Plasma, Matrix, Energy Flow, all with audio reactivity

### Web Interface
- Responsive design (desktop, tablet, mobile)
- Auto-save configuration
- Real-time effect preview
- Intuitive controls with helpful descriptions
- Accordion sections for advanced settings

## 📦 Installation

### Flash via ESP-IDF
```bash
idf.py set-target esp32p4
idf.py -p COM3 flash
```

### Flash via Flash Download Tool
1. Download `ledbrain-v0.1.0-esp32p4.bin`
2. Use ESP32 Flash Download Tool
3. Flash at offset `0x20000`

## 🚀 Quick Start

1. Flash firmware to ESP32-P4 board
2. Connect Ethernet cable
3. Access web interface at `http://ledbrain.local` or device IP
4. Configure network settings
5. Add LED segments or WLED devices
6. Assign effects and enjoy!

## 📋 Requirements

- ESP32-P4 development board (tested on JC-ESP32P4-M3-DEV)
- Ethernet PHY (LAN8720, IP101, or DP83848)
- ESP-IDF 5.5.0+ for building from source

## 📚 Documentation

See [README.md](../README.md) and [docs/README.md](../docs/README.md) for detailed documentation.

## ⚠️ Known Issues

- List of effects may show limited options in some browsers (workaround: refresh page)
- WLED DDP requires manual enable in device configuration
- Some advanced settings may need page refresh to apply

## 🔄 What's Next

- More WLED and LEDFx effects
- Improved effect catalog filtering
- Enhanced audio reactivity profiles
- Matrix layout editor
- Scene scheduling

## 🙏 Acknowledgments

- Inspired by [WLED](https://github.com/Aircoookie/WLED) and [LEDFx](https://github.com/LedFx/LedFx)
- Built with ESP-IDF framework

