<script setup>
import { ref, onMounted, computed } from 'vue'
import { getBands, lockBands as apiLockBands, unlockBands as apiUnlockBands, getCells, lockCell as apiLockCell, unlockCell as apiUnlockCell } from '../composables/useApi'
import { useToast } from '../composables/useToast'
import { useConfirm } from '../composables/useConfirm'

const { success, error: showError } = useToast()
const { confirm } = useConfirm()

const bands = ref({ '4G_TDD': [], '4G_FDD': [], '5G': [] })
const cells = ref([])
const loading = ref(false)
const lockingBands = ref(false)
const lockingCell = ref(false)
const refreshing = ref(false)

const servingCell = computed(() => cells.value.find(c => c.isServing))
const neighborCells = computed(() => cells.value.filter(c => !c.isServing))
const selectedBandsCount = computed(() => {
  let count = 0
  Object.values(bands.value).forEach(group => group.forEach(band => { if (band.locked) count++ }))
  return count
})

async function fetchBands() {
  loading.value = true
  try { bands.value = await getBands() }
  catch (err) { showError('获取频段失败: ' + err.message) }
  finally { loading.value = false }
}

async function handleLockBands() {
  const selectedBands = []
  Object.values(bands.value).forEach(group => group.forEach(band => { if (band.locked) selectedBands.push(band.name) }))
  if (selectedBands.length === 0) { showError('请至少选择一个频段'); return }
  lockingBands.value = true
  try { await apiLockBands(selectedBands); success(`已锁定 ${selectedBands.length} 个频段`); await fetchBands() }
  catch (err) { showError('锁定失败: ' + err.message) }
  finally { lockingBands.value = false }
}

async function handleUnlockAllBands() {
  if (!await confirm({ title: '解锁频段', message: '确认解锁所有频段？' })) return
  lockingBands.value = true
  try { await apiUnlockBands(); Object.values(bands.value).forEach(g => g.forEach(b => b.locked = false)); success('已解锁所有频段'); await fetchBands() }
  catch (err) { showError('解锁失败: ' + err.message) }
  finally { lockingBands.value = false }
}

async function fetchCells() {
  refreshing.value = true
  try { const res = await getCells(); cells.value = res.Code === 0 && res.Data ? res.Data : [] }
  catch (err) { showError('获取小区失败: ' + err.message) }
  finally { refreshing.value = false }
}

async function handleLockCell(cell) {
  if (!await confirm({ title: '锁定小区', message: `确认锁定小区 PCI=${cell.pci}？` })) return
  lockingCell.value = true
  try { await apiLockCell(cell.rat, cell.arfcn, cell.pci); success('小区锁定成功'); await fetchCells() }
  catch (err) { showError('锁定失败: ' + err.message) }
  finally { lockingCell.value = false }
}

async function handleUnlockCell() {
  if (!await confirm({ title: '解锁小区', message: '确认解锁小区？' })) return
  lockingCell.value = true
  try { await apiUnlockCell(); success('小区解锁成功'); await fetchCells() }
  catch (err) { showError('解锁失败: ' + err.message) }
  finally { lockingCell.value = false }
}

function getSignalColor(rsrp) {
  if (rsrp >= -80) return 'text-green-500'
  if (rsrp >= -90) return 'text-yellow-500'
  if (rsrp >= -100) return 'text-orange-500'
  return 'text-red-500'
}

onMounted(() => { fetchBands(); fetchCells() })
</script>

