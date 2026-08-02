idf_component_register(
    SRCS "app_display.c"
         "app_improv.c"
         "app_lora.c"
         "app_mqtt.c"
         "app_web_ui.c"
         "app_wifi.c"
         "improv.c"
         "main.c"
         "smoke_x.c"
         "smoke_x_parser.c"
    INCLUDE_DIRS "."
    REQUIRES driver
             esp_app_format
             esp_event
             esp_http_server
             esp_hw_support
             esp_netif
             esp_timer
             esp_wifi
             json
             lora
             mqtt
             nvs_flash
             ra01s
             spi_flash
             spiffs
             vfs
             wpa_supplicant)
