import { ref } from 'vue'

const confirmState = ref({
  show: false,
  title: '确认',
  message: '',
  confirmText: '确定',
  cancelText: '取消',
  danger: false,
  resolve: null
})

export function useConfirm() {
  function confirm({ title = '确认', message, confirmText = '确定', cancelText = '取消', danger = false } = {}) {
    return new Promise((resolve) => {
      confirmState.value = { show: true, title, message, confirmText, cancelText, danger, resolve }
    })
  }

  function handleConfirm() {
    confirmState.value.show = false
    confirmState.value.resolve?.(true)
  }

  function handleCancel() {
    confirmState.value.show = false
    confirmState.value.resolve?.(false)
  }

  return { confirmState, confirm, handleConfirm, handleCancel }
}
