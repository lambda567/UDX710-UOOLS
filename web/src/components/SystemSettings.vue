<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
import { deviceControl, getRebootConfig, setReboot, clearReboot, getSystemTime, syncSystemTime, useApi } from '../composables/useApi'
import { useToast } from '../composables/useToast'
import { useConfirm } from '../composables/useConfirm'

const { success, error } = useToast()
const { confirm } = useConfirm()
const api = useApi()

const rebooting = ref(false)
const shuttingDown = ref(false)
const resetting = ref(false)
const rebootEnabled = ref(false)
const rebootTime = ref('')
const selectedDays = ref([])
const savingReboot = ref(false)
const currentTime = ref('')
const syncingTime = ref(false)

const weekDays = [
  { value: '1', label: '周一', short: '一' },
  { value: '2', label: '周二', short: '二' },
  { value: '3', label: '周三', short: '三' },
  { value: '4', label: '周四', short: '四' },
  { value: '5', label: '周五', short: '五' },
  { value: '6', label: '周六', short: '六' },
  { value: '0', label: '周日', short: '日' }
]

async function fetchRebootConfig() {
  try {
    const data = await getRebootConfig()
    if (data.success && data.job) {
      rebootEnabled.value = true
      const [minute, hour, , , days] = data.job.split(' ')
      rebootTime.value = `${hour.padStart(2, '0')}:${minute.padStart(2, '0')}`
      selectedDays.value = days.split(',')
    }
  } catch (err) {
    console.error('获取定时重启配置失败:', err)
  }
}

async function fetchSystemTime() {
  try {
    const data = await getSystemTime()
    if (data.Code === 0 && data.Data) {
      currentTime.value = data.Data.datetime
    }
  } catch (err) {
    console.error('获取系统时间失败:', err)
  }
}

async function handleSyncTime() {
  syncingTime.value = true
  try {
    const data = await syncSystemTime()
    if (data.Code === 0) {
      success('时间同步成功: ' + (data.server || 'NTP'))
      await fetchSystemTime()
    } else {
      error(data.Error || '时间同步失败')
    }
  } catch (err) {
    error('时间同步失败: ' + err.message)
  } finally {
    syncingTime.value = false
  }
}

async function handleReboot() {
  if (!await confirm({ title: '设备重启', message: '确定要重启设备吗？此操作不可撤销。', danger: true })) return
  rebooting.value = true
  try {
    await deviceControl('reboot')
    success('重启命令已发送，设备将在几秒后重启')
  } catch (err) {
    error('重启失败: ' + err.message)
  } finally {
    rebooting.value = false
  }
}

async function handleShutdown() {
  if (!await confirm({ title: '设备关机', message: '确定要关闭设备吗？此操作不可撤销。', danger: true })) return
  shuttingDown.value = true
  try {
    await deviceControl('poweroff')
    success('关机命令已发送，设备将在30秒内关闭')
  } catch (err) {
    error('关机失败: ' + err.message)
  } finally {
    shuttingDown.value = false
  }
}

async function handleFactoryReset() {
  if (!await confirm({ 
    title: '恢复出厂设置', 
    message: '此操作将清除所有配置数据（包括WiFi设置、流量统计等），设备将重启。确定要继续吗？', 
    confirmText: '确认恢复',
    danger: true 
  })) return
  resetting.value = true
  try {
    const res = await api.get('/api/factory-reset')
    if (res.success) {
      success('恢复出厂设置成功，设备正在重启...')
    } else {
      error(res.msg || '恢复出厂设置失败')
    }
  } catch (err) {
    error('恢复出厂设置失败: ' + err.message)
  } finally {
    resetting.value = false
  }
}

function toggleAllDays() {
  if (selectedDays.value.length === 7) {
    selectedDays.value = []
  } else {
    selectedDays.value = ['1', '2', '3', '4', '5', '6', '0']
  }
}

async function saveRebootConfig() {
  if (rebootEnabled.value) {
    if (!rebootTime.value) {
      error('请选择重启时间')
      return
    }
    if (selectedDays.value.length === 0) {
      error('请选择重启日期')
      return
    }
    savingReboot.value = true
    try {
      const [hour, minute] = rebootTime.value.split(':')
      await setReboot(selectedDays.value, hour, minute)
      success('定时重启设置成功')
    } catch (err) {
      error('设置失败: ' + err.message)
    } finally {
      savingReboot.value = false
    }
  } else {
    savingReboot.value = true
    try {
      await clearReboot()
      success('已清除定时重启')
    } catch (err) {
      error('清除失败: ' + err.message)
    } finally {
      savingReboot.value = false
    }
  }
}

// 每秒更新系统时间（从API获取）
let timeInterval = null

onMounted(() => {
  fetchRebootConfig()
  fetchSystemTime()
  timeInterval = setInterval(fetchSystemTime, 1000)
})

onUnmounted(() => {
  if (timeInterval) {
    clearInterval(timeInterval)
  }
})
</script>

