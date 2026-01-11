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
        esp_http_server
        json
        json
    PRIV_REQUIRES
        lwip
        esp_driver_gpio
)

