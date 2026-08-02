<script setup>
import { ref } from "vue"
import { togglePasswordVisibility } from "../formkit-password"

const clientCertAuth = ref(false)
const useTLS = ref(false)
const HADiscovery = ref(false)
const uriPattern =
  /^mqtts?:\/\/(?:(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(?:\.(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}|(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)(?:\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*)(?::\d{1,5})?\/?$/
</script>

<template>
  <div id="mqtt-config-form">
    <loading
      v-model:active="isLoading"
      color="var(--brand-amber)"
      background-color="var(--bg)"
      :opacity="0.9"
      :z-index="490"
    />
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
        placeholder="mqtt://192.168.1.10:1883"
        :validation="[['required'], ['matches', uriPattern]]"
        :validation-messages="{
          matches:
            'Enter a full URI: mqtt:// or mqtts:// followed by a hostname or IP address, e.g. mqtt://192.168.1.10:1883',
        }"
      />
      <FormKit id="identity" type="text" name="identity" label="Identity" />
      <FormKit id="username" type="text" name="username" label="Username" />
      <FormKit
        id="password"
        type="password"
        name="password"
        label="Password"
        suffix-icon="eyeClosed"
        @suffix-icon-click="togglePasswordVisibility"
      />
      <FormKit
        id="use_tls"
        v-model="useTLS"
        type="checkbox"
        label="Use TLS"
        name="use_tls"
      />
      <FormKit
       v-show="useTLS"
        id="ca_cert"
        type="textarea"
        name="ca_cert"
        label="CA Certificate"
        rows="10"
        placeholder="-----BEGIN CERTIFICATE-----
Paste CA certificate in PEM format
-----END CERTIFICATE-----"
      />
      <FormKit
      v-show="useTLS"
      id="cert_auth"
      v-model="clientCertAuth"
      type="checkbox"
      label="Use Client Certificate Authentication"
      name="cert_auth"
      />
    <FormKit
      v-show="clientCertAuth && useTLS"
      id="client_cert"
      type="textarea"
      name="client_cert"
      label="Client Certificate"
      rows="10"
      placeholder="-----BEGIN CERTIFICATE-----
Paste client certificate in PEM format
-----END CERTIFICATE-----"
    />
    <FormKit
      v-show="clientCertAuth && useTLS"
      id="client_key"
      type="textarea"
      name="client_key"
      label="Client Key"
      rows="10"
      placeholder="-----BEGIN KEY-----
Paste client key in PEM format
-----END KEY-----"
    />
      <FormKit
        id="ha_discovery"
        v-model="HADiscovery"
        type="checkbox"
        label="Enable Home Assistant Device Discovery"
        name="ha_discovery"
      />
      <FormKit
        v-show="HADiscovery"
        id="ha_base_topic"
        type="text"
        name="ha_base_topic"
        label="Home Assistant Base Topic"
        validation="required"
        value="homeassistant"
      />
      <FormKit
        v-show="HADiscovery"
        id="ha_status_topic"
        type="text"
        name="ha_status_topic"
        label="Home Assistant Status Topic"
        validation="required"
        value="homeassistant/status"
      />
      <FormKit
        v-show="HADiscovery"
        id="ha_birth_payload"
        type="text"
        name="ha_birth_payload"
        label="Home Assistant Birth Payload"
        validation="required"
        value="online"
      />
      <FormKit
        v-show="HADiscovery"
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
import { getJSON, postJSON } from "../api"
import { getNode } from "@formkit/core"
import Loading from "vue-loading-overlay"
import "vue-loading-overlay/dist/css/index.css"
import { notify } from "../toasts"

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
  mounted: function () {
    getJSON("mqtt-config")
      .then((data) => {
        const hasTLSConfig =
          (typeof data.uri === "string" &&
            data.uri.toLowerCase().startsWith("mqtts://")) ||
          !!(data.ca_cert || data.cert_auth || data.client_cert || data.client_key)
        getNode("enabled").input(data.enabled)
        getNode("uri").input(data.uri)
        getNode("identity").input(data.identity)
        getNode("username").input(data.username)
        getNode("password").input(data.password)
        getNode("use_tls").input(hasTLSConfig)
        getNode("ca_cert").input(data.ca_cert)
        getNode("cert_auth").input(data.cert_auth)
        getNode("client_cert").input(data.client_cert)
        getNode("client_key").input(data.client_key)
        getNode("ha_discovery").input(data.ha_discovery)
        getNode("ha_base_topic").input(data.ha_base_topic)
        getNode("ha_status_topic").input(data.ha_status_topic)
        getNode("ha_birth_payload").input(data.ha_birth_payload)
        getNode("state_topic").input(data.state_topic)
        this.isLoading = false
      })
      .catch(() => {
        this.isLoading = false
        notify("Failed to load MQTT settings", "error")
      })
  },
  methods: {
    async sendToServer(fields) {
      const mqttConfig = { ...fields }

      if (mqttConfig.use_tls) {
        if (typeof mqttConfig.uri === "string") {
          mqttConfig.uri = mqttConfig.uri.replace(/^mqtt:\/\//i, "mqtts://")
        }
      } else {
        if (typeof mqttConfig.uri === "string") {
          mqttConfig.uri = mqttConfig.uri.replace(/^mqtts:\/\//i, "mqtt://")
        }
        mqttConfig.ca_cert = ""
        mqttConfig.cert_auth = false
        mqttConfig.client_cert = ""
        mqttConfig.client_key = ""
      }

      delete mqttConfig.use_tls

      try {
        await postJSON("mqtt-config", mqttConfig)
        notify("MQTT settings saved")
      } catch (error) {
        notify("Failed to save MQTT settings", "error")
      }
    },
  },
}
</script>

<style>
/* PEM certificates and keys are far easier to read and paste in a fixed-width
   terminal font. Applies to all three cert/key textareas so they match (the
   CA, client cert, and client key boxes were previously inconsistent). */
#mqtt-config-form textarea {
  font-family: ui-monospace, "SF Mono", Menlo, Consolas, "Roboto Mono",
    "Courier New", monospace;
  font-size: 0.85em;
  line-height: 1.45;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
}
</style>
