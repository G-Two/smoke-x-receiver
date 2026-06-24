# Smoke X Receiver
#
# Prerequisites:
#   brew bundle        # python@3.11, cmake, ninja (see Brewfile)
#   make check-python  # verify a supported Python is available
#   make init-idf      # verify ESP-IDF (firmware targets auto-source export.sh)
#   make setup
#
# Options:
#   PORT=/dev/cu.usbserial-XXXX  Serial port for flash/monitor (auto-detect if omitted)
#   IDF_EXPORT=/path/to/esp-idf/export.sh  Override ESP-IDF location

.DEFAULT_GOAL := help

IDF_PY ?= idf.py
PORT ?=

# ESP-IDF v5.4 supports Python 3.9+ (no upper bound). Prefer Homebrew python@3.x
# shims, then versioned python3.x binaries already on PATH.
IDF_PYTHON_DIR := $(shell \
	python_ok() { "$$1" -c 'import sys; sys.exit(0 if sys.version_info[:2] >= (3,9) else 1)' 2>/dev/null; }; \
	brew_prefix=$$(brew --prefix 2>/dev/null); \
	for ver in 3.14 3.13 3.12 3.11 3.10 3.9; do \
		for prefix in /opt/homebrew /usr/local $$brew_prefix; do \
			test -n "$$prefix" || continue; \
			dir="$$prefix/opt/python@$$ver/libexec/bin"; \
			test -x "$$dir/python3" && python_ok "$$dir/python3" && { echo "$$dir"; exit 0; }; \
			test -x "$$dir/python" && python_ok "$$dir/python" && { echo "$$dir"; exit 0; }; \
		done; \
	done; \
	for cmd in python3.14 python3.13 python3.12 python3.11 python3.10 python3.9 python3 python; do \
		path=$$(command -v $$cmd 2>/dev/null) || continue; \
		python_ok "$$path" && { dirname "$$path"; exit 0; }; \
	done \
)
IDF_PYTHON_PATH := $(if $(IDF_PYTHON_DIR),PATH="$(IDF_PYTHON_DIR):$$PATH",)

ifdef IDF_PATH
IDF_EXPORT ?= $(IDF_PATH)/export.sh
else
IDF_EXPORT ?= $(HOME)/esp/esp-idf/export.sh
endif

COMMON_DEFAULTS := sdkconfig.defaults
V2_DEFAULTS := $(COMMON_DEFAULTS);sdkconfig.defaults.heltec-v2
V3_DEFAULTS := $(COMMON_DEFAULTS);sdkconfig.defaults.heltec-v3

V2_BUILD := build/heltec-v2
V3_BUILD := build/heltec-v3

PORT_ARG := $(if $(PORT),-p $(PORT),)

define source_idf
	$(IDF_PYTHON_PATH) . "$(IDF_EXPORT)"
endef

# check-python: Verify ESP-IDF-compatible Python (3.9+) is available
check-python:
	@if [ -n "$(IDF_PYTHON_DIR)" ]; then \
		if [ -x "$(IDF_PYTHON_DIR)/python3" ]; then \
			py="$(IDF_PYTHON_DIR)/python3"; \
		else \
			py="$(IDF_PYTHON_DIR)/python"; \
		fi; \
		echo "ESP-IDF Python: $$($$py --version) ($$py)"; \
	else \
		echo ""; \
		echo "Error: No supported Python found (ESP-IDF v5.4 needs 3.9+)."; \
		echo ""; \
		echo "Install dependencies and retry:"; \
		echo "  brew bundle"; \
		echo "  make check-python"; \
		echo ""; \
		current=$$(command -v python3 2>/dev/null); \
		if [ -n "$$current" ]; then \
			echo "Current python3: $$($$current --version) ($$current)"; \
		else \
			echo "Current python3: not found"; \
		fi; \
		exit 1; \
	fi

