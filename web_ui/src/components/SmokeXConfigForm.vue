<template>
  <div id="smoke-x-config-form">
    <center>
      <form class="status">
        <b>Status:</b> {{ isPaired ? "PAIRED" : "NOT PAIRED" }} <br />
        <b>Device Type:</b> {{ isPaired ? "X2" : "---" }} <br />
        <b>Device ID:</b> {{ deviceId ? deviceId : "---" }} <br />
        <b>RF Frequency:</b>
        {{
          currentFrequency
            ? (currentFrequency / 1000000).toPrecision(6) + " MHz"
            : "---"
        }}
      </form>
      <b-button
        block
        variant="danger"
        v-if="isPaired == true"
        v-on:click.prevent="unpair()"
      >
        Unpair
      </b-button>
      <b-form v-else>
        Please enable "SYNC" mode on your Smoke X transmitter unit. This device
        will pair automatically. Other Smoke X receivers that have already been
        paired with the same transmitter will not be affected.
      </b-form>
    </center>
  </div>
</template>

<script>
import * as axios from "axios";

export default {
  name: "smoke-x-config-form",
  data() {
    return {
      isPaired: false,
      currentFrequency: 0,
      deviceId: null,
      deviceType: null,
      ws: null
    };
  },
  mounted: async function() {
    axios
      .get("pairing-status")
      .then(res => {
        this.isPaired = res.data.isPaired;
        this.currentFrequency = res.data.currentFrequency;
        this.deviceId = res.data.deviceId;
      })
      .catch(error => {
        console.log(error);
      });
  },
  methods: {
    unpair() {
      var json = {
        command: "unpair"
      };
      axios.post("cmd", json);
    }
  }
};
</script>

<style scoped>
</style>
