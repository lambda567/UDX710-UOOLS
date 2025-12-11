import { ref } from 'vue'

const toast = ref({ show: false, success: true, message: '' })
let timer = null

export function useToast() {
  function showToast(success, message, duration = 3000) {
    if (timer) clearTimeout(timer)
    toast.value = { show: true, success, message }
    timer = setTimeout(() => { toast.value.show = false }, duration)
  }

  function success(message, duration) { showToast(true, message, duration) }
  function error(message, duration) { showToast(false, message, duration) }

  return { toast, showToast, success, error }
}
