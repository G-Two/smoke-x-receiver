<template>
  <div id="wlan-config-form">
    <b-overlay :show="!dataReceived" rounded="sm" variant="white" opacity=0.85 no-fade>
    <center>
        <b-form>
          <label>Mode</label>
          <b-form-group>
            <b-form-radio
              v-model.number="mode"
              v-b-tooltip.hover.left
              title="Connect to existing wireless network"
              value="1"
              >Client</b-form-radio
            >
            <b-form-radio
              v-model.number="mode"
              v-b-tooltip.hover.left
              title="Other devices connect directly to this device"
              value="2"
              >Access Point</b-form-radio
            >
          </b-form-group>

          <label>Authentication</label>
          <b-form-group>
            <b-form-radio v-model.number="authType" value="0"
              >Open</b-form-radio
            >
            <b-form-radio v-model.number="authType" value="3"
              >WPA2 PSK</b-form-radio
            >
            <b-form-radio v-model.number="authType" v-if="mode == 1" value="5"
              >WPA2 Enterprise</b-form-radio
            >
          </b-form-group>

          <label for="SSID">SSID</label>
          <b-form-input id="SSID" v-model="ssid" />

          <label for="username" v-if="authType == 5">Username</label>
          <b-form-input id="username" v-model="username" v-if="authType == 5" />

          <label for="password" v-if="authType != 0">Password</label>
          <b-form-input id="password" v-model="password" v-if="authType != 0" />
          <center>
          <b-button id="apply_button" block variant="primary" v-on:click.prevent="sendToServer"
            >Save & Apply</b-button
          ></center>
        </b-form>
    </center>
      </b-overlay>
  </div>
</template>

<script>
import * as axios from "axios";

export default {
  name: "wlan-config-form",
  data() {
    return {
      mode: 1,
      authType: 5,
      ssid: "",
      username: "",
      password: "",
      dataReceived: false,
    };
  },
  mounted: async function() {
    axios
      .get("wlan-config")
      .then(res => {
        console.log(res);
        this.mode = res.data.mode;
        this.authType = res.data.authType;
        this.ssid = res.data.ssid;
        this.username = res.data.username;
        this.password = res.data.password;
        this.dataReceived = true
      })
      .catch(error => {
        console.log(error);
      });
  },
  methods: {
    sendToServer() {
      axios.post("wlan-config", {
        mode: this.mode,
        ssid: this.ssid,
        authType: this.authType,
        username: this.username,
        password: this.password
      });
    }
  }
};
</script>

<style scoped>

label {
  font-weight: bold;
  margin-bottom: 0;
}

#apply_button {
 width: 8rem
}

</style>
