<script setup>
import { ref, computed, onMounted } from 'vue'
import { useToast } from '../composables/useToast'
import { useConfirm } from '../composables/useConfirm'
import { useApi } from '../composables/useApi'

const api = useApi()

const { success, error } = useToast()
const { confirm } = useConfirm()

// 更新模式: 'url' | 'file'
const updateMode = ref('url')
const updateUrl = ref('')
const selectedFile = ref(null)
const fileName = ref('')
const fileSize = ref(0)
const isDragging = ref(false)

// 更新状态
const checking = ref(false)
const uploading = ref(false)
const installing = ref(false)
const uploadProgress = ref(0)
const installProgress = ref(0)
const installStage = ref('')
const currentVersion = ref('1.0.0')
const latestVersion = ref('')
const updateAvailable = ref(false)
const updateLog = ref([])

// 计算属性
const canUpdate = computed(() => {
  if (updateMode.value === 'file') return selectedFile.value !== null
  return updateUrl.value.trim() !== '' && (updateUrl.value.startsWith('http://') || updateUrl.value.startsWith('https://'))
})

const formattedFileSize = computed(() => {
  if (fileSize.value < 1024) return fileSize.value + ' B'
  if (fileSize.value < 1024 * 1024) return (fileSize.value / 1024).toFixed(1) + ' KB'
  return (fileSize.value / (1024 * 1024)).toFixed(2) + ' MB'
})

// 文件选择
function handleFileSelect(event) {
  const file = event.target.files?.[0]
  if (file) validateAndSetFile(file)
}

function handleDrop(event) {
  isDragging.value = false
  const file = event.dataTransfer?.files?.[0]
  if (file) validateAndSetFile(file)
}

function validateAndSetFile(file) {
  if (!file.name.endsWith('.zip')) {
    error('请选择ZIP格式的更新包')
    return
  }
  if (file.size > 100 * 1024 * 1024) {
    error('文件大小不能超过100MB')
    return
  }
  selectedFile.value = file
  fileName.value = file.name
  fileSize.value = file.size
  success('文件已选择: ' + file.name)
}

function clearFile() {
  selectedFile.value = null
  fileName.value = ''
  fileSize.value = 0
}


// 获取当前版本
async function fetchCurrentVersion() {
  try {
    const res = await api.get('/api/update/version')
    if (res.version) currentVersion.value = res.version
  } catch (e) {
    console.error('获取版本失败:', e)
  }
}

// 检查更新
async function checkUpdate() {
  checking.value = true
  updateLog.value = []
  addLog('正在连接更新服务器...')
  
  try {
    const res = await api.get('/api/update/check')
    addLog('正在检查最新版本...')
    
    if (res.has_update) {
      latestVersion.value = res.latest_version
      updateAvailable.value = true
      updateUrl.value = res.url || ''
      addLog(`发现新版本: v${res.latest_version}`)
      if (res.changelog) addLog(`更新内容: ${res.changelog}`)
      success('发现新版本 v' + res.latest_version)
    } else {
      latestVersion.value = res.current_version
      updateAvailable.value = false
      addLog('当前已是最新版本')
      success('当前已是最新版本')
    }
  } catch (e) {
    addLog('检查更新失败: ' + (e.message || '网络错误'))
    error('检查更新失败')
  } finally {
    checking.value = false
  }
}

