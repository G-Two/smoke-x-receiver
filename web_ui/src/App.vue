<script setup>
import { computed, onMounted } from "vue"
import { preference, cyclePreference } from "./theme"
import {
  isPaired,
  deviceModel,
  reachable,
  startDevicePolling,
} from "./device"
import ToastHost from "./components/ToastHost.vue"
import ConfirmDialog from "./components/ConfirmDialog.vue"

const THEME_ICONS = { auto: "◐", light: "☀", dark: "☾" }

// Feather-style icons (stroke, currentColor). Each entry is a list of paths.
const TABS = [
  { to: "/", label: "Status", icon: ["M22 12h-4l-3 9L9 3l-3 9H2"] },
  {
    to: "/wlan",
    label: "WLAN",
    icon: [
      "M5 12.55a11 11 0 0 1 14.08 0",
      "M1.42 9a16 16 0 0 1 21.16 0",
      "M8.53 16.11a6 6 0 0 1 6.95 0",
      "M12 20h.01",
    ],
  },
  {
    to: "/pairing",
    label: "Pairing",
    icon: [
      "M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71",
      "M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71",
    ],
  },
  { to: "/mqtt", label: "MQTT", icon: ["M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z"] },
]

const statusKind = computed(() =>
  !reachable.value ? "offline" : isPaired.value ? "paired" : "unpaired"
)
const statusText = computed(() =>
  statusKind.value === "offline"
    ? "Offline"
    : statusKind.value === "paired"
    ? `Paired · ${deviceModel.value || "?"}`
    : "Unpaired"
)

onMounted(startDevicePolling)
</script>

<template>
  <div>
    <header class="site-header">
      <div class="appbar">
        <span class="brand">Smoke X Receiver</span>
        <div class="appbar-right">
          <span
            class="status-pill"
            :class="`pill-${statusKind}`"
            role="status"
            aria-live="polite"
          >
            {{ statusText }}
          </span>
          <button
            class="theme-toggle"
            type="button"
            :title="`Theme: ${preference} (click to change)`"
            @click="cyclePreference"
          >
            {{ THEME_ICONS[preference] }}
          </button>
        </div>
      </div>

      <nav class="tabbar">
        <router-link
          v-for="t in TABS"
          :key="t.to"
          :to="t.to"
          class="tab"
        >
          <svg
            class="tab-icon"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            stroke-width="2"
            stroke-linecap="round"
            stroke-linejoin="round"
            aria-hidden="true"
          >
            <path v-for="(d, i) in t.icon" :key="i" :d="d" />
          </svg>
          <span class="tab-label">{{ t.label }}</span>
        </router-link>
      </nav>
    </header>

    <router-view />
    <ToastHost />
    <ConfirmDialog />
  </div>
</template>

<style>
.site-header {
  position: sticky;
  top: 0;
  z-index: 500;
  background: var(--surface);
}

.appbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.5em;
  padding: 0.6em 1em;
  border-bottom: 1px solid var(--border);
}

.brand {
  font-weight: bold;
  font-size: 1.05em;
  color: var(--text);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.appbar-right {
  display: flex;
  align-items: center;
  gap: 0.5em;
  flex-shrink: 0;
}

.status-pill {
  font-size: 0.72em;
  font-weight: bold;
  padding: 0.25em 0.6em;
  border-radius: 1em;
  white-space: nowrap;
  border: 1px solid var(--border);
  color: var(--text-muted);
}
.pill-paired {
  background: var(--accent);
  color: #fff;
  border-color: var(--accent);
}
.pill-offline {
  background: var(--alarm-high-bg);
  color: var(--alarm-high-fg);
  border-color: transparent;
}

.theme-toggle {
  border: 1px solid var(--border);
  background: var(--surface);
  color: var(--text);
  border-radius: 0.4em;
  width: 2em;
  height: 2em;
  cursor: pointer;
  font-size: 1em;
  line-height: 1;
  flex-shrink: 0;
}

.tabbar {
  display: flex;
  max-width: 720px;
  margin: 0 auto;
  border-bottom: 1px solid var(--border);
}

.tab {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 3px;
  padding: 0.55em 0.25em;
  color: var(--text-muted);
  text-decoration: none;
  font-family: Avenir, Helvetica, Arial, sans-serif;
  font-size: 0.72em;
  font-weight: bold;
  border-bottom: 2px solid transparent;
  margin-bottom: -1px;
}
.tab.router-link-exact-active {
  color: var(--accent);
  border-bottom-color: var(--accent);
}

.tab-icon {
  width: 20px;
  height: 20px;
  display: block;
}
</style>
