# LEDBrain

> ⚠️ **Note: Project in active development**  
> This project is continuously being improved and may contain bugs or incomplete features. Some functionality may not work as expected. Use at your own risk.

**Unified LED controller combining WLED effects with LEDFx audio reactivity** - all in one ESP32-P4 device.

LEDBrain merges **WLED's 30+ visual effects** and **LEDFx's audio-reactive capabilities** into a single, powerful controller. It offers **real-time audio analysis**, **unified device management**, and **synchronized multi-device effects** without external servers.

## 🎯 Key Advantages

### Hardware Acceleration
- **ESP32-P4 PPA (Pixel Processing Accelerator)**: Hardware-accelerated alpha blending and color fill operations for smoother effects
- **ESP-DSP with SIMD/Xai**: Hardware-accelerated FFT processing (~3-5x faster than software-only implementations)
- **High-performance MCU**: ESP32-P4 with FPU, DSP extensions, and 128-bit SIMD (Xai) for optimized audio processing
- **AI/ML infrastructure ready**: ESP-DL library integrated for machine learning model inference (audio-to-visual mapping, pattern generation, effect blending)

### Unique Features
- **Dual effect engine**: Switch between WLED and LEDFx effects in real-time without restart
- **Snapcast Light client**: Direct PCM audio reception from Snapcast server for zero-latency audio visualization
- **WLED device control**: Send effects to existing WLED devices via DDP protocol from the same interface
- **Parallel LED output**: Drive up to 4 LED strips simultaneously using ESP32-P4's parallel RMT capabilities
- **Ethernet-first design**: Stable network connection optimized for audio streaming (optional WiFi via ESP32-C6 coprocessor)
- **AI/ML ready**: ESP-DL library integrated for machine learning model inference - ready for audio-to-visual mapping, pattern generation, and intelligent effect blending

### Integration
- **Home Assistant**: MQTT integration with auto-discovery
- **SD card support**: Optional 64GB microSD for data storage (FAT32/exFAT)
- **OTA updates**: Firmware updates via web interface (URL or file upload)

## ✨ Core Features

### 🎨 Unified Effect System
- **30+ WLED effects** (Rainbow, Fire, Meteor, Scanner, Energy Flow, Beat Pulse, etc.)
- **10+ LEDFx audio-reactive effects** (Energy Flow, Waves, Plasma, Matrix, Hyperspace)
- **Switch engines instantly** - no restart needed, hot-swap between WLED and LEDFx
- **Per-segment effect assignment** - different effects on different strips
- **Hardware-accelerated blending** - ESP32-P4 PPA for faster alpha blending and color operations

### 🎵 Real-time Audio Processing
- **Direct Snapcast PCM** - zero-latency audio reception via Snapcast Light client
- **Hardware-accelerated FFT** - ESP-DSP library with SIMD/Xai optimizations for ESP32-P4
- **Frequency band control** - Bass, Mids, Treble, or custom frequency ranges
- **Beat detection** - automatic rhythm synchronization
- **32-band GEQ** - granular frequency analysis for precise audio visualization
- **Optimized processing** - ~3-5x faster FFT with hardware acceleration

### 💡 Advanced LED Control
- **Hardware RMT driver** - direct WS2812/SK6812 control, no bit-banging
- **Parallel output mode** - drive up to 4 LED strips simultaneously
- **Matrix layouts** - rotation, mirroring, custom dimensions
- **Power management** - per-segment limits with automatic calculation
- **Segment grouping** - control multiple LEDs as one unit
- **Framebuffer support** - multi-pass rendering for complex effects

### 🌐 Network & Ecosystem
- **Ethernet primary** - stable connection via LAN8720/IP101/DP83848 PHY (DHCP or static IP)
- **Optional WiFi** - ESP32-C6 coprocessor via UART (PPP) as fallback when Ethernet unavailable
- **WLED device control** - send effects to remote WLED devices via DDP protocol
- **Auto-discovery** - find WLED devices on network automatically via mDNS
- **Home Assistant** - MQTT integration with entity auto-discovery
- **OTA updates** - firmware updates via web interface (URL or file upload)
- **mDNS** - access device at `http://ledbrain.local`

