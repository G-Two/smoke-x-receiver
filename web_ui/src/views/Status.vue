<template>
  <div class="home">
    <status-form />
  </div>
</template>

<script>
// @ is an alias to /src
import StatusForm from "@/components/StatusForm.vue";

export default {
  name: "StatusComponent",
  components: {
    StatusForm,
  },
};
</script>
