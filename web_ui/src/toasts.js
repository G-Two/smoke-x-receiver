// Minimal toast store. notify() pushes a message; the ToastHost component
// renders the reactive list. Auto-dismisses after `timeout` ms (0 = sticky).
import { ref } from "vue"

export const toasts = ref([])

let nextId = 1

export function notify(message, type = "success", timeout = 4000) {
  const id = nextId++
  toasts.value.push({ id, message, type })
  if (timeout) setTimeout(() => dismiss(id), timeout)
  return id
}

export function dismiss(id) {
  toasts.value = toasts.value.filter((t) => t.id !== id)
}
