<template>
  <div id="mqtt-config-form">
    <loading v-model:active="isLoading" />
    <FormKit type="form" @submit="sendToServer">
      <FormKit
        id="enabled"
        type="checkbox"
        label="Enable MQTT"
        name="enabled"
      />
      <FormKit
        id="uri"
        type="text"
        name="uri"
        label="MQTT Broker URI"
        validation="required"
      />
      <FormKit id="identity" type="text" name="identity" label="Identity" />
      <FormKit id="username" type="text" name="username" label="Username" />
      <FormKit id="password" type="password" name="password" label="Password" />
      <FormKit
        id="ca_cert"
        type="textarea"
        name="ca_cert"
        label="CA Certificate"
        rows="10"
        placeholder="-----BEGIN CERTIFICATE-----
Paste CA certificate in PEM format
-----END CERTIFICATE-----"
        validation="required"
      />
      <FormKit
        id="ha_discovery"
        type="checkbox"
        label="Enable Home Assistant Device Discovery"
        name="ha_discovery"
      />
      <FormKit
        id="ha_base_topic"
        type="text"
        name="ha_base_topic"
        label="Home Assistant Base Topic"
        validation="required"
        value="homeassistant"
      />
      <FormKit
        id="ha_status_topic"
        type="text"
        name="ha_status_topic"
        label="Home Assistant Status Topic"
        validation="required"
        value="homeassistant/status"
      />
      <FormKit
        id="ha_birth_payload"
        type="text"
        name="ha_birth_payload"
        label="Home Assistant Birth Payload"
        validation="required"
        value="online"
      />
      <FormKit
        id="state_topic"
        type="text"
        name="state_topic"
        label="Smoke X State Topic"
        validation="required"
        value="homeassistant/smoke-x/state"
      />
      <!-- <pre>{{ value }}</pre> -->
    </FormKit>
  </div>
</template>

<script>
import * as axios from "axios"
import { getNode } from "@formkit/core"
import Loading from "vue-loading-overlay"
import "vue-loading-overlay/dist/css/index.css"

export default {
  name: "MqttConfigForm",
  components: {
    Loading,
  },
  data() {
    return {
      isLoading: true,
    }
  },
  mounted: async function () {
    axios
      .get("mqtt-config")
      .then((res) => {
        console.log(res)
        getNode("enabled").input(res.data.enabled)
        getNode("uri").input(res.data.uri)
        getNode("identity").input(res.data.identity)
        getNode("username").input(res.data.username)
        getNode("password").input(res.data.password)
        getNode("ca_cert").input(res.data.ca_cert)
        getNode("enabled").input(res.data.enabled)
        getNode("ha_discovery").input(res.data.ha_discovery)
        getNode("ha_base_topic").input(res.data.ha_base_topic)
        getNode("ha_status_topic").input(res.data.ha_status_topic)
        getNode("ha_birth_payload").input(res.data.ha_birth_payload)
        getNode("state_topic").input(res.data.state_topic)
        this.isLoading = false
      })
      .catch((error) => {
        console.log(error)
      })
  },
  methods: {
    async sendToServer(fields) {
      if (confirm("Commit these settings to NVRAM?")) {
        axios.post("mqtt-config", fields).catch((error) => {
          console.log(error)
        })
      }
    },
  },
}
</script>

<style>
#ca_cert {
  font-family: "Courier New", Courier, monospace;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  font-size: medium;
  font-weight: bold;
  margin: 0;
  text-align: justify;
}
</style>
