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
        <b>Device ID:</b> {{ deviceId ? deviceId : "---" }} <br />
        <b>Frequency:</b>
        {{
          currentFrequency
            ? (currentFrequency / 1000000).toPrecision(6) + " MHz"
            : "---"
        }}
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
  currentFrequency,
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
      currentFrequency,
      isLoading: computed(() => !loaded.value),
    }
  },
  data() {
    return {
      image: Img1,
    }
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
</style>
