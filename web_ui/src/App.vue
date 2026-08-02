<script setup>
import { computed, onMounted, ref } from "vue"
import { useRoute, useRouter } from "vue-router"
import { preference, cyclePreference } from "./theme"
import {
  isPaired,
  deviceModel,
  reachable,
  startDevicePolling,
  firmwareVersion,
  loadFirmwareVersion,
} from "./device"
import ToastHost from "./components/ToastHost.vue"
import ConfirmDialog from "./components/ConfirmDialog.vue"
import SettingsMenu from "./components/SettingsMenu.vue"

const THEME_ICONS = { auto: "◐", light: "☀", dark: "☾" }

// Feather-style icons (stroke, currentColor). Each entry is a list of paths.
// The dashboard ("/") is intentionally NOT a tab — it's reached by closing the
// menu (or the brand wordmark). The menu is a "configuration mode".
const TABS = [
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
  { to: "/system", label: "System", icon: ["M22 12h-4l-3 9L9 3l-3 9H2"] },
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

// The tab strip is a "configuration mode": opening the menu reveals the three
// config pages; the dashboard is home. The strip stays visible while on any
// config page (and on deep-links/refresh), and closing it (the ✕) always
// returns to the dashboard.
const route = useRoute()
const router = useRouter()
const navOpen = ref(route.path !== "/")
// The dashboard is "/"; the tab strip is only relevant while on a config page.
const onConfigPage = computed(() => route.path !== "/")

// Gear: enter settings (show the hub) from the dashboard, or exit to the
// dashboard from anywhere in settings.
function toggleNav() {
  if (navOpen.value) {
    navOpen.value = false
    if (route.path !== "/") router.push("/")
  } else {
    navOpen.value = true
  }
}

// Back arrow (shown on a config page): return up to the settings hub.
function goToHub() {
  navOpen.value = true
  if (route.path !== "/") router.push("/")
}

onMounted(() => {
  startDevicePolling()
  loadFirmwareVersion()
})
</script>

<template>
  <div>
    <header class="site-header">
      <div class="appbar">
        <div class="appbar-left">
          <!-- Back to the settings hub — only shown while on a config page. -->
          <button
            v-if="onConfigPage"
            class="nav-toggle"
            type="button"
            aria-label="Back to settings"
            @click="goToHub"
          >
            <svg
              class="nav-toggle-icon"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
              stroke-linecap="round"
              stroke-linejoin="round"
              aria-hidden="true"
            >
              <path d="M15 18l-6-6 6-6" />
            </svg>
          </button>
          <router-link
            to="/"
            class="brand"
            aria-label="Go to dashboard"
            @click="navOpen = false"
          >
            Smoke X Receiver
          </router-link>
        </div>
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
            :aria-label="`Theme: ${preference}. Click to change.`"
            @click="cyclePreference"
          >
            {{ THEME_ICONS[preference] }}
          </button>
          <!-- Settings gear — enters settings (the hub) from the dashboard, or
               exits to the dashboard from any settings view. -->
          <button
            class="nav-toggle"
            :class="{ active: navOpen }"
            type="button"
            :aria-label="
              navOpen ? 'Close settings and return to dashboard' : 'Open settings'
            "
            @click="toggleNav"
          >
            <svg
              class="nav-toggle-icon"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
              stroke-linecap="round"
              stroke-linejoin="round"
              aria-hidden="true"
            >
              <circle cx="12" cy="12" r="3" />
              <path
                d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"
              />
            </svg>
          </button>
        </div>
      </div>

      <nav id="primary-nav" class="tabbar" :class="{ open: onConfigPage }">
        <router-link
          v-for="t in TABS"
          :key="t.to"
          :to="t.to"
          class="tab"
          :tabindex="onConfigPage ? 0 : -1"
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

    <!-- In settings mode on the dashboard route, replace the dashboard with the
         settings guide; otherwise show the routed view (dashboard or a config
         page). -->
    <SettingsMenu v-if="navOpen && !onConfigPage" />
    <router-view v-else />

    <!-- Tiny build stamp, always visible. Links to the full System page. -->
    <footer class="site-footer">
      <router-link to="/system" class="footer-link">
        Smoke X Receiver{{ firmwareVersion ? ` · ${firmwareVersion}` : "" }}
      </router-link>
    </footer>

    <ToastHost />
    <ConfirmDialog />
  </div>
</template>

<style>
.site-header {
  position: sticky;
  top: 0;
  z-index: 500;
  background: var(--header-bg);
}

.appbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.5em;
  padding: 0.6em 1em;
  border-bottom: 1px solid var(--header-border);
}

