<script setup>
// Read-only device information & health page (reached from the settings hub).
// Polls /system-info for live metrics: firmware/build, chip, RAM, flash, NVS,
// and Wi-Fi/LoRa signal. Poll pauses while the tab is hidden.
import { computed, onMounted, onUnmounted, ref } from "vue"
import { getJSON } from "../api"
import { firmwareVersion } from "../device"

const info = ref(null)
const loaded = ref(false)
const error = ref(false)

const POLL_MS = 5000
let timer = null

async function poll() {
  clearTimeout(timer)
  if (!document.hidden) {
    try {
      const d = await getJSON("system-info")
      info.value = d
      // Keep the shared footer version in sync with what we just fetched.
      if (d?.firmware?.version) firmwareVersion.value = d.firmware.version
      error.value = false
    } catch (e) {
      error.value = true
    } finally {
      loaded.value = true
    }
  }
  timer = setTimeout(poll, POLL_MS)
}

onMounted(poll)
onUnmounted(() => clearTimeout(timer))

// ---- formatting helpers -------------------------------------------------
const DASH = "—"

function fmtBytes(n) {
  if (n == null) return DASH
  if (n < 1024) return `${n} B`
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`
  return `${(n / 1024 / 1024).toFixed(2)} MB`
}

function fmtUptime(s) {
  if (s == null) return DASH
  const d = Math.floor(s / 86400)
  const h = Math.floor((s % 86400) / 3600)
  const m = Math.floor((s % 3600) / 60)
  const sec = Math.floor(s % 60)
  const parts = []
  if (d) parts.push(`${d}d`)
  if (h || d) parts.push(`${h}h`)
  if (m || h || d) parts.push(`${m}m`)
  parts.push(`${sec}s`)
  return parts.join(" ")
}

function fmtAge(ms) {
  if (ms == null) return DASH
  const s = Math.round(ms / 1000)
  if (s < 60) return `${s}s ago`
  const m = Math.floor(s / 60)
  if (m < 60) return `${m}m ${s % 60}s ago`
  const h = Math.floor(m / 60)
  return `${h}h ${m % 60}m ago`
}

function fmtMHz(hz) {
  if (hz == null) return DASH
  return `${(hz / 1e6).toFixed(3)} MHz`
}

function pct(used, total) {
  if (!total) return 0
  return Math.min(100, Math.max(0, Math.round((used / total) * 100)))
}

// Wi-Fi signal strength → 0..4 bars and a label.
function wifiLevel(rssi) {
  if (rssi == null) return 0
  if (rssi >= -55) return 4
  if (rssi >= -66) return 3
  if (rssi >= -77) return 2
  if (rssi >= -88) return 1
  return 0
}
const WIFI_LABELS = ["No signal", "Weak", "Fair", "Good", "Excellent"]

// LoRa link quality is better judged by SNR than raw RSSI (processing gain).
function loraQuality(snr) {
  if (snr == null) return { label: DASH, cls: "" }
  if (snr >= 8) return { label: "Excellent", cls: "q-ok" }
  if (snr >= 3) return { label: "Good", cls: "q-ok" }
  if (snr >= 0) return { label: "Fair", cls: "q-warn" }
  return { label: "Weak", cls: "q-bad" }
}

// ---- derived view models ------------------------------------------------
const fw = computed(() => info.value?.firmware || {})
const chip = computed(() => info.value?.chip || {})
const mem = computed(() => info.value?.memory || {})
const flash = computed(() => info.value?.flash || {})
const nvs = computed(() => info.value?.nvs || {})
const wifi = computed(() => info.value?.wifi || {})
const lora = computed(() => info.value?.lora || {})
const mqtt = computed(() => info.value?.mqtt || {})
const sys = computed(() => info.value?.system || {})

// "Reconnects" = connections after the first, so a stable link reads 0.
const reconnects = computed(() =>
  typeof mqtt.value.connectCount === "number"
    ? Math.max(0, mqtt.value.connectCount - 1)
    : null
)
const fmtCount = (n) => (typeof n === "number" ? n.toLocaleString() : DASH)

const ramUsed = computed(() =>
  mem.value.heapTotal != null && mem.value.heapFree != null
    ? mem.value.heapTotal - mem.value.heapFree
    : null
)
const ramPct = computed(() => pct(ramUsed.value, mem.value.heapTotal))
const spiffsPct = computed(() =>
  pct(flash.value.spiffsUsed, flash.value.spiffsTotal)
)
const nvsPct = computed(() => pct(nvs.value.usedEntries, nvs.value.totalEntries))
</script>

<template>
  <div id="system-info">
    <div v-if="error" class="banner banner-error" role="alert">
      Device unreachable &mdash; retrying&hellip;
    </div>

    <h1 class="page-title">Device Information</h1>
    <p class="page-hint">Firmware, storage, and radio health.</p>

    <div v-if="loaded" class="cards">
      <!-- Firmware -->
      <section class="card">
        <h2 class="card-title">Firmware</h2>
        <dl class="rows">
          <div class="row"><dt>Version</dt><dd class="mono">{{ fw.version || DASH }}</dd></div>
          <div class="row"><dt>Project</dt><dd>{{ fw.project || DASH }}</dd></div>
          <div class="row"><dt>ESP-IDF</dt><dd class="mono">{{ fw.idf || DASH }}</dd></div>
          <div class="row">
            <dt>Built</dt>
            <dd class="mono">{{ fw.buildDate || DASH }} {{ fw.buildTime || "" }}</dd>
          </div>
        </dl>
      </section>

      <!-- Device / chip -->
      <section class="card">
        <h2 class="card-title">Device</h2>
        <dl class="rows">
          <div class="row"><dt>Chip</dt><dd>{{ chip.model || DASH }}</dd></div>
          <div class="row"><dt>Revision</dt><dd>{{ chip.revision ?? DASH }}</dd></div>
          <div class="row"><dt>Cores</dt><dd>{{ chip.cores ?? DASH }}</dd></div>
          <div class="row"><dt>MAC</dt><dd class="mono">{{ chip.mac || DASH }}</dd></div>
          <div class="row"><dt>Uptime</dt><dd>{{ fmtUptime(sys.uptime) }}</dd></div>
          <div class="row"><dt>Last reset</dt><dd>{{ sys.resetReason || DASH }}</dd></div>
        </dl>
      </section>

      <!-- Wi-Fi -->
      <section class="card">
        <h2 class="card-title">Wi-Fi</h2>
        <dl class="rows">
          <div class="row"><dt>Mode</dt><dd>{{ wifi.mode === "AP" ? "Access Point" : "Station" }}</dd></div>
          <div class="row">
            <dt>Status</dt>
            <dd>
              <span class="chip" :class="wifi.connected ? 'q-ok' : 'q-bad'">
                {{ wifi.connected ? "Connected" : "Disconnected" }}
              </span>
            </dd>
          </div>
          <div class="row"><dt>SSID</dt><dd>{{ wifi.ssid || DASH }}</dd></div>
          <div class="row"><dt>IP address</dt><dd class="mono">{{ wifi.ip || DASH }}</dd></div>
          <div class="row"><dt>Channel</dt><dd>{{ wifi.channel ?? DASH }}</dd></div>
          <div class="row">
            <dt>Signal</dt>
            <dd v-if="wifi.rssi != null" class="signal-cell">
              <span class="bars" :aria-label="`${WIFI_LABELS[wifiLevel(wifi.rssi)]} signal`">
                <span
                  v-for="i in 4"
                  :key="i"
                  class="bar"
                  :class="{ on: i <= wifiLevel(wifi.rssi) }"
                  :style="{ height: `${i * 25}%` }"
                />
              </span>
              <span class="mono">{{ wifi.rssi }} dBm</span>
              <span class="muted">{{ WIFI_LABELS[wifiLevel(wifi.rssi)] }}</span>
            </dd>
            <dd v-else class="muted">{{ DASH }}</dd>
          </div>
        </dl>
      </section>

      <!-- LoRa -->
      <section class="card">
        <h2 class="card-title">LoRa</h2>
        <dl class="rows">
          <div class="row"><dt>Frequency</dt><dd class="mono">{{ fmtMHz(lora.frequency) }}</dd></div>
          <template v-if="lora.everReceived">
            <div class="row"><dt>Last packet RSSI</dt><dd class="mono">{{ lora.rssi }} dBm</dd></div>
            <div class="row">
              <dt>SNR</dt>
              <dd>
                <span class="mono">{{ lora.snr?.toFixed(2) }} dB</span>
                <span class="chip" :class="loraQuality(lora.snr).cls">
                  {{ loraQuality(lora.snr).label }}
                </span>
              </dd>
            </div>
            <div class="row"><dt>Last packet</dt><dd>{{ fmtAge(lora.ageMs) }}</dd></div>
          </template>
          <div v-else class="row"><dt>Status</dt><dd class="muted">No packets received yet</dd></div>
        </dl>
      </section>

      <!-- MQTT -->
      <section class="card">
        <h2 class="card-title">MQTT</h2>
        <dl class="rows">
          <div class="row">
            <dt>Status</dt>
            <dd>
              <span v-if="!mqtt.enabled" class="chip">Disabled</span>
              <span v-else class="chip" :class="mqtt.connected ? 'q-ok' : 'q-bad'">
                {{ mqtt.connected ? "Connected" : "Disconnected" }}
              </span>
            </dd>
          </div>
          <template v-if="mqtt.enabled">
            <div class="row"><dt>Broker</dt><dd class="mono break">{{ mqtt.broker || DASH }}</dd></div>
            <div class="row">
              <dt>HA discovery</dt>
              <dd v-if="mqtt.haDiscovery">
                On<span class="muted"> · {{ mqtt.discoveryPublished ? "published" : "pending" }}</span>
              </dd>
              <dd v-else class="muted">Off</dd>
            </div>
            <div class="row"><dt>Last publish</dt><dd>{{ fmtAge(mqtt.lastPublishMsAgo) }}</dd></div>
            <div class="row"><dt>Publishes</dt><dd class="mono">{{ fmtCount(mqtt.publishCount) }}</dd></div>
            <div class="row"><dt>Reconnects</dt><dd class="mono">{{ fmtCount(reconnects) }}</dd></div>
            <div class="row">
              <dt>Connected for</dt>
              <dd>{{ mqtt.connectedForMs != null ? fmtUptime(Math.round(mqtt.connectedForMs / 1000)) : DASH }}</dd>
            </div>
            <div v-if="mqtt.lastError" class="row">
              <dt>Last error</dt>
              <dd class="err-cell">
                <span :class="{ 'q-bad': !mqtt.connected }" class="chip">{{ mqtt.lastError }}</span>
                <span class="muted">{{ fmtAge(mqtt.lastErrorMsAgo) }}</span>
              </dd>
            </div>
          </template>
        </dl>
      </section>

      <!-- Memory -->
      <section class="card">
        <h2 class="card-title">Memory (RAM)</h2>
        <div class="meter" :title="`${ramPct}% used`">
          <div class="meter-fill" :class="{ high: ramPct >= 85 }" :style="{ width: `${ramPct}%` }" />
        </div>
        <p class="meter-caption">
          {{ fmtBytes(ramUsed) }} used of {{ fmtBytes(mem.heapTotal) }} ({{ ramPct }}%)
        </p>
        <dl class="rows">
          <div class="row"><dt>Free</dt><dd class="mono">{{ fmtBytes(mem.heapFree) }}</dd></div>
          <div class="row"><dt>Min free (worst)</dt><dd class="mono">{{ fmtBytes(mem.heapMinFree) }}</dd></div>
          <div class="row"><dt>Largest block</dt><dd class="mono">{{ fmtBytes(mem.heapLargestBlock) }}</dd></div>
        </dl>
      </section>

      <!-- Flash -->
      <section class="card">
        <h2 class="card-title">Flash Storage</h2>
        <div class="meter" :title="`${spiffsPct}% used`">
          <div class="meter-fill" :class="{ high: spiffsPct >= 90 }" :style="{ width: `${spiffsPct}%` }" />
        </div>
        <p class="meter-caption">
          Web assets: {{ fmtBytes(flash.spiffsUsed) }} of
          {{ fmtBytes(flash.spiffsTotal) }} ({{ spiffsPct }}%)
        </p>
        <dl class="rows">
          <div class="row"><dt>Flash chip size</dt><dd class="mono">{{ fmtBytes(flash.chipSize) }}</dd></div>
        </dl>
      </section>

      <!-- NVS -->
      <section class="card">
        <h2 class="card-title">NVS (Config Store)</h2>
        <template v-if="nvs.ok">
          <div class="meter" :title="`${nvsPct}% used`">
            <div class="meter-fill" :class="{ high: nvsPct >= 90 }" :style="{ width: `${nvsPct}%` }" />
          </div>
          <p class="meter-caption">
            {{ nvs.usedEntries }} of {{ nvs.totalEntries }} entries used ({{ nvsPct }}%)
          </p>
          <dl class="rows">
            <div class="row"><dt>Free entries</dt><dd class="mono">{{ nvs.freeEntries }}</dd></div>
            <div class="row"><dt>Namespaces</dt><dd class="mono">{{ nvs.namespaceCount }}</dd></div>
          </dl>
        </template>
        <p v-else class="muted">Statistics unavailable</p>
      </section>
    </div>

    <p v-else class="page-hint">Loading&hellip;</p>
  </div>
</template>

<style scoped>
#system-info {
  max-width: 720px;
  margin: 1.5em auto;
  padding: 0 1em;
  box-sizing: border-box;
  font-family: Avenir, Helvetica, Arial, sans-serif;
  color: var(--text);
}
.page-title {
  font-size: 1.4em;
  font-weight: bold;
  margin: 0.2em 0 0.1em;
}
.page-hint {
  margin: 0 0 1em;
  color: var(--text-muted);
  font-size: 0.9em;
}
.banner {
  padding: 0.6em 0.9em;
  border-radius: 0.4em;
  margin-bottom: 1em;
  font-size: 0.9em;
}
.banner-error {
  background: var(--pill-off-bg, #b00020);
  color: var(--pill-off-fg, #fff);
}

.cards {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
  gap: 1em;
}
.card {
  border: 1px solid var(--border);
  border-radius: 0.5em;
  background: var(--surface);
  box-shadow: 0 0 1em var(--shadow);
  padding: 1em 1.1em;
}
.card-title {
  font-size: 0.78em;
  font-weight: bold;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  color: var(--brand-amber);
  margin: 0 0 0.7em;
}

.rows {
  margin: 0;
}
.row {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 1em;
  padding: 0.28em 0;
  border-top: 1px solid var(--border);
}
.row:first-child {
  border-top: none;
}
.row dt {
  color: var(--text-muted);
  font-size: 0.85em;
  flex-shrink: 0;
}
.row dd {
  margin: 0;
  text-align: right;
  font-size: 0.9em;
  display: flex;
  align-items: center;
  gap: 0.5em;
  justify-content: flex-end;
  flex-wrap: wrap;
}
.mono {
  font-family: ui-monospace, "SF Mono", Menlo, Consolas, "Roboto Mono",
    monospace;
  font-size: 0.92em;
}
/* Let long values (e.g. a broker URI) wrap instead of overflowing the row. */
.break {
  word-break: break-all;
}
.muted {
  color: var(--text-muted);
}

/* Last-error row: the message can be long, so allow the chip to wrap. */
.err-cell {
  align-items: flex-end;
}
.err-cell .chip {
  white-space: normal;
  text-align: left;
}

.chip {
  font-size: 0.72em;
  font-weight: bold;
  padding: 0.12em 0.5em;
  border-radius: 1em;
  border: 1px solid var(--border);
  color: var(--text-muted);
  white-space: nowrap;
}
.q-ok {
  background: var(--pill-ok-bg, #1a7f37);
  color: var(--pill-ok-fg, #fff);
  border-color: transparent;
}
.q-warn {
  background: #b7791f;
  color: #fff;
  border-color: transparent;
}
.q-bad {
  background: var(--pill-off-bg, #b00020);
  color: var(--pill-off-fg, #fff);
  border-color: transparent;
}

/* Usage meter (RAM / flash / NVS) */
.meter {
  height: 8px;
  border-radius: 4px;
  background: var(--border);
  overflow: hidden;
  margin-bottom: 0.4em;
}
.meter-fill {
  height: 100%;
  background: var(--brand-amber);
  border-radius: 4px;
  transition: width 0.3s ease;
}
.meter-fill.high {
  background: var(--pill-off-bg, #b00020);
}
.meter-caption {
  margin: 0 0 0.6em;
  font-size: 0.8em;
  color: var(--text-muted);
}

/* Signal strength bars */
.signal-cell {
  gap: 0.5em;
}
.bars {
  display: inline-flex;
  align-items: flex-end;
  gap: 2px;
  height: 16px;
  width: 22px;
}
.bar {
  flex: 1;
  background: var(--border);
  border-radius: 1px;
  align-self: flex-end;
}
.bar.on {
  background: var(--brand-amber);
}
</style>
