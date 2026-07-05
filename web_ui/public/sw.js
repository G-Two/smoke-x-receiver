// Minimal service worker for the Smoke X Receiver PWA.
//
// Strategy:
//  - API endpoints: NOT intercepted → always live network. Temperatures and
//    pairing status must never be served from a stale cache.
//  - Navigations: network-first, falling back to the cached app shell so the
//    UI still opens when the device/Wi-Fi briefly drops (common outdoors).
//  - Hashed static assets (JS/CSS/PNG): cache-first, since their filenames
//    are content-hashed and immutable.
//  - Stable-name assets (manifest, icons): network-first with cache fallback,
//    so updates are picked up without bumping CACHE.
const CACHE = "smokex-v1"

// Stable-name (non-hashed) assets that must be re-checked on each load.
const STABLE_ASSETS = /\/(manifest\.webmanifest|icon-[^/]+\.png)$/

// Live-data endpoints served by the firmware — leave these to the network.
const API_PATHS = [
  "/data",
  "/pairing-status",
  "/rf-params",
  "/wlan-config",
  "/mqtt-config",
  "/cmd",
]

self.addEventListener("install", (event) => {
  self.skipWaiting()
  event.waitUntil(caches.open(CACHE).then((c) => c.add("./")))
})

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches
      .keys()
      .then((keys) =>
        Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k)))
      )
      .then(() => self.clients.claim())
  )
})

self.addEventListener("fetch", (event) => {
  const req = event.request
  if (req.method !== "GET") return // POSTs (saves, commands) go straight through

  const url = new URL(req.url)
  if (url.origin !== self.location.origin) return

  // Never cache live API responses.
  if (API_PATHS.some((p) => url.pathname.endsWith(p))) return

  // App navigations: try the network, fall back to the cached shell offline.
  if (req.mode === "navigate") {
    event.respondWith(fetch(req).catch(() => caches.match("./", { ignoreSearch: true })))
    return
  }

  // Stable-name assets (manifest, icons): network-first so updates are picked
  // up without bumping CACHE; fall back to cache when offline.
  if (STABLE_ASSETS.test(url.pathname)) {
    event.respondWith(
      fetch(req)
        .then((res) => {
          if (res.ok) {
            const copy = res.clone()
            caches.open(CACHE).then((c) => c.put(req, copy))
          }
          return res
        })
        .catch(() => caches.match(req))
    )
    return
  }

  // Hashed static assets: serve from cache, else fetch and cache for next time.
  event.respondWith(
    caches.match(req).then((cached) => {
      if (cached) return cached
      return fetch(req).then((res) => {
        if (res.ok) {
          const copy = res.clone()
          caches.open(CACHE).then((c) => c.put(req, copy))
        }
        return res
      })
    })
  )
})