# check-idf: Verify ESP-IDF export script exists and idf.py runs (internal)
check-idf: check-python
	@test -f "$(IDF_EXPORT)" || { \
		echo ""; \
		echo "Error: ESP-IDF export script not found at:"; \
		echo "  $(IDF_EXPORT)"; \
		echo ""; \
		echo "Install ESP-IDF v5.4, then retry or set IDF_EXPORT:"; \
		echo "  https://docs.espressif.com/projects/esp-idf/en/release-v5.4/esp32/get-started/"; \
		echo "  make init-idf IDF_EXPORT=/path/to/esp-idf/export.sh"; \
		echo ""; \
		exit 1; \
	}
	@$(source_idf) && command -v $(IDF_PY) >/dev/null || { \
		echo ""; \
		echo "Error: sourced $(IDF_EXPORT) but idf.py is still unavailable."; \
		echo "Re-run ESP-IDF install.sh with a supported Python, e.g.:"; \
		echo "  PATH=\"$(IDF_PYTHON_DIR):\$$PATH\" $(dir $(IDF_EXPORT))install.sh"; \
		echo ""; \
		exit 1; \
	}

define idf
	$(IDF_PYTHON_PATH) . "$(IDF_EXPORT)" && SDKCONFIG_DEFAULTS="$(1)" $(IDF_PY) -B $(2) $(3)
endef

.PHONY: help check-python check-idf init-idf \
	build-heltec-v2 build-heltec-v3 \
	flash-heltec-v2 flash-heltec-v3 \
	flash-monitor-heltec-v2 flash-monitor-heltec-v3 \
	install-heltec-v2 install-heltec-v3 \
	monitor-heltec-v2 monitor-heltec-v3 \
	menuconfig-heltec-v2 menuconfig-heltec-v3 \
	clean clean-heltec-v2 clean-heltec-v3 \
	heltec-v2 heltec-v3 \
	submodules setup \
	mock-web-ui build-web-ui web-ui-install web-ui-lint web-ui-format clean-web-ui

# help: Show available targets and usage
help:
	@echo "Smoke X Receiver"
	@echo ""
	@awk '\
		/^# --- .+ ---$$/ { \
			if (sections++) print ""; \
			gsub(/^# --- | ---$$/, "", $$0); \
			printf "%s:\n", $$0; \
			next \
		} \
		/^# [a-zA-Z0-9_.-]+: / { \
			if ($$0 ~ /^# help: / || $$0 ~ /^# check-idf: /) next; \
			target = $$0; \
			sub(/^# /, "", target); \
			sub(/: .*$$/, "", target); \
			desc = $$0; \
			sub(/^# [a-zA-Z0-9_.-]+: /, "", desc); \
			printf "  \033[36m%s\033[0m   %s\n", target, desc; \
			next \
		}' $(MAKEFILE_LIST)
	@echo ""
	@echo "Firmware targets auto-source: $(IDF_EXPORT)"
	@echo "For interactive shells: source \"$(IDF_EXPORT)\""

# --- Project setup ---

# init-idf: Verify ESP-IDF is installed and print idf.py version
init-idf: check-idf
	@echo "ESP-IDF ready: $(IDF_EXPORT)"
	@$(source_idf) && $(IDF_PY) --version

# --- Heltec install ---

define ensure_clean_build_dir
	if [ -d "$(1)" ] && [ ! -f "$(1)/CMakeCache.txt" ]; then rm -rf "$(1)"; fi
endef

$(V2_BUILD)/.target-esp32: check-idf
	$(call ensure_clean_build_dir,$(V2_BUILD))
	$(call idf,$(V2_DEFAULTS),$(V2_BUILD),set-target esp32)
	@touch $@

# build-heltec-v2: Build firmware for Heltec WiFi LoRa 32 V2 (ESP32, SX1276)
build-heltec-v2: $(V2_BUILD)/.target-esp32
	$(call idf,$(V2_DEFAULTS),$(V2_BUILD),build)

# flash-heltec-v2: Build and flash Heltec WiFi LoRa 32 V2
flash-heltec-v2: $(V2_BUILD)/.target-esp32
	$(call idf,$(V2_DEFAULTS),$(V2_BUILD),build flash $(PORT_ARG))

