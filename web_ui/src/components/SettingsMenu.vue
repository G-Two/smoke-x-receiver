<script setup>
// Shown in the main area (in place of the dashboard) while the settings gear is
// active, guiding the user to one of the three configuration pages.
const SECTIONS = [
  {
    to: "/wlan",
    label: "WLAN",
    desc: "Connect the receiver to your Wi-Fi network, or run it as an access point.",
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
    desc: "Pair with your Smoke X2/X4 base station and view RF status.",
    icon: [
      "M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71",
      "M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71",
    ],
  },
  {
    to: "/mqtt",
    label: "MQTT",
    desc: "Publish to an MQTT broker and integrate with Home Assistant.",
    icon: ["M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z"],
  },
]
const CHEVRON = "M9 18l6-6-6-6"
</script>

<template>
  <div class="settings-menu">
    <h1 class="settings-title">Settings</h1>
    <p class="settings-hint">Choose a section to configure your receiver.</p>
    <nav class="settings-list">
      <router-link
        v-for="s in SECTIONS"
        :key="s.to"
        :to="s.to"
        class="settings-item"
      >
        <svg
          class="settings-item-icon"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          stroke-width="2"
          stroke-linecap="round"
          stroke-linejoin="round"
          aria-hidden="true"
        >
          <path v-for="(d, i) in s.icon" :key="i" :d="d" />
        </svg>
        <span class="settings-item-text">
          <span class="settings-item-label">{{ s.label }}</span>
          <span class="settings-item-desc">{{ s.desc }}</span>
        </span>
        <svg
          class="settings-item-chevron"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          stroke-width="2"
          stroke-linecap="round"
          stroke-linejoin="round"
          aria-hidden="true"
        >
          <path :d="CHEVRON" />
        </svg>
      </router-link>
    </nav>
  </div>
</template>

<style scoped>
.settings-menu {
  max-width: 720px;
  margin: 1.5em auto;
  padding: 0 1em;
  box-sizing: border-box;
  font-family: Avenir, Helvetica, Arial, sans-serif;
  color: var(--text);
}
.settings-title {
  font-size: 1.4em;
  font-weight: bold;
  margin: 0.2em 0 0.1em;
}
.settings-hint {
  margin: 0 0 1em;
  color: var(--text-muted);
  font-size: 0.9em;
}
.settings-list {
  display: flex;
  flex-direction: column;
  gap: 0.75em;
}
.settings-item {
  display: flex;
  align-items: center;
  gap: 0.9em;
  padding: 1em;
  border: 1px solid var(--border);
  border-radius: 0.5em;
  background: var(--surface);
  box-shadow: 0 0 1em var(--shadow);
  color: var(--text);
  text-decoration: none;
}
.settings-item:hover,
.settings-item:focus-visible {
  border-color: var(--brand-amber);
}
.settings-item-icon {
  width: 26px;
  height: 26px;
  flex-shrink: 0;
  color: var(--brand-amber);
}
.settings-item-text {
  display: flex;
  flex-direction: column;
  gap: 0.15em;
  min-width: 0;
}
.settings-item-label {
  font-weight: bold;
}
.settings-item-desc {
  font-size: 0.82em;
  color: var(--text-muted);
}
.settings-item-chevron {
  width: 20px;
  height: 20px;
  flex-shrink: 0;
  margin-left: auto;
  color: var(--text-muted);
}
</style>
