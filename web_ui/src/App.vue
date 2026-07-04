<script setup>
import { preference, cyclePreference } from "./theme"
import ToastHost from "./components/ToastHost.vue"
import ConfirmDialog from "./components/ConfirmDialog.vue"

const ICONS = { auto: "◐", light: "☀", dark: "☾" }
</script>

<template>
  <div>
    <div id="nav">
      <h2>Smoke X Receiver</h2>
      <div class="nav-links">
        <router-link to="/"> Status </router-link> |
        <router-link to="/wlan"> WLAN </router-link> |
        <router-link to="/pairing"> Pairing </router-link> |
        <router-link to="/mqtt"> MQTT </router-link>
        <button
          class="theme-toggle"
          type="button"
          :title="`Theme: ${preference} (click to change)`"
          @click="cyclePreference"
        >
          {{ ICONS[preference] }}
        </button>
      </div>
    </div>
    <router-view />
    <ToastHost />
    <ConfirmDialog />
  </div>
</template>

<style>
#nav {
  text-align: center;
  font-weight: bold;
  color: var(--text);
  padding-top: 0.5em;
}

#nav a {
  font-weight: bold;
  color: var(--text);
  text-decoration: none;
}

#nav a.router-link-exact-active {
  color: var(--accent);
}

.nav-links {
  display: inline-flex;
  align-items: center;
  gap: 0.4em;
  flex-wrap: wrap;
  justify-content: center;
}

.theme-toggle {
  margin-left: 0.5em;
  border: 1px solid var(--border);
  background: var(--surface);
  color: var(--text);
  border-radius: 0.4em;
  width: 2em;
  height: 2em;
  cursor: pointer;
  font-size: 1em;
  line-height: 1;
}
</style>
