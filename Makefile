# Smoke X Receiver
#
# Prerequisites (install via your system package manager, e.g. brew / apt / dnf):
#   - Python 3.9+, Node.js + npm, CMake >= 3.16
#   - ESP-IDF v5.4 (firmware targets auto-source export.sh)
#
# First-time setup:
#   make check-python  # verify a supported Python is detected
#   make check-idf     # verify ESP-IDF export.sh is reachable
#   make setup         # init submodules and install web UI deps
#
# Options:
#   PORT=/dev/...                          Serial port for flash/monitor (auto-detect if omitted)
#                                          Linux: /dev/ttyUSB0, /dev/ttyACM0
#                                          macOS: /dev/cu.usbserial-XXXX
#   IDF_EXPORT=/path/to/esp-idf/export.sh  Override ESP-IDF location
#                                          (also honors IDF_PATH; defaults to ~/esp/esp-idf)

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

# Per-board sdkconfig files at the project root. Keeping them separate
# prevents v2/v3 builds from clobbering each other (idf.py writes "sdkconfig"
# at the project root by default and refuses to build if it doesn't match
# the build dir's target).
V2_SDKCONFIG := sdkconfig.heltec-v2
V3_SDKCONFIG := sdkconfig.heltec-v3

V2_BUILD := build/heltec-v2
V3_BUILD := build/heltec-v3

PORT_ARG := $(if $(PORT),-p $(PORT),)

# Override the firmware's reported app version (embedded in esp_app_desc and
# reported via MQTT discovery) by setting PROJECT_VER. CI passes the git tag;
# locally, leaving it unset falls back to ESP-IDF's git-describe behavior.
PROJECT_VER ?=
PROJECT_VER_ARG := $(if $(PROJECT_VER),-DPROJECT_VER=$(PROJECT_VER),)

define source_idf
	$(IDF_PYTHON_PATH) . "$(IDF_EXPORT)"
endef

# --- Project setup ---

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
		echo "Install Python 3.9+ via your system package manager, e.g.:"; \
		echo "  macOS:        brew install python@3.12"; \
		echo "  Debian/Ubuntu: sudo apt install python3"; \
		echo "  Fedora/RHEL:   sudo dnf install python3"; \
		echo ""; \
		current=$$(command -v python3 2>/dev/null); \
		if [ -n "$$current" ]; then \
			echo "Current python3: $$($$current --version) ($$current)"; \
		else \
			echo "Current python3: not found"; \
		fi; \
		exit 1; \
	fi

# check-idf: Verify ESP-IDF export.sh is reachable
# (Deeper checks — sourcing and locating idf.py — happen in `init-idf`. This
# target stays source-free so each firmware build only sources export.sh once.)
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
	@echo "ESP-IDF: $(IDF_EXPORT)"

# idf: source export.sh once, then run idf.py with the supplied SDKCONFIG /
# defaults / build dir / args. SDKCONFIG must be passed via -D (idf.py does
# not honor an env var for this).
# $(1) = sdkconfig path, $(2) = sdkconfig defaults, $(3) = build dir, $(4) = idf.py args
define idf
	$(IDF_PYTHON_PATH) . "$(IDF_EXPORT)" && SDKCONFIG_DEFAULTS="$(2)" $(IDF_PY) -B $(3) -DSDKCONFIG=$(1) $(PROJECT_VER_ARG) $(4)
endef

# idf_with_target: source export.sh once, run set-target if the build dir has
# never been configured (no CMakeCache.txt), then run the requested idf.py
# command. Combines all work into a single shell so export.sh is sourced once.
# $(1) = build dir, $(2) = chip (esp32 / esp32s3), $(3) = sdkconfig defaults,
# $(4) = sdkconfig path, $(5) = idf.py args (build / flash / menuconfig / ...)
define idf_with_target
	$(IDF_PYTHON_PATH) . "$(IDF_EXPORT)" && { \
		if [ ! -f $(1)/CMakeCache.txt ]; then \
			rm -rf $(1) && \
			SDKCONFIG_DEFAULTS="$(3)" $(IDF_PY) -B $(1) -DSDKCONFIG=$(4) $(PROJECT_VER_ARG) set-target $(2); \
		fi && \
		SDKCONFIG_DEFAULTS="$(3)" $(IDF_PY) -B $(1) -DSDKCONFIG=$(4) $(PROJECT_VER_ARG) $(5); \
	}
