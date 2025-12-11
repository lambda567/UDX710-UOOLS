<script setup>
defineProps({
  activeTab: {
    type: String,
    default: 'monitor'
  }
})

const emit = defineEmits(['change'])

const tabs = [
  { id: 'monitor', label: '系统监控', icon: 'fa-tachometer-alt' },
  { id: 'network', label: '网络管理', icon: 'fa-network-wired' },
  { id: 'traffic', label: '流量统计', icon: 'fa-chart-line' },
  { id: 'settings', label: '系统设置', icon: 'fa-cogs' }
]

function handleClick(tabId) {
  emit('change', tabId)
}
</script>

<template>
  <nav class="bg-white shadow-sm border-b sticky top-[72px] z-40">
    <div class="container mx-auto px-4">
      <div class="flex space-x-0 overflow-x-auto scrollbar-hide">
        <button
          v-for="tab in tabs"
          :key="tab.id"
          @click="handleClick(tab.id)"
          class="tab-btn px-4 md:px-6 py-4 text-sm font-medium border-b-2 whitespace-nowrap transition-all duration-300"
          :class="[
            activeTab === tab.id
              ? 'border-primary text-primary bg-primary/5'
              : 'border-transparent text-gray-500 hover:text-primary hover:border-gray-300 hover:bg-gray-50'
          ]"
        >
          <i :class="['fas', tab.icon, 'mr-2']"></i>
          <span>{{ tab.label }}</span>
        </button>
      </div>
    </div>
  </nav>
</template>

<style scoped>
.scrollbar-hide::-webkit-scrollbar {
  display: none;
}
.scrollbar-hide {
  -ms-overflow-style: none;
  scrollbar-width: none;
}
</style>
