<template>
  <div class="home">
    <mqtt-config-form />
  </div>
</template>

<script>
// @ is an alias to /src
import MqttConfigForm from "@/components/MqttConfigForm.vue";

export default {
  name: "Mqtt",
  components: {
    MqttConfigForm
  }
};
</script>
