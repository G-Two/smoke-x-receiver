idf_component_register(
    SRCS "app_display.c"
         "app_lora.c"
         "app_mqtt.c"
         "app_web_ui.c"
         "app_wifi.c"
         "main.c"
         "smoke_x.c"
    INCLUDE_DIRS "."
    REQUIRES driver
             esp_event
             esp_http_server
             esp_netif
             esp_timer
             esp_wifi
             json
             lora
             mqtt
             nvs_flash
             ra01s
             spiffs
             vfs
             wpa_supplicant)
