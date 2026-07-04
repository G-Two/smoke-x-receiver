<template>
  <div id="lora-advanced" class="vl-parent">
    <loading
      v-model:active="isLoading"
      color="var(--accent)"
      background-color="var(--bg)"
      :opacity="0.9"
    />

    <div class="warn-card">
      <div class="warn-title">⚠ Advanced RF configuration</div>
      <p>
        Changing these values or transmitting arbitrary messages may cause this
        receiver to lose pairing with your Smoke X base station. If that
        happens, just re-pair from the <strong>Pairing</strong> tab. Only
        proceed if you understand LoRa PHY settings.
      </p>
      <label class="ack">
        <input v-model="acknowledged" type="checkbox" />
        I understand — enable editing
      </label>
    </div>

    <FormKit
      type="form"
      submit-label="Save RF Parameters"
      :disabled="!acknowledged"
      @submit="saveRfParams"
    >
      <h4>RF Parameters</h4>
      <FormKit
        id="frequency"
        type="number"
        name="frequency"
        label="Frequency (Hz)"
        validation="required"
      />
      <FormKit id="txPower" type="number" name="txPower" label="TX Power (dBm)" />
      <FormKit id="bandwidth" type="number" name="bandwidth" label="Bandwidth (Hz)" />
      <FormKit
        id="spreadingFactor"
        type="number"
        name="spreadingFactor"
        label="Spreading Factor"
      />
      <FormKit
        id="codingRate"
        type="number"
        name="codingRate"
        label="Coding Rate denominator (5 = 4/5)"
      />
      <FormKit
        id="preambleLength"
        type="number"
        name="preambleLength"
        label="Preamble Length"
      />
      <FormKit
        id="messageLength"
        type="number"
        name="messageLength"
        label="Message Length"
      />
      <FormKit
        id="syncWord"
        type="number"
        name="syncWord"
        label="Sync Word (decimal)"
      />
      <FormKit id="enableCRC" type="checkbox" name="enableCRC" label="Enable CRC" />
      <FormKit
        id="implicitHeader"
        type="checkbox"
        name="implicitHeader"
        label="Implicit Header"
      />
    </FormKit>

    <FormKit
      type="form"
      submit-label="Transmit"
      :disabled="!acknowledged"
      @submit="transmit"
    >
      <h4>Arbitrary Transmit</h4>
      <p class="tx-help">
        Send a raw LoRa message using the current RF parameters. A repeat
        interval of 0 sends once; a positive value repeats every N milliseconds
        until stopped.
      </p>
      <FormKit type="text" name="message" label="Message" />
      <FormKit
        type="number"
        name="repeatInterval"
        label="Repeat interval (ms)"
        value="0"
      />
    </FormKit>

    <div class="stop-row">
      <button
        class="stop-btn"
        type="button"
        :disabled="!acknowledged"
        @click="stopTx"
      >
        Stop transmitting
      </button>
    </div>
  </div>
</template>

<script>
import { getJSON, postJSON } from "../api"
import { getNode } from "@formkit/core"
import Loading from "vue-loading-overlay"
import "vue-loading-overlay/dist/css/index.css"
import { notify } from "../toasts"
import { loadRfParams } from "../device"

export default {
  name: "LoraAdvancedForm",
  components: { Loading },
  data() {
    return {
      isLoading: true,
      acknowledged: false,
    }
  },
  mounted() {
    getJSON("rf-params")
      .then((d) => {
        getNode("frequency").input(d.frequency)
        getNode("txPower").input(d.txPower)
        getNode("bandwidth").input(d.bandwidth)
        getNode("spreadingFactor").input(d.spreadingFactor)
        getNode("codingRate").input(d.codingRate)
        getNode("preambleLength").input(d.preambleLength)
        getNode("messageLength").input(d.messageLength)
        getNode("syncWord").input(d.syncWord)
        getNode("enableCRC").input(d.enableCRC)
        getNode("implicitHeader").input(d.implicitHeader)
        this.isLoading = false
      })
      .catch(() => {
        this.isLoading = false
        notify("Failed to load RF parameters", "error")
      })
  },
  methods: {
    async saveRfParams(fields) {
      // The firmware reads every field unconditionally (no null checks), so
      // always send the full set with correct numeric/boolean types.
      const payload = {
        frequency: parseInt(fields.frequency),
        txPower: parseInt(fields.txPower),
        bandwidth: parseInt(fields.bandwidth),
        spreadingFactor: parseInt(fields.spreadingFactor),
        codingRate: parseInt(fields.codingRate),
        preambleLength: parseInt(fields.preambleLength),
        messageLength: parseInt(fields.messageLength),
        syncWord: parseInt(fields.syncWord),
        enableCRC: !!fields.enableCRC,
        implicitHeader: !!fields.implicitHeader,
      }
      try {
        await postJSON("rf-params", payload)
        notify("RF parameters saved")
        loadRfParams()
      } catch (error) {
        notify("Failed to save RF parameters", "error")
      }
    },
    async transmit(fields) {
      try {
        await postJSON("cmd", {
          command: "startTx",
          message: fields.message || "",
          repeatInterval: parseInt(fields.repeatInterval) || 0,
        })
        notify("Transmitting")
      } catch (error) {
        notify("Failed to start transmit", "error")
      }
    },
    async stopTx() {
      try {
        await postJSON("cmd", { command: "stopTx" })
        notify("Transmit stopped")
      } catch (error) {
        notify("Failed to stop transmit", "error")
      }
    },
  },
}
</script>

<style scoped>
.warn-card {
  max-width: 480px;
  margin: 1em auto 0;
  box-sizing: border-box;
  padding: 1em 1.25em;
  border-radius: 0.5em;
  border-left: 4px solid var(--alarm-high-fg);
  background: var(--alarm-high-bg);
  color: var(--text);
  font-family: Avenir, Helvetica, Arial, sans-serif;
}
.warn-title {
  font-weight: bold;
  margin-bottom: 0.4em;
}
.warn-card p {
  margin: 0 0 0.5em;
  font-size: 0.9em;
  line-height: 1.4;
}
.ack {
  display: flex;
  align-items: center;
  gap: 0.5em;
  font-weight: bold;
  cursor: pointer;
}

.tx-help {
  font-size: 0.85em;
  color: var(--text-muted);
  margin-top: 0;
}

.stop-row {
  max-width: 480px;
  margin: 0 auto 2em;
  padding: 0 2em;
  box-sizing: border-box;
}
.stop-btn {
  border: 1px solid var(--alarm-high-fg);
  background: transparent;
  color: var(--alarm-high-fg);
  border-radius: 0.4em;
  padding: 0.5em 1em;
  font-weight: bold;
  cursor: pointer;
}
.stop-btn:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}
</style>
