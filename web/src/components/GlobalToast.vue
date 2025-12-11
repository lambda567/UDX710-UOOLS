<script setup>
import { useToast } from '../composables/useToast'
const { toast } = useToast()
</script>

<template>
  <Teleport to="body">
    <Transition name="slide">
      <div v-if="toast.show" class="fixed bottom-6 right-6 px-6 py-4 rounded-2xl shadow-2xl border z-50"
        :class="toast.success ? 'bg-emerald-500/90 border-emerald-400/50 text-white' : 'bg-red-500/90 border-red-400/50 text-white'">
        <div class="flex items-center space-x-3">
          <i :class="toast.success ? 'fas fa-check-circle' : 'fas fa-times-circle'" class="text-xl"></i>
          <span class="font-medium">{{ toast.message }}</span>
        </div>
      </div>
    </Transition>
  </Teleport>
</template>

<style scoped>
.slide-enter-active, .slide-leave-active { transition: all 0.3s ease; }
.slide-enter-from, .slide-leave-to { opacity: 0; transform: translateX(100px); }
</style>
