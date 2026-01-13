# LEDBrain v0.1.6 - UI Improvements & Bug Fixes

## 🎨 UI Enhancements

### Switch Controls
- ✅ Fixed switch display: red background when disabled, neon green when enabled
- ✅ Changed green color to vibrant neon/radioactive green (#39ff14)
- ✅ Improved switch visibility and contrast

### Form Layout
- ✅ Fixed MQTT broker form layout (removed checkerboard pattern)
- ✅ Fixed overlapping elements in configuration card
- ✅ Improved card header layout with flex-wrap

### Device Control
- ✅ Auto-enable device/segment when effect is selected
- ✅ Device automatically starts displaying effect after selection
- ✅ Improved visual feedback for device state

## 🐛 Bug Fixes

### Network Statistics
- ✅ Fixed network traffic statistics (TX/RX rates)
- ✅ Added DDP packet tracking for accurate statistics
- ✅ Network traffic now shows real values when UDP stream is active

### Display Issues
- ✅ Fixed ethernet/wifi icon display
- ✅ Improved network status indicator

## 📦 Installation

### Flash via ESP-IDF
```bash
idf.py set-target esp32p4
idf.py -p COMx flash
```

### Flash via Flash Download Tool
1. Download `ledbrain-v0.1.6-esp32p4.bin`
2. Use ESP32 Flash Download Tool
3. Flash at offset `0x20000` (OTA partition)

## 🔄 Upgrade Notes

- This version includes UI improvements and bug fixes
- Switches now have better visual feedback
- Network statistics are now accurate
- No configuration migration needed

## 📋 Requirements

- ESP32-P4 development board
- Ethernet PHY (LAN8720, IP101, or DP83848) or WiFi via ESP32-C6
- ESP-IDF 5.5.0+ for building from source

## 🙏 Acknowledgments

- Built with ESP-IDF framework
- Inspired by WLED and LEDFx projects
