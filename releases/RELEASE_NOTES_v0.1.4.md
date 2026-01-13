# LEDBrain v0.1.4 - UI Improvements & Bug Fixes

## 🎨 UI Enhancements

### Diagnostic Bar Improvements
- ✅ Changed control buttons to functional icons (planet, lightbulb, speaker)
- ✅ Added color coding: green for enabled, red for disabled (matching project theme)
- ✅ Removed text labels, using icons only for cleaner interface

### Device Control Improvements
- ✅ Added individual play/pause button for WLED devices
- ✅ Visual state updates for device toggle buttons
- ✅ Improved button states (play icon for stopped, pause icon for playing)

### Notification System
- ✅ Completely removed popup notifications system
- ✅ Cleaner user experience without interrupting messages

## 🐛 Bug Fixes

### WLED Device Management
- ✅ Fixed individual device stop/start functionality
- ✅ Devices now properly exit UDP/DDP mode when stopped individually
- ✅ Devices return to local effects when stopped
- ✅ Fixed effect selection not applying to WLED devices
- ✅ Fixed device state persistence when globally toggling effects

### State Management
- ✅ Added device state memory system
- ✅ When effects are globally disabled, device states are saved
- ✅ When effects are re-enabled, only previously enabled devices are restored
- ✅ Prevents unwanted devices from starting after global toggle

### Network Status
- ✅ Improved network traffic display (TX/RX rates)
- ✅ Better handling of missing traffic data

## 🔧 Technical Improvements

### Build System
- ✅ Added automatic sdkconfig fix script (`fix_sdkconfig.ps1`)
- ✅ Automatic OTA partition configuration on build
- ✅ Fixed flash size configuration (4MB)
- ✅ Build script now automatically fixes partition table settings

### Configuration
- ✅ Enhanced `sdkconfig.defaults.txt` with explicit partition settings
- ✅ Improved build reliability and consistency

## 📦 Installation

### Flash via ESP-IDF
```bash
idf.py set-target esp32p4
idf.py -p COMx flash
```

### Flash via Flash Download Tool
1. Download `ledbrain-v0.1.4-esp32p4.bin`
2. Use ESP32 Flash Download Tool
3. Flash at offset `0x20000` (OTA partition)

## 🔄 Upgrade Notes

- This version includes UI changes that improve usability
- Device states are now preserved when toggling effects globally
- No configuration migration needed

## 📋 Requirements

- ESP32-P4 development board
- Ethernet PHY (LAN8720, IP101, or DP83848) or WiFi via ESP32-C6
- ESP-IDF 5.5.0+ for building from source

## 🙏 Acknowledgments

- Built with ESP-IDF framework
- Inspired by WLED and LEDFx projects
