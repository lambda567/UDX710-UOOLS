<script setup>
import { useConfirm } from '../composables/useConfirm'
const { confirmState, handleConfirm, handleCancel } = useConfirm()
</script>

<template>
  <Teleport to="body">
    <Transition name="fade">
      <div v-if="confirmState.show" class="fixed inset-0 z-50 flex items-center justify-center">
        <div class="absolute inset-0 bg-black/60 backdrop-blur-sm" @click="handleCancel"></div>
        <div class="relative bg-white dark:bg-slate-800/95 border border-slate-200 dark:border-white/20 rounded-2xl shadow-2xl p-6 max-w-sm w-full mx-4">
          <h3 class="text-lg font-semibold text-slate-900 dark:text-white mb-3 flex items-center">
            <i :class="confirmState.danger ? 'fas fa-exclamation-triangle text-red-500' : 'fas fa-question-circle text-cyan-500'" class="mr-2"></i>
            {{ confirmState.title }}
          </h3>
          <p class="text-slate-600 dark:text-white/70 mb-6">{{ confirmState.message }}</p>
          <div class="flex justify-end space-x-3">
            <button @click="handleCancel" class="px-4 py-2 bg-slate-100 hover:bg-slate-200 dark:bg-white/10 dark:hover:bg-white/20 text-slate-700 dark:text-white rounded-xl transition-colors">
              {{ confirmState.cancelText }}
            </button>
            <button @click="handleConfirm" :class="confirmState.danger ? 'bg-red-500 hover:bg-red-600' : 'bg-cyan-500 hover:bg-cyan-600'" class="px-4 py-2 text-white rounded-xl transition-colors">
              {{ confirmState.confirmText }}
            </button>
          </div>
        </div>
      </div>
    </Transition>
  </Teleport>
</template>

<style scoped>
.fade-enter-active, .fade-leave-active { transition: all 0.2s ease; }
.fade-enter-from, .fade-leave-to { opacity: 0; }
</style>