<template>
  <div class="space-y-4 sm:space-y-6">
    <!-- 设备控制 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20 hover:shadow-xl hover:shadow-slate-300/50 dark:hover:shadow-black/30 transition-all duration-300">
      <h3 class="text-slate-900 dark:text-white font-semibold mb-6 flex items-center">
        <i class="fas fa-power-off text-red-500 dark:text-red-400 mr-2"></i>
        设备控制
      </h3>
      
      <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
        <!-- 重启按钮 -->
        <button 
          @click="handleReboot" 
          :disabled="rebooting || shuttingDown"
          class="group relative overflow-hidden p-6 rounded-2xl bg-gradient-to-br from-blue-500/20 to-cyan-500/20 border border-blue-500/30 hover:border-blue-500/50 transition-all disabled:opacity-50"
        >
          <div class="absolute inset-0 bg-gradient-to-r from-blue-500/0 via-blue-500/10 to-blue-500/0 translate-x-[-100%] group-hover:translate-x-[100%] transition-transform duration-1000"></div>
          <div class="relative flex items-center space-x-4">
            <div class="w-16 h-16 rounded-2xl bg-gradient-to-br from-blue-500 to-cyan-400 flex items-center justify-center shadow-lg shadow-blue-500/30">
              <i :class="rebooting ? 'fas fa-spinner animate-spin' : 'fas fa-sync-alt'" class="text-white text-2xl"></i>
            </div>
            <div class="text-left">
              <p class="text-slate-900 dark:text-white font-bold text-xl">{{ rebooting ? '重启中...' : '设备重启' }}</p>
              <p class="text-slate-500 dark:text-white/50 text-sm mt-1">重新启动设备系统</p>
            </div>
          </div>
        </button>

        <!-- 关机按钮 -->
        <button 
          @click="handleShutdown" 
          :disabled="rebooting || shuttingDown"
          class="group relative overflow-hidden p-6 rounded-2xl bg-gradient-to-br from-red-500/20 to-orange-500/20 border border-red-500/30 hover:border-red-500/50 transition-all disabled:opacity-50"
        >
          <div class="absolute inset-0 bg-gradient-to-r from-red-500/0 via-red-500/10 to-red-500/0 translate-x-[-100%] group-hover:translate-x-[100%] transition-transform duration-1000"></div>
          <div class="relative flex items-center space-x-4">
            <div class="w-16 h-16 rounded-2xl bg-gradient-to-br from-red-500 to-orange-400 flex items-center justify-center shadow-lg shadow-red-500/30">
              <i :class="shuttingDown ? 'fas fa-spinner animate-spin' : 'fas fa-power-off'" class="text-white text-2xl"></i>
            </div>
            <div class="text-left">
              <p class="text-slate-900 dark:text-white font-bold text-xl">{{ shuttingDown ? '关机中...' : '设备关机' }}</p>
              <p class="text-slate-500 dark:text-white/50 text-sm mt-1">安全关闭设备电源</p>
            </div>
          </div>
        </button>
      </div>

      <!-- 恢复出厂设置 -->
      <div class="mt-6 pt-6 border-t border-slate-200 dark:border-white/10">
        <div class="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-4">
          <div class="flex items-center space-x-3 sm:space-x-4">
            <div class="w-12 h-12 sm:w-16 sm:h-16 rounded-xl sm:rounded-2xl bg-gradient-to-br from-rose-500 to-pink-500 flex items-center justify-center shadow-lg shadow-rose-500/30 flex-shrink-0">
              <i class="fas fa-rotate-left text-white text-lg sm:text-2xl"></i>
            </div>
            <div>
              <p class="text-slate-900 dark:text-white font-bold text-base sm:text-xl">恢复出厂设置</p>
              <p class="text-slate-500 dark:text-white/50 text-xs sm:text-sm mt-0.5 sm:mt-1">清除所有配置，恢复默认状态</p>
            </div>
          </div>
          <button 
            @click="handleFactoryReset" 
            :disabled="rebooting || shuttingDown || resetting"
            class="w-full sm:w-auto px-4 sm:px-6 py-2.5 sm:py-3 bg-gradient-to-r from-rose-500 to-pink-500 text-white font-medium rounded-xl hover:shadow-lg hover:shadow-rose-500/30 transition-all disabled:opacity-50 text-sm sm:text-base"
          >
            <i :class="resetting ? 'fas fa-spinner animate-spin' : 'fas fa-rotate-left'" class="mr-2"></i>
            {{ resetting ? '重置中...' : '恢复出厂' }}
          </button>
        </div>
      </div>

      <div class="mt-4 p-3 bg-yellow-500/10 border border-yellow-500/20 rounded-xl">
        <p class="text-yellow-600 dark:text-yellow-400 text-sm flex items-center">
          <i class="fas fa-exclamation-triangle mr-2"></i>
          操作将立即执行，请确保重要数据已保存
        </p>
      </div>
    </div>

    <!-- 定时重启 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20 hover:shadow-xl hover:shadow-slate-300/50 dark:hover:shadow-black/30 transition-all duration-300">
      <h3 class="text-slate-900 dark:text-white font-semibold mb-6 flex items-center">
        <i class="fas fa-clock text-amber-500 dark:text-amber-400 mr-2"></i>
        定时重启
      </h3>

      <!-- 系统时间 -->
      <div class="flex items-center justify-between p-4 bg-slate-50 dark:bg-white/5 rounded-xl mb-6">
        <div class="flex items-center space-x-3">
          <div class="w-10 h-10 rounded-lg bg-amber-500/20 flex items-center justify-center">
            <i class="fas fa-calendar-alt text-amber-500 dark:text-amber-400"></i>
          </div>
          <div>
            <p class="text-slate-500 dark:text-white/50 text-xs">系统时间</p>
            <p class="text-slate-900 dark:text-white font-medium">{{ currentTime || '加载中...' }}</p>
          </div>
        </div>
        <button 
          @click="handleSyncTime" 
          :disabled="syncingTime"
          class="px-4 py-2 bg-gradient-to-r from-blue-500 to-cyan-400 text-white text-sm font-medium rounded-lg hover:shadow-lg hover:shadow-blue-500/30 transition-all disabled:opacity-50 flex items-center"
        >
          <i :class="syncingTime ? 'fas fa-spinner fa-spin' : 'fas fa-sync-alt'" class="mr-2"></i>
          {{ syncingTime ? '同步中...' : 'NTP同步' }}
        </button>
      </div>

      <!-- 启用开关 -->
      <div class="flex items-center justify-between p-4 bg-slate-50 dark:bg-white/5 rounded-xl mb-6">
        <div class="flex items-center space-x-3">
          <div class="w-10 h-10 rounded-lg bg-green-500/20 flex items-center justify-center">
            <i class="fas fa-toggle-on text-green-600 dark:text-green-400"></i>
          </div>
          <div>
            <p class="text-slate-900 dark:text-white font-medium">启用定时重启</p>
            <p class="text-slate-500 dark:text-white/50 text-sm">按计划自动重启设备</p>
          </div>
        </div>
        <label class="relative cursor-pointer">
          <input type="checkbox" v-model="rebootEnabled" class="sr-only peer">
          <div class="w-14 h-7 bg-slate-200 dark:bg-white/20 rounded-full peer peer-checked:bg-green-500 transition-colors"></div>
          <div class="absolute top-0.5 left-0.5 w-6 h-6 bg-white rounded-full shadow transition-transform peer-checked:translate-x-7"></div>
        </label>
      </div>

      <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
        <!-- 重启时间 -->
        <div>
          <label class="block text-slate-600 dark:text-white/60 text-sm mb-2">重启时间</label>
          <input 
            type="time" 
            v-model="rebootTime" 
            :disabled="!rebootEnabled"
            class="w-full px-4 py-3 bg-slate-50 dark:bg-white/10 border border-slate-200 dark:border-white/20 rounded-xl text-slate-900 dark:text-white focus:border-amber-400 focus:ring-2 focus:ring-amber-400/20 transition-all disabled:opacity-50"
          >
        </div>

        <!-- 重启日期 -->
        <div>
          <label class="block text-slate-600 dark:text-white/60 text-sm mb-2">重启日期</label>
          <div class="flex flex-wrap gap-2">
            <button 
              @click="toggleAllDays"
              :disabled="!rebootEnabled"
              class="px-3 py-2 rounded-lg text-sm transition-all disabled:opacity-50"
              :class="selectedDays.length === 7 ? 'bg-amber-500 text-white' : 'bg-slate-100 dark:bg-white/10 text-slate-600 dark:text-white/60 hover:bg-slate-200 dark:hover:bg-white/20'"
            >
              全选
            </button>
            <button
              v-for="day in weekDays"
              :key="day.value"
              @click="selectedDays.includes(day.value) ? selectedDays = selectedDays.filter(d => d !== day.value) : selectedDays.push(day.value)"
              :disabled="!rebootEnabled"
              class="w-10 h-10 rounded-lg text-sm transition-all disabled:opacity-50"
              :class="selectedDays.includes(day.value) ? 'bg-amber-500 text-white' : 'bg-slate-100 dark:bg-white/10 text-slate-600 dark:text-white/60 hover:bg-slate-200 dark:hover:bg-white/20'"
            >
              {{ day.short }}
            </button>
          </div>
        </div>
      </div>

      <!-- 保存按钮 -->
      <button 
        @click="saveRebootConfig" 
        :disabled="savingReboot"
        class="w-full mt-6 py-3 bg-gradient-to-r from-amber-500 to-orange-400 text-white font-medium rounded-xl hover:shadow-lg hover:shadow-amber-500/30 transition-all disabled:opacity-50"
      >
        <i :class="savingReboot ? 'fas fa-spinner animate-spin' : 'fas fa-save'" class="mr-2"></i>
        {{ savingReboot ? '保存中...' : '保存设置' }}
      </button>
    </div>
  </div>
</template>
