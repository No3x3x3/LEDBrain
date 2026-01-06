# LEDBrain

**ESP32-P4 based LED strip controller with audio visualization** - combining WLED and LEDFx functionality in one powerful device.

LEDBrain is a comprehensive LED control system that brings together the best of both worlds: **WLED's visual effects** and **LEDFx's audio-reactive capabilities**. Perfect for home automation, music visualization, and synchronized lighting across multiple devices.

## ✨ Key Features

### 🎨 Dual Effect Engine
- **WLED Effects**: Classic visual effects (Rainbow, Fire, Meteor, Scanner, etc.) for consistent lighting
- **LEDFx Effects**: Advanced audio-reactive effects (Energy Flow, Waves, Plasma, Matrix, etc.) that respond to music
- **Seamless Switching**: Change between engines on-the-fly via web interface

### 🎵 Audio Reactivity
- **Snapcast Integration**: Real-time audio analysis from Snapcast server
- **Frequency Band Control**: Customize reactivity for Bass, Mids, Treble, or custom ranges
- **Beat Detection**: Automatic beat detection and synchronization
- **Audio Profiles**: Pre-configured profiles for different music styles

### 💡 LED Control
- **Physical Strips**: Direct control of WS2812/SK6812 LED strips via ESP-RMT driver
- **Matrix Support**: Full support for LED matrix layouts with rotation and mirroring
- **Power Management**: Automatic power limit calculation and per-segment power control
- **Gamma Correction**: Adjustable gamma correction for color accuracy

### 🌐 Network & Integration
- **WLED Integration**: Send effects to remote WLED devices via DDP protocol
- **Ethernet Support**: Reliable network connection via Ethernet (DHCP or static IP)
- **mDNS Discovery**: Access device at `http://ledbrain.local`
- **MQTT Integration**: Home Assistant discovery and control
- **OTA Updates**: Over-the-air firmware updates via URL or file upload

### 🖥️ Modern Web Interface
- **Responsive Design**: Works on desktop, tablet, and mobile devices
- **Multi-language**: Polish and English support
- **Real-time Configuration**: Changes apply immediately with auto-save
- **Intuitive Controls**: Easy-to-use interface with helpful descriptions

## Hardware Requirements

### Supported Hardware

- **ESP32-P4** development board (tested on JC-ESP32P4-M3-DEV)
- **Ethernet PHY** (LAN8720, IP101, or DP83848) - configure in `main/eth_init.cpp`
- **WS2812/SK6812 LED strips** (optional, for physical LED control)
- **Ethernet cable** for network connection

### Recommended Hardware

- **JC-ESP32P4-M3-DEV** development board (full documentation in `docs/hardware/`)
- High-quality 5V power supply for LED strips
- Ethernet connection for stable network

For detailed hardware specifications, schematics, and pinout information, see [Hardware Documentation](docs/README.md).

## Building

### Prerequisites

- ESP-IDF 5.5.0 or later
- Python 3.8+

### Build Steps

```bash
# Set target
idf.py set-target esp32p4

# Build
idf.py build

# Flash and monitor
idf.py flash monitor
```

## Configuration

After first boot, access the web interface at `http://ledbrain.local`:

1. **Network**: Configure Ethernet (DHCP or static IP)
2. **MQTT**: Set up MQTT broker connection (optional, for Home Assistant)
3. **LED Segments**: Configure physical LED strips (chipset, color order, matrix layout)
4. **WLED Devices**: Add remote WLED devices for synchronized effects
5. **Effects**: Assign WLED or LEDFx effects to segments/devices

## Audio Reactivity

- **LEDFx Effects**: Full audio reactivity support with customizable frequency ranges
  - Kick (bass + beat detection)
  - Bass (low frequencies)
  - Mids (mid frequencies)
  - Treble (high frequencies)
  - Custom range (freq_min/freq_max)
- **WLED Effects**: Non-audio-reactive for local segments, but can be audio-reactive when sent to WLED devices via DDP

## Effect Engines

### WLED Effects (30+ effects)
**Classic Effects:**
- Solid, Blink, Breathe, Colorloop, Candle, Candle Multi
- Rainbow, Rainbow Runner, Rainbow Bands
- Chase, Theater, Scanner, Scanner Dual

