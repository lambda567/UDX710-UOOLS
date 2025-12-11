<script setup>
import { ref, provide, onMounted, onUnmounted, computed } from 'vue'
import SystemMonitor from './components/SystemMonitor.vue'
import NetworkManager from './components/NetworkManager.vue'
import WifiManager from './components/WifiManager.vue'
import AdvancedNetwork from './components/AdvancedNetwork.vue'
import SmsManager from './components/SmsManager.vue'
import DeviceManager from './components/DeviceManager.vue'
import TrafficStats from './components/TrafficStats.vue'
import BatteryManager from './components/BatteryManager.vue'
import SystemSettings from './components/SystemSettings.vue'
import SystemUpdate from './components/SystemUpdate.vue'
import ATDebug from './components/ATDebug.vue'
import WebTerminal from './components/WebTerminal.vue'
import GlobalToast from './components/GlobalToast.vue'
import GlobalConfirm from './components/GlobalConfirm.vue'

// 当前激活的菜单
const activeMenu = ref('monitor')
const sidebarCollapsed = ref(false)

// 移动端适配
const isMobile = ref(false)
const sidebarOpen = ref(false)

// 主题模式：true=深色(深夜模式), false=浅色(白天模式)
const isDark = ref(true)

// 初始化主题
function initTheme() {
  const saved = localStorage.getItem('theme')
  if (saved) {
    isDark.value = saved === 'dark'
  } else {
    // 默认深色模式（深夜模式）
    isDark.value = true
  }
  applyTheme()
}

// 应用主题到DOM
function applyTheme() {
  if (isDark.value) {
    document.documentElement.classList.add('dark')
  } else {
    document.documentElement.classList.remove('dark')
  }
}

// 切换主题
function toggleTheme() {
  isDark.value = !isDark.value
  localStorage.setItem('theme', isDark.value ? 'dark' : 'light')
  applyTheme()
}

// 提供主题状态给子组件
provide('isDark', isDark)

function checkMobile() {
  isMobile.value = window.innerWidth < 768
  if (isMobile.value) {
    sidebarCollapsed.value = true
    sidebarOpen.value = false
  }
}

// 滚动穿透处理
function lockBodyScroll(lock) {
  if (lock) {
    document.body.classList.add('body-lock')
  } else {
    document.body.classList.remove('body-lock')
  }
}

function toggleSidebar() {
  if (isMobile.value) {
    sidebarOpen.value = !sidebarOpen.value
    lockBodyScroll(sidebarOpen.value)
  } else {
    sidebarCollapsed.value = !sidebarCollapsed.value
  }
}

function handleMenuClick(menuId) {
  activeMenu.value = menuId
  if (isMobile.value) {
    sidebarOpen.value = false
    lockBodyScroll(false)
  }
}

// 系统信息数据
const systemInfo = ref({})
const loading = ref(false)
const lastUpdate = ref('')

provide('systemInfo', systemInfo)
provide('loading', loading)

// 菜单配置
const menuItems = [
  { id: 'monitor', label: '系统监控', icon: 'fa-tachometer-alt', color: 'from-blue-500 to-cyan-400' },
  { id: 'network', label: '网络管理', icon: 'fa-network-wired', color: 'from-purple-500 to-pink-400' },
  { id: 'wifi', label: 'WiFi控制', icon: 'fa-wifi', color: 'from-indigo-500 to-violet-400' },
  { id: 'devices', label: '设备接入', icon: 'fa-laptop', color: 'from-rose-500 to-pink-400' },
  { id: 'advanced', label: '高级网络', icon: 'fa-tower-cell', color: 'from-cyan-500 to-blue-500' },
  { id: 'sms', label: '短信管理', icon: 'fa-envelope', color: 'from-emerald-500 to-teal-400' },
  { id: 'traffic', label: '流量统计', icon: 'fa-chart-area', color: 'from-green-500 to-emerald-400' },
  { id: 'battery', label: '充电控制', icon: 'fa-battery-half', color: 'from-yellow-500 to-amber-400' },
  { id: 'update', label: '系统更新', icon: 'fa-cloud-download-alt', color: 'from-violet-500 to-purple-400' },
  { id: 'at', label: 'AT调试', icon: 'fa-terminal', color: 'from-cyan-500 to-teal-400' },
  { id: 'terminal', label: 'Web终端', icon: 'fa-desktop', color: 'from-slate-500 to-gray-400' },
  { id: 'settings', label: '系统设置', icon: 'fa-sliders-h', color: 'from-orange-500 to-amber-400' }
]

