<script setup>
import { ref } from 'vue'
import { executeAT } from '../composables/useApi'

const command = ref('')
const result = ref('')
const loading = ref(false)
const history = ref([])

// 常用AT命令（点击直接发送）
const quickCommands = [
  { name: 'IMEI', cmd: 'AT+CGSN' },
  { name: 'ICCID', cmd: 'AT+CCID' },
  { name: '签约速率', cmd: 'AT+CGEQOSRDP' },
]

// 模板命令（点击填充到输入框，不发送）
const templateCommands = [
  { name: '修改卡1 IMEI', cmd: 'AT+SPIMEI=0,"你的IMEI"', desc: '修改SIM卡1的IMEI' },
  { name: '修改卡2 IMEI', cmd: 'AT+SPIMEI=1,"你的IMEI"', desc: '修改SIM卡2的IMEI' },
]

async function sendCommand() {
  if (!command.value.trim() || loading.value) return
  loading.value = true
  result.value = ''
  const cmd = command.value.trim()
  
  try {
    const res = await executeAT(cmd)
    if (res.Code === 0) {
      result.value = res.Data || 'OK'
    } else {
      result.value = `错误: ${res.Error}\n${res.Data || ''}`
    }
    history.value.unshift({ cmd, result: result.value, time: new Date().toLocaleTimeString() })
    if (history.value.length > 20) history.value.pop()
  } catch (error) {
    result.value = `请求失败: ${error.message}`
  } finally {
    loading.value = false
  }
}

function useQuickCommand(cmd) {
  command.value = cmd
  sendCommand()
}

// 填充模板命令到输入框（不发送）
function fillTemplate(cmd) {
  command.value = cmd
}

function clearHistory() {
  history.value = []
  result.value = ''
}
</script>

<template>
  <div class="space-y-6">
    <!-- 输入区域 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20 hover:shadow-xl hover:shadow-slate-300/50 dark:hover:shadow-black/30 transition-all duration-300">
      <h3 class="text-slate-900 dark:text-white font-semibold mb-4 flex items-center">
        <i class="fas fa-terminal text-cyan-600 dark:text-cyan-400 mr-2"></i>
        AT命令调试
      </h3>
      <div class="flex space-x-3 mb-4">
        <input v-model="command" @keyup.enter="sendCommand" type="text" placeholder="输入AT命令，如: AT+CSQ"
          class="flex-1 px-4 py-3 bg-slate-100 dark:bg-black/30 border border-slate-200 dark:border-white/20 rounded-xl text-slate-900 dark:text-white placeholder-slate-400 dark:placeholder-white/30 focus:outline-none focus:border-cyan-500 font-mono">
        <button @click="sendCommand" :disabled="loading || !command.trim()"
          class="px-6 py-3 bg-gradient-to-r from-cyan-500 to-blue-500 text-white font-medium rounded-xl hover:shadow-lg hover:shadow-cyan-500/30 transition-all disabled:opacity-50">
          <i :class="loading ? 'fas fa-spinner animate-spin' : 'fas fa-paper-plane'" class="mr-2"></i>发送
        </button>
      </div>
      <!-- 快捷命令（点击直接发送） -->
      <div class="mb-4">
        <p class="text-slate-500 dark:text-white/50 text-xs mb-2">快捷查询（点击直接发送）</p>
        <div class="flex flex-wrap gap-2">
          <button v-for="qc in quickCommands" :key="qc.cmd" @click="useQuickCommand(qc.cmd)"
            class="px-3 py-1.5 text-xs bg-slate-100 dark:bg-white/10 hover:bg-slate-200 dark:hover:bg-white/20 text-slate-600 dark:text-white/70 hover:text-slate-900 dark:hover:text-white rounded-lg transition-colors">
            {{ qc.name }}
          </button>
        </div>
      </div>
      
      <!-- 模板命令（点击填充，不发送） -->
      <div class="flex flex-wrap gap-2">
        <button v-for="tc in templateCommands" :key="tc.cmd" @click="fillTemplate(tc.cmd)"
          :title="tc.desc"
          class="px-3 py-1.5 text-xs bg-amber-100 dark:bg-amber-500/20 hover:bg-amber-200 dark:hover:bg-amber-500/30 text-amber-700 dark:text-amber-400 hover:text-amber-900 dark:hover:text-amber-300 rounded-lg transition-colors border border-amber-200 dark:border-amber-500/30">
          <i class="fas fa-edit mr-1"></i>{{ tc.name }}
        </button>
      </div>
    </div>

    <!-- 结果显示 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20 hover:shadow-xl hover:shadow-slate-300/50 dark:hover:shadow-black/30 transition-all duration-300">
      <div class="flex items-center justify-between mb-4">
        <h3 class="text-slate-900 dark:text-white font-semibold flex items-center">
          <i class="fas fa-code text-green-600 dark:text-green-400 mr-2"></i>执行结果
        </h3>
        <button v-if="history.length > 0" @click="clearHistory" class="text-xs text-slate-500 dark:text-white/50 hover:text-slate-900 dark:hover:text-white transition-colors">
          <i class="fas fa-trash mr-1"></i>清空历史
        </button>
      </div>
      <div class="bg-slate-900 dark:bg-black/50 rounded-xl p-4 font-mono text-sm min-h-[120px] max-h-[300px] overflow-auto">
        <pre v-if="result" class="text-green-400 whitespace-pre-wrap">{{ result }}</pre>
        <p v-else class="text-slate-500 dark:text-white/30">等待执行...</p>
      </div>
    </div>

    <!-- 历史记录 -->
    <div v-if="history.length > 0" class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20 hover:shadow-xl hover:shadow-slate-300/50 dark:hover:shadow-black/30 transition-all duration-300">
      <h3 class="text-slate-900 dark:text-white font-semibold mb-4 flex items-center">
        <i class="fas fa-history text-amber-500 dark:text-amber-400 mr-2"></i>历史记录
      </h3>
      <div class="space-y-3 max-h-[400px] overflow-auto">
        <div v-for="(item, index) in history" :key="index" class="p-3 bg-slate-900 dark:bg-black/30 rounded-xl">
          <div class="flex items-center justify-between mb-2">
            <code class="text-cyan-400 text-sm">{{ item.cmd }}</code>
            <span class="text-slate-500 dark:text-white/30 text-xs">{{ item.time }}</span>
          </div>
          <pre class="text-green-400/70 text-xs whitespace-pre-wrap max-h-[100px] overflow-auto">{{ item.result }}</pre>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
pre { font-family: 'Consolas', 'Monaco', monospace; }
</style>
