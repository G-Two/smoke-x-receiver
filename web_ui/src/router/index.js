import { createWebHistory, createRouter } from "vue-router"
import WlanConfigForm from "../components/WlanConfigForm.vue"
import StatusForm from "../components/StatusForm.vue"
import MqttConfigForm from "../components/MqttConfigForm.vue"
import SmokeXConfigForm from "../components/SmokeXConfigForm.vue"
import LoraAdvancedForm from "../components/LoraAdvancedForm.vue"
import SystemInfoForm from "../components/SystemInfoForm.vue"

const routes = [
  {
    path: "/",
    name: "Status",
    component: StatusForm,
  },
  {
    path: "/wlan",
    name: "Wlan",
    component: WlanConfigForm,
  },
  {
    path: "/mqtt",
    name: "mqtt",
    component: MqttConfigForm,
  },
  {
    path: "/pairing",
    name: "pairing",
    component: SmokeXConfigForm,
  },
  {
    // Deep-linkable: "/lora" is whitelisted in the firmware's SPA routing.
    // Reached from the Pairing page, intentionally not a top-level nav tab.
    path: "/lora",
    name: "lora",
    component: LoraAdvancedForm,
  },
  {
    // Deep-linkable: "/system" is whitelisted in the firmware's SPA routing.
    path: "/system",
    name: "system",
    component: SystemInfoForm,
  },
]

const router = createRouter({
  history: createWebHistory(),
  routes,
})

export default router
