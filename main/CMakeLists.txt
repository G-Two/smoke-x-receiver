# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "app_lora.c" "app_web_ui.c" "app_mqtt.c" "app_wifi.c" "smoke_x.c" "main.c")
set(COMPONENT_ADD_INCLUDEDIRS "")

register_component()
