# LEDBrain

ESP32-P4 based LED strip controller with audio visualization - combining WLED and LEDFx functionality in one device.

## Features

- **Dual Engine Support**: WLED effects for visual consistency + LEDFx effects for audio-reactive visualization
- **Physical LED Control**: Direct control of WS2812/SK6812 LED strips via ESP-RMT driver
- **WLED Integration**: Send effects to remote WLED devices via DDP protocol
- **Audio Visualization**: Snapcast Light client for real-time audio analysis and reactive effects
- **Web Interface**: Modern, responsive web GUI (Polish/English) for configuration
- **MQTT Integration**: Home Assistant discovery and control
- **OTA Updates**: Over-the-air firmware updates via URL or file upload
- **Ethernet Support**: Reliable network connection via Ethernet (DHCP or static IP)
- **mDNS Discovery**: Access device at `http://ledbrain.local`

## Hardware Requirements

- ESP32-P4 development board
- Ethernet PHY (LAN8720, IP101, or DP83848) - configure in `main/eth_init.cpp`
- WS2812/SK6812 LED strips (optional, for physical LED control)

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

### WLED Effects
- Solid, Blink, Breathe, Colorloop, Candle
- Sinelon, Noise, Fireworks, Heartbeat
- Pacifica, Scanner Dual, Rainbow Runner
- And many more...

### LEDFx Effects
- Energy Flow, Fire/Flame, Matrix
- Waves, Plasma, Ripple Flow
- Rain, Aura, Hyperspace
- All with audio reactivity support

## OTA Updates

Two methods available:

1. **URL Update**: Automatically check and install from GitHub releases
2. **File Upload**: Upload firmware `.bin` file directly through web interface

## Project Structure

```
LEDBrain/
├── main/                    # Main application code
│   ├── wled_effects.cpp     # WLED effects runtime
│   ├── ledfx_effects.cpp    # LEDFx effects runtime
│   ├── web_setup.cpp        # Web server and API
│   ├── ota.cpp              # OTA update handling
│   └── spiffs/              # Web interface files
├── components/
│   ├── led_engine/          # LED control engine
│   │   └── rmt_driver.cpp   # ESP-RMT driver for WS2812/SK6812
│   └── snapclient_light/    # Snapcast audio client
└── partitions.csv           # Partition table
```

## License

[Add your license here]

## Contributing

[Add contribution guidelines if needed]

## Acknowledgments

- Inspired by [WLED](https://github.com/Aircoookie/WLED) and [LEDFx](https://github.com/LedFx/LedFx)
- Built with ESP-IDF framework


