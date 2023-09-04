<template>
  <div id="smoke-x-config-form" class="vl-parent">
    <FormKit
      type="form"
      submit-label="Unpair"
      :disabled="!isPaired"
      @submit="unpair"
    >
      <loading v-model:active="isLoading" />
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
import Loading from "vue-loading-overlay"
import "vue-loading-overlay/dist/css/index.css"
import Img1 from "/src/sync_button.png"

export default {
  name: "SmokeXConfigForm",
  components: {
    Loading,
  },
  data() {
    return {
      isPaired: false,
      currentFrequency: null,
      deviceId: null,
      deviceModel: null,
      isLoading: true,
      image: Img1,
    }
  },
  created: async function () {
    await this.getData()
    this.timer = setInterval(this.getData, 2000)
  },
  beforeUnmount: function () {
    clearInterval(this.timer)
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
        this.isPaired = false
        this.currentFrequency = null
        this.deviceId = null
        this.deviceModel = null
        this.timer = setInterval(this.getData, 2000)
      }
    },
    async getData() {
      axios
        .get("pairing-status")
        .then((res) => {
          this.isPaired = res.data.isPaired
          this.currentFrequency = res.data.currentFrequency
          this.deviceId = res.data.deviceId
          this.deviceModel = res.data.deviceModel
          this.isLoading = false
          if (this.isPaired) {
            clearInterval(this.timer)
          }
        })
        .catch((error) => {
          console.log(error)
        })
    },
  },
}
</script>

<style>
.status {
  font-family: Avenir, Helvetica, Arial, sans-serif;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  color: #2c3e50;
  padding-top: 1rem;
  margin: 0;
  margin-bottom: 2rem;
  text-align: left;
  color: #2c3e50;
}
</style>
