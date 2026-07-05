# Contributing to Smoke X Receiver

Bug fixes and feature additions are welcome via pull request. This document covers everything you need to build the firmware from source, run the web UI locally, and submit changes.

If you just want to **install the firmware on a board** (no source build required), use the [browser-based installer](https://g-two.github.io/smoke-x-receiver/) instead.

---

- [Build](#build)
- [Development](#development)

---

## Build

Supported on Linux and macOS. The typical first-time setup is six steps. If you already have a working ESP-IDF v5.4 installation, skip to step 3.

### 1. Install host tools

Required system-wide:

- **Python 3.9+** on `$PATH` — ESP-IDF itself needs this to install in the next step
- **Node.js + npm** — for the web UI build (any recent LTS)
- **CMake ≥ 3.16**

Use your system package manager (Homebrew, apt, dnf, etc.).

### 2. Install ESP-IDF v5.4

```bash
mkdir -p ~/esp
cd ~/esp
git clone -b release/v5.4 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32,esp32s3
```

See the official guide for platform-specific prerequisites:
https://docs.espressif.com/projects/esp-idf/en/release-v5.4/esp32/get-started/index.html

### 3. Clone this repo

```bash
git clone git@github.com:G-Two/smoke-x-receiver.git
cd smoke-x-receiver
```

This repo includes a `Makefile` that is the recommended entry point: it sources the ESP-IDF environment, picks a compatible Python, applies the right SDKCONFIG defaults per board, and drives `idf.py` for you. (You can call `idf.py` directly — see [Using idf.py directly](#using-idfpy-directly) at the end of this section.)

The Makefile looks for ESP-IDF's `export.sh` in this order:

- `$IDF_PATH/export.sh` — if the `IDF_PATH` environment variable is set
- `~/esp/esp-idf/export.sh` — the default install location (matches step 2)

If you installed ESP-IDF elsewhere, either export `IDF_PATH` in your shell profile, or pass `IDF_EXPORT=/path/to/export.sh` on the `make` command line.

### 4. Verify environment and fetch project dependencies

```bash
make check-python  # confirms a supported Python is detected
make check-idf     # confirms ESP-IDF export.sh is reachable
make setup         # initializes submodules and installs web UI dependencies
```

### 5. Configure (optional)

The Makefile targets will automatically set the required configuration options for Heltec V2 and V3 boards, but if further customizations are desired, enter the ESP configuration menu with:

```bash
make menuconfig-heltec-v3   # or menuconfig-heltec-v2
```

> **Note**
> The Heltec WiFi LoRa 32 V4 shares the same ESP32-S3 chip, SX1262 transceiver, and GPIO pinout as the V3. It is untested but expected to work — use the `heltec-v3` Makefile targets.

> **Note**
> Configuration changes may be needed to support non-Heltec V2/V3 boards

### 6. Build and flash

Connect your ESP32 over USB and run the install target for your board:

```bash
make flash-heltec-v3   # Heltec WiFi LoRa 32 V3 (and untested V4)
make flash-heltec-v2   # Heltec WiFi LoRa 32 V2
```

This builds the application + web assets and flashes both to the ESP32. The flash utility auto-detects the serial port; pass `PORT=/dev/cu.usbserial-XXXX` (macOS) or `PORT=/dev/ttyUSBn` (Linux) to override.

To flash and immediately open the serial monitor:

```bash
make flash-monitor-heltec-v3
```

Run `make help` for the full list of targets (build, flash, monitor, clean, menuconfig, web UI helpers).

### Using idf.py directly

The Makefile is recommended because it sources `export.sh` and passes the necessary `SDKCONFIG_DEFAULTS` per board. If you'd rather invoke `idf.py` yourself:

```bash
source ~/esp/esp-idf/export.sh   # adjust path if you cloned elsewhere
idf.py -B build/heltec-v3 \
       -DSDKCONFIG=sdkconfig.heltec-v3 \
       -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.heltec-v3" \
       set-target esp32s3 build flash
# For Heltec V2, swap heltec-v3 -> heltec-v2, esp32s3 -> esp32, and build dir.
```

> **Note**
> The Makefile keeps a separate `sdkconfig.heltec-v2` and `sdkconfig.heltec-v3` at the project root so the two board variants don't clobber each other. If you skip the `-DSDKCONFIG=...` arg above, `idf.py` will write to the default `sdkconfig` and you'll need to delete or re-run `set-target` whenever you switch boards.

---

## Development

PRs to fix bugs or enhance/add functionality are welcome! If you have successfully built the application, you have everything needed to modify it.

### Main Application

This application targets ESP-IDF v5.4. The LoRa modem drivers it uses are two third-party open-source projects by [nopnop2002](https://github.com/nopnop2002), pulled in as git submodules pinned to specific commits:

- [esp-idf-sx126x](https://github.com/nopnop2002/esp-idf-sx126x) — driver for SX1262 (used by Heltec WiFi LoRa 32 V3)
- [esp-idf-sx127x](https://github.com/nopnop2002/esp-idf-sx127x) — driver for SX1276 (used by Heltec WiFi LoRa 32 V2)

### Debugging

It may be helpful to monitor the ESP32 logs during initial application setup to aid in debugging. While the ESP32 is still plugged into your computer:

```bash
make monitor-heltec-v3   # or monitor-heltec-v2
```

### Web UI

The web interface is written in Vue and is loaded onto the ESP32 flash file system as compressed static web assets which are served by the ESP32 web server. To aid in development and manual testing, the web interface can be previewed with:

```bash
make mock-web-ui
```

#### Icons

All icons (the web UI favicon/PWA icons and the GitHub Pages installer favicon) are generated from a single source, `web_ui/icons/icon.svg`. After editing that SVG, regenerate them with:

```bash
npm --prefix web_ui run icons
```

This writes the icons into both `web_ui/public/` (firmware-served UI) and `docs/` (GitHub Pages installer page), so the two never drift. `docs/` is served as committed static files by GitHub Pages, so commit the regenerated copies.
