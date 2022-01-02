<template>
  <div id="wifi-config-form">
    <center>
        <form>
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
        </form>
    </center>
  </div>
</template>

<script>
import * as axios from "axios";

export default {
  name: "wifi-config-form",
  data() {
    return {
      mode: 1,
      authType: 3,
      ssid: "",
      username: "",
      password: ""
    };
  },
  mounted: async function() {
    axios
      .get("wifi-config")
      .then(res => {
        console.log(res);
        this.mode = res.data.mode;
        this.authType = res.data.authType;
        this.ssid = res.data.ssid;
        this.username = res.data.username;
        this.password = res.data.password;
      })
      .catch(error => {
        console.log(error);
      });
  },
  methods: {
    sendToServer() {
      axios.post("wifi-config", {
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
