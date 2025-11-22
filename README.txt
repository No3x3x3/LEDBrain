LEDBrain (ESP32-P4, ESP-IDF 5.5.1)
- Ethernet (DHCP lub statyczne IP)
- Web GUI (PL/EN) z SPIFFS: Network/MQTT/WLED devices + effects
- HA Discovery: switch.ledbrain_enabled
- DDP do WLED (host z GUI) + WLED effects runtime per device/segment
- Snapclient Light: lekki klient Snapcast (PCM) dla wizualizacji, bez wyjscia audio

Flash:
  idf.py set-target esp32p4
  idf.py build
  idf.py flash monitor

Po starcie:
  http://ledbrain.local -> ustaw Siec/MQTT/WLED -> Zapisz i restart
  W HA pojawi sie przelacznik - wlacz = DDP test do WLED; efekty WLED ustawiasz w GUI.

Uwaga Ethernet:
  Jezeli link nie wstaje, dostosuj PHY w main/eth_init.cpp (LAN8720/IP101/DP83848) i piny RMII.