.appbar-left {
  display: flex;
  align-items: center;
  gap: 0.5em;
  min-width: 0;
}

.nav-toggle {
  display: flex;
  align-items: center;
  justify-content: center;
  border: 1px solid var(--header-border);
  background: var(--header-btn-bg);
  color: var(--header-fg);
  border-radius: 0.4em;
  width: 2rem;
  height: 2rem;
  cursor: pointer;
  flex-shrink: 0;
}
/* Pressed/active look while settings mode is on (both the gear on the hub and
   the back arrow on a config page). Theme-safe tint that reads on the dark
   header and the amber header alike. */
.nav-toggle.active {
  background: var(--header-active-bg);
  border-color: var(--brand);
}
.nav-toggle-icon {
  width: 22px;
  height: 22px;
  display: block;
  /* Match the nav tabs: muted when not selected, brand color when active. */
  color: var(--header-muted);
}
.nav-toggle.active .nav-toggle-icon {
  color: var(--brand);
}

.brand {
  /* Tall, narrow wordmark echoing ThermoWorks' condensed branding. These are
     all standard system condensed faces (plus Oswald if the user happens to
     have it), so no webfont fetch is needed on the firmware-served build. */
  font-family: "Avenir Next Condensed", "Roboto Condensed", "Arial Narrow",
    "Helvetica Neue", "Oswald", sans-serif;
  font-stretch: condensed;
  font-weight: 400;
  font-size: 1.35em;
  color: var(--brand);
  text-decoration: none;
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
  border: 1px solid var(--header-border);
  color: var(--header-muted);
}
.pill-paired {
  background: var(--pill-ok-bg);
  color: var(--pill-ok-fg);
  border-color: var(--pill-ok-bg);
}
.pill-offline {
  background: var(--pill-off-bg);
  color: var(--pill-off-fg);
  border-color: transparent;
}

.theme-toggle {
  display: flex;
  align-items: center;
  justify-content: center;
  border: 1px solid var(--header-border);
  background: var(--header-btn-bg);
  color: var(--header-fg);
  border-radius: 0.4em;
  width: 2rem;
  height: 2rem;
  padding: 0;
  cursor: pointer;
  /* Larger than the box's base so the text glyph fills the button as much as
     the gear SVG does — box is in rem so this doesn't resize it. */
  font-size: 1.4rem;
  line-height: 1;
  flex-shrink: 0;
}

.tabbar {
  display: flex;
  max-width: 720px;
  margin: 0 auto;
  max-height: 0;
  opacity: 0;
  overflow: hidden;
  transition: max-height 0.22s ease, opacity 0.22s ease;
}
.tabbar.open {
  max-height: 5em;
  opacity: 1;
  border-bottom: 1px solid var(--header-border);
}
@media (prefers-reduced-motion: reduce) {
  .tabbar {
    transition: none;
  }
}

.tab {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 3px;
  padding: 0.55em 0.25em;
  color: var(--header-muted);
  text-decoration: none;
  font-family: Avenir, Helvetica, Arial, sans-serif;
  font-size: 0.72em;
  font-weight: bold;
  border-bottom: 2px solid transparent;
  margin-bottom: -1px;
}
.tab.router-link-exact-active {
  color: var(--brand);
  border-bottom-color: var(--brand);
  /* Background highlight so selection is clear even in light mode, where the
     brand and muted colors are both dark ink on the amber header. */
  background: var(--header-active-bg);
}

.tab-icon {
  width: 20px;
  height: 20px;
  display: block;
}

.site-footer {
  max-width: 720px;
  margin: 2em auto 1em;
  padding: 0 1em;
  text-align: center;
}
.footer-link {
  font-size: 0.68em;
  color: var(--text-muted);
  text-decoration: none;
  letter-spacing: 0.03em;
}
.footer-link:hover,
.footer-link:focus-visible {
  color: var(--brand-amber);
  text-decoration: underline;
}
</style>
