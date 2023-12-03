<template>
  <div id="wlan-config-form">
    <loading v-model:active="isLoading" />
    <FormKit v-slot="{ value }" type="form" @submit="sendToServer">
      <FormKit
        id="mode"
        type="radio"
        name="mode"
        label="WLAN Mode"
        :options="{
          1: 'Client',
          2: 'Access Point',
        }"
        value=1
        validation="required"
      />
      <FormKit
        id="authType"
        type="radio"
        name="authType"
        label="WLAN Security"
        :options="
          value.mode == 1
            ? {
                0: 'Open',
                3: 'WPA2 Pre-Shared Key',
                5: 'WPA2 Enterprise',
              }
            : {
                0: 'Open',
                3: 'WPA2 Pre-Shared Key',
              }
        "
        value=3
        validation="required"
      />
      <FormKit
        id="ssid"
        type="text"
        name="ssid"
        label="SSID"
        validation="required"
      />
      <FormKit
        id="username"
        :disabled="value.authType != 5"
        type="text"
        name="username"
        label="Username"
        :validation="value.authType != 5 ? 'optional' : 'required'"
      />
      <FormKit
        id="password"
        :disabled="value.authType == 0"
        type="password"
        name="password"
        label="Password"
        :validation="value.authType == 0 ? 'optional' : 'required'"
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
  name: "WlanConfigForm",
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
      .get("wlan-config")
      .then((res) => {
        console.log(res)
        getNode("mode").input(res.data.mode)
        getNode("authType").input(res.data.authType)
        getNode("ssid").input(res.data.ssid)
        getNode("username").input(res.data.username)
        getNode("password").input(res.data.password)
        this.isLoading = false
      })
      .catch((error) => {
        console.log(error)
      })
  },
  methods: {
    async sendToServer(fields) {
      if (confirm("Commit these settings to NVRAM?")) {
        fields.authType=parseInt(fields.authType)
        fields.mode=parseInt(fields.mode)
        axios.post("wlan-config", fields).catch((error) => {
          console.log(error)
        })
      }
    },
  },
}
</script>
