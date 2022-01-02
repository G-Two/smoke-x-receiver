<template>
  <div id="mqtt-config-form">
    <center>
        <form>
          <label for="uri">MQTT Broker URI</label>
          <b-form-input id="uri" v-model="uri" />
          <label for="identity">Identity</label>
          <b-form-input id="identity" v-model="identity" />
          <label for="username">Username</label>
          <b-form-input id="username" v-model="username" />
          <label for="username">Password</label>
          <b-form-input id="password" v-model="password" />
          <b-form-checkbox
          name="enabled-switch"
          v-model="switchEnabled"
          switch
          size="lg"
        >
          Enabled
        </b-form-checkbox>
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
  name: "mqtt-config-form",
  data() {
    return {
      uri: "",
      identity: "",
      username: "",
      password: "",
      switchEnabled: false,
    };
  },
  mounted: async function() {
    axios
      .get("mqtt-config")
      .then(res => {
        console.log(res);
        this.uri = res.data.uri;
        this.identity = res.data.identity;
        this.username = res.data.username;
        this.password = res.data.password;
        this.switchEnabled = res.data.enabled;
      })
      .catch(error => {
        console.log(error);
      });
  },
  methods: {
    sendToServer() {
      axios.post("mqtt-config", {
        uri: this.uri,
        identity: this.identity,
        username: this.username,
        password: this.password,
        enabled: this.switchEnabled,
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