**Dynamic Effects:**
- Energy Flow, Energy Burst, Energy Waves
- Power+, Power Cycle
- Fire 2012, Fireworks
- Meteor, Meteor Smooth
- Rain, Rain (Dual)
- Sinelon, Noise, Heartbeat, Pacifica, Ripple

**Rhythm Effects:**
- Beat Pulse, Beat Bars, Beat Scatter, Beat Light
- Strobe

### LEDFx Effects (10+ effects)
**All with full audio reactivity:**
- Energy Flow, Energy Burst, Energy Waves
- Waves, Plasma, Aura
- Rain, Ripple Flow
- Matrix, Hyperspace
- Fire/Flame

Each effect supports:
- Custom colors (Primary, Secondary, Tertiary)
- Gradient and palette support
- Adjustable brightness, intensity, and speed
- Audio reactivity with frequency band selection
- Advanced settings (gamma, blend mode, layers, etc.)

## OTA Updates

Two methods available:

1. **URL Update**: Automatically check and install from GitHub releases
2. **File Upload**: Upload firmware `.bin` file directly through web interface

## Project Structure

```
LEDBrain/
├── main/                          # Main application code
│   ├── wled_effects.cpp           # WLED effects runtime (30+ effects)
│   ├── ledfx_effects.cpp          # LEDFx effects runtime (10+ effects)
│   ├── web_setup.cpp              # Web server and REST API
│   ├── ota.cpp                    # OTA update handling
│   ├── ddp_tx.cpp                 # DDP protocol for WLED devices
│   ├── wled_discovery.cpp         # WLED device auto-discovery
│   ├── mqtt_ha.cpp                # MQTT Home Assistant integration
│   └── spiffs/                    # Web interface files
│       ├── index.html             # Main web interface
│       ├── app.js                 # Frontend JavaScript
│       ├── style.css              # Styling
│       └── lang_*.json            # Language files
├── components/
│   ├── led_engine/                # LED control engine
│   │   ├── led_engine.cpp         # Main engine logic
│   │   ├── rmt_driver.cpp         # ESP-RMT driver for WS2812/SK6812
│   │   ├── color_processing.cpp   # Color conversion and gamma
│   │   ├── matrix_utils.cpp       # Matrix layout utilities
│   │   └── audio_pipeline.cpp    # Audio processing pipeline
│   └── snapclient_light/          # Snapcast audio client
├── docs/                          # Documentation
│   ├── README.md                  # Detailed documentation
│   └── hardware/                  # Hardware documentation
│       ├── JC-ESP32P4-M3-DEV Specifications-EN.pdf
│       ├── Getting started JC-ESP32P4-M3-DEV.pdf
│       └── schematics/            # Schematic diagrams
└── partitions.csv                 # Partition table
```

## 📚 Documentation

- **[Hardware Documentation](docs/README.md)**: Detailed hardware specs, schematics, and pinout
- **[Getting Started Guide](docs/README.md#building-and-flashing)**: Step-by-step setup instructions
- **[Configuration Guide](docs/README.md#configuration)**: How to configure network, LEDs, and effects
- **[Troubleshooting](docs/README.md#troubleshooting)**: Common issues and solutions

## 🚀 Quick Start

1. **Flash firmware** (see [Building](#building) section)
2. **Access web interface** at `http://ledbrain.local` or device IP
3. **Configure network** (Ethernet settings)
4. **Add LED segments** (physical strips) or **WLED devices** (remote)
5. **Assign effects** to segments/devices
6. **Enable audio** (if using Snapcast) for reactive effects

## 📦 Releases

Pre-built firmware binaries are available in [Releases](https://github.com/No3x3x3/LEDBrain/releases).

Latest release includes:
- Full WLED and LEDFx effect support
- Modern web interface with auto-save
- Audio reactivity with Snapcast
- MQTT Home Assistant integration
- OTA update support

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## 📄 License

[Add your license here]

## 🙏 Acknowledgments

- Inspired by [WLED](https://github.com/Aircoookie/WLED) and [LEDFx](https://github.com/LedFx/LedFx)
- Built with [ESP-IDF](https://github.com/espressif/esp-idf) framework
- Hardware: [JC-ESP32P4-M3-DEV](docs/hardware/) development board




