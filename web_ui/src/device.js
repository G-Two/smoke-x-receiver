// Single shared poll of the device's pairing/reachability status, consumed by
// both the nav status pill and the Pairing page. Polls fast while unpaired (to
// catch a pairing quickly) and slowly once paired; skips network requests while
// the tab is hidden.
import { ref } from "vue"
import { getJSON } from "./api"

export const loaded = ref(false)
export const reachable = ref(true)
export const isPaired = ref(false)
export const deviceModel = ref(null)
export const deviceId = ref(null)
export const currentFrequency = ref(null)
export const packetAgeMs = ref(null)
export const mqttEnabled = ref(false)
export const mqttConnected = ref(false)

// RF/LoRa parameters change rarely, so this is fetched on demand (not polled)
// and shared between the read-only Pairing display and the Advanced page.
export const rfParams = ref(null)

export async function loadRfParams() {
  try {
    rfParams.value = await getJSON("rf-params")
  } catch (e) {
    /* leave previous value */
  }
}

// Firmware version for the always-visible footer. Static for a given build, so
// it's fetched once (lazily) rather than polled; the System page polls the full
// /system-info payload separately for live health metrics.
export const firmwareVersion = ref(null)

export async function loadFirmwareVersion() {
  if (firmwareVersion.value) return
  try {
    const info = await getJSON("system-info")
    firmwareVersion.value = info?.firmware?.version || null
  } catch (e) {
    /* leave null; footer simply omits the version */
  }
}

const FAST_MS = 2000
const SLOW_MS = 10000

let started = false
let timer = null

async function poll() {
  clearTimeout(timer)
  if (!document.hidden) {
    try {
      const d = await getJSON("pairing-status")
      isPaired.value = !!d.isPaired
      deviceModel.value = d.deviceModel
      deviceId.value = d.deviceId
      currentFrequency.value = d.currentFrequency
      packetAgeMs.value = typeof d.packetAgeMs === "number" ? d.packetAgeMs : null
      mqttEnabled.value = !!d.mqttEnabled
      mqttConnected.value = !!d.mqttConnected
      reachable.value = true
    } catch (e) {
      reachable.value = false
    } finally {
      loaded.value = true
    }
  }
  timer = setTimeout(poll, isPaired.value ? SLOW_MS : FAST_MS)
}

export function startDevicePolling() {
  if (started) return
  started = true
  poll()
}

// Called after an unpair command so the pill/page update immediately and the
// poller resumes its fast cadence to detect the next pairing.
export function markUnpaired() {
  isPaired.value = false
  deviceModel.value = null
  deviceId.value = null
  currentFrequency.value = null
  packetAgeMs.value = null
  if (started) {
    clearTimeout(timer)
    timer = setTimeout(poll, FAST_MS)
  }
}
