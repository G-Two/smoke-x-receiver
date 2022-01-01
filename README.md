# ThermoWorks Smoke X Receiver

An ESP32 application to receive ThermoWorks Smoke X remote thermometer RF transmissions and publish the data to an MQTT broker. This project was designed specifically to integrate with Home Assistant, but may also be used for any other MQTT consumer. This will work alongside any existing Smoke X receivers (i.e. all paired receivers will still work).

## Motivation

I love the Smoke X's long RF range and the fact that it doesn't need an internet connection or yet another cloud account to work. But the receiver unit only shows current/max/min probe readings, with no means of watching for trends. This ESP32+LoRa application allows Smoke X users to ingest the temperature data into Home Assistant for monitoring and recording.

### Prerequisites

To build the ESP32 application:

- ESP-IDF SDK v4.x (you may optionally install to the project directory with the `make sdk` target)
- Node.js and npm (to build the web UI assets)

To run the ESP32 application:

- [Heltec WiFi LoRa 32 (V2)](https://heltec.org/project/wifi-lora-32/) or similar development board
  - Note: Any ESP32 board with a SPI connected Semtech SX1276/1277/1279 LoRa transceiver should work
    - The default SPI pin connections are: CS: 18, RST: 14, MOSI: 27, MISO: 19, SCK: 5
    - If your hardware is wired differently, simply update via `make menuconfig` prior to building

### Prepare Environment

Clone the repo (with `--recurse-submodules`) and enter the directory.

```
# Ensure ESP-IDF is installed and activated. If you are unsure, try the following:
$ make sdk
$ source ./esp-idf/export.sh
```

### Build

If you need to make changes to the SPI configuration

```
$ make menuconfig
```

Otherwise, just build and flash

```
$ make
$ make flash
```

## Device Configuration

After the ESP32 is flashed, several items need to be configured:

- Wifi Network
- Smoke X Pairing
- MQTT Setup

### Wifi Network

The device will default to AP mode if Wifi information has not been configured, or if the configuration fails. The default AP mode information is:

- SSID: "Smoke X Receiver"
- PSK: "the extra b is for byob"

Connect to the ESP32 with any device and use a web browser to navigate to http://192.168.4.1
You will be presented with a self-explanatory web UI to configure the device to your home network. Once you apply the new information, reconnect to your home network.

### Smoke X Pairing

The web UI will indicate that the device requires pairing to a Smoke X base unit. If the device is ever unpaired, it will automatically monitor the sync frequency of 920.0 MHz, and will pair with the first Smoke X sync transmission it receives. To pair, simply place the Smoke X base unit in sync mode. Within a few seconds, the base unit will beep and return to normal operation. At this point you can confirm in the web UI that the device is paired with a specific device ID and frequency. This is the only time the device will normally transmit. The device may always be unpaired via the web UI.

### MQTT Setup

The web UI is also used to configure the device to point to an MQTT broker. Anonymous as well as username/password authentication is supported. TLS is not currently supported.

## Message Schema

TODO: Document this...
Home Assistant knows what to do already. If your Home Assistant instance has the MQTT integration setup, the Smoke X sensors will automatically be added.

## TODO

- X4 Support (currently only supports X2)
- MQTTS (PKI and PSK)
- Monitoring via web UI (for BBQ away from your home network)
- Code quality...
