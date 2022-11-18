cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS esp-idf-sx127x/components/lora)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(smoke-x)

add_custom_target(www ${PROJECT_SOURCE_DIR}/web_ui/build.sh ${PROJECT_SOURCE_DIR}/web_ui)
spiffs_create_partition_image(storage web_ui/dist FLASH_IN_PROJECT DEPENDS www)