endef

.PHONY: help check-python check-idf init-idf test \
	build-heltec-v2 build-heltec-v3 \
	flash-heltec-v2 flash-heltec-v3 \
	flash-monitor-heltec-v2 flash-monitor-heltec-v3 \
	monitor-heltec-v2 monitor-heltec-v3 \
	menuconfig-heltec-v2 menuconfig-heltec-v3 \
	clean clean-heltec-v2 clean-heltec-v3 clean-test \
	dist dist-heltec-v2 dist-heltec-v3 \
	serve-pages \
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
			if ($$0 ~ /^# (help|init-idf|idf|idf_with_target): /) next; \
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

# init-idf: Verify ESP-IDF is installed and print idf.py version
init-idf: check-idf
	@echo "ESP-IDF ready: $(IDF_EXPORT)"
	@$(source_idf) && $(IDF_PY) --version

submodules:
	git submodule update --init --recursive

# setup: Initialize submodules and install web UI dependencies
setup: submodules web-ui-install

# --- Heltec V2 ---

# menuconfig-heltec-v2: Open menuconfig for V2
menuconfig-heltec-v2: check-idf
	$(call idf_with_target,$(V2_BUILD),esp32,$(V2_DEFAULTS),$(V2_SDKCONFIG),menuconfig)

# build-heltec-v2: Build firmware for Heltec WiFi LoRa 32 V2 (ESP32, SX1276)
build-heltec-v2: check-idf
	$(call idf_with_target,$(V2_BUILD),esp32,$(V2_DEFAULTS),$(V2_SDKCONFIG),build)

# flash-heltec-v2: Build and flash Heltec WiFi LoRa 32 V2
flash-heltec-v2: check-idf
	$(call idf_with_target,$(V2_BUILD),esp32,$(V2_DEFAULTS),$(V2_SDKCONFIG),build flash $(PORT_ARG))

# flash-monitor-heltec-v2: Build, flash, and open serial monitor (V2)
flash-monitor-heltec-v2: check-idf
	$(call idf_with_target,$(V2_BUILD),esp32,$(V2_DEFAULTS),$(V2_SDKCONFIG),build flash monitor $(PORT_ARG))

# monitor-heltec-v2: Open serial monitor for V2 build
monitor-heltec-v2: check-idf
	$(call idf,$(V2_SDKCONFIG),$(V2_DEFAULTS),$(V2_BUILD),monitor $(PORT_ARG))

# clean-heltec-v2: Remove V2 build directory
clean-heltec-v2:
	rm -rf $(V2_BUILD)

# --- Heltec V3 ---

# menuconfig-heltec-v3: Open menuconfig for V3
menuconfig-heltec-v3: check-idf
	$(call idf_with_target,$(V3_BUILD),esp32s3,$(V3_DEFAULTS),$(V3_SDKCONFIG),menuconfig)

# build-heltec-v3: Build firmware for Heltec WiFi LoRa 32 V3 (ESP32-S3, SX1262)
build-heltec-v3: check-idf
	$(call idf_with_target,$(V3_BUILD),esp32s3,$(V3_DEFAULTS),$(V3_SDKCONFIG),build)

# flash-heltec-v3: Build and flash Heltec WiFi LoRa 32 V3
flash-heltec-v3: check-idf
	$(call idf_with_target,$(V3_BUILD),esp32s3,$(V3_DEFAULTS),$(V3_SDKCONFIG),build flash $(PORT_ARG))

# flash-monitor-heltec-v3: Build, flash, and open serial monitor (V3)
flash-monitor-heltec-v3: check-idf
	$(call idf_with_target,$(V3_BUILD),esp32s3,$(V3_DEFAULTS),$(V3_SDKCONFIG),build flash monitor $(PORT_ARG))

# monitor-heltec-v3: Open serial monitor for V3 build
monitor-heltec-v3: check-idf
	$(call idf,$(V3_SDKCONFIG),$(V3_DEFAULTS),$(V3_BUILD),monitor $(PORT_ARG))

# clean-heltec-v3: Remove V3 build directory
clean-heltec-v3:
	rm -rf $(V3_BUILD)

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

# --- Distribution ---

# dist-heltec-v2: Build V2 and merge into a single flashable .bin
dist-heltec-v2:
	$(call idf_with_target,$(V2_BUILD),esp32,$(V2_DEFAULTS),$(V2_SDKCONFIG),build) && \
		cd $(V2_BUILD) && \
		esptool.py --chip esp32 merge_bin \
			-o smoke-x-receiver-heltec-v2.bin \
			--flash_mode dio --flash_size 8MB \
			0x1000 bootloader/bootloader.bin \
			0x8000 partition_table/partition-table.bin \
			0x10000 smoke-x.bin \
			0x210000 storage.bin

# dist-heltec-v3: Build V3 and merge into a single flashable .bin
dist-heltec-v3:
	$(call idf_with_target,$(V3_BUILD),esp32s3,$(V3_DEFAULTS),$(V3_SDKCONFIG),build) && \
		cd $(V3_BUILD) && \
		esptool.py --chip esp32s3 merge_bin \
			-o smoke-x-receiver-heltec-v3.bin \
			--flash_mode dio --flash_size 8MB \
			0x0 bootloader/bootloader.bin \
			0x8000 partition_table/partition-table.bin \
			0x10000 smoke-x.bin \
			0x210000 storage.bin

# dist: Build both boards and produce merged distribution binaries
dist: dist-heltec-v2 dist-heltec-v3

# --- Install page (local preview) ---

# Port for the local preview server. Override with `make serve-pages HTTP_PORT=...`.
HTTP_PORT ?= 8000

# Where docs/ + firmware/ get staged for the local preview. Mirrors the
# layout that release.yml + pages.yml deploy to GitHub Pages.
PAGES_STAGE := build/pages

# serve-pages: Stage docs and firmware locally and serve the install page
# at http://localhost:$(HTTP_PORT) for end-to-end install + Improv testing.
# Run `make dist-heltec-vN` first to produce the binaries; missing boards
# are skipped with a warning (you can still test the install button for
# whichever board you did build).
serve-pages:
	@rm -rf $(PAGES_STAGE)
	@mkdir -p $(PAGES_STAGE)/firmware/heltec-v2 $(PAGES_STAGE)/firmware/heltec-v3
	@cp -r docs/. $(PAGES_STAGE)/
	@for board in heltec-v2 heltec-v3; do \
		for src in \
			build/$$board/bootloader/bootloader.bin \
			build/$$board/partition_table/partition-table.bin \
			build/$$board/smoke-x.bin \
			build/$$board/storage.bin \
			build/$$board/smoke-x-receiver-$$board.bin; do \
			if [ -f "$$src" ]; then \
				cp "$$src" "$(PAGES_STAGE)/firmware/$$board/"; \
			else \
				echo "  note: $$src not found (run 'make dist-$$board' to include this board)"; \
			fi; \
		done; \
	done
	@(cd $(PAGES_STAGE)/firmware && find . -type f -name '*.bin' | sort | xargs sha256sum > SHA256SUMS.txt 2>/dev/null || true)
	@echo ""
	@echo "Serving install page at http://localhost:$(HTTP_PORT)/"
	@echo "WebSerial requires a Chromium-based browser or recent Firefox."
	@echo "Press Ctrl+C to stop."
	@cd $(PAGES_STAGE) && python3 -m http.server $(HTTP_PORT)

# --- Tests ---

TEST_BUILD := build/test

# test: Build and run host unit tests (no ESP-IDF required)
test:
	cmake -S test -B $(TEST_BUILD)
	cmake --build $(TEST_BUILD)
	ctest --test-dir $(TEST_BUILD) --verbose

# clean-test: Remove host test build directory
clean-test:
	rm -rf $(TEST_BUILD)

# --- Clean ---

# clean: Remove all build/test artifacts
clean: clean-heltec-v2 clean-heltec-v3 clean-test clean-web-ui
	rm -rf build
