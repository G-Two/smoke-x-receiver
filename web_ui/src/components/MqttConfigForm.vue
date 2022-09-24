<template>
  <div id="mqtt-config-form">
    <center>
      <b-overlay
        :show="!dataReceived"
        rounded="sm"
        variant="white"
        opacity="0.85"
        no-fade
      >
        <form>
          <b-form-checkbox
            v-model="enabled"
            name="enabled-switch"
            switch
            size="lg"
          >
            Enable MQTT
          </b-form-checkbox>

          <div v-if="enabled">
            <hr>
            <label for="uri">MQTT Broker URI</label>
            <b-form-input
              id="uri"
              v-model="uri"
            />
            <label for="identity">Identity</label>
            <b-form-input
              id="identity"
              v-model="identity"
            />
            <label for="username">Username</label>
            <b-form-input
              id="username"
              v-model="username"
            />
            <label for="username">Password</label>
            <b-form-input
              id="password"
              v-model="password"
            />
            <label for="ca_cert">CA Certificate</label>
            <b-form-textarea
              id="ca_cert"
              v-model="ca_cert"
              rows="20"
            />

            <b-form-checkbox
              v-model="ha_discovery"
              name="ha-discovery-switch"
              switch
              size="lg"
            >
              Enable Home Assistant Device Discovery
            </b-form-checkbox>
            <hr>
            <div v-if="ha_discovery">
              <label for="ha_base_topic">Home Assistant Base Topic</label>
              <b-form-input
                id="ha_base_topic"
                v-model="ha_base_topic"
              />

              <label for="ha_status_topic">Home Assistant Status Topic</label>
              <b-form-input
                id="ha_status_topic"
                v-model="ha_status_topic"
              />

              <label for="ha_birth_payload">Home Assistant Birth Payload</label>
              <b-form-input
                id="ha_birth_payload"
                v-model="ha_birth_payload"
              />
            </div>

            <label for="state_topic">Smoke X State Topic</label>
            <b-form-input
              id="state_topic"
              v-model="state_topic"
            />
          </div>
          <center>
            <b-button
              id="apply_button"
              block
              variant="primary"
              @click.prevent="sendToServer"
            >
              Save & Apply
            </b-button>
          </center>
        </form>
      </b-overlay>
    </center>
  </div>
</template>

<script>
import * as axios from "axios"

export default {
  name: "MqttConfigForm",
  data() {
    return {
      uri: "",
      identity: "",
      username: "",
      password: "",
      ca_cert: "",
      enabled: false,
      ha_discovery: false,
      ha_base_topic: "",
      ha_status_topic: "",
      ha_birth_payload: "",
      state_topic: "",
      dataReceived: false,
    }
  },
  mounted: async function () {
    axios
      .get("mqtt-config")
      .then((res) => {
        console.log(res)
        this.uri = res.data.uri
        this.identity = res.data.identity
        this.username = res.data.username
        this.password = res.data.password
        this.ca_cert = res.data.ca_cert
        this.enabled = res.data.enabled
        this.ha_discovery = res.data.ha_discovery
        this.ha_base_topic = res.data.ha_base_topic
        this.ha_status_topic = res.data.ha_status_topic
        this.ha_birth_payload = res.data.ha_birth_payload
        this.state_topic = res.data.state_topic
        this.dataReceived = true
      })
      .catch((error) => {
        console.log(error)
      })
  },
  methods: {
    sendToServer() {
      if (confirm("Commit these settings to NVRAM?")) {
        axios
          .post("mqtt-config", {
            uri: this.uri,
            identity: this.identity,
            username: this.username,
            password: this.password,
            ca_cert: this.ca_cert,
            enabled: this.enabled,
            ha_discovery: this.ha_discovery,
            ha_base_topic: this.ha_base_topic,
            ha_status_topic: this.ha_status_topic,
            ha_birth_payload: this.ha_birth_payload,
            state_topic: this.state_topic,
          })
          .catch((error) => {
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

#apply_button {
  width: 8rem;
}

#ca_cert {
  font-family: monospace;
}
</style>
