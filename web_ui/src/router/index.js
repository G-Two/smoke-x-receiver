import { createWebHistory, createRouter } from "vue-router"
import WlanConfigForm from "../components/WlanConfigForm.vue"
import StatusForm from "../components/StatusForm.vue"
import MqttConfigForm from "../components/MqttConfigForm.vue"
import SmokeXConfigForm from "../components/SmokeXConfigForm.vue"

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
]

const router = createRouter({
  history: createWebHistory(),
  routes,
})

export default router