<template>
  <div class="space-y-4">
    <!-- 频段锁定 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-4 shadow-lg shadow-slate-200/40 dark:shadow-black/20 hover:shadow-xl hover:shadow-slate-300/50 dark:hover:shadow-black/30 transition-all duration-300">
      <!-- 标题栏 -->
      <div class="flex items-center justify-between mb-4">
        <div class="flex items-center space-x-3">
          <div class="w-10 h-10 rounded-xl bg-gradient-to-br from-purple-500 to-pink-500 flex items-center justify-center shadow-lg shadow-purple-500/30">
            <i class="fas fa-broadcast-tower text-white"></i>
          </div>
          <div>
            <h3 class="text-slate-900 dark:text-white font-semibold text-sm">频段锁定</h3>
            <p class="text-slate-500 dark:text-white/50 text-xs">已选 {{ selectedBandsCount }} 个频段</p>
          </div>
        </div>
        <div class="flex items-center gap-2">
          <button @click="handleUnlockAllBands" :disabled="lockingBands"
            class="px-3 py-1.5 bg-slate-100 dark:bg-white/10 text-slate-600 dark:text-white/70 text-xs rounded-lg hover:bg-slate-200 dark:hover:bg-white/20 transition-all disabled:opacity-50">
            <i class="fas fa-unlock mr-1"></i>解锁
          </button>
          <button @click="handleLockBands" :disabled="lockingBands || selectedBandsCount === 0"
            class="px-3 py-1.5 bg-gradient-to-r from-purple-500 to-pink-500 text-white text-xs rounded-lg hover:shadow-lg hover:shadow-purple-500/30 transition-all disabled:opacity-50">
            <i :class="lockingBands ? 'fas fa-spinner animate-spin' : 'fas fa-lock'" class="mr-1"></i>
            {{ lockingBands ? '锁定中' : '锁定' }}
          </button>
        </div>
      </div>

      <!-- 4G TDD -->
      <div class="mb-4">
        <div class="flex items-center space-x-2 mb-2">
          <div class="w-2 h-2 rounded-full bg-blue-500"></div>
          <span class="text-slate-700 dark:text-white/80 text-xs font-medium">4G TDD</span>
          <div class="flex-1 h-px bg-slate-200 dark:bg-white/10"></div>
          <span class="text-slate-400 dark:text-white/40 text-xs">{{ bands['4G_TDD'].filter(b => b.locked).length }}/{{ bands['4G_TDD'].length }}</span>
        </div>
        <div class="grid grid-cols-4 sm:grid-cols-5 md:grid-cols-6 gap-2">
          <button v-for="band in bands['4G_TDD']" :key="band.name" @click="band.locked = !band.locked"
            class="relative p-2 rounded-xl border-2 transition-all hover:scale-105"
            :class="band.locked ? 'bg-blue-50 dark:bg-blue-500/20 border-blue-500 shadow-md shadow-blue-500/20' : 'bg-slate-50 dark:bg-white/5 border-slate-200 dark:border-white/10 hover:border-blue-300 dark:hover:border-blue-500/50'">
            <div v-if="band.locked" class="absolute -top-1 -right-1 w-4 h-4 rounded-full bg-blue-500 flex items-center justify-center">
              <i class="fas fa-check text-white text-[8px]"></i>
            </div>
            <p class="text-slate-900 dark:text-white font-semibold text-sm text-center">{{ band.label }}</p>
          </button>
        </div>
      </div>

      <!-- 4G FDD -->
      <div class="mb-4">
        <div class="flex items-center space-x-2 mb-2">
          <div class="w-2 h-2 rounded-full bg-green-500"></div>
          <span class="text-slate-700 dark:text-white/80 text-xs font-medium">4G FDD</span>
          <div class="flex-1 h-px bg-slate-200 dark:bg-white/10"></div>
          <span class="text-slate-400 dark:text-white/40 text-xs">{{ bands['4G_FDD'].filter(b => b.locked).length }}/{{ bands['4G_FDD'].length }}</span>
        </div>
        <div class="grid grid-cols-3 sm:grid-cols-4 md:grid-cols-5 gap-2">
          <button v-for="band in bands['4G_FDD']" :key="band.name" @click="band.locked = !band.locked"
            class="relative p-2 rounded-xl border-2 transition-all hover:scale-105"
            :class="band.locked ? 'bg-green-50 dark:bg-green-500/20 border-green-500 shadow-md shadow-green-500/20' : 'bg-slate-50 dark:bg-white/5 border-slate-200 dark:border-white/10 hover:border-green-300 dark:hover:border-green-500/50'">
            <div v-if="band.locked" class="absolute -top-1 -right-1 w-4 h-4 rounded-full bg-green-500 flex items-center justify-center">
              <i class="fas fa-check text-white text-[8px]"></i>
            </div>
            <p class="text-slate-900 dark:text-white font-semibold text-sm text-center">{{ band.label }}</p>
          </button>
        </div>
      </div>

      <!-- 5G -->
      <div>
        <div class="flex items-center space-x-2 mb-2">
          <div class="w-2 h-2 rounded-full bg-purple-500"></div>
          <span class="text-slate-700 dark:text-white/80 text-xs font-medium">5G</span>
          <div class="flex-1 h-px bg-slate-200 dark:bg-white/10"></div>
          <span class="text-slate-400 dark:text-white/40 text-xs">{{ bands['5G'].filter(b => b.locked).length }}/{{ bands['5G'].length }}</span>
        </div>
        <div class="grid grid-cols-4 sm:grid-cols-6 md:grid-cols-8 gap-2">
          <button v-for="band in bands['5G']" :key="band.name" @click="band.locked = !band.locked"
            class="relative p-2 rounded-xl border-2 transition-all hover:scale-105"
            :class="band.locked ? 'bg-purple-50 dark:bg-purple-500/20 border-purple-500 shadow-md shadow-purple-500/20' : 'bg-slate-50 dark:bg-white/5 border-slate-200 dark:border-white/10 hover:border-purple-300 dark:hover:border-purple-500/50'">
            <div v-if="band.locked" class="absolute -top-1 -right-1 w-4 h-4 rounded-full bg-purple-500 flex items-center justify-center">
              <i class="fas fa-check text-white text-[8px]"></i>
            </div>
            <p class="text-slate-900 dark:text-white font-semibold text-sm text-center">{{ band.label }}</p>
          </button>
        </div>
      </div>
    </div>


    <!-- 小区锁定 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-4 shadow-lg shadow-slate-200/40 dark:shadow-black/20 hover:shadow-xl hover:shadow-slate-300/50 dark:hover:shadow-black/30 transition-all duration-300">
      <!-- 标题栏 -->
      <div class="flex items-center justify-between mb-4">
        <div class="flex items-center space-x-3">
          <div class="w-10 h-10 rounded-xl bg-gradient-to-br from-cyan-500 to-blue-500 flex items-center justify-center shadow-lg shadow-cyan-500/30">
            <i class="fas fa-tower-cell text-white"></i>
          </div>
          <div>
            <h3 class="text-slate-900 dark:text-white font-semibold text-sm">小区锁定</h3>
            <p class="text-slate-500 dark:text-white/50 text-xs">发现 {{ cells.length }} 个小区</p>
          </div>
        </div>
        <div class="flex items-center gap-2">
          <button @click="fetchCells" :disabled="refreshing"
            class="px-3 py-1.5 bg-slate-100 dark:bg-white/10 text-slate-600 dark:text-white/70 text-xs rounded-lg hover:bg-slate-200 dark:hover:bg-white/20 transition-all disabled:opacity-50">
            <i :class="refreshing ? 'fas fa-spinner animate-spin' : 'fas fa-sync-alt'" class="mr-1"></i>
            {{ refreshing ? '扫描中' : '扫描' }}
          </button>
          <button @click="handleUnlockCell" :disabled="lockingCell"
            class="px-3 py-1.5 bg-gradient-to-r from-cyan-500 to-blue-500 text-white text-xs rounded-lg hover:shadow-lg hover:shadow-cyan-500/30 transition-all disabled:opacity-50">
            <i class="fas fa-unlock mr-1"></i>解锁
          </button>
        </div>
      </div>

      <!-- 主小区 -->
      <div v-if="servingCell" class="mb-4">
        <div class="flex items-center space-x-2 mb-2">
          <div class="w-2 h-2 rounded-full bg-green-500 animate-pulse"></div>
          <span class="text-slate-700 dark:text-white/80 text-xs font-medium">主小区</span>
          <span class="px-2 py-0.5 bg-green-100 dark:bg-green-500/20 text-green-600 dark:text-green-400 text-[10px] rounded-full">已连接</span>
        </div>
        <div class="p-3 bg-green-50 dark:bg-green-500/10 rounded-xl border border-green-200 dark:border-green-500/30">
          <div class="grid grid-cols-4 sm:grid-cols-7 gap-3">
            <div class="text-center">
              <p class="text-slate-500 dark:text-white/50 text-[10px]">制式</p>
              <p class="text-slate-900 dark:text-white font-semibold text-sm">{{ servingCell.rat }}</p>
            </div>
            <div class="text-center">
              <p class="text-slate-500 dark:text-white/50 text-[10px]">频段</p>
              <p class="text-purple-600 dark:text-purple-400 font-semibold text-sm">{{ servingCell.band }}</p>
            </div>
            <div class="text-center hidden sm:block">
              <p class="text-slate-500 dark:text-white/50 text-[10px]">频点</p>
              <p class="text-slate-900 dark:text-white font-mono text-xs">{{ servingCell.arfcn }}</p>
            </div>
            <div class="text-center">
              <p class="text-slate-500 dark:text-white/50 text-[10px]">PCI</p>
              <p class="text-slate-900 dark:text-white font-semibold text-sm">{{ servingCell.pci }}</p>
            </div>
            <div class="text-center">
              <p class="text-slate-500 dark:text-white/50 text-[10px]">RSRP</p>
              <p :class="getSignalColor(servingCell.rsrp)" class="font-semibold text-sm">{{ servingCell.rsrp?.toFixed(0) }}</p>
            </div>
            <div class="text-center hidden sm:block">
              <p class="text-slate-500 dark:text-white/50 text-[10px]">RSRQ</p>
              <p class="text-cyan-600 dark:text-cyan-400 font-semibold text-sm">{{ servingCell.rsrq?.toFixed(1) }}</p>
            </div>
            <div class="text-center hidden sm:block">
              <p class="text-slate-500 dark:text-white/50 text-[10px]">SINR</p>
              <p class="text-indigo-600 dark:text-indigo-400 font-semibold text-sm">{{ servingCell.sinr?.toFixed(1) }}</p>
            </div>
          </div>
        </div>
      </div>


      <!-- 邻小区 -->
      <div v-if="neighborCells.length > 0">
        <div class="flex items-center space-x-2 mb-2">
          <div class="w-2 h-2 rounded-full bg-cyan-500"></div>
          <span class="text-slate-700 dark:text-white/80 text-xs font-medium">邻小区</span>
          <div class="flex-1 h-px bg-slate-200 dark:bg-white/10"></div>
          <span class="text-slate-400 dark:text-white/40 text-xs">{{ neighborCells.length }}个</span>
        </div>
        <div class="space-y-2">
          <div v-for="(cell, index) in neighborCells" :key="index"
            class="group flex items-center justify-between p-3 bg-slate-50 dark:bg-white/5 rounded-xl border border-slate-200 dark:border-white/10 hover:border-cyan-300 dark:hover:border-cyan-500/50 transition-all">
            <div class="grid grid-cols-4 sm:grid-cols-6 gap-3 flex-1">
              <div class="text-center">
                <p class="text-slate-500 dark:text-white/50 text-[10px]">制式</p>
                <p class="text-slate-900 dark:text-white text-xs">{{ cell.rat }}</p>
              </div>
              <div class="text-center">
                <p class="text-slate-500 dark:text-white/50 text-[10px]">频段</p>
                <p class="text-purple-600 dark:text-purple-400 text-xs">{{ cell.band }}</p>
              </div>
              <div class="text-center hidden sm:block">
                <p class="text-slate-500 dark:text-white/50 text-[10px]">频点</p>
                <p class="text-slate-900 dark:text-white font-mono text-xs">{{ cell.arfcn }}</p>
              </div>
              <div class="text-center">
                <p class="text-slate-500 dark:text-white/50 text-[10px]">PCI</p>
                <p class="text-slate-900 dark:text-white text-xs">{{ cell.pci }}</p>
              </div>
              <div class="text-center">
                <p class="text-slate-500 dark:text-white/50 text-[10px]">RSRP</p>
                <p :class="getSignalColor(cell.rsrp)" class="text-xs">{{ cell.rsrp?.toFixed(0) }}</p>
              </div>
              <div class="text-center hidden sm:block">
                <p class="text-slate-500 dark:text-white/50 text-[10px]">SINR</p>
                <p class="text-indigo-600 dark:text-indigo-400 text-xs">{{ cell.sinr?.toFixed(1) }}</p>
              </div>
            </div>
            <button @click="handleLockCell(cell)" :disabled="lockingCell"
              class="ml-2 px-2 py-1.5 bg-cyan-500 text-white text-xs rounded-lg hover:bg-cyan-600 transition-all disabled:opacity-50 opacity-0 group-hover:opacity-100">
              <i class="fas fa-lock"></i>
            </button>
          </div>
        </div>
      </div>

      <!-- 空状态 -->
      <div v-if="!refreshing && cells.length === 0" class="text-center py-8">
        <div class="w-16 h-16 mx-auto mb-3 bg-slate-100 dark:bg-white/10 rounded-full flex items-center justify-center">
          <i class="fas fa-tower-cell text-slate-400 dark:text-white/40 text-2xl"></i>
        </div>
        <p class="text-slate-600 dark:text-white/60 text-sm mb-1">未发现小区信息</p>
        <p class="text-slate-400 dark:text-white/40 text-xs">点击"扫描"获取周边基站</p>
      </div>

      <!-- 扫描动画 -->
      <div v-if="refreshing && cells.length === 0" class="text-center py-8">
        <div class="relative w-16 h-16 mx-auto mb-3">
          <div class="w-16 h-16 bg-cyan-100 dark:bg-cyan-500/20 rounded-full flex items-center justify-center">
            <i class="fas fa-tower-cell text-cyan-500 text-2xl"></i>
          </div>
          <div class="absolute inset-0 border-2 border-cyan-400/30 border-t-cyan-400 rounded-full animate-spin"></div>
        </div>
        <p class="text-cyan-600 dark:text-cyan-400 text-sm">正在扫描小区...</p>
      </div>
    </div>
  </div>
</template>
