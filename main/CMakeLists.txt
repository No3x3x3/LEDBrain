idf_component_register(
  SRCS 
      "main.cpp"
      "eth_init.cpp"
      "wifi_c6.cpp"  # WiFi via ESP32-C6 coprocessor (PPP over UART)
      "wifi_c6_ctrl.cpp"  # Control protocol for ESP32-P4 ↔ ESP32-C6 communication
      "sd_card.cpp"  # SD card support for 64GB microSD (FAT32/exFAT)
      "mdns_init.cpp"
      "web_setup.cpp"
      "config.cpp"
      "mqtt_ha.cpp"
      "ddp_tx.cpp"
      "heartbeat.cpp"
      "wled_discovery.cpp"
      "wled_effects.cpp"
      "ledfx_effects.cpp"
      "ota.cpp"
  INCLUDE_DIRS "."
  EMBED_FILES 
      "spiffs/index.html"
      "spiffs/app.js"
      "spiffs/style.css"
      "spiffs/lang_pl.json"
      "spiffs/lang_en.json"
      "spiffs/config.json"
  REQUIRES 
      esp_netif
      esp_eth
      nvs_flash
      mqtt
      esp_http_server
      esp_http_client
      esp_https_ota
      json
      esp_driver_sdmmc
      fatfs
      eppp_link
      esp_driver_uart
  PRIV_REQUIRES 
      mdns
      esp_driver_gpio
      app_update
      led_engine
      snapclient_light
      esp_wifi
)
