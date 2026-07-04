// Single shared poll of the device's pairing/reachability status, consumed by
// both the nav status pill and the Pairing page. Polls fast while unpaired (to
// catch a pairing quickly) and slowly once paired; pauses while the tab is
// hidden.
import { ref } from "vue"
import * as axios from "axios"

export const loaded = ref(false)
export const reachable = ref(true)
export const isPaired = ref(false)
export const deviceModel = ref(null)
export const deviceId = ref(null)
export const currentFrequency = ref(null)

const FAST_MS = 2000
const SLOW_MS = 10000

let started = false
let timer = null

async function poll() {
  if (!document.hidden) {
    try {
      const res = await axios.get("pairing-status")
      isPaired.value = !!res.data.isPaired
      deviceModel.value = res.data.deviceModel
      deviceId.value = res.data.deviceId
      currentFrequency.value = res.data.currentFrequency
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
  if (started) {
    clearTimeout(timer)
    timer = setTimeout(poll, FAST_MS)
  }
}
