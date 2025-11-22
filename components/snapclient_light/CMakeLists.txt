idf_component_register(
  SRCS
    "snapclient_light.cpp"
  INCLUDE_DIRS "include"
  REQUIRES esp_netif lwip led_engine
)
