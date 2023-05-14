import { createApp } from "vue"
import App from "./App.vue"
import { plugin } from "@formkit/vue"
import formKitConfig from "./formkit.config"
import router from "./router"

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

startApp()
