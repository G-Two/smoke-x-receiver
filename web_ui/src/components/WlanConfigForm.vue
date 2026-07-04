<script setup>
import { togglePasswordVisibility } from "../formkit-password"
</script>

<template>
  <div id="wlan-config-form">
    <loading
      v-model:active="isLoading"
      color="var(--brand-amber)"
      background-color="var(--bg)"
      :opacity="0.9"
      :z-index="490"
    />
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
                3: 'WPA2/WPA3 Pre-Shared Key',
                5: 'WPA2/WPA3 Enterprise',
              }
            : {
                0: 'Open',
                3: 'WPA2/WPA3 Pre-Shared Key',
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
        v-show="value.authType == 5"
        id="username"
        type="text"
        name="username"
        label="Username"
        :validation="value.authType == 5 ? 'required' : 'optional'"
      />
      <FormKit
        v-show="value.authType != 0"
        id="password"
        type="password"
        name="password"
        label="Password"
        :validation="value.authType != 0 ? 'required' : 'optional'"
        suffix-icon="eyeClosed"
        @suffix-icon-click="togglePasswordVisibility"
      />
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
  name: "WlanConfigForm",
  components: {
    Loading,
  },
  data() {
    return {
      isLoading: true,
    }
  },
  mounted: function () {
    getJSON("wlan-config")
      .then((data) => {
        getNode("mode").input(data.mode)
        getNode("authType").input(data.authType)
        getNode("ssid").input(data.ssid)
        getNode("username").input(data.username)
        getNode("password").input(data.password)
        this.isLoading = false
      })
      .catch(() => {
        this.isLoading = false
        notify("Failed to load WLAN settings", "error")
      })
  },
  methods: {
    async sendToServer(fields) {
      fields.authType = parseInt(fields.authType)
      fields.mode = parseInt(fields.mode)
      try {
        await postJSON("wlan-config", fields)
        notify("WLAN settings saved")
      } catch (error) {
        notify("Failed to save WLAN settings", "error")
      }
    },
  },
}
</script>