// 开始更新
async function startUpdate() {
  if (!canUpdate.value) return
  
  const confirmed = await confirm({
    title: '系统更新',
    message: updateMode.value === 'file' 
      ? `确认使用 ${fileName.value} 进行更新？更新过程中请勿断电。`
      : '确认从远程链接下载并安装更新？更新过程中请勿断电。',
    danger: true
  })
  if (!confirmed) return

  uploading.value = true
  uploadProgress.value = 0
  updateLog.value = []
  
  try {
    // 步骤1: 上传或下载
    if (updateMode.value === 'file') {
      addLog('正在上传更新包...')
      const formData = new FormData()
      formData.append('file', selectedFile.value)
      
      const uploadRes = await fetch('/api/update/upload', {
        method: 'POST',
        body: formData
      })
      const uploadData = await uploadRes.json()
      
      if (uploadData.error) throw new Error(uploadData.error)
      uploadProgress.value = 100
      addLog('上传完成: ' + (uploadData.size ? Math.round(uploadData.size/1024) + 'KB' : ''))
    } else {
      addLog('正在下载更新包...')
      const downloadRes = await api.post('/api/update/download', { url: updateUrl.value })
      if (downloadRes.error) throw new Error(downloadRes.error)
      uploadProgress.value = 100
      addLog('下载完成')
    }
    
    await sleep(500)
    uploading.value = false
    installing.value = true
    installProgress.value = 0
    
    // 步骤2: 解压
    installStage.value = '解压更新包'
    addLog('正在解压更新包...')
    installProgress.value = 30
    
    const extractRes = await api.post('/api/update/extract')
    if (extractRes.error) throw new Error(extractRes.error)
    addLog('解压完成')
    installProgress.value = 50
    
    // 步骤3: 安装
    installStage.value = '执行安装脚本'
    addLog('正在执行安装脚本...')
    installProgress.value = 70
    
    const installRes = await api.post('/api/update/install')
    if (installRes.error) throw new Error(installRes.error)
    
    installProgress.value = 100
    addLog('✓ 安装完成！')
    if (installRes.output) addLog('输出: ' + installRes.output)
    addLog('设备即将重启...')
    
    success('系统更新成功，设备正在重启')
    
  } catch (e) {
    addLog('✗ 更新失败: ' + (e.message || '未知错误'))
    error('更新失败: ' + (e.message || '未知错误'))
  } finally {
    uploading.value = false
    installing.value = false
    clearFile()
  }
}

function addLog(message) {
  const time = new Date().toLocaleTimeString()
  updateLog.value.push({ time, message })
}

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms))
}
onMounted(() => {
  fetchCurrentVersion()
})
</script>


