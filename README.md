# ThermoWorks Smoke X Receiver

<img width="405" alt="screenshot" src="https://user-images.githubusercontent.com/7310260/150705723-5d0896be-6a5a-4b4c-8cb7-534bb355dd1f.png">

This is an ESP32+LoRa application that pairs with a [ThermoWorks Smoke X2](https://www.thermoworks.com/smokex2/) or [Smoke X4](https://www.thermoworks.com/smokex4/) remote thermometer and serves as an RF gateway to publish temperature data to an MQTT broker. This project was designed specifically to integrate with Home Assistant, but may also be used for any other MQTT-based application. This device will work alongside any existing Smoke X receivers (i.e. all paired receivers will still work). In addition, this application may also operate in a limited standalone fashion (operating in AP mode without MQTT or Home Assistant) for field use.

_NOTE:_ This application is **not** compatible with the original [ThermoWorks Smoke](https://www.thermoworks.com/smoke/) or [ThermoWorks Signals](https://www.thermoworks.com/signals/) products.

---

- [Motivation](#motivation)
- [Requirements](#requirements)
- [Build](#build)
- [Initial Application Setup](#initial-application-setup)
- [MQTT Schema](#mqtt-schema)
- [Home Assistant](#home-assistant)
- [HTTP API](#http-api)
- [LoRa](#lora)
- [Development](#development)

---

## Motivation

I love the Smoke X's long RF range and the fact that it doesn't need an internet connection or mobile app to function. But the receiver unit provided by TheroWorks only shows current/max/min probe readings, with no means of watching for trends. This ESP32+LoRa application allows Smoke X users to have all the benefits of the Smoke X and also visualize the temperature history as well as have the ability to send the data to a recorder via MQTT. I use a kamado style grill for smoking and am constantly refining my methods. It is helpful for me to keep data to learn from mistakes such as the example below when I lost control of my grill temperature.

<img width="1602" alt="screenshot" src="https://user-images.githubusercontent.com/7310260/190879396-e9dd20b8-dca5-4b48-9fbd-fe4c0fc18d6c.png">

My dataflow is:

```
Smoke X2 (Sensor) --> ESP32 (LoRa Rx and Translation) --> Mosquitto (MQTT Broker) --> Home Assistant (Data Aggregation and Automation) --> InfluxDB (Data Storage) --> Grafana (Visualization)
```

Note that this dataflow does not involve the internet or any cloud services. All data is acquired, processed, and stored locally!

---

## Requirements

### Hardware

An ESP32 with attached Semtech SX1276 LoRa transceiver is required. A combined ESP32+LoRa development board such as the [Heltec WiFi LoRa 32 (V2)](https://heltec.org/project/wifi-lora-32/) is ideal, but any ESP32 board with a SPI connected SX1276 will work.

- The Smoke X operates in the 915 MHz ISM band
- The default SPI pin connections are: CS: 18, RST: 14, MOSI: 27, MISO: 19, SCK: 5
- If your hardware is wired differently, update the pin assignments by editing `sdkconfig.defaults` prior to building

### Software

- ESP-IDF SDK v4.x (you may optionally install this SDK to the project directory with the `make sdk` target)
- Node.js and npm (to build the web UI assets)

---

## Build

### Prepare Environment

- Clone this repo (with `--recurse-submodules`) and enter the directory.
- If you don't already have the ESP-IDF SDK, the `make sdk` target will download it for you:

```
$ make sdk
```

- Activate ESP-IDF (either your preexisting one or the one downloaded from the previous step):

```
$ source ./esp-idf/export.sh
```

### Configure

- Edit `sdkconfig.defaults` to match your hardware by either modifying or adding additional entries.
  - The most common necessary change is CONFIG_ESPTOOLPY_PORT to match the serial device name assigned by your computer when the ESP32 is plugged in
  - Other changes may include XTAL frequency, CPU frequency, and SPI pin assignments (defaults will work for the Heltec WiFi LoRa 32 v2)
  - See [ESP-IDF Project Configuration documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4.2/esp32/api-reference/kconfig.html) for additional information
- OPTIONAL: Additional ESP32 configuration changes can also be made by manually editing `sdkconfig` or running:

```
$ idf.py menuconfig
```

### Build and Flash

- Connect ESP32 to your computer and run:

```
$ idf.py flash
```

The application and web assets will be built and written to the ESP32 flash. After flashing is complete, it may be helpful to monitor the ESP32 logs during initial application setup to aid in debugging. While the ESP32 is still plugged into your computer, monitor logs by running:

```
$ idf.py monitor
```

---

## Initial Application Setup

After the ESP32 is flashed with the application, several items need to be configured and saved to NVRAM:

- WLAN Network
- Smoke X Pairing
- MQTT Setup

### WLAN Network

The device will default to AP mode if WLAN information has not been configured, or if the connection fails. The default AP mode information is:

- SSID: "Smoke X Receiver"
- PSK: "The extra B is for BYOBB"

Connect to the ESP32 by using a web browser to navigate to http://192.168.4.1/wlan
You will be presented with a self-explanatory web UI to configure the device to your home network. Once you apply the new information, the device will reset and attempt to join your home network. The ESP32 will supply a DHCP client hostname request for `smoke_x`. Once you find the ESP32 on your home network, you may proceed with the remainder of the setup process.

### Smoke X Pairing

The web UI will indicate that the device requires pairing to a Smoke X base unit. If the device is in an unpaired state, it will alternate monitoring the two sync channels (920 MHz for X2, 915 MHz for X4), and will pair with the first Smoke X sync transmission it receives. To pair, place the Smoke X base unit in sync mode which will cause it to send sync bursts every three seconds. Once the ESP32 receives and parses the burst, it will transmit a sync response on the target frequency, and the base unit will return to normal operation. At this point you can confirm in the web UI that the device is paired with a specific device ID and frequency. This is the only time the ESP32 will transmit a LoRa signal. The device may always be unpaired via the web UI.

### MQTT Setup

The web UI is also used to configure the device to connect to an MQTT broker. The MQTT URI is the only mandatory field, the rest are optional and will depend on your specific MQTT broker configuration. MQTTS server authentication is supported by entering a trusted CA PEM via the web UI. PKI client auth is not currently supported.

## MQTT Schema

If MQTT is configured and enabled, the application will publish status messages upon receipt of an RF transmission from the Smoke X base station. The base station transmits every thirty seconds. The published message contents are:

```json
{
  "probe_1_attached": "ON",
  "probe_1_alarm": "ON",
  "probe_1_temp": 70.4,
  "probe_1_max": 185,
  "probe_1_min": 32,
  "billows_target": "offline",
  "probe_2_attached": "ON",
  "probe_2_alarm": "ON",
  "probe_2_temp": 70.4,
  "probe_2_max": 91,
  "probe_2_min": 50,
  "billows_attached": "OFF"
}
```

_NOTE:_ X4 devices will also include additional data for probes 3 and 4

---

## Home Assistant

This application supports [Home Assistant MQTT discovery](https://www.home-assistant.io/docs/mqtt/discovery/). If your Home Assistant instance has the MQTT integration configured for discovery, the following Smoke X sensors will automatically be added:

- Sensors
  - Current Temperature
  - Alarm Min Temperature
  - Alarm Max Temperature
  - Billows Set Temperature
- Binary Sensors
  - Probe Attached
  - Billows Attached
  - Alarm Enabled

After successful connection to the MQTT broker, the device will configure each sensor by publishing discovery messages similar to the following (one for each entity):

```json
{
  "dev": {
    "name": "Smoke X Receiver",
    "identifiers": "|ABC12",
    "sw_version": "0.1.0",
    "model": "X2",
    "manufacturer": "ThermoWorks"
  },
  "exp_aft": 120,
  "pl_not_avail": "offline",
  "stat_t": "homeassistant/smoke-x/state",
  "dev_cla": "temperature",
  "unit_of_meas": "°F",
  "uniq_id": "smoke-x_probe_1_temp",
  "name": "Smoke X Probe 1 Temp",
  "val_tpl": "{{value_json.probe_1_temp}}"
}
```

In addition, the application will subscribe to the Home Assistant status topic for birth announcements. If Home Assistant restarts, the birth announcement will signal to the application to re-publish the discovery messages. Default Home Assistant topic names are used, but may be customized in the ESP32 web UI.

---

## HTTP API

The application provides an HTTP API (used by the web UI's temperature history graph) that may also be used by any other client that is able to send HTTP requests to the ESP32.

### GET /data

Response:

```json
{
  "probe_1": {
    "current_temp": 95.3,
    "alarm_max": 185,
    "alarm_min": 32,
    "history": [95.1, 95.2, 95.3]
  },
  "probe_2": {
    "current_temp": 165.9,
    "alarm_max": 91,
    "alarm_min": 50,
    "history": [165.7, 165.8, 165.9]
  },
  "billows": false
}
```

_NOTE:_ X4 devices will also include additional data for probes 3 and 4

---

## LoRa

There is a LoRa configuration page in the web UI. Configuration of the LoRa modem is done automatically, and changing any of these settings will likely result in loss of sync with your Smoke X. Changes done in this menu are non-persistent, and a reboot of the ESP32 will return the device to a mode compatible with the Smoke X.

<img width="409" alt="LoRa" src="https://user-images.githubusercontent.com/7310260/150706197-6107c5c5-6f34-4970-8ba2-f622217ae69e.png">
***

## Development

PRs to fix bugs or enhance/add functionality are welcome! If you have successfully built the application, you have everything needed to modify it.

### Main Application

This application is built with the ESP-IDF v4 SDK and has one external dependency, [esp32-lora-library](https://github.com/Inteform/esp32-lora-library/tree/a06fc122096db8d68362701aa8c8d79a26ae2a74).

### Web UI

The web interface is written in Vue and is loaded onto the ESP32 flash file system as compressed static web assets which are served by the ESP32 web server. To aid in development and manual testing, the web interface can be previewed with:

```
$ make mock-www
```
