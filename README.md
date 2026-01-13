# LEDBrain

> ⚠️ **Warning: Project in active development**  
> This project is currently under construction and not ready for production use. Features may be incomplete, unstable, or non-functional. Do not install for production environments.

**Unified LED controller combining WLED effects with LEDFx audio reactivity** - all in one ESP32-P4 device.

LEDBrain merges **WLED's 30+ visual effects** and **LEDFx's audio-reactive capabilities** into a single, powerful controller. Unlike standalone solutions, it offers **real-time audio analysis**, **unified device management**, and **synchronized multi-device effects** without external servers.

## 🎯 Why LEDBrain?

### vs. WLED
- ✅ **Audio reactivity built-in** - no external server needed
- ✅ **Unified control** - manage physical strips and remote WLED devices from one interface
- ✅ **Real-time Snapcast integration** - synchronized audio analysis
- ✅ **Advanced segment options** - grouping, spacing, brightness per segment

### vs. LEDFx
- ✅ **Standalone device** - no PC/server required, runs on ESP32-P4
- ✅ **Direct LED control** - hardware RMT driver for WS2812/SK6812 strips
- ✅ **Lower latency** - local processing eliminates network delays
- ✅ **Ethernet support** - stable connection for audio streaming

### Key Advantages
- **Dual-chip architecture**: ESP32-P4 (main) + ESP32-C6 (WiFi) for optimal performance
- **Snapcast-native**: Direct PCM audio reception without intermediate processing
- **WLED-compatible**: Send effects to existing WLED devices via DDP protocol
- **Home Assistant ready**: MQTT integration with auto-discovery

## ✨ Core Features

### 🎨 Unified Effect System
- **30+ WLED effects** (Rainbow, Fire, Meteor, Scanner, etc.) + **10+ LEDFx audio-reactive effects** (Energy Flow, Waves, Plasma, Matrix)
- **Switch engines instantly** - no restart needed
- **Per-segment effect assignment** - different effects on different strips

### 🎵 Real-time Audio Processing
- **Direct Snapcast PCM** - zero-latency audio reception
- **Hardware-accelerated FFT** - efficient frequency analysis on ESP32-P4
- **Frequency band control** - Bass, Mids, Treble, or custom ranges
- **Beat detection** - automatic synchronization

### 💡 Advanced LED Control
- **Hardware RMT driver** - direct WS2812/SK6812 control, no bit-banging
- **Matrix layouts** - rotation, mirroring, custom dimensions
- **Power management** - per-segment limits with automatic calculation
- **Segment grouping** - control multiple LEDs as one unit

### 🌐 Network & Ecosystem
- **Ethernet + WiFi** - dual connectivity with automatic failover
- **WLED device control** - send effects to remote WLED devices via DDP
- **Auto-discovery** - find WLED devices on network automatically
- **Home Assistant** - MQTT integration with entity discovery
- **OTA updates** - firmware updates via web interface

## Hardware

**Required:**
- **ESP32-P4** board (tested: JC-ESP32P4-M3-DEV) - main controller
- **ESP32-C6** board - WiFi coprocessor (via UART)
- **Ethernet PHY** (LAN8720/IP101/DP83848) or WiFi via ESP32-C6

**Optional:**
- WS2812/SK6812 LED strips for physical control
- Ethernet cable for stable audio streaming

See [Hardware Documentation](docs/README.md) for schematics and pinout.

## Quick Start

**Prerequisites:** ESP-IDF 5.5.0+, Python 3.8+

**Build & Flash:**

```bash
# Main firmware (ESP32-P4)
idf.py set-target esp32p4
idf.py build flash monitor

# WiFi coprocessor (ESP32-C6)
cd esp32c6_firmware
idf.py set-target esp32c6
idf.py build flash -p COM5  # Adjust port
```

**First Run:**
1. Access `http://ledbrain.local` or device IP
2. Configure network (Ethernet recommended for audio)
3. Add LED segments or WLED devices
4. Assign effects and enable audio (Snapcast)

## Effects

**WLED (30+):** Rainbow, Fire, Meteor, Scanner, Chase, Theater, Energy Flow, Beat Pulse, and more  
**LEDFx (10+):** Energy Waves, Plasma, Matrix, Hyperspace, Waves - all audio-reactive

All effects support custom colors, gradients, speed/intensity control, and per-segment assignment.

## Documentation

- **[Hardware Docs](docs/README.md)** - Schematics, pinout, specifications
- **[WiFi Coprocessor](esp32c6_firmware/FLASHING_GUIDE.md)** - ESP32-C6 setup
- **[Configuration Guide](docs/README.md#configuration)** - Network, LEDs, effects

## Releases

Pre-built firmware: [Releases](https://github.com/No3x3x3/LEDBrain/releases)

**Features:** WLED + LEDFx effects, Snapcast audio, MQTT/Home Assistant, OTA updates

## License

MIT License - see [LICENSE](LICENSE) for details.

**Attributions:**
- Effects inspired by [WLED](https://github.com/Aircoookie/WLED) and [LEDFx](https://github.com/LedFx/LedFx)
- Built with [ESP-IDF](https://github.com/espressif/esp-idf)




