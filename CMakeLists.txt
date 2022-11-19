cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS esp-idf-sx127x/components/lora esp-idf-sx126x/components/ra01s)
set(COMPONENTS json lora main mqtt esp-tls mbedtls ra01s spiffs tcp_transport esp_http_server)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(smoke-x)

add_custom_target(www ${PROJECT_SOURCE_DIR}/web_ui/build.sh ${PROJECT_SOURCE_DIR}/web_ui)
spiffs_create_partition_image(storage web_ui/dist FLASH_IN_PROJECT DEPENDS www)
