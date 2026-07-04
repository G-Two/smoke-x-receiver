import { createApp } from "vue"
import App from "./App.vue"
import { plugin } from "@formkit/vue"
import formKitConfig from "./formkit.config"
import router from "./router"
// Imported after formkit.config (which pulls in the Genesis theme) so our
// dark-mode variable overrides take precedence in source order.
import "./styles/theme.css"
import "./theme"

async function startApp() {
  if (process.env.NODE_ENV === "development") {
    const { worker } = await import("./mocks/browser")
    worker.start()
  }

  const app = createApp(App)
  app.use(plugin, formKitConfig)
  app.use(router)
  app.mount("#app")
}

// Register the PWA service worker only in production builds. In dev the MSW
// mock worker owns the same scope, so registering here would conflict.
function registerServiceWorker() {
  if (!import.meta.env.PROD || !("serviceWorker" in navigator)) return
  window.addEventListener("load", () => {
    navigator.serviceWorker
      .register(import.meta.env.BASE_URL + "sw.js")
      .catch(() => {})
  })
}

startApp()
registerServiceWorker()
