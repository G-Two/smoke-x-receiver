// Promise-based confirmation dialog. confirm() sets the reactive state that
// the ConfirmDialog component renders and returns a Promise<boolean> that
// resolves when the user picks an action (or dismisses the dialog).
import { ref } from "vue"

export const confirmState = ref(null)

export function confirm(message, opts = {}) {
  // If a confirmation is already open, resolve it as cancelled (false) to
  // prevent the previous Promise from leaking when the state is replaced.
  if (confirmState.value) {
    confirmState.value.resolve(false)
  }
  return new Promise((resolve) => {
    confirmState.value = {
      message,
      confirmLabel: opts.confirmLabel || "Confirm",
      danger: !!opts.danger,
      resolve,
    }
  })
}

export function resolveConfirm(result) {
  const state = confirmState.value
  confirmState.value = null
  if (state) state.resolve(result)
}
