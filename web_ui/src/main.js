import Vue from "vue"
import App from "./App.vue"
import {
  TooltipPlugin,
  FormPlugin,
  LayoutPlugin,
  ButtonPlugin,
  FormInputPlugin,
  FormTextareaPlugin,
  FormCheckboxPlugin,
  FormRadioPlugin,
  FormGroupPlugin,
  OverlayPlugin,
} from "bootstrap-vue"
import "bootstrap/dist/css/bootstrap.css"
//import "bootstrap-vue/dist/bootstrap-vue.css";
import router from "./router"

// For tree shaking purposes
Vue.use(TooltipPlugin)
Vue.use(FormPlugin)
Vue.use(LayoutPlugin)
Vue.use(ButtonPlugin)
Vue.use(FormCheckboxPlugin)
Vue.use(FormInputPlugin)
Vue.use(FormTextareaPlugin)
Vue.use(FormGroupPlugin)
Vue.use(FormRadioPlugin)
Vue.use(OverlayPlugin)

Vue.config.productionTip = false

if (process.env.NODE_ENV === "development") {
  const { worker } = require("./mocks/browser")
  worker.start()
}

new Vue({
  router,
  render: (h) => h(App),
}).$mount("#app")
