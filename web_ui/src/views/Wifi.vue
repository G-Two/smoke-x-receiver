<template>
  <div class="home">
    <wifi-config-form />
  </div>
</template>

<script>
// @ is an alias to /src
import WifiConfigForm from "@/components/WifiConfigForm.vue";

export default {
  name: "Wifi",
  components: {
    WifiConfigForm
  }
};
</script>
