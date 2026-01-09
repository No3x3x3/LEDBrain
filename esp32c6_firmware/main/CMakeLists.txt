idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        eppp_link
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        esp_driver_uart
    PRIV_REQUIRES
        lwip
)

