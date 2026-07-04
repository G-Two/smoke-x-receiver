<template>
  <div id="smoke-x-config-form" class="vl-parent">
    <FormKit
      type="form"
      submit-label="Unpair"
      :disabled="!isPaired"
      @submit="unpair"
    >
      <loading
        v-model:active="isLoading"
        color="var(--accent)"
        background-color="var(--bg)"
        :opacity="0.9"
      />
      <div class="status">
        <b>Status:</b>
        {{ isLoading ? "---" : isPaired ? "PAIRED" : "NOT PAIRED" }} <br />
        <b>Model:</b>
        {{ isPaired ? deviceModel : "---" }} <br />
        <b>Device ID:</b> {{ deviceId ? deviceId : "---" }}
        <div v-if="isLoading == false">
          <div v-if="isPaired == false">
            <br />
            <img
              :src="image"
              style="
                display: block;
                margin-left: auto;
                margin-right: auto;
                width: 75%;
              "
            />
            <br />
            Press and hold the sync button on your Smoke X base station until
            "SYNC" appears on the display. This device will pair with the base
            station automatically within a few seconds. Other Smoke X receivers
            that have already been paired with the same base station will not be
            affected.
          </div>
        </div>
      </div>
    </FormKit>

    <div v-if="rfParams" class="rf-readout">
      <div class="rf-title">RF Parameters</div>
      <div class="rf-grid">
        <span>Frequency</span
        ><span>{{ (rfParams.frequency / 1000000).toPrecision(6) }} MHz</span>
        <span>Spreading Factor</span><span>{{ rfParams.spreadingFactor }}</span>
        <span>Bandwidth</span><span>{{ rfParams.bandwidth / 1000 }} kHz</span>
        <span>Coding Rate</span><span>4/{{ rfParams.codingRate }}</span>
        <span>TX Power</span><span>{{ rfParams.txPower }} dBm</span>
        <span>Sync Word</span
        ><span>0x{{ rfParams.syncWord.toString(16) }}</span>
        <span>CRC</span><span>{{ rfParams.enableCRC ? "on" : "off" }}</span>
      </div>
      <router-link to="/lora" class="advanced-link">
        Advanced RF settings &rarr;
      </router-link>
    </div>
  </div>
</template>

<script>
import * as axios from "axios"
import { computed } from "vue"
import Loading from "vue-loading-overlay"
import "vue-loading-overlay/dist/css/index.css"
import Img1 from "/src/sync_button.png"
import { notify } from "../toasts"
import { confirm } from "../confirm"
import {
  loaded,
  isPaired,
  deviceModel,
  deviceId,
  rfParams,
  loadRfParams,
  markUnpaired,
} from "../device"

export default {
  name: "SmokeXConfigForm",
  components: {
    Loading,
  },
  setup() {
    // Pairing status comes from the shared device store (also drives the nav
    // pill), so this page no longer runs its own poll.
    return {
      isPaired,
      deviceModel,
      deviceId,
      rfParams,
      isLoading: computed(() => !loaded.value),
    }
  },
  data() {
    return {
      image: Img1,
    }
  },
  mounted() {
    loadRfParams()
  },
  methods: {
    async unpair() {
      const ok = await confirm(
        "Unpair from this Smoke X base station? Other paired receivers are unaffected.",
        { confirmLabel: "Unpair", danger: true }
      )
      if (!ok) return
      try {
        await axios.post("cmd", { command: "unpair" })
        notify("Unpaired from base station")
      } catch (error) {
        notify("Failed to unpair", "error")
      }
      markUnpaired()
    },
  },
}
</script>

<style>
.status {
  font-family: Avenir, Helvetica, Arial, sans-serif;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  color: var(--text);
  padding-top: 1rem;
  margin: 0;
  margin-bottom: 2rem;
  text-align: left;
}

.rf-readout {
  width: calc(100% - 2em);
  max-width: 480px;
  box-sizing: border-box;
  margin: 1em auto;
  padding: 1.25em 2em;
  background: var(--surface);
  color: var(--text);
  border-radius: 0.5em;
  box-shadow: 0 0 1em var(--shadow);
  font-family: Avenir, Helvetica, Arial, sans-serif;
}
.rf-title {
  font-weight: bold;
  margin-bottom: 0.75em;
}
.rf-grid {
  display: grid;
  grid-template-columns: 1fr auto;
  gap: 0.35em 1em;
  font-size: 0.9em;
}
.rf-grid span:nth-child(odd) {
  color: var(--text-muted);
}
.rf-grid span:nth-child(even) {
  text-align: right;
  font-variant-numeric: tabular-nums;
}
.advanced-link {
  display: inline-block;
  margin-top: 1em;
  color: var(--accent);
  font-weight: bold;
  text-decoration: none;
  font-size: 0.85em;
}
</style>
