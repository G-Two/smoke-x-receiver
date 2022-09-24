import Vue from "vue"
import VueRouter from "vue-router"
import Wlan from "../views/Wlan.vue"

Vue.use(VueRouter)

const routes = [
  {
    path: "/wlan",
    name: "Wlan",
    component: Wlan,
  },
  {
    path: "/lora",
    name: "Lora",
    // route level code-splitting
    // this generates a separate chunk (about.[hash].js) for this route
    // which is lazy-loaded when the route is visited.
    component: () =>
      import(/* webpackChunkName: "about" */ "../views/Lora.vue"),
  },
  {
    path: "/pairing",
    name: "Pairing",
    // route level code-splitting
    // this generates a separate chunk (about.[hash].js) for this route
    // which is lazy-loaded when the route is visited.
    component: () =>
      import(/* webpackChunkName: "about" */ "../views/SmokeX.vue"),
  },
  {
    path: "/mqtt",
    name: "MQTT",
    // route level code-splitting
    // this generates a separate chunk (about.[hash].js) for this route
    // which is lazy-loaded when the route is visited.
    component: () =>
      import(/* webpackChunkName: "about" */ "../views/Mqtt.vue"),
  },
  {
    path: "/",
    name: "Status",
    // route level code-splitting
    // this generates a separate chunk (about.[hash].js) for this route
    // which is lazy-loaded when the route is visited.
    component: () =>
      import(/* webpackChunkName: "about" */ "../views/Status.vue"),
  },
]

const router = new VueRouter({
  mode: "history",
  base: process.env.BASE_URL,
  routes,
})

export default router
