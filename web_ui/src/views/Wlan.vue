<template>
  <div class="home">
    <wlan-config-form />
  </div>
</template>

<script>
// @ is an alias to /src
import WlanConfigForm from "@/components/WlanConfigForm.vue"

export default {
  name: "WlanComponent",
  components: {
    WlanConfigForm,
  },
}
</script>
