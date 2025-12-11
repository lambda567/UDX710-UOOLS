<script setup>
import { ref, onMounted, onUnmounted, computed } from 'vue'
import { getCells, lockCell as apiLockCell, unlockCell as apiUnlockCell } from '../composables/useApi'
import { useToast } from '../composables/useToast'
import { useConfirm } from '../composables/useConfirm'

const { success, error: showError } = useToast()
const { confirm } = useConfirm()

const cells = ref([])
const loading = ref(true)
const errorMsg = ref('')
const updateInterval = ref(null)
const lockingCell = ref(false)

const servingCell = computed(() => cells.value.find(cell => cell.isServing) || null)
const neighborCells = computed(() => cells.value.filter(cell => !cell.isServing))

async function fetchCells() {
  try {
    const res = await getCells()
    if (res.Code === 0 && res.Data) {
      cells.value = res.Data
      errorMsg.value = ''
    } else {
      errorMsg.value = res.Error || '获取小区信息失败'
    }
  } catch (err) {
    errorMsg.value = '网络请求失败'
  } finally {
    loading.value = false
  }
}

async function lockCell(cell) {
  if (!await confirm({ title: '锁定小区', message: `确认锁定到小区 PCI=${cell.pci} (频段${cell.band})？` })) return
  lockingCell.value = true
  try {
    const tech = cell.band.startsWith('N') ? 'NR' : 'LTE'
    const res = await apiLockCell(tech, cell.arfcn, cell.pci)
    if (res.Code === 0) {
      success(`已锁定到小区 PCI=${cell.pci}`)
      await fetchCells()
    } else {
      showError('锁定失败: ' + res.Error)
    }
  } catch (err) {
    showError('锁定失败: ' + err.message)
  } finally {
    lockingCell.value = false
  }
}

async function unlockCell() {
  if (!await confirm({ title: '解锁小区', message: '确认解锁小区锁定？' })) return
  lockingCell.value = true
  try {
    const res = await apiUnlockCell()
    if (res.Code === 0) {
      success('已解锁小区锁定')
      await fetchCells()
    } else {
      showError('解锁失败: ' + res.Error)
    }
  } catch (err) {
    showError('解锁失败: ' + err.message)
  } finally {
    lockingCell.value = false
  }
}

function getSignalColor(rsrp) {
  if (rsrp >= -80) return 'text-green-400'
  if (rsrp >= -90) return 'text-yellow-400'
  if (rsrp >= -100) return 'text-orange-400'
  return 'text-red-400'
}

function getQualityColor(rsrq) {
  if (rsrq >= -10) return 'text-green-400'
  if (rsrq >= -15) return 'text-yellow-400'
  if (rsrq >= -20) return 'text-orange-400'
  return 'text-red-400'
}

function getSinrColor(sinr) {
  if (sinr >= 20) return 'text-green-400'
  if (sinr >= 13) return 'text-yellow-400'
  if (sinr >= 0) return 'text-orange-400'
  return 'text-red-400'
}

function getBandColor(band) {
  if (band && band.startsWith('N')) return 'from-purple-500 to-pink-500'
  return 'from-blue-500 to-cyan-500'
}

function getSignalLevel(rsrp) {
  if (rsrp >= -80) return '优秀'
  if (rsrp >= -90) return '良好'
  if (rsrp >= -100) return '一般'
  return '较差'
}

onMounted(() => {
  fetchCells()
  updateInterval.value = setInterval(fetchCells, 5000)
})

onUnmounted(() => {
  if (updateInterval.value) clearInterval(updateInterval.value)
})
</script>

