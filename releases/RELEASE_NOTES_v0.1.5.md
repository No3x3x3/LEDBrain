# LEDBrain v0.1.5 - OTA Update Detection Fix

## 🐛 Bug Fixes

### OTA Update Detection
- ✅ Fixed OTA update detection bug (`endswith` → `endsWith`)
- ✅ Release v0.1.4 is now properly detected as available update
- ✅ Added debug logging for OTA version comparison

## 📦 Installation

### Flash via ESP-IDF
```bash
idf.py set-target esp32p4
idf.py -p COMx flash
```

### Flash via Flash Download Tool
1. Download `ledbrain-v0.1.5-esp32p4.bin`
2. Use ESP32 Flash Download Tool
3. Flash at offset `0x20000` (OTA partition)

## 🔄 Upgrade Notes

- This is a hotfix for OTA update detection
- If you're on v0.1.3 or earlier, you can now update to v0.1.4 or v0.1.5 via OTA
- No configuration migration needed

## 📋 Requirements

- ESP32-P4 development board
- Ethernet PHY (LAN8720, IP101, or DP83848) or WiFi via ESP32-C6
- ESP-IDF 5.5.0+ for building from source

## 🙏 Acknowledgments

- Built with ESP-IDF framework
- Inspired by WLED and LEDFx projects
