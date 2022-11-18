PROJECT_NAME := smoke-x
EXTRA_COMPONENT_DIRS := esp32-lora-library/components

ifndef IDF_TOOLS_EXPORT_CMD # Is ESP-IDF activated?
IDF_PATH := ./esp-idf

ifneq (,$(wildcard $(IDF_PATH)))

help:
	@echo "ESP SDK appears to be in $(IDF_PATH) but not activated. Use the following to activate the SDK:"
	@echo ""
	@echo "\t$$ source $(IDF_PATH)/export.sh"
	@echo ""

else

help:
	@echo "IDF_PATH not set. Ensure ESP SDK is installed and activated, or use the following to obtain the SDK:"
	@echo ""
	@echo "\tmake sdk - Download and install the ESP SDK in this project directory"
	@echo ""

sdk:
	git clone -b v4.4.3 --recursive https://github.com/espressif/esp-idf.git esp-idf
	$(IDF_PATH)/install.sh
	@echo "=============================================================================================="
	@echo "ESP SDK installed at $(IDF_PATH). Use the following to activate the SDK:"
	@echo ""
	@echo "\t$$ source $(IDF_PATH)/export.sh"
	@echo ""
endif

else

include $(IDF_PATH)/make/project.mk

sdk:
	$(error IDF_PATH already defined as $(IDF_PATH). You probably already have the ESP SDK in this location. Unset IDF_PATH if you want to download the toolchain to this project directory)

SPIFFS_IMAGE_DEPENDS := www
SPIFFS_IMAGE_FLASH_IN_PROJECT := 1
$(eval $(call spiffs_create_partition_image,storage,web_ui/dist))

www:
	./build_www.sh

mock-www:
	npm install --prefix web_ui
	npm run serve --prefix web_ui

endif