# install-heltec-v2: Build and flash Heltec WiFi LoRa 32 V2
install-heltec-v2: flash-heltec-v2

# flash-monitor-heltec-v2: Build, flash, and open serial monitor (V2)
flash-monitor-heltec-v2: $(V2_BUILD)/.target-esp32
	$(call idf,$(V2_DEFAULTS),$(V2_BUILD),build flash monitor $(PORT_ARG))

# monitor-heltec-v2: Open serial monitor for V2 build
monitor-heltec-v2: check-idf
	$(call idf,$(V2_DEFAULTS),$(V2_BUILD),monitor $(PORT_ARG))

# menuconfig-heltec-v2: Open menuconfig for V2
menuconfig-heltec-v2: $(V2_BUILD)/.target-esp32
	$(call idf,$(V2_DEFAULTS),$(V2_BUILD),menuconfig)

# clean-heltec-v2: Remove V2 build directory
clean-heltec-v2:
	rm -rf $(V2_BUILD)

heltec-v2: install-heltec-v2

$(V3_BUILD)/.target-esp32s3: check-idf
	$(call ensure_clean_build_dir,$(V3_BUILD))
	$(call idf,$(V3_DEFAULTS),$(V3_BUILD),set-target esp32s3)
	@touch $@

# build-heltec-v3: Build firmware for Heltec WiFi LoRa 32 V3 (ESP32-S3, SX1262)
build-heltec-v3: $(V3_BUILD)/.target-esp32s3
	$(call idf,$(V3_DEFAULTS),$(V3_BUILD),build)

# flash-heltec-v3: Build and flash Heltec WiFi LoRa 32 V3
flash-heltec-v3: $(V3_BUILD)/.target-esp32s3
	$(call idf,$(V3_DEFAULTS),$(V3_BUILD),build flash $(PORT_ARG))

# install-heltec-v3: Build and flash Heltec WiFi LoRa 32 V3
install-heltec-v3: flash-heltec-v3

# flash-monitor-heltec-v3: Build, flash, and open serial monitor (V3)
flash-monitor-heltec-v3: $(V3_BUILD)/.target-esp32s3
	$(call idf,$(V3_DEFAULTS),$(V3_BUILD),build flash monitor $(PORT_ARG))

# monitor-heltec-v3: Open serial monitor for V3 build
monitor-heltec-v3: check-idf
	$(call idf,$(V3_DEFAULTS),$(V3_BUILD),monitor $(PORT_ARG))

# menuconfig-heltec-v3: Open menuconfig for V3
menuconfig-heltec-v3: $(V3_BUILD)/.target-esp32s3
	$(call idf,$(V3_DEFAULTS),$(V3_BUILD),menuconfig)

# clean-heltec-v3: Remove V3 build directory
clean-heltec-v3:
	rm -rf $(V3_BUILD)

heltec-v3: install-heltec-v3

# --- Web UI ---

# mock-web-ui: Run web UI dev server with mocked API (./mock_web_ui.sh)
mock-web-ui:
	./mock_web_ui.sh

# build-web-ui: Build production web assets for ESP32 flash
build-web-ui:
	./web_ui/build.sh web_ui

# web-ui-install: npm install in web_ui/
web-ui-install:
	npm install --prefix web_ui

# web-ui-lint: Lint web UI sources
web-ui-lint:
	npm run lint --prefix web_ui

# web-ui-format: Format web UI sources with Prettier
web-ui-format:
	npm run format --prefix web_ui

# clean-web-ui: Remove web_ui/node_modules and web_ui/dist
clean-web-ui:
	rm -rf web_ui/node_modules web_ui/dist

# submodules: Initialize and update git submodules
submodules:
	git submodule update --init --recursive

# setup: Initialize submodules and install web UI dependencies
setup: submodules web-ui-install

# --- Clean ---

# clean: Remove all board build directories
clean: clean-heltec-v2 clean-heltec-v3