<template>
  <div class="space-y-6">
    <div class="text-center">
      <h1 class="text-3xl font-bold text-white mb-2">小区管理</h1>
      <p class="text-white/60">查看主小区和邻小区信息，支持小区锁定功能</p>
    </div>

    <div class="flex justify-center space-x-4">
      <button @click="fetchCells" :disabled="loading"
        class="px-6 py-3 bg-gradient-to-r from-cyan-500 to-blue-500 text-white font-medium rounded-xl hover:shadow-lg transition-all disabled:opacity-50">
        <i :class="loading ? 'fas fa-spinner animate-spin' : 'fas fa-sync-alt'" class="mr-2"></i>
        {{ loading ? '扫描中...' : '刷新扫描' }}
      </button>
      <button @click="unlockCell" :disabled="lockingCell"
        class="px-6 py-3 bg-gradient-to-r from-red-500 to-pink-500 text-white font-medium rounded-xl hover:shadow-lg transition-all disabled:opacity-50">
        <i :class="lockingCell ? 'fas fa-spinner animate-spin' : 'fas fa-unlock'" class="mr-2"></i>
        解锁小区
      </button>
    </div>

    <div v-if="loading && cells.length === 0" class="flex items-center justify-center py-12">
      <i class="fas fa-spinner animate-spin text-3xl text-white/50"></i>
    </div>

    <div v-else-if="error" class="text-center py-12">
      <i class="fas fa-exclamation-triangle text-red-400 text-3xl mb-4"></i>
      <p class="text-red-400">{{ error }}</p>
      <button @click="fetchCells" class="mt-4 px-4 py-2 bg-red-500/20 text-red-400 rounded-lg">重试</button>
    </div>

    <template v-else>
      <div v-if="servingCell" class="rounded-2xl bg-gradient-to-br from-green-500/10 to-emerald-500/10 border border-green-500/20 p-6">
        <div class="flex items-center justify-between mb-4">
          <h2 class="text-xl font-semibold text-white flex items-center">
            <i class="fas fa-star text-yellow-400 mr-3"></i>主小区 (服务小区)
          </h2>
          <span class="text-green-400 font-semibold">{{ getSignalLevel(servingCell.rsrp) }}</span>
        </div>
        <div class="grid grid-cols-2 lg:grid-cols-4 gap-4 mb-4">
          <div class="p-3 bg-black/20 rounded-xl text-center">
            <p class="text-white/50 text-xs mb-1">频段</p>
            <p class="text-white font-bold text-lg">{{ servingCell.band }}</p>
          </div>
          <div class="p-3 bg-black/20 rounded-xl text-center">
            <p class="text-white/50 text-xs mb-1">PCI</p>
            <p class="text-white font-bold text-lg">{{ servingCell.pci }}</p>
          </div>
          <div class="p-3 bg-black/20 rounded-xl text-center">
            <p class="text-white/50 text-xs mb-1">ARFCN</p>
            <p class="text-white font-bold text-lg">{{ servingCell.arfcn }}</p>
          </div>
          <div class="p-3 bg-black/20 rounded-xl text-center">
            <p class="text-white/50 text-xs mb-1">RAT</p>
            <p class="text-white font-bold text-lg">{{ servingCell.rat }}</p>
          </div>
        </div>
        <div class="grid grid-cols-3 gap-4">
          <div class="flex items-center justify-between p-3 bg-black/20 rounded-xl">
            <span class="text-white/60 text-sm">RSRP</span>
            <span class="font-bold" :class="getSignalColor(servingCell.rsrp)">{{ servingCell.rsrp?.toFixed(1) }} dBm</span>
          </div>
          <div class="flex items-center justify-between p-3 bg-black/20 rounded-xl">
            <span class="text-white/60 text-sm">RSRQ</span>
            <span class="font-bold" :class="getQualityColor(servingCell.rsrq)">{{ servingCell.rsrq?.toFixed(1) }} dB</span>
          </div>
          <div class="flex items-center justify-between p-3 bg-black/20 rounded-xl">
            <span class="text-white/60 text-sm">SINR</span>
            <span class="font-bold" :class="getSinrColor(servingCell.sinr)">{{ servingCell.sinr?.toFixed(1) }} dB</span>
          </div>
        </div>
      </div>

      <div v-if="neighborCells.length > 0" class="rounded-2xl bg-white/5 backdrop-blur border border-white/10 p-6">
        <h2 class="text-xl font-semibold text-white flex items-center mb-4">
          <i class="fas fa-broadcast-tower text-cyan-400 mr-3"></i>邻小区 ({{ neighborCells.length }}个)
        </h2>
        <div class="space-y-3">
          <div v-for="(cell, index) in neighborCells" :key="index" class="p-4 bg-black/20 rounded-xl hover:bg-black/30 transition-colors">
            <div class="flex items-center justify-between">
              <div class="flex items-center space-x-4">
                <div class="w-12 h-12 rounded-xl bg-gradient-to-br flex items-center justify-center" :class="getBandColor(cell.band)">
                  <span class="text-white font-bold text-sm">{{ cell.band }}</span>
                </div>
                <div>
                  <p class="text-white font-semibold">PCI {{ cell.pci }}</p>
                  <p class="text-white/50 text-sm">ARFCN {{ cell.arfcn }}</p>
                </div>
              </div>
              <div class="flex items-center space-x-6">
                <div class="text-center">
                  <p class="text-white/50 text-xs">RSRP</p>
                  <p class="font-semibold" :class="getSignalColor(cell.rsrp)">{{ cell.rsrp?.toFixed(1) }}</p>
                </div>
                <div class="text-center">
                  <p class="text-white/50 text-xs">RSRQ</p>
                  <p class="font-semibold" :class="getQualityColor(cell.rsrq)">{{ cell.rsrq?.toFixed(1) }}</p>
                </div>
                <button @click="lockCell(cell)" :disabled="lockingCell"
                  class="px-4 py-2 bg-gradient-to-r from-purple-500 to-pink-500 text-white text-sm font-medium rounded-lg hover:shadow-lg transition-all disabled:opacity-50">
                  <i :class="lockingCell ? 'fas fa-spinner animate-spin' : 'fas fa-lock'" class="mr-1"></i>锁定
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>

      <div v-if="!servingCell && neighborCells.length === 0" class="text-center py-12">
        <i class="fas fa-satellite-dish text-white/30 text-4xl mb-4"></i>
        <p class="text-white/50">未检测到小区信息</p>
      </div>
    </template>
  </div>
</template>
