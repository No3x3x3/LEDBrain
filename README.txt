LEDBrain MVP 0.1 (ESP32-P4, ESP-IDF 5.5.1)
- Ethernet (DHCP / statyczne IP)
- Web GUI (PL/EN) hostowane z SPIFFS
- Konfiguracja MQTT przez GUI
- HA Discovery: switch.ledbrain_enabled
- DDP test do WLED (ustaw host w GUI)

Flash:
  idf.py set-target esp32p4
  idf.py build
  idf.py flash monitor

Po starcie:
  http://ledbrain.local  -> ustaw Sieć/MQTT -> Zapisz i restart
  W HA pojawi się przełącznik - włącz = DDP test do WLED.

Uwaga Ethernet:
  Jeśli nie działa, dostosuj PHY w main/eth_init.cpp (LAN8720/IP101/DP83848) i piny RMII.
