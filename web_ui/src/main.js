import Vue from "vue";
import App from "./App.vue";
import {
  TooltipPlugin,
  FormPlugin,
  LayoutPlugin,
  ButtonPlugin,
  FormInputPlugin,
  FormCheckboxPlugin,
  FormRadioPlugin,
  FormGroupPlugin,
} from "bootstrap-vue";
import "bootstrap/dist/css/bootstrap.css";
import "bootstrap-vue/dist/bootstrap-vue.css";
import router from "./router";

// For tree shaking purposes
Vue.use(TooltipPlugin);
Vue.use(FormPlugin);
Vue.use(LayoutPlugin);
Vue.use(ButtonPlugin);
Vue.use(FormCheckboxPlugin);
Vue.use(FormInputPlugin);
Vue.use(FormGroupPlugin);
Vue.use(FormRadioPlugin);

Vue.config.productionTip = false;

new Vue({
  router,
  render: (h) => h(App),
}).$mount("#app");