### 🤖 AI/ML Capabilities
- **ESP-DL library integrated**: Ready-to-use infrastructure for machine learning model inference
- **Audio-to-visual mapping**: Framework for ML models that map audio features to visual effects
- **Pattern generation**: Infrastructure for generative models creating unique LED patterns
- **Effect blending**: ML-based intelligent mixing of multiple effects based on audio analysis
- **Hardware-accelerated inference**: ESP32-P4 AI/DSP extensions optimize model execution

### 🛠️ Additional Features
- **SD card support** - optional 64GB microSD for data storage (FAT32/exFAT)
- **Temperature monitoring** - ESP32-P4 TSENS for thermal monitoring
- **Modern web interface** - responsive design with Polish/English localization
- **Real-time configuration** - changes apply instantly without restart

## Hardware

**Required:**
- **ESP32-P4** board (tested: JC-ESP32P4-M3-DEV) - main controller
- **Ethernet PHY** (LAN8720/IP101/DP83848) - for network connectivity

**Optional:**
- **ESP32-C6** board - WiFi coprocessor via UART (PPP) - fallback when Ethernet unavailable
- **WS2812/SK6812 LED strips** - for physical LED control
- **64GB microSD card** - for data storage (FAT32/exFAT)
- **Ethernet cable** - recommended for stable audio streaming

**Note:** The project works standalone on ESP32-P4 with Ethernet. ESP32-C6 WiFi coprocessor is optional and only used as a fallback when Ethernet is not available.

See [Hardware Documentation](docs/README.md) for schematics and pinout.

## Quick Start

**Prerequisites:** ESP-IDF 5.5.0+, Python 3.8+

**Build & Flash:**

```bash
# Main firmware (ESP32-P4)
idf.py set-target esp32p4
idf.py build flash -p COM6  # Adjust port

# Optional: WiFi coprocessor (ESP32-C6) - only if you need WiFi fallback
cd esp32c6_firmware
idf.py set-target esp32c6
idf.py build flash -p COM5  # Adjust port
```

**First Run:**
1. Connect Ethernet cable (recommended for stable audio streaming)
2. Access `http://ledbrain.local` or device IP (check serial monitor for IP address)
3. Configure network settings if needed (DHCP is default)
4. Add LED segments (physical strips) or WLED devices (remote)
5. Assign effects and enable audio (Snapcast) if desired

## Effects

**WLED (30+):** Rainbow, Fire, Meteor, Scanner, Chase, Theater, Energy Flow, Beat Pulse, Beat Bars, Beat Scatter, Fireworks, Rain, Pacifica, Ripple, and more  
**LEDFx (10+):** Energy Flow, Waves, Plasma, Matrix, Hyperspace, Energy Waves - all audio-reactive

All effects support:
- Custom colors (Primary, Secondary, Tertiary)
- Gradients and color palettes
- Speed, intensity, and brightness control
- Audio reactivity with frequency band selection (for LEDFx effects)
- Per-segment assignment - different effects on different LED strips
- Hardware-accelerated blending (ESP32-P4 PPA) for smoother transitions

## Documentation

- **[Hardware Docs](docs/README.md)** - Schematics, pinout, specifications
- **[WiFi Coprocessor](esp32c6_firmware/FLASHING_GUIDE.md)** - ESP32-C6 setup
- **[Configuration Guide](docs/README.md#configuration)** - Network, LEDs, effects

## Releases

Pre-built firmware: [Releases](https://github.com/No3x3x3/LEDBrain/releases)

**Latest release includes:**
- WLED + LEDFx dual effect engine
- Snapcast audio integration with hardware-accelerated FFT
- ESP32-P4 PPA hardware acceleration for pixel operations
- ESP-DL library for AI/ML model inference (ready for custom models)
- MQTT/Home Assistant integration
- OTA updates via web interface
- SD card support
- Temperature monitoring
- Modern web UI (Polish/English)

## License

MIT License - see [LICENSE](LICENSE) for details.

**Attributions:**
- Effects inspired by [WLED](https://github.com/Aircoookie/WLED) and [LEDFx](https://github.com/LedFx/LedFx)
- Built with [ESP-IDF](https://github.com/espressif/esp-idf)




