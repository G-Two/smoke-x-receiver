import { rest } from "msw"

export default [
  rest.get("/status", (req, res, ctx) => {
    return res(
      ctx.delay(500),
      ctx.json({
        wifi: 1,
        paired: true,
        num_probes: 4,
        probe1: 160.4,
        probe2: 162.4,
        probe3: 163.4,
        probe4: 164.4,
        billows: true,
        billows_target: 225,
      })
    )
  }),
  rest.get("/data", (req, res, ctx) => {
    return res(
      ctx.delay(500),
      ctx.json({
        probe_1: {
          current_temp: 84,
          alarm_max: 120,
          alarm_min: 35,
          history: [
            70.1, 70.1, 70.1, 70.1, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.4,
            70.5, 70.5, 70.5, 70.6, 70.7, 70.8, 70.9, 70.9, 70.9, 71, 71, 70.9,
            70.9, 70.8, 70.7, 70.7, 70.7, 70.7, 70.6, 70.6, 70.6, 70.5, 70.5,
            70.5, 70.4, 70.5, 70.5, 70.4, 70.4, 70.4, 70.4, 70.4, 70.4, 70.4,
            70.4, 70.4, 70.4, 70.4, 70.4, 70.3, 70.3, 70.3, 70.2, 70.3, 70.3,
            70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2,
            70.2, 70.2, 70.2, 70.2, 71.3, 72.6, 70, 64.2, 71.7, 72, 72, 72.1,
            72.5, 72.7, 73.2, 73.4, 74.2, 74, 74.8, 74.9, 75.7, 76.3, 76.7,
            77.4, 77.8, 78.7, 79.6, 79.8, 80.3, 80.9, 81.5, 82, 82.8, 83.3, 84,
          ],
        },
        probe_2: {
          current_temp: 179,
          alarm_max: 120,
          alarm_min: 35,
          history: [
            70.1, 70.1, 70, 70.1, 70.1, 70.2, 70.2, 70.2, 70.2, 70.2, 70.3,
            70.4, 70.4, 70.5, 70.6, 70.7, 70.7, 70.7, 70.7, 70.9, 71, 70.9,
            70.9, 70.8, 70.7, 70.7, 70.7, 70.7, 70.6, 70.6, 70.5, 70.5, 70.5,
            70.5, 70.5, 70.4, 70.4, 70.4, 70.4, 70.4, 70.4, 70.4, 70.4, 70.4,
            70.3, 70.4, 70.4, 70.3, 70.4, 70.3, 70.3, 70.3, 70.3, 70.2, 70.2,
            70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2,
            70.2, 70.2, 70.2, 70.2, 70.2, 72.2, 73.4, 73.9, 74.4, 74.1, 76.6,
            80.7, 87.6, 96.9, 110.5, 126.1, 147.4, 168.9, 183.1, 180.4, 179.8,
            179.9, 180.3, 180.9, 181.5, 181.9, 182.1, 182.3, 182.3, 182.1,
            181.7, 181.3, 180.9, 180.4, 180, 179.4,
          ],
        },
        probe_3: {
          current_temp: 70,
          alarm_max: 120,
          alarm_min: 35,
          history: [
            84, 83.3, 82.8, 82, 81.5, 80.9, 80.3, 79.8, 79.6, 78.7, 77.8, 77.4,
            76.7, 76.3, 75.7, 74.9, 74.8, 74, 74.2, 73.4, 73.2, 72.7, 72.5,
            72.1, 72, 72, 71.7, 64.2, 70, 72.6, 71.3, 70.2, 70.2, 70.2, 70.2,
            70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2,
            70.3, 70.3, 70.2, 70.3, 70.3, 70.3, 70.4, 70.4, 70.4, 70.4, 70.4,
            70.4, 70.4, 70.4, 70.4, 70.4, 70.4, 70.4, 70.5, 70.5, 70.4, 70.5,
            70.5, 70.5, 70.6, 70.6, 70.6, 70.7, 70.7, 70.7, 70.7, 70.8, 70.9,
            70.9, 71, 71, 70.9, 70.9, 70.9, 70.8, 70.7, 70.6, 70.5, 70.5, 70.5,
            70.4, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.1, 70.1, 70.1, 70.1,
          ],
        },
        probe_4: {
          current_temp: 70,
          alarm_max: 120,
          alarm_min: 35,
          history: [
            179.4, 180, 180.4, 180.9, 181.3, 181.7, 182.1, 182.3, 182.3, 182.1,
            181.9, 181.5, 180.9, 180.3, 179.9, 179.8, 180.4, 183.1, 168.9,
            147.4, 126.1, 110.5, 96.9, 87.6, 80.7, 76.6, 74.1, 74.4, 73.9, 73.4,
            72.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2,
            70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.2, 70.3, 70.3, 70.3,
            70.3, 70.4, 70.3, 70.4, 70.4, 70.3, 70.4, 70.4, 70.4, 70.4, 70.4,
            70.4, 70.4, 70.4, 70.4, 70.5, 70.5, 70.5, 70.5, 70.5, 70.6, 70.6,
            70.7, 70.7, 70.7, 70.7, 70.8, 70.9, 70.9, 71, 70.9, 70.7, 70.7,
            70.7, 70.7, 70.6, 70.5, 70.4, 70.4, 70.3, 70.2, 70.2, 70.2, 70.2,
            70.2, 70.1, 70.1, 70, 70.1, 70.1,
          ],
        },
        billows: false,
      })
    )
  }),
  rest.get("/wlan-config", (req, res, ctx) => {
    return res(
      ctx.delay(500),
      ctx.json({
        mode: 1,
        authType: 5,
        ssid: "fun_wifi_thing",
        username: "a_username",
        password: "a_password",
      })
    )
  }),
  rest.post("/wlan-config", (req, res, ctx) => {
    ctx.body
    return res(
      ctx.delay(500),
      ctx.json({
        success: true,
      })
    )
  }),
  rest.get("/pairing-status", (req, res, ctx) => {
    return res(
      ctx.delay(500),
      ctx.json({
        isPaired: true,
        deviceId: "|ABC12",
        currentFrequency: 915000000,
        deviceModel: "X2",
      })
    )
  }),
  rest.get("/mqtt-config", (req, res, ctx) => {
    return res(
      ctx.delay(500),
      ctx.json({
        uri: "mqtt://your.mqtt.broker",
        identity: "your_identity",
        username: "your_username",
        password: "your_password",
        ca_cert:
          "-----BEGIN CERTIFICATE-----\n\
Paste Certificate Here\n\
-----END CERTIFICATE-----",
        enabled: true,
        ha_discovery: true,
        ha_base_topic: "homeassistant",
        ha_status_topic: "homeassistant/status",
        ha_birth_payload: "online",
        state_topic: "homeassistant/smoke-x/state",
      })
    )
  }),
  rest.get("/rf-params", (req, res, ctx) => {
    return res(
      ctx.delay(500),
      ctx.json({
        frequency: 915000000,
        txPower: 10,
        bandwidth: 125000,
        spreadingFactor: 7,
        codingRate: 5,
        implicitHeader: false,
        enableCRC: true,
        messageLength: 20,
        preambleLength: 10,
        syncWord: 0x12,
      })
    )
  }),
]
