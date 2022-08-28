# ThermoWorks Smoke X Receiver

<img width="405" alt="screenshot" src="https://user-images.githubusercontent.com/7310260/150705723-5d0896be-6a5a-4b4c-8cb7-534bb355dd1f.png">

This is an ESP32+LoRa application that pairs with a [ThermoWorks Smoke X](https://www.thermoworks.com/smokex2/) remote thermometer and serves as an RF gateway to publish temperature data to an MQTT broker. This project was designed specifically to integrate with Home Assistant, but may also be used for any other MQTT-based application. This device will work alongside any existing Smoke X receivers (i.e. all paired receivers will still work). In addition, this application may also operate in a limited standalone fashion (operating in AP mode without MQTT or Home Assistant) for field use.

## Motivation

I love the Smoke X's long RF range and the fact that it doesn't need an internet connection or mobile app to function. But the receiver unit only shows current/max/min probe readings, with no means of watching for trends. This ESP32+LoRa application allows Smoke X users to have all the benefits of the Smoke X and also record the data.

## Requirements

### Hardware

An ESP32 with attached Semtech SX1276 LoRa transceiver is required. A combined ESP32+LoRa development board such as the [Heltec WiFi LoRa 32 (V2)](https://heltec.org/project/wifi-lora-32/) is ideal, but any ESP32 board with a SPI connected SX1276 will work.

- The Smoke X operates in the 915 MHz ISM band
- The default SPI pin connections are: CS: 18, RST: 14, MOSI: 27, MISO: 19, SCK: 5
- If your hardware is wired differently, update the pin assignments via `make menuconfig` prior to building

### Software

- ESP-IDF SDK v4.x (you may optionally install to the project directory with the `make sdk` target)
- Node.js and npm (to build the web UI assets)

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

- Create sdkconfig file:

```
$ make defconfig
```

- Edit `CONFIG_ESPTOOLPY_PORT` as necessary to reflect your ESP32 serial port name, either with `make menuconfig` or by manually editing the sdkconfig file.

### Build and Flash

- Connect ESP32 to your computer and run:

```
$ make flash
```

## Initial Application Setup

After the ESP32 is flashed with the application, several items need to be configured and saved to NVRAM:

- WLAN Network
- Smoke X Pairing
- MQTT Setup

### WLAN Network

The device will default to AP mode if WLAN information has not been configured, or if the connection fails. The default AP mode information is:

- SSID: "Smoke X Receiver"
- PSK: "the extra b is for byob"

Connect to the ESP32 by using a web browser to navigate to http://192.168.4.1/wlan
You will be presented with a self-explanatory web UI to configure the device to your home network. Once you apply the new information, reconnect to your home network. The ESP32 will supply a DHCP client hostname of `smoke_x`.

### Smoke X Pairing

The web UI will indicate that the device requires pairing to a Smoke X base unit. If the device is ever unpaired, it will alternate monitoring the two sync channels (920 MHz for X2, 915 MHz for X4), and will pair with the first Smoke X sync transmission it receives. To pair, place the Smoke X base unit in sync mode which will cause it to send sync bursts every three seconds. Once the ESP32 receives and parses the burst, it will transmit a sync response on the target frequency, and the base unit will return to normal operation. At this point you can confirm in the web UI that the device is paired with a specific device ID and frequency. This is the only time the ESP32 will transmit a LoRa signal. The device may always be unpaired via the web UI.

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

## LoRa

There is a LoRa configuration page in the web UI. Configuration of the LoRa modem is done automatically, and changing any of these settings will likely result in loss of sync with your Smoke X. Changes done in this menu are non-persistent, and a reboot of the ESP32 will return the device to a mode compatible with the Smoke X.

<img width="409" alt="LoRa" src="https://user-images.githubusercontent.com/7310260/150706197-6107c5c5-6f34-4970-8ba2-f622217ae69e.png">
