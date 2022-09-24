<template>
  <div id="smoke-x-config-form">
    <center>
      <b-overlay
        :show="!dataReceived"
        rounded="sm"
        variant="white"
        opacity="0.85"
        no-fade
      >
        <form class="status">
          <b>Status:</b> {{ isPaired ? "PAIRED" : "NOT PAIRED" }} <br>
          <b>Model:</b> {{ deviceModel ? deviceModel : "---" }} <br>
          <b>Device ID:</b> {{ deviceId ? deviceId : "---" }} <br>
          <b>Frequency:</b>
          {{
            currentFrequency
              ? (currentFrequency / 1000000).toPrecision(6) + " MHz"
              : "---"
          }}
          <HR />
          <center>
            <div v-if="dataReceived">
              <b-button
                v-if="isPaired == true"
                id="unpair_button"
                block
                variant="danger"
                @click.prevent="unpair()"
              >
                Unpair
              </b-button>
              <b-form v-else>
                Please enable "SYNC" mode on your Smoke X transmitter unit. This
                device will pair automatically. Other Smoke X receivers that
                have already been paired with the same transmitter will not be
                affected.
              </b-form>
            </div>
          </center>
        </form>
      </b-overlay>
    </center>
  </div>
</template>

<script>
import * as axios from "axios"

export default {
  name: "SmokeXConfigForm",
  data() {
    return {
      isPaired: false,
      currentFrequency: 0,
      deviceId: null,
      deviceModel: null,
      dataReceived: false,
    }
  },
  mounted: async function () {
    axios
      .get("pairing-status")
      .then((res) => {
        this.isPaired = res.data.isPaired
        this.currentFrequency = res.data.currentFrequency
        this.deviceId = res.data.deviceId
        this.deviceModel = res.data.deviceModel
        this.dataReceived = true
      })
      .catch((error) => {
        console.log(error)
      })
  },
  methods: {
    unpair() {
      var json = {
        command: "unpair",
      }
      if (confirm("Do you really want to unpair?")) {
        axios.post("cmd", json).catch((error) => {
          console.log(error)
        })
      }
    },
  },
}
</script>

<style scoped>
label {
  font-weight: bold;
  margin-bottom: 0;
}

#unpair_button {
  width: 8rem;
}
</style>
