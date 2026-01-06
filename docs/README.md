# LEDBrain Documentation

## Hardware Documentation

This project is designed for the **JC-ESP32P4-M3-DEV** development board.

### Hardware Specifications

For detailed hardware specifications, schematics, and user manual, please refer to the documentation in the `docs/hardware/` directory:

- **Specifications**: `docs/hardware/JC-ESP32P4-M3-DEV Specifications-EN.pdf`
- **User Manual**: `docs/hardware/Getting started JC-ESP32P4-M3-DEV.pdf`
- **Schematics**: `docs/hardware/schematics/` (PNG files)

### Key Hardware Features

- **ESP32-P4** microcontroller (High-Performance MCU)
- **Ethernet** support via PHY (LAN8720, IP101, or DP83848)
- **MIPI DSI** and **MIPI CSI** interfaces
- **USB** and **RS485** interfaces
- **Battery** support with ADC monitoring
- **Expandable I/O** pins

### Pin Configuration

The pin configuration for LED control and other peripherals can be configured through the web interface. Default GPIO pins are defined in `components/led_engine/pinout.cpp`.

## Software Features

### Core Functionality

- **Dual Engine Support**: WLED effects for visual consistency + LEDFx effects for audio-reactive visualization
- **Physical LED Control**: Direct control of WS2812/SK6812 LED strips via ESP-RMT driver
- **WLED Integration**: Send effects to remote WLED devices via DDP protocol
- **Audio Visualization**: Snapcast Light client for real-time audio analysis and reactive effects
- **Web Interface**: Modern, responsive web GUI (Polish/English) for configuration
- **MQTT Integration**: Home Assistant discovery and control
- **OTA Updates**: Over-the-air firmware updates via URL or file upload
- **Ethernet Support**: Reliable network connection via Ethernet (DHCP or static IP)
- **mDNS Discovery**: Access device at `http://ledbrain.local`

### Effect Engines

#### WLED Effects
- Solid, Blink, Breathe, Colorloop, Candle
- Sinelon, Noise, Fireworks, Heartbeat
- Pacifica, Scanner Dual, Rainbow Runner
- Energy Flow, Power Cycle, Energy Burst
- Rain, Meteor, Fire 2012
- And many more...

#### LEDFx Effects
- Energy Flow, Fire/Flame, Matrix
- Waves, Plasma, Ripple Flow
- Rain, Aura, Hyperspace
- All with full audio reactivity support

### Audio Reactivity

- **LEDFx Effects**: Full audio reactivity support with customizable frequency ranges
  - Kick (bass + beat detection)
  - Bass (low frequencies)
  - Mids (mid frequencies)
  - Treble (high frequencies)
  - Custom range (freq_min/freq_max)
- **WLED Effects**: Non-audio-reactive for local segments, but can be audio-reactive when sent to WLED devices via DDP

## Building and Flashing

### Prerequisites

- ESP-IDF 5.5.0 or later
- Python 3.8+
- USB-to-Serial driver (CH340 or similar)

### Build Steps

```bash
# Set target
idf.py set-target esp32p4

# Configure (optional)
idf.py menuconfig

# Build
idf.py build

# Flash and monitor
idf.py -p COM3 flash monitor
```

### First Boot

After flashing, the device will:

1. Start with default network configuration (DHCP)
2. Announce itself via mDNS as `ledbrain.local`
3. Start web server on port 80

Access the web interface at:
- `http://ledbrain.local` (if mDNS works)
- `http://<device-ip>` (check serial monitor for IP address)

## Configuration

### Network Setup

1. Access web interface
2. Go to **Network** tab
3. Configure Ethernet:
   - DHCP (automatic) or Static IP
   - Gateway, Netmask, DNS
4. Save and restart

### LED Segments

1. Go to **Devices** tab
2. Configure physical LED strips:
   - LED count
   - GPIO pin
   - Chipset (WS2812, SK6812, etc.)
   - Color order (RGB, GRB, etc.)
   - Matrix layout (if applicable)
   - Power limits

### WLED Devices

1. Go to **Devices** tab
2. Click **Add WLED** or wait for auto-discovery
3. Configure:
   - Device name
   - IP address
   - Segment index
   - Enable DDP stream
4. Save

### Effects

1. Go to **Devices** tab
2. Select a segment or WLED device
3. Configure effect:
   - Engine (WLED or LEDFx)
   - Effect name
   - Colors (Primary, Secondary, Tertiary)
   - Brightness, Intensity, Speed
   - Audio reactivity (LEDFx only)
   - Advanced settings (gamma, blend mode, etc.)

## Troubleshooting

### Ethernet Not Working

1. Check PHY type in `main/eth_init.cpp`
2. Verify RMII pin configuration
3. Check cable connection
4. Try different PHY: LAN8720, IP101, or DP83848

### WLED Not Receiving Data

1. Verify DDP is enabled in WLED device configuration
2. Check WLED device has DDP enabled (port 4048)
3. Verify IP address is correct
4. Check network connectivity

### Audio Not Working

1. Verify Snapcast server is running
2. Check Snapcast host/port in audio configuration
3. Enable audio reactivity for LEDFx effects
4. Check audio sensitivity settings

## License

This project is licensed under the **MIT License**. See the main [LICENSE](../LICENSE) file for details.

### Third-Party Components

LEDBrain uses effects and algorithms inspired by:
- **WLED** (MIT License) - LED effect algorithms
- **LEDFx** (GPL-3.0 License) - Audio-reactive visualization techniques
- **ESP-IDF** (Apache License 2.0) - ESP32 framework

All effect implementations are reimplemented for ESP32-P4 and adapted for real-time audio reactivity.

## Acknowledgments

- Inspired by [WLED](https://github.com/Aircoookie/WLED) and [LEDFx](https://github.com/LedFx/LedFx)
- Built with ESP-IDF framework
- Hardware: JC-ESP32P4-M3-DEV development board