<template>
  <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-4 sm:p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20 hover:shadow-xl hover:shadow-slate-300/50 dark:hover:shadow-black/30 transition-all duration-300">
    <!-- 标题 -->
    <div class="flex items-center justify-between mb-6">
      <div class="flex items-center space-x-3">
        <div class="w-10 h-10 sm:w-12 sm:h-12 rounded-xl bg-gradient-to-br from-emerald-500 to-teal-500 flex items-center justify-center shadow-lg shadow-emerald-500/30">
          <i class="fas fa-cloud-download-alt text-white text-lg sm:text-xl"></i>
        </div>
        <div>
          <h3 class="text-slate-900 dark:text-white font-semibold text-sm sm:text-base">系统更新</h3>
          <p class="text-slate-500 dark:text-white/50 text-xs sm:text-sm">当前版本: v{{ currentVersion }}</p>
        </div>
      </div>
      <button @click="checkUpdate" :disabled="checking || uploading || installing"
        class="px-3 py-1.5 sm:px-4 sm:py-2 bg-emerald-500/10 hover:bg-emerald-500/20 text-emerald-600 dark:text-emerald-400 rounded-lg sm:rounded-xl text-xs sm:text-sm font-medium transition-all disabled:opacity-50">
        <i :class="checking ? 'fas fa-spinner animate-spin' : 'fas fa-sync-alt'" class="mr-1 sm:mr-2"></i>
        <span class="hidden sm:inline">{{ checking ? '检查中...' : '检查更新' }}</span>
        <span class="sm:hidden">{{ checking ? '检查中' : '检查' }}</span>
      </button>
    </div>

    <!-- 更新模式切换 -->
    <div class="flex p-1 bg-slate-100 dark:bg-white/10 rounded-xl mb-4 sm:mb-6">
      <button @click="updateMode = 'url'" :disabled="uploading || installing"
        class="flex-1 py-2 sm:py-2.5 px-3 sm:px-4 rounded-lg text-xs sm:text-sm font-medium transition-all disabled:opacity-50"
        :class="updateMode === 'url' ? 'bg-white dark:bg-white/20 text-emerald-600 dark:text-emerald-400 shadow-sm' : 'text-slate-500 dark:text-white/50 hover:text-slate-700 dark:hover:text-white/70'">
        <i class="fas fa-link mr-1 sm:mr-2"></i>远程链接
      </button>
      <button @click="updateMode = 'file'" :disabled="uploading || installing"
        class="flex-1 py-2 sm:py-2.5 px-3 sm:px-4 rounded-lg text-xs sm:text-sm font-medium transition-all disabled:opacity-50"
        :class="updateMode === 'file' ? 'bg-white dark:bg-white/20 text-emerald-600 dark:text-emerald-400 shadow-sm' : 'text-slate-500 dark:text-white/50 hover:text-slate-700 dark:hover:text-white/70'">
        <i class="fas fa-file-archive mr-1 sm:mr-2"></i>本地文件
      </button>
    </div>


    <!-- URL输入区域 / 文件上传区域 -->
    <Transition name="fade" mode="out-in">
      <!-- URL输入区域 -->
      <div v-if="updateMode === 'url'" key="url" class="mb-4 sm:mb-6">
        <div class="relative">
          <div class="absolute left-3 sm:left-4 top-1/2 -translate-y-1/2 w-8 h-8 sm:w-10 sm:h-10 rounded-lg bg-emerald-500/10 flex items-center justify-center">
            <i class="fas fa-link text-emerald-500 text-sm sm:text-base"></i>
          </div>
          <input type="url" v-model="updateUrl" :disabled="uploading || installing"
            placeholder="https://example.com/update.zip"
            class="w-full pl-14 sm:pl-16 pr-4 py-3 sm:py-4 bg-slate-50 dark:bg-white/10 border border-slate-200 dark:border-white/20 rounded-xl sm:rounded-2xl text-slate-900 dark:text-white placeholder-slate-400 dark:placeholder-white/30 focus:border-emerald-400 focus:ring-2 focus:ring-emerald-400/20 transition-all disabled:opacity-50 text-sm sm:text-base">
        </div>
        <p class="mt-2 text-slate-500 dark:text-white/50 text-xs sm:text-sm">
          <i class="fas fa-info-circle mr-1"></i>输入更新包的下载链接
        </p>
      </div>

      <!-- 文件上传区域 -->
      <div v-else key="file" class="mb-4 sm:mb-6">
        <div v-if="!selectedFile"
          @dragover.prevent="isDragging = true"
          @dragleave.prevent="isDragging = false"
          @drop.prevent="handleDrop"
          class="relative border-2 border-dashed rounded-xl sm:rounded-2xl p-6 sm:p-8 text-center transition-all duration-300 cursor-pointer"
          :class="isDragging ? 'border-emerald-500 bg-emerald-500/10' : 'border-slate-300 dark:border-white/20 hover:border-emerald-400 dark:hover:border-emerald-500/50 hover:bg-slate-50 dark:hover:bg-white/5'">
          <input type="file" accept=".zip" @change="handleFileSelect" class="absolute inset-0 opacity-0 cursor-pointer" :disabled="uploading || installing">
          <div class="w-12 h-12 sm:w-16 sm:h-16 mx-auto mb-3 sm:mb-4 rounded-xl sm:rounded-2xl bg-emerald-500/10 flex items-center justify-center">
            <i class="fas fa-cloud-upload-alt text-emerald-500 text-xl sm:text-2xl"></i>
          </div>
          <p class="text-slate-700 dark:text-white/80 font-medium text-sm sm:text-base mb-1">
            {{ isDragging ? '释放文件' : '点击或拖拽上传' }}
          </p>
          <p class="text-slate-500 dark:text-white/50 text-xs sm:text-sm">支持 ZIP 格式，最大 100MB</p>
        </div>
        
        <!-- 已选文件 -->
        <div v-else class="p-3 sm:p-4 bg-emerald-50 dark:bg-emerald-500/10 border border-emerald-200 dark:border-emerald-500/30 rounded-xl sm:rounded-2xl">
          <div class="flex items-center justify-between">
            <div class="flex items-center space-x-3 min-w-0 flex-1">
              <div class="w-10 h-10 sm:w-12 sm:h-12 rounded-lg sm:rounded-xl bg-emerald-500/20 flex items-center justify-center flex-shrink-0">
                <i class="fas fa-file-archive text-emerald-600 dark:text-emerald-400 text-lg sm:text-xl"></i>
              </div>
              <div class="min-w-0 flex-1">
                <p class="text-slate-900 dark:text-white font-medium text-sm sm:text-base truncate">{{ fileName }}</p>
                <p class="text-slate-500 dark:text-white/50 text-xs sm:text-sm">{{ formattedFileSize }}</p>
              </div>
            </div>
            <button @click="clearFile" :disabled="uploading || installing"
              class="w-8 h-8 sm:w-10 sm:h-10 rounded-lg bg-red-500/10 hover:bg-red-500/20 text-red-500 flex items-center justify-center transition-all disabled:opacity-50 flex-shrink-0 ml-2">
              <i class="fas fa-times text-sm sm:text-base"></i>
            </button>
          </div>
        </div>
      </div>
    </Transition>


    <!-- 进度显示 -->
    <Transition name="slide">
      <div v-if="uploading || installing" class="mb-4 sm:mb-6 p-4 sm:p-5 bg-slate-50 dark:bg-white/5 rounded-xl sm:rounded-2xl">
        <div class="flex items-center justify-between mb-3">
          <div class="flex items-center space-x-2 sm:space-x-3">
            <div class="w-8 h-8 sm:w-10 sm:h-10 rounded-lg bg-emerald-500/20 flex items-center justify-center">
              <i :class="[uploading ? 'fas fa-cloud-upload-alt' : 'fas fa-cog', 'text-emerald-500 text-sm sm:text-base', { 'animate-spin': installing }]"></i>
            </div>
            <div>
              <p class="text-slate-900 dark:text-white font-medium text-sm sm:text-base">
                {{ uploading ? (updateMode === 'file' ? '上传中' : '下载中') : installStage }}
              </p>
              <p class="text-slate-500 dark:text-white/50 text-xs sm:text-sm">
                {{ uploading ? '请勿关闭页面' : '正在安装更新' }}
              </p>
            </div>
          </div>
          <span class="text-emerald-600 dark:text-emerald-400 font-bold text-lg sm:text-xl">
            {{ Math.round(uploading ? uploadProgress : installProgress) }}%
          </span>
        </div>
        
        <!-- 进度条 -->
        <div class="h-2 sm:h-3 bg-slate-200 dark:bg-white/10 rounded-full overflow-hidden">
          <div class="h-full bg-gradient-to-r from-emerald-500 to-teal-400 rounded-full transition-all duration-300 ease-out"
            :style="{ width: (uploading ? uploadProgress : installProgress) + '%' }">
          </div>
        </div>
      </div>
    </Transition>

    <!-- 更新日志 -->
    <Transition name="slide">
      <div v-if="updateLog.length > 0" class="mb-4 sm:mb-6">
        <div class="flex items-center space-x-2 mb-2 sm:mb-3">
          <i class="fas fa-terminal text-slate-400 dark:text-white/40 text-xs sm:text-sm"></i>
          <span class="text-slate-500 dark:text-white/50 text-xs sm:text-sm font-medium">更新日志</span>
        </div>
        <div class="bg-slate-900 dark:bg-black/50 rounded-xl sm:rounded-2xl p-3 sm:p-4 max-h-32 sm:max-h-40 overflow-y-auto font-mono text-xs sm:text-sm">
          <div v-for="(log, index) in updateLog" :key="index" class="flex space-x-2 sm:space-x-3 mb-1 last:mb-0">
            <span class="text-slate-500 flex-shrink-0">{{ log.time }}</span>
            <span class="text-emerald-400">{{ log.message }}</span>
          </div>
        </div>
      </div>
    </Transition>

    <!-- 更新按钮 -->
    <button @click="startUpdate" :disabled="!canUpdate || uploading || installing"
      class="w-full py-3 sm:py-4 bg-gradient-to-r from-emerald-500 to-teal-500 text-white font-medium rounded-xl sm:rounded-2xl hover:shadow-lg hover:shadow-emerald-500/30 transition-all disabled:opacity-50 disabled:cursor-not-allowed text-sm sm:text-base">
      <i :class="uploading || installing ? 'fas fa-spinner animate-spin' : 'fas fa-rocket'" class="mr-2"></i>
      {{ uploading ? '上传中...' : installing ? '安装中...' : '开始更新' }}
    </button>

    <!-- 提示信息 -->
    <div class="mt-4 p-3 bg-amber-500/10 border border-amber-500/20 rounded-xl">
      <p class="text-amber-600 dark:text-amber-400 text-xs sm:text-sm flex items-start">
        <i class="fas fa-exclamation-triangle mr-2 mt-0.5 flex-shrink-0"></i>
        <span>更新过程中请勿断电或关闭页面，更新完成后建议重启设备</span>
      </p>
    </div>

  </div>
</template>

<style scoped>
.fade-enter-active, .fade-leave-active { transition: all 0.2s ease; }
.fade-enter-from, .fade-leave-to { opacity: 0; transform: translateY(-10px); }
.slide-enter-active, .slide-leave-active { transition: all 0.3s ease; }
.slide-enter-from, .slide-leave-to { opacity: 0; max-height: 0; overflow: hidden; }
.slide-enter-to, .slide-leave-from { max-height: 500px; }
.modal-enter-active, .modal-leave-active { transition: all 0.3s ease; }
.modal-enter-from, .modal-leave-to { opacity: 0; }
.modal-enter-from > div:last-child, .modal-leave-to > div:last-child { transform: scale(0.95); }
</style>
