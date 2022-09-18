<template>
  <div class="home">
    <lora-config-form />
  </div>
</template>

<script>
// @ is an alias to /src
import LoraConfigForm from "@/components/LoraConfigForm.vue";

export default {
  name: "LoraComponent",
  components: {
    LoraConfigForm,
  },
};
</script>
