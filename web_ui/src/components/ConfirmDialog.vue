<script setup>
import { confirmState, resolveConfirm } from "../confirm"
</script>

<template>
  <Teleport to="body">
    <div
      v-if="confirmState"
      class="confirm-overlay"
      @click.self="resolveConfirm(false)"
    >
      <div class="confirm-dialog" role="alertdialog" aria-modal="true">
        <p class="confirm-message">{{ confirmState.message }}</p>
        <div class="confirm-actions">
          <button
            class="confirm-btn confirm-cancel"
            type="button"
            @click="resolveConfirm(false)"
          >
            Cancel
          </button>
          <button
            class="confirm-btn"
            :class="confirmState.danger ? 'confirm-danger' : 'confirm-primary'"
            type="button"
            @click="resolveConfirm(true)"
          >
            {{ confirmState.confirmLabel }}
          </button>
        </div>
      </div>
    </div>
  </Teleport>
</template>

<style scoped>
.confirm-overlay {
  position: fixed;
  inset: 0;
  z-index: 1100;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 1em;
  background: rgba(0, 0, 0, 0.5);
}

.confirm-dialog {
  width: 100%;
  max-width: 360px;
  background: var(--surface);
  color: var(--text);
  border-radius: 0.6em;
  padding: 1.5em;
  box-shadow: 0 8px 2em var(--shadow);
  font-family: Avenir, Helvetica, Arial, sans-serif;
}

.confirm-message {
  margin: 0 0 1.25em;
  font-size: 1em;
  line-height: 1.4;
}

.confirm-actions {
  display: flex;
  justify-content: flex-end;
  gap: 0.5em;
}

.confirm-btn {
  border: 1px solid var(--border);
  border-radius: 0.4em;
  padding: 0.5em 1em;
  font-weight: bold;
  cursor: pointer;
}

.confirm-cancel {
  background: var(--surface);
  color: var(--text);
}

.confirm-primary {
  background: var(--accent);
  border-color: var(--accent);
  color: #fff;
}

.confirm-danger {
  background: var(--alarm-high-fg);
  border-color: var(--alarm-high-fg);
  color: #fff;
}
</style>
