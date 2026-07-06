import { rest } from "msw"

// --- Mock data: a ~4 hour low-and-slow BBQ smoke ----------------------------
// The base station transmits every 30s, so 4 hours ≈ 480 samples. Probe 4 is
// the smoker/pit (holding ~250°F); probes 1-3 are beef, pork, and chicken.
const SAMPLES = 480

const round1 = (v) => Math.round(v * 10) / 10

// Deterministic "sensor noise" so the curves look organic but stay stable
// across reloads (nicer for screenshots than Math.random).
const noise = (i, amp, freq = 0.6, phase = 0) =>
  amp * (0.6 * Math.sin(i * freq + phase) + 0.4 * Math.sin(i * freq * 2.7 + phase))

// Meat: exponential rise toward a stall temperature, then a slow creep upward.
function meatCurve({ start, stall, creepEnd, riseTau, stallAt = 0.45 }) {
  const out = []
  for (let i = 0; i < SAMPLES; i++) {
    const f = i / (SAMPLES - 1)
    const rise = start + (stall - start) * (1 - Math.exp(-i / riseTau))
    const creep =
      f > stallAt ? ((f - stallAt) / (1 - stallAt)) * (creepEnd - stall) : 0
    out.push(round1(rise + creep + noise(i, 0.4)))
  }
  return out
}

// Pit: warms up, then holds near the target with wander and a few lid-open dips.
function pitCurve({ target = 250, warmupSamples = 24, dips = [130, 250, 370] }) {
  const out = []
  for (let i = 0; i < SAMPLES; i++) {
    const base =
      i < warmupSamples ? 205 + (target - 205) * (i / warmupSamples) : target
    let dip = 0
    for (const d of dips) {
      if (i >= d && i < d + 12) dip -= 32 * Math.sin((Math.PI * (i - d)) / 12)
    }
    out.push(round1(base + noise(i, 6, 0.2) + noise(i, 2.5, 0.85, 1.1) + dip))
  }
  return out
}

const probe = (history, alarm_min, alarm_max) => ({
  current_temp: history[history.length - 1],
  alarm_min,
  alarm_max,
  history,
})

function buildSmokeData() {
  // Beef (brisket) and pork (shoulder) are mid-cook and stalled; chicken has
  // just reached its 165°F target (so it trips the high alarm); pit holds ~250.
  const beef = meatCurve({ start: 39, stall: 150, creepEnd: 158, riseTau: 72 })
  const pork = meatCurve({ start: 41, stall: 158, creepEnd: 168, riseTau: 84 })
  const chicken = meatCurve({
    start: 40,
    stall: 160,
    creepEnd: 166,
    riseTau: 56,
    stallAt: 0.6,
  })
  const pit = pitCurve({ target: 250 })
  return {
    probe_1: probe(beef, 32, 203), // beef (brisket) — target 203°F
    probe_2: probe(pork, 32, 203), // pork (shoulder) — target 203°F
    probe_3: probe(chicken, 32, 165), // chicken — target 165°F
    probe_4: probe(pit, 250, 250), // smoker/pit — Billows control probe, setpoint 250°F
    billows: true,
  }
}

export default [
  rest.get("/data", (req, res, ctx) => {
    return res(ctx.delay(500), ctx.json(buildSmokeData()))
  }),
  rest.get("/wlan-config", (req, res, ctx) => {
    return res(
      ctx.delay(500),
      ctx.json({
        mode: 1,
        authType: 3,
        ssid: "BackyardBBQ",
        // The firmware never returns the stored password (always blank).
        username: "",
        password: "",
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
        deviceModel: "X4",
      })
    )
  }),
  rest.get("/mqtt-config", (req, res, ctx) => {
    return res(
      ctx.delay(500),
      ctx.json({
        uri: "mqtt://homeassistant.local:1883",
        identity: "",
        username: "smoke-x",
        password: "",
        ca_cert: "",
        client_cert: "",
        client_key: "",
        use_mqtt: true,
        cert_auth: false,
        enabled: true,
        ha_discovery: true,
        ha_base_topic: "homeassistant",
        ha_status_topic: "homeassistant/status",
        ha_birth_payload: "online",
        state_topic: "homeassistant/smoke-x/state",
      })
    )
  }),
  rest.post("/mqtt-config", (req, res, ctx) => {
    return res(ctx.delay(300), ctx.text("OK"))
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
  rest.post("/rf-params", (req, res, ctx) => {
    return res(ctx.delay(300), ctx.text("OK"))
  }),
  rest.post("/cmd", (req, res, ctx) => {
    return res(ctx.delay(300), ctx.text("OK"))
  }),
]
