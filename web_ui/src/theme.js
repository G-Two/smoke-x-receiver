// Reactive theme controller. Preference is one of "auto" | "light" | "dark"
// and is persisted in localStorage; it is always resolved to an explicit
// data-theme attribute on <html> so the CSS only needs a single dark block.
// An inline script in index.html sets data-theme before first paint to avoid
// a flash; this module keeps it in sync at runtime and exposes `isDark` for
// the canvas chart, which can't read CSS variables directly.
import { ref } from "vue"

const KEY = "theme"
const mql = window.matchMedia("(prefers-color-scheme: dark)")

function stored() {
  try {
    const pref = localStorage.getItem(KEY)
    return pref === "light" || pref === "dark" ? pref : "auto"
  } catch (e) {
    return "auto"
  }
}

function resolve(pref) {
  if (pref === "dark") return true
  if (pref === "light") return false
  return mql.matches
}

export const preference = ref(stored())
export const isDark = ref(resolve(preference.value))

function apply() {
  isDark.value = resolve(preference.value)
  document.documentElement.setAttribute(
    "data-theme",
    isDark.value ? "dark" : "light"
  )
}

export function setPreference(pref) {
  preference.value = pref
  try {
    if (pref === "auto") localStorage.removeItem(KEY)
    else localStorage.setItem(KEY, pref)
  } catch (e) {
    /* storage unavailable — keep in-memory preference only */
  }
  apply()
}

export function cyclePreference() {
  const order = ["auto", "light", "dark"]
  setPreference(order[(order.indexOf(preference.value) + 1) % order.length])
}

const onSchemeChange = () => {
  if (preference.value === "auto") apply()
}

try {
  if (typeof mql.addEventListener === "function") {
    mql.addEventListener("change", onSchemeChange)
  } else if (typeof mql.addListener === "function") {
    mql.addListener(onSchemeChange)
  }
} catch (e) {
  /* listener registration unavailable — theme still applies on load/manual toggle */
}

apply()
