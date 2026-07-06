<script setup>
import { toasts, dismiss } from "../toasts"
</script>

<template>
  <Teleport to="body">
    <TransitionGroup tag="div" name="toast" class="toast-host">
      <div
        v-for="t in toasts"
        :key="t.id"
        class="toast"
        :class="`toast-${t.type}`"
        :role="t.type === 'error' ? 'alert' : 'status'"
        :aria-live="t.type === 'error' ? 'assertive' : 'polite'"
        tabindex="0"
        @click="dismiss(t.id)"
        @keydown.enter="dismiss(t.id)"
        @keydown.space.prevent="dismiss(t.id)"
      >
        {{ t.message }}
      </div>
    </TransitionGroup>
  </Teleport>
</template>

<style scoped>
.toast-host {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 1em;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 0.5em;
  z-index: 1000;
  padding: 0 1em;
  pointer-events: none;
}

.toast {
  pointer-events: auto;
  cursor: pointer;
  box-sizing: border-box;
  width: 100%;
  max-width: 420px;
  background: var(--surface);
  color: var(--text);
  border-left: 4px solid var(--accent);
  border-radius: 0.5em;
  padding: 0.85em 1em;
  box-shadow: 0 4px 1.5em var(--shadow);
  font-family: Avenir, Helvetica, Arial, sans-serif;
  font-size: 0.9em;
  font-weight: bold;
}

.toast-error {
  border-left-color: var(--alarm-high-fg);
}

.toast-enter-active,
.toast-leave-active {
  transition: opacity 0.25s ease, transform 0.25s ease;
}
.toast-enter-from,
.toast-leave-to {
  opacity: 0;
  transform: translateY(1em);
}
</style>