// 获取系统信息
async function fetchSystemInfo() {
  loading.value = true
  try {
    const response = await fetch('/api/info')
    if (!response.ok) throw new Error('获取失败')
    systemInfo.value = await response.json()
    lastUpdate.value = new Date().toLocaleTimeString()
  } catch (error) {
    console.error('获取系统信息错误:', error)
  } finally {
    loading.value = false
  }
}

let refreshInterval = null
onMounted(() => {
  fetchSystemInfo()
  refreshInterval = setInterval(fetchSystemInfo, 30000)
  // 移动端检测
  checkMobile()
  window.addEventListener('resize', checkMobile)
  // 初始化主题
  initTheme()
})
onUnmounted(() => {
  if (refreshInterval) clearInterval(refreshInterval)
  window.removeEventListener('resize', checkMobile)
})
</script>

<template>
  <div class="min-h-screen bg-gradient-to-br from-slate-50 via-blue-50/40 to-indigo-50/30 dark:from-slate-900 dark:via-slate-800 dark:to-slate-900 flex transition-all duration-500">
    <!-- 移动端遮罩层 -->
    <div 
      v-if="isMobile && sidebarOpen"
      class="fixed inset-0 bg-black/40 dark:bg-black/60 z-40 backdrop-blur-sm"
      @click="sidebarOpen = false; lockBodyScroll(false)"
    ></div>

    <!-- 左侧侧边栏 -->
    <aside 
      class="fixed left-0 top-0 h-full z-50 transition-all duration-300 ease-in-out"
      :class="[
        isMobile 
          ? (sidebarOpen ? 'w-64 translate-x-0' : 'w-64 -translate-x-full') 
          : (sidebarCollapsed ? 'w-20' : 'w-64')
      ]"
    >
      <!-- 毛玻璃背景 -->
      <div class="absolute inset-0 bg-gradient-to-b from-white via-slate-50/95 to-blue-50/90 dark:from-white/10 dark:via-white/5 dark:to-white/10 backdrop-blur-xl border-r border-slate-200/70 dark:border-white/10 shadow-xl shadow-slate-200/20 dark:shadow-black/20 transition-all duration-500"></div>
      
      <div class="relative h-full flex flex-col">
        <!-- Logo区域 -->
        <div class="p-4 md:p-6 border-b border-slate-200 dark:border-white/10">
          <div class="flex items-center space-x-3">
            <div class="w-10 h-10 rounded-xl bg-gradient-to-br from-blue-500 to-cyan-400 flex items-center justify-center shadow-lg shadow-blue-500/30">
              <font-awesome-icon icon="wifi" class="text-white text-lg" />
            </div>
            <div v-show="!sidebarCollapsed || isMobile" class="overflow-hidden">
              <h1 class="text-slate-900 dark:text-white font-bold text-lg whitespace-nowrap">5G-MIFI</h1>
              <p class="text-slate-500 dark:text-white/50 text-xs">管理中心</p>
            </div>
          </div>
        </div>

        <!-- 导航菜单 -->
        <nav class="flex-1 p-2 md:p-4 space-y-1 md:space-y-2 overflow-y-auto">
          <button
            v-for="item in menuItems"
            :key="item.id"
            @click="handleMenuClick(item.id)"
            class="w-full group relative overflow-hidden rounded-xl transition-all duration-300"
            :class="activeMenu === item.id ? 'bg-gradient-to-r from-blue-100/80 to-indigo-100/60 dark:from-white/20 dark:to-white/10 shadow-md shadow-blue-200/30 dark:shadow-black/10' : 'hover:bg-slate-100/80 dark:hover:bg-white/10'"
          >
            <!-- 激活指示器 -->
            <div 
              v-if="activeMenu === item.id"
              class="absolute left-0 top-1/2 -translate-y-1/2 w-1 h-8 rounded-r-full bg-gradient-to-b"
              :class="item.color"
            ></div>
            
            <div class="flex items-center p-2 md:p-3" :class="(sidebarCollapsed && !isMobile) ? 'justify-center' : 'space-x-3'">
              <div 
                class="w-9 h-9 md:w-10 md:h-10 rounded-lg flex items-center justify-center transition-all duration-300 flex-shrink-0"
                :class="activeMenu === item.id ? `bg-gradient-to-br ${item.color} shadow-lg` : 'bg-slate-200 dark:bg-white/5 group-hover:bg-slate-300 dark:group-hover:bg-white/10'"
              >
                <font-awesome-icon :icon="item.icon.replace('fa-', '')" :class="activeMenu === item.id ? 'text-white' : 'text-slate-600 dark:text-white/60'" />
              </div>
              <span 
                v-show="!sidebarCollapsed || isMobile"
                class="font-medium transition-colors text-sm md:text-base"
                :class="activeMenu === item.id ? 'text-slate-900 dark:text-white' : 'text-slate-600 dark:text-white/60 group-hover:text-slate-900 dark:group-hover:text-white/80'"
              >
                {{ item.label }}
              </span>
            </div>
          </button>
        </nav>

        <!-- GitHub链接 -->
        <div class="px-2 md:px-4 pb-2">
          <a 
            href="https://github.com/LeoChen-CoreMind/UDX710-UOOLS" 
            target="_blank"
            class="w-full group relative overflow-hidden rounded-xl transition-all duration-300 hover:bg-slate-100/80 dark:hover:bg-white/10 flex items-center p-2 md:p-3"
            :class="(sidebarCollapsed && !isMobile) ? 'justify-center' : 'space-x-3'"
          >
            <div class="w-9 h-9 md:w-10 md:h-10 rounded-lg flex items-center justify-center transition-all duration-300 flex-shrink-0 bg-slate-200 dark:bg-white/5 group-hover:bg-gradient-to-br group-hover:from-gray-700 group-hover:to-gray-900">
              <font-awesome-icon :icon="['fab', 'github']" class="text-slate-600 dark:text-white/60 group-hover:text-white" />
            </div>
            <span 
              v-show="!sidebarCollapsed || isMobile"
              class="font-medium transition-colors text-sm md:text-base text-slate-600 dark:text-white/60 group-hover:text-slate-900 dark:group-hover:text-white/80"
            >
              开源项目
            </span>
          </a>
        </div>

        <!-- 底部折叠按钮 (仅桌面端显示) -->
        <div v-if="!isMobile" class="p-4 border-t border-slate-200 dark:border-white/10">
          <button 
            @click="toggleSidebar"
            class="w-full p-3 rounded-xl bg-slate-200 dark:bg-white/5 hover:bg-slate-300 dark:hover:bg-white/10 transition-colors flex items-center justify-center"
          >
            <font-awesome-icon :icon="sidebarCollapsed ? 'chevron-right' : 'chevron-left'" class="text-slate-600 dark:text-white/60" />
          </button>
        </div>
      </div>
    </aside>

    <!-- 主内容区 -->
    <main 
      class="flex-1 transition-all duration-300 w-full"
      :class="isMobile ? 'ml-0' : (sidebarCollapsed ? 'ml-20' : 'ml-64')"
    >
      <!-- 顶部状态栏 -->
      <header class="sticky top-0 z-30 px-3 md:px-6 py-2 md:py-4">
        <div class="bg-white/90 dark:bg-white/10 backdrop-blur-xl rounded-xl md:rounded-2xl border border-slate-200/70 dark:border-white/10 px-3 md:px-6 py-3 md:py-4 shadow-lg shadow-slate-200/30 dark:shadow-black/20 transition-all duration-500">
          <div class="flex items-center justify-between gap-2 md:gap-4">
            <!-- 左侧：汉堡菜单 + 标题 -->
            <div class="flex items-center space-x-3">
              <!-- 汉堡菜单按钮 (仅移动端) -->
              <button 
                v-if="isMobile"
                @click="toggleSidebar"
                class="w-10 h-10 rounded-xl bg-slate-200 dark:bg-white/10 hover:bg-slate-300 dark:hover:bg-white/20 transition-colors flex items-center justify-center flex-shrink-0"
              >
                <font-awesome-icon icon="bars" class="text-slate-700 dark:text-white/80" />
              </button>
              <div class="min-w-0">
                <h2 class="text-lg md:text-2xl font-bold text-slate-900 dark:text-white truncate">
                  {{ menuItems.find(m => m.id === activeMenu)?.label }}
                </h2>
                <p class="text-slate-500 dark:text-white/50 text-xs md:text-sm mt-0.5 md:mt-1 hidden sm:block">实时监控设备状态</p>
              </div>
            </div>
            <!-- 右侧：状态信息 -->
            <div class="flex items-center space-x-2 md:space-x-4 flex-shrink-0">
              <!-- 状态指示器 -->
              <div class="flex items-center space-x-1.5 md:space-x-2 px-2 md:px-4 py-1.5 md:py-2 bg-slate-200 dark:bg-white/5 rounded-lg md:rounded-xl">
                <span class="w-2 h-2 rounded-full animate-pulse" :class="loading ? 'bg-yellow-400' : 'bg-green-400'"></span>
                <span class="text-slate-600 dark:text-white/60 text-xs md:text-sm hidden sm:inline">{{ loading ? '同步中...' : '已连接' }}</span>
              </div>
              <!-- 主题切换按钮 -->
              <button 
                @click="toggleTheme"
                class="w-9 h-9 md:w-10 md:h-10 rounded-lg md:rounded-xl bg-slate-200 dark:bg-white/10 hover:bg-slate-300 dark:hover:bg-white/20 transition-colors flex items-center justify-center"
                :title="isDark ? '切换到白天模式' : '切换到深夜模式'"
              >
                <font-awesome-icon :icon="isDark ? 'sun' : 'moon'" class="text-amber-500 dark:text-amber-400 text-sm md:text-base" />
              </button>
              <!-- 刷新按钮 -->
              <button 
                @click="fetchSystemInfo"
                :disabled="loading"
                class="w-9 h-9 md:w-10 md:h-10 rounded-lg md:rounded-xl bg-slate-200 dark:bg-white/10 hover:bg-slate-300 dark:hover:bg-white/20 transition-colors flex items-center justify-center"
              >
                <font-awesome-icon icon="sync-alt" class="text-slate-600 dark:text-white/60 text-sm md:text-base" :class="{ 'animate-spin': loading }" />
              </button>
              <!-- 时间显示 (仅桌面端) -->
              <div class="text-right hidden md:block">
                <div class="text-slate-500 dark:text-white/40 text-xs">最后更新</div>
                <div class="text-slate-700 dark:text-white/80 text-sm font-medium">{{ lastUpdate || '--:--:--' }}</div>
              </div>
            </div>
          </div>
        </div>
      </header>

      <!-- 内容区域 -->
      <div class="p-3 md:p-6">
        <Transition name="fade" mode="out-in">
          <SystemMonitor v-if="activeMenu === 'monitor'" key="monitor" />
          <NetworkManager v-else-if="activeMenu === 'network'" key="network" />
          <WifiManager v-else-if="activeMenu === 'wifi'" key="wifi" />
          <DeviceManager v-else-if="activeMenu === 'devices'" key="devices" />
          <AdvancedNetwork v-else-if="activeMenu === 'advanced'" key="advanced" />
          <SmsManager v-else-if="activeMenu === 'sms'" key="sms" />
          <TrafficStats v-else-if="activeMenu === 'traffic'" key="traffic" />
          <BatteryManager v-else-if="activeMenu === 'battery'" key="battery" />
          <SystemUpdate v-else-if="activeMenu === 'update'" key="update" />
          <ATDebug v-else-if="activeMenu === 'at'" key="at" />
          <WebTerminal v-else-if="activeMenu === 'terminal'" key="terminal" />
          <SystemSettings v-else-if="activeMenu === 'settings'" key="settings" />
        </Transition>
      </div>
    </main>
    <GlobalToast />
    <GlobalConfirm />
  </div>
</template>

<style>
.fade-enter-active {
  transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1);
}
.fade-leave-active {
  transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1);
}
.fade-enter-from {
  opacity: 0;
  transform: translateX(16px) scale(0.98);
}
.fade-leave-to {
  opacity: 0;
  transform: translateX(-16px) scale(0.98);
}
</style>
