<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useConfirm } from '../composables/useConfirm'

const { confirm } = useConfirm()
const activeTab = ref('inbox')
const loading = ref(false)
const messages = ref([])
const sentMessages = ref([])
const selectedMessages = ref(new Set())
const selectAll = ref(false)
const selectedSentMessages = ref(new Set())
const selectAllSent = ref(false)
const currentPage = ref(1)
const pageSize = 5
const currentMessage = ref(null)
const showDialog = ref(false)
const replyContent = ref('')
const newSms = ref({ recipient: '', content: '' })
const sendStatus = ref({ show: false, success: false, message: '' })
const smsConfig = ref({ max_count: 50, max_sent_count: 10 })
const smsFixEnabled = ref(false)
const smsFixLoading = ref(false)
const showSelectMode = ref(false)
const showSentSelectMode = ref(false)

const webhookConfig = ref({
  enabled: false, platform: 'pushplus', url: 'http://www.pushplus.plus/send',
  body: '{"token":"YOUR_TOKEN","title":"新短信","content":"发件人: #{sender}\\n内容: #{content}"}',
  headers: 'Content-Type: application/json'
})

const platformTemplates = {
  pushplus: { url: 'http://www.pushplus.plus/send', body: '{"token":"YOUR_TOKEN","title":"新短信","content":"发件人: #{sender}\\n内容: #{content}"}', headers: 'Content-Type: application/json' },
  serverchan: { url: 'https://sctapi.ftqq.com/YOUR_KEY.send', body: '{"title":"新短信","desp":"发件人: #{sender}\\n内容: #{content}"}', headers: 'Content-Type: application/json' },
  bark: { url: 'https://api.day.app/YOUR_KEY/新短信/#{content}', body: '', headers: '' },
  dingtalk: { url: 'https://oapi.dingtalk.com/robot/send?access_token=YOUR_TOKEN', body: '{"msgtype":"text","text":{"content":"新短信\\n发件人: #{sender}\\n内容: #{content}"}}', headers: 'Content-Type: application/json' },
  feishu: { url: 'https://open.feishu.cn/open-apis/bot/v2/hook/YOUR_TOKEN', body: '{"msg_type":"text","content":{"text":"新短信\\n发件人: #{sender}\\n内容: #{content}"}}', headers: 'Content-Type: application/json' },
  discord: { url: 'https://discord.com/api/webhooks/YOUR_WEBHOOK', body: '{"content":"**新短信**\\n发件人: #{sender}\\n内容: #{content}"}', headers: 'Content-Type: application/json' }
}

const totalPages = computed(() => Math.ceil(messages.value.length / pageSize) || 1)
const paginatedMessages = computed(() => {
  const start = (currentPage.value - 1) * pageSize
  return messages.value.slice(start, start + pageSize)
})
const unreadCount = computed(() => messages.value.filter(m => !m.read).length)

// API调用
async function fetchSmsList() {
  loading.value = true
  try {
    const res = await fetch('/api/sms')
    if (res.ok) messages.value = await res.json()
  } catch (e) { console.error('获取短信列表失败:', e) }
  finally { loading.value = false }
}

async function fetchSentList() {
  try {
    const res = await fetch('/api/sms/sent')
    if (res.ok) sentMessages.value = await res.json()
  } catch (e) { console.error('获取发送记录失败:', e) }
}

async function fetchWebhookConfig() {
  try {
    const res = await fetch('/api/sms/webhook')
    if (res.ok) {
      const data = await res.json()
      webhookConfig.value = data
      // 如果URL为空，应用默认PushPlus模板
      if (!data.url || data.url === '') {
        applyTemplate('pushplus')
      }
    }
  } catch (e) { console.error('获取Webhook配置失败:', e) }
}

async function fetchSmsConfig() {
  try {
    const res = await fetch('/api/sms/config')
    if (res.ok) smsConfig.value = await res.json()
  } catch (e) { console.error('获取短信配置失败:', e) }
}

async function sendSmsApi(recipient, content) {
  const res = await fetch('/api/sms/send', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ recipient, content })
  })
  return res.json()
}

async function deleteSmsApi(id) {
  const res = await fetch(`/api/sms/${id}`, { method: 'DELETE' })
  return res.json()
}

async function saveWebhookApi() {
  const res = await fetch('/api/sms/webhook', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(webhookConfig.value)
  })
  return res.json()
}

async function testWebhookApi() {
  const res = await fetch('/api/sms/webhook/test', { method: 'POST' })
  return res.json()
}

async function saveSmsConfigApi() {
  const res = await fetch('/api/sms/config', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(smsConfig.value)
  })
  return res.json()
}

async function fetchSmsFixStatus() {
  try {
    const res = await fetch('/api/sms/fix')
    if (res.ok) {
      const data = await res.json()
      smsFixEnabled.value = data.enabled
    }
  } catch (e) { console.error('获取短信修复状态失败:', e) }
}

async function toggleSmsFix() {
  smsFixLoading.value = true
  try {
    const res = await fetch('/api/sms/fix', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ enabled: !smsFixEnabled.value })
    })
    const result = await res.json()
    if (result.status === 'success') {
      smsFixEnabled.value = result.enabled
      showStatus(true, result.message)
    } else {
      showStatus(false, result.error || '设置失败')
    }
  } catch (e) { showStatus(false, '设置失败') }
  finally { smsFixLoading.value = false }
}

let refreshTimer = null
onMounted(() => {
  fetchSmsList(); fetchSentList(); fetchWebhookConfig(); fetchSmsConfig(); fetchSmsFixStatus()
  refreshTimer = setInterval(() => { fetchSmsList(); fetchSentList() }, 10000)
})
onUnmounted(() => { if (refreshTimer) clearInterval(refreshTimer) })

function formatTime(timestamp, full = false) {
  const date = new Date(typeof timestamp === 'number' ? timestamp * 1000 : timestamp)
  if (full) {
    return date.toLocaleString('zh-CN', { year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit' })
  }
  const now = new Date(), diff = now - date
  if (diff < 86400000) return date.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' })
  if (diff < 172800000) return '昨天 ' + date.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' })
  return date.toLocaleDateString('zh-CN', { month: '2-digit', day: '2-digit' }) + ' ' + date.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' })
}

function toggleSelect(id) { selectedMessages.value.has(id) ? selectedMessages.value.delete(id) : selectedMessages.value.add(id) }
function toggleSelectAll() { selectAll.value ? paginatedMessages.value.forEach(m => selectedMessages.value.add(m.id)) : paginatedMessages.value.forEach(m => selectedMessages.value.delete(m.id)) }

async function deleteSelected() {
  if (selectedMessages.value.size === 0 || !await confirm({ title: '删除短信', message: `确定删除选中的 ${selectedMessages.value.size} 条短信吗？`, danger: true })) return
  for (const id of selectedMessages.value) await deleteSmsApi(id)
  selectedMessages.value.clear(); selectAll.value = false; fetchSmsList()
}

function viewMessage(msg) { currentMessage.value = { ...msg }; showDialog.value = true; replyContent.value = '' }
function closeDialog() { showDialog.value = false; currentMessage.value = null }
async function deleteMessage(id) { await deleteSmsApi(id); fetchSmsList(); closeDialog() }
async function deleteSentMessage(id) {
  try {
    const res = await fetch(`/api/sms/sent/${id}`, { method: 'DELETE' })
    const result = await res.json()
    if (result.status === 'success') { fetchSentList(); return true }
    else { showStatus(false, result.error || '删除失败'); return false }
  } catch (e) { showStatus(false, '删除失败'); return false }
}

function toggleSelectSent(id) { selectedSentMessages.value.has(id) ? selectedSentMessages.value.delete(id) : selectedSentMessages.value.add(id) }
function toggleSelectAllSent() { selectAllSent.value ? sentMessages.value.forEach(m => selectedSentMessages.value.add(m.id)) : selectedSentMessages.value.clear() }

async function deleteSelectedSent() {
  if (selectedSentMessages.value.size === 0 || !await confirm({ title: '删除记录', message: `确定删除选中的 ${selectedSentMessages.value.size} 条发送记录吗？`, danger: true })) return
  for (const id of selectedSentMessages.value) await deleteSentMessage(id)
  selectedSentMessages.value.clear(); selectAllSent.value = false
  showStatus(true, '删除成功')
}

function showStatus(success, message) {
  sendStatus.value = { show: true, success, message }
  setTimeout(() => sendStatus.value.show = false, 3000)
}

async function sendReply() {
  if (!replyContent.value.trim()) return
  try {
    const result = await sendSmsApi(currentMessage.value.sender, replyContent.value)
    if (result.status === 'success') {
      showStatus(true, '回复发送成功！'); replyContent.value = ''; fetchSentList()
    } else showStatus(false, result.error || '发送失败')
  } catch (e) { showStatus(false, '发送失败') }
}

async function sendNewSms() {
  if (!newSms.value.recipient.trim() || !newSms.value.content.trim()) { showStatus(false, '请填写完整信息'); return }
  try {
    const result = await sendSmsApi(newSms.value.recipient, newSms.value.content)
    if (result.status === 'success') {
      showStatus(true, '短信发送成功！'); newSms.value = { recipient: '', content: '' }; fetchSentList()
    } else showStatus(false, result.error || '发送失败')
  } catch (e) { showStatus(false, '发送失败') }
}

function applyTemplate(platform) {
  const tpl = platformTemplates[platform]
  if (tpl) { webhookConfig.value.url = tpl.url; webhookConfig.value.body = tpl.body; webhookConfig.value.headers = tpl.headers }
}

async function saveWebhook() {
  try {
    const result = await saveWebhookApi()
    showStatus(result.status === 'success', result.status === 'success' ? 'Webhook配置已保存！' : '保存失败')
  } catch (e) { showStatus(false, '保存失败') }
}

async function testWebhook() {
  try {
    const result = await testWebhookApi()
    showStatus(result.status === 'success', result.status === 'success' ? '测试通知已发送！' : result.error || '测试失败')
  } catch (e) { showStatus(false, '测试失败') }
}

async function saveSmsConfig() {
  try {
    const result = await saveSmsConfigApi()
    showStatus(result.status === 'success', result.status === 'success' ? '配置已保存！' : '保存失败')
  } catch (e) { showStatus(false, '保存失败') }
}
</script>

<template>
  <div class="space-y-6">
    

    <!-- Tab导航 - 移动端优化：最小触控区域44px -->
    <div class="flex space-x-1 sm:space-x-2 bg-slate-100 dark:bg-white/5 backdrop-blur rounded-xl sm:rounded-2xl p-1.5 sm:p-2 border border-slate-200 dark:border-white/10 overflow-x-auto">
      <button v-for="tab in [{id:'inbox',label:'收件箱',icon:'fa-inbox'},{id:'outbox',label:'发件箱',icon:'fa-paper-plane'},{id:'forward',label:'转发',icon:'fa-share-nodes'},{id:'config',label:'配置',icon:'fa-gear'}]"
        :key="tab.id" @click="activeTab = tab.id"
        class="flex-1 flex items-center justify-center space-x-1 sm:space-x-2 px-2 sm:px-4 py-3 min-h-[44px] rounded-lg sm:rounded-xl transition-all whitespace-nowrap"
        :class="activeTab === tab.id ? 'bg-gradient-to-r from-emerald-500/20 to-teal-500/20 text-emerald-600 dark:text-emerald-400 border border-emerald-500/30' : 'text-slate-600 dark:text-white/60 hover:text-slate-900 dark:hover:text-white hover:bg-slate-200 dark:hover:bg-white/5'">
        <i :class="'fas ' + tab.icon" class="text-sm sm:text-base"></i>
        <span class="font-medium text-xs sm:text-sm md:text-base">{{ tab.label }}</span>
      </button>
    </div>

    <!-- 收件箱 -->
    <div v-if="activeTab === 'inbox'" class="space-y-4">
      <div class="flex items-center justify-between bg-white dark:bg-white/5 backdrop-blur rounded-2xl p-4 border border-slate-200 dark:border-white/10">
        <div class="flex items-center space-x-4">
          <label v-if="showSelectMode" class="flex items-center space-x-2 cursor-pointer">
            <input type="checkbox" v-model="selectAll" @change="toggleSelectAll" class="w-4 h-4 rounded border-slate-300 dark:border-white/30 bg-white dark:bg-white/10 text-emerald-500 focus:ring-emerald-500" />
            <span class="text-slate-600 dark:text-white/60 text-sm">全选</span>
          </label>
          <button v-if="showSelectMode" @click="deleteSelected" :disabled="selectedMessages.size === 0" class="px-4 py-2 bg-red-500/20 text-red-600 dark:text-red-400 rounded-xl hover:bg-red-500/30 transition-all disabled:opacity-50 disabled:cursor-not-allowed border border-red-500/30">
            删除 ({{ selectedMessages.size }})
          </button>
          <button v-if="showSelectMode" @click="showSelectMode = false; selectedMessages.clear(); selectAll = false" class="px-4 py-2 bg-slate-100 dark:bg-white/10 text-slate-600 dark:text-white/60 rounded-xl hover:bg-slate-200 dark:hover:bg-white/20 transition-all border border-slate-200 dark:border-white/10">
            取消
          </button>
          <button v-if="!showSelectMode" @click="showSelectMode = true" class="px-4 py-2 bg-red-500/20 text-red-600 dark:text-red-400 rounded-xl hover:bg-red-500/30 transition-all border border-red-500/30">
            删除
          </button>
        </div>
        <button @click="fetchSmsList" :disabled="loading" class="px-4 py-2 bg-emerald-500/20 text-emerald-600 dark:text-emerald-400 rounded-xl hover:bg-emerald-500/30 transition-all border border-emerald-500/30">
          {{ loading ? '加载中...' : '刷新' }}
        </button>
      </div>
      <div v-if="messages.length === 0" class="text-center py-12 text-slate-400 dark:text-white/40"><div class="text-4xl mb-4"><i class="fas fa-inbox text-slate-300 dark:text-white/30"></i></div><p>暂无短信</p></div>
      <div v-else class="space-y-3">
        <div v-for="msg in paginatedMessages" :key="msg.id" @click="viewMessage(msg)" class="group relative overflow-hidden rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-4 shadow-md shadow-slate-200/30 dark:shadow-black/10 hover:shadow-lg hover:shadow-emerald-200/40 dark:hover:shadow-black/20 hover:bg-slate-50/80 dark:hover:bg-white/10 hover:border-emerald-500/30 hover:-translate-y-0.5 transition-all duration-300 cursor-pointer">
          <div class="absolute top-0 left-0 w-1 h-full rounded-r" :class="msg.is_read ? 'bg-slate-200 dark:bg-white/20' : 'bg-emerald-500'"></div>
          <div class="flex items-start space-x-4 pl-3">
            <div v-if="showSelectMode" class="flex items-center" @click.stop><input type="checkbox" :checked="selectedMessages.has(msg.id)" @change="toggleSelect(msg.id)" class="w-4 h-4 rounded border-slate-300 dark:border-white/30 bg-white dark:bg-white/10 text-emerald-500 focus:ring-emerald-500" /></div>
            <div class="w-12 h-12 rounded-xl bg-gradient-to-br from-emerald-500/20 to-teal-500/20 flex items-center justify-center flex-shrink-0 border border-emerald-500/20"><i class="fas fa-user text-emerald-600 dark:text-emerald-400"></i></div>
            <div class="flex-1 min-w-0 overflow-hidden">
              <div class="flex items-center justify-between mb-1">
                <span class="text-slate-900 dark:text-white font-medium truncate max-w-[60%]">{{ msg.sender }}</span>
                <span class="text-slate-400 dark:text-white/40 text-xs flex-shrink-0 ml-2">{{ formatTime(msg.timestamp, true) }}</span>
              </div>
              <p class="text-slate-600 dark:text-white/60 text-sm line-clamp-2 break-words overflow-hidden text-ellipsis">{{ msg.content }}</p>
            </div>
            <button @click.stop="deleteMessage(msg.id)" class="opacity-0 group-hover:opacity-100 p-2 text-red-600 dark:text-red-400 hover:bg-red-500/20 rounded-lg transition-all text-sm">删除</button>
          </div>
        </div>
      </div>
      <div v-if="messages.length > 0" class="flex items-center justify-center space-x-4 py-4">
        <button @click="currentPage--" :disabled="currentPage <= 1" class="px-4 py-2 bg-slate-100 dark:bg-white/10 text-slate-600 dark:text-white/60 rounded-xl hover:bg-slate-200 dark:hover:bg-white/20 transition-all disabled:opacity-50 disabled:cursor-not-allowed"><i class="fas fa-chevron-left mr-1"></i>上一页</button>
        <span class="text-slate-600 dark:text-white/60">第 {{ currentPage }} / {{ totalPages }} 页</span>
        <button @click="currentPage++" :disabled="currentPage >= totalPages" class="px-4 py-2 bg-slate-100 dark:bg-white/10 text-slate-600 dark:text-white/60 rounded-xl hover:bg-slate-200 dark:hover:bg-white/20 transition-all disabled:opacity-50 disabled:cursor-not-allowed">下一页<i class="fas fa-chevron-right ml-1"></i></button>
      </div>
    </div>

    <!-- 发件箱 -->
    <div v-if="activeTab === 'outbox'" class="space-y-6">
      <div class="rounded-3xl bg-white dark:bg-white/5 backdrop-blur border border-slate-200 dark:border-white/10 p-6">
        <h3 class="text-slate-900 dark:text-white font-bold mb-4">发送新短信</h3>
        <div class="space-y-4">
          <div>
            <label class="text-slate-600 dark:text-white/60 text-sm mb-2 block">收件人</label>
            <!-- 手机号输入：使用tel类型和inputmode优化移动端键盘 -->
            <input v-model="newSms.recipient" type="tel" inputmode="tel" autocomplete="tel" placeholder="请输入手机号码" class="w-full px-4 py-3 min-h-[44px] bg-slate-50 dark:bg-white/5 border border-slate-200 dark:border-white/10 rounded-xl text-slate-900 dark:text-white text-base placeholder-slate-400 dark:placeholder-white/30 focus:border-emerald-500/50 focus:outline-none transition-all" />
          </div>
          <div>
            <label class="text-slate-600 dark:text-white/60 text-sm mb-2 block">短信内容</label>
            <!-- 文本域：确保字体16px防止iOS缩放 -->
            <textarea v-model="newSms.content" rows="4" placeholder="请输入短信内容" class="w-full px-4 py-3 bg-slate-50 dark:bg-white/5 border border-slate-200 dark:border-white/10 rounded-xl text-slate-900 dark:text-white text-base placeholder-slate-400 dark:placeholder-white/30 focus:border-emerald-500/50 focus:outline-none transition-all resize-none"></textarea>
            <div class="text-right text-slate-400 dark:text-white/40 text-xs mt-1">{{ newSms.content.length }} 字符</div>
          </div>
          <button @click="sendNewSms" class="px-6 py-3 bg-gradient-to-r from-emerald-500 to-teal-500 text-white font-medium rounded-xl hover:shadow-lg hover:shadow-emerald-500/30 transition-all">发送短信</button>
        </div>
      </div>
      <div class="rounded-3xl bg-white dark:bg-white/5 backdrop-blur border border-slate-200 dark:border-white/10 p-6">
        <div class="flex items-center justify-between mb-4">
          <h3 class="text-slate-900 dark:text-white font-bold">已发送</h3>
          <div class="flex items-center space-x-3">
            <label v-if="showSentSelectMode" class="flex items-center space-x-2 cursor-pointer">
              <input type="checkbox" v-model="selectAllSent" @change="toggleSelectAllSent" class="w-4 h-4 rounded border-slate-300 dark:border-white/30 bg-white dark:bg-white/10 text-emerald-500" />
              <span class="text-slate-600 dark:text-white/60 text-sm">全选</span>
            </label>
            <button v-if="showSentSelectMode" @click="deleteSelectedSent" :disabled="selectedSentMessages.size === 0" class="px-3 py-2 bg-red-500/20 text-red-600 dark:text-red-400 rounded-xl hover:bg-red-500/30 text-sm disabled:opacity-50 border border-red-500/30">删除 ({{ selectedSentMessages.size }})</button>
            <button v-if="showSentSelectMode" @click="showSentSelectMode = false; selectedSentMessages.clear(); selectAllSent = false" class="px-3 py-2 bg-slate-100 dark:bg-white/10 text-slate-600 dark:text-white/60 rounded-xl hover:bg-slate-200 dark:hover:bg-white/20 text-sm border border-slate-200 dark:border-white/10">取消</button>
            <button v-if="!showSentSelectMode" @click="showSentSelectMode = true" class="px-3 py-2 bg-red-500/20 text-red-600 dark:text-red-400 rounded-xl hover:bg-red-500/30 text-sm border border-red-500/30">删除</button>
            <button @click="fetchSentList" class="px-3 py-2 bg-emerald-500/20 text-emerald-600 dark:text-emerald-400 rounded-xl hover:bg-emerald-500/30 text-sm border border-emerald-500/30">刷新</button>
          </div>
        </div>
        <div v-if="sentMessages.length === 0" class="text-center py-8 text-slate-400 dark:text-white/40"><p>暂无发送记录</p></div>
        <div v-else class="space-y-3 max-h-96 overflow-y-auto">
          <div v-for="msg in sentMessages" :key="msg.id" class="group p-4 bg-slate-50 dark:bg-white/5 rounded-xl border border-slate-200 dark:border-white/10 hover:bg-slate-100 dark:hover:bg-white/10 transition-all">
            <div class="flex items-start space-x-3">
              <input v-if="showSentSelectMode" type="checkbox" :checked="selectedSentMessages.has(msg.id)" @change="toggleSelectSent(msg.id)" class="w-4 h-4 mt-1 rounded border-slate-300 dark:border-white/30 bg-white dark:bg-white/10 text-emerald-500" />
              <div class="flex-1 min-w-0">
                <div class="flex items-center justify-between mb-1">
                  <span class="text-slate-600 dark:text-white/60 text-sm">发送至: <span class="text-slate-900 dark:text-white">{{ msg.recipient }}</span></span>
                  <span class="text-slate-400 dark:text-white/40 text-xs">{{ formatTime(msg.timestamp, true) }}</span>
                </div>
                <p class="text-slate-700 dark:text-white/80 text-sm line-clamp-2 break-all">{{ msg.content }}</p>
                <div class="mt-2"><span class="px-2 py-1 bg-green-500/20 text-green-600 dark:text-green-400 text-xs rounded-lg"><i class="fas fa-check mr-1"></i>{{ msg.status || '已发送' }}</span></div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- 短信转发 -->
    <div v-if="activeTab === 'forward'" class="rounded-3xl bg-white dark:bg-white/5 backdrop-blur border border-slate-200 dark:border-white/10 p-6">
      <div class="flex items-center justify-between mb-6">
        <div class="flex items-center space-x-3">
          <div class="w-10 h-10 rounded-xl bg-gradient-to-br from-orange-500 to-amber-500 flex items-center justify-center"><i class="fas fa-share-nodes text-white"></i></div>
          <h3 class="text-slate-900 dark:text-white font-bold">Webhook转发配置</h3>
        </div>
        <div class="flex space-x-2">
          <button @click="saveWebhook" class="px-4 py-2 bg-emerald-500/20 text-emerald-600 dark:text-emerald-400 rounded-xl hover:bg-emerald-500/30 transition-all border border-emerald-500/30">保存</button>
          <button @click="testWebhook" class="px-4 py-2 bg-blue-500/20 text-blue-600 dark:text-blue-400 rounded-xl hover:bg-blue-500/30 transition-all border border-blue-500/30">测试</button>
        </div>
      </div>
      <div class="space-y-6">
        <div>
          <label class="text-slate-600 dark:text-white/60 text-sm mb-2 block">选择平台模板</label>
          <select v-model="webhookConfig.platform" @change="applyTemplate(webhookConfig.platform)" class="w-full px-4 py-3 bg-slate-50 dark:bg-white/5 border border-slate-200 dark:border-white/10 rounded-xl text-slate-900 dark:text-white focus:border-emerald-500/50 focus:outline-none transition-all">
            <option value="pushplus">PushPlus</option><option value="serverchan">Server酱</option><option value="bark">Bark</option>
            <option value="dingtalk">钉钉机器人</option><option value="feishu">飞书机器人</option><option value="discord">Discord</option>
          </select>
        </div>
        <div class="flex items-center justify-between p-4 bg-slate-50 dark:bg-white/5 rounded-xl border border-slate-200 dark:border-white/10">
          <div class="flex items-center space-x-3">
            <div class="w-10 h-10 rounded-xl bg-gradient-to-br from-emerald-500/20 to-teal-500/20 flex items-center justify-center"><i class="fas fa-bell text-emerald-600 dark:text-emerald-400"></i></div>
            <div><p class="text-slate-900 dark:text-white font-medium">启用Webhook通知</p><p class="text-slate-500 dark:text-white/40 text-xs">开启后将在收到新短信时发送通知</p></div>
          </div>
          <label class="relative inline-flex items-center cursor-pointer">
            <input type="checkbox" v-model="webhookConfig.enabled" class="sr-only peer" />
            <div class="w-14 h-7 bg-slate-200 dark:bg-white/10 peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-0.5 after:left-[4px] after:bg-white after:rounded-full after:h-6 after:w-6 after:transition-all peer-checked:bg-emerald-500"></div>
          </label>
        </div>
        <div>
          <label class="text-slate-600 dark:text-white/60 text-sm mb-2 block">Webhook URL</label>
          <input v-model="webhookConfig.url" type="text" placeholder="https://api.example.com/webhook" class="w-full px-4 py-3 bg-slate-50 dark:bg-white/5 border border-slate-200 dark:border-white/10 rounded-xl text-slate-900 dark:text-white placeholder-slate-400 dark:placeholder-white/30 focus:border-emerald-500/50 focus:outline-none transition-all font-mono text-sm" />
        </div>
        <div>
          <label class="text-slate-600 dark:text-white/60 text-sm mb-2 block">请求体 (JSON)</label>
          <textarea v-model="webhookConfig.body" rows="4" class="w-full px-4 py-3 bg-slate-50 dark:bg-white/5 border border-slate-200 dark:border-white/10 rounded-xl text-slate-900 dark:text-white placeholder-slate-400 dark:placeholder-white/30 focus:border-emerald-500/50 focus:outline-none transition-all resize-none font-mono text-sm"></textarea>
          <p class="text-slate-500 dark:text-white/40 text-xs mt-1">支持变量: #{sender}, #{content}, #{time}</p>
        </div>
        <div>
          <label class="text-slate-600 dark:text-white/60 text-sm mb-2 block">请求头</label>
          <textarea v-model="webhookConfig.headers" rows="2" class="w-full px-4 py-3 bg-slate-50 dark:bg-white/5 border border-slate-200 dark:border-white/10 rounded-xl text-slate-900 dark:text-white placeholder-slate-400 dark:placeholder-white/30 focus:border-emerald-500/50 focus:outline-none transition-all resize-none font-mono text-sm"></textarea>
        </div>
      </div>
    </div>

    <!-- 配置 -->
    <div v-if="activeTab === 'config'" class="rounded-3xl bg-white dark:bg-white/5 backdrop-blur border border-slate-200 dark:border-white/10 p-6">
      <div class="flex items-center justify-between mb-6">
        <div class="flex items-center space-x-3">
          <div class="w-10 h-10 rounded-xl bg-gradient-to-br from-violet-500 to-purple-500 flex items-center justify-center"><i class="fas fa-gear text-white"></i></div>
          <h3 class="text-slate-900 dark:text-white font-bold">短信存储配置</h3>
        </div>
        <button @click="saveSmsConfig" class="px-4 py-2 bg-emerald-500/20 text-emerald-600 dark:text-emerald-400 rounded-xl hover:bg-emerald-500/30 transition-all border border-emerald-500/30"><i class="fas fa-save mr-1"></i>保存</button>
      </div>
      <div class="space-y-6">
        <div class="p-4 bg-slate-50 dark:bg-white/5 rounded-xl border border-slate-200 dark:border-white/10">
          <div class="flex items-center justify-between mb-4">
            <div>
              <p class="text-slate-900 dark:text-white font-medium">收件箱最大存储</p>
              <p class="text-slate-500 dark:text-white/40 text-xs">接收短信的最大存储条数 (10-150)</p>
            </div>
            <div class="flex items-center space-x-3">
              <input v-model.number="smsConfig.max_count" type="number" min="10" max="150" class="w-24 px-3 py-2 bg-white dark:bg-white/5 border border-slate-200 dark:border-white/10 rounded-xl text-slate-900 dark:text-white text-center focus:border-emerald-500/50 focus:outline-none" />
              <span class="text-slate-600 dark:text-white/60">条</span>
            </div>
          </div>
          <input type="range" v-model.number="smsConfig.max_count" min="10" max="150" step="10" class="w-full h-2 bg-slate-200 dark:bg-white/10 rounded-lg appearance-none cursor-pointer accent-emerald-500" />
          <div class="flex justify-between text-slate-400 dark:text-white/40 text-xs mt-2"><span>10</span><span>80</span><span>150</span></div>
        </div>
        <div class="p-4 bg-slate-50 dark:bg-white/5 rounded-xl border border-slate-200 dark:border-white/10">
          <div class="flex items-center justify-between mb-4">
            <div>
              <p class="text-slate-900 dark:text-white font-medium">发件箱最大存储</p>
              <p class="text-slate-500 dark:text-white/40 text-xs">发送记录的最大存储条数 (1-50)</p>
            </div>
            <div class="flex items-center space-x-3">
              <input v-model.number="smsConfig.max_sent_count" type="number" min="1" max="50" class="w-24 px-3 py-2 bg-white dark:bg-white/5 border border-slate-200 dark:border-white/10 rounded-xl text-slate-900 dark:text-white text-center focus:border-emerald-500/50 focus:outline-none" />
              <span class="text-slate-600 dark:text-white/60">条</span>
            </div>
          </div>
          <input type="range" v-model.number="smsConfig.max_sent_count" min="1" max="50" step="1" class="w-full h-2 bg-slate-200 dark:bg-white/10 rounded-lg appearance-none cursor-pointer accent-blue-500" />
          <div class="flex justify-between text-slate-400 dark:text-white/40 text-xs mt-2"><span>1</span><span>25</span><span>50</span></div>
        </div>
        <div class="grid grid-cols-2 gap-4">
          <div class="p-4 bg-slate-50 dark:bg-white/5 rounded-xl border border-slate-200 dark:border-white/10">
            <p class="text-slate-600 dark:text-white/60 text-sm">当前收件箱</p>
            <p class="text-2xl font-bold text-slate-900 dark:text-white mt-1">{{ messages.length }} <span class="text-slate-400 dark:text-white/40 text-sm font-normal">/ {{ smsConfig.max_count }}</span></p>
          </div>
          <div class="p-4 bg-slate-50 dark:bg-white/5 rounded-xl border border-slate-200 dark:border-white/10">
            <p class="text-slate-600 dark:text-white/60 text-sm">当前发件箱</p>
            <p class="text-2xl font-bold text-slate-900 dark:text-white mt-1">{{ sentMessages.length }} <span class="text-slate-400 dark:text-white/40 text-sm font-normal">/ {{ smsConfig.max_sent_count }}</span></p>
          </div>
        </div>
        <div class="p-4 bg-slate-50 dark:bg-white/5 rounded-xl border border-slate-200 dark:border-white/10">
          <div class="flex items-center justify-between">
            <div class="flex items-center space-x-3">
              <div class="w-10 h-10 rounded-xl bg-gradient-to-br from-amber-500/20 to-orange-500/20 flex items-center justify-center"><i class="fas fa-wrench text-amber-500 dark:text-amber-400"></i></div>
              <div>
                <p class="text-slate-900 dark:text-white font-medium">修复短信接收</p>
                <p class="text-slate-500 dark:text-white/40 text-xs">开启后发送AT命令修复短信接收问题，开机自动应用(R106专用修复)</p>
              </div>
            </div>
            <button @click="toggleSmsFix" :disabled="smsFixLoading" 
              class="px-4 py-2 rounded-xl transition-all border"
              :class="smsFixEnabled ? 'bg-emerald-500/20 text-emerald-600 dark:text-emerald-400 border-emerald-500/30 hover:bg-emerald-500/30' : 'bg-slate-100 dark:bg-white/10 text-slate-600 dark:text-white/60 border-slate-200 dark:border-white/10 hover:bg-slate-200 dark:hover:bg-white/20'">
              <i :class="smsFixLoading ? 'fas fa-spinner animate-spin' : (smsFixEnabled ? 'fas fa-check' : '')"></i> {{ smsFixLoading ? '' : (smsFixEnabled ? '已开启' : '关闭') }}
            </button>
          </div>
        </div>
      </div>
    </div>

    <!-- 状态提示 -->
    <Transition name="slide">
      <div v-if="sendStatus.show" class="fixed bottom-6 right-6 px-6 py-4 rounded-2xl shadow-2xl border z-50" :class="sendStatus.success ? 'bg-emerald-500/90 border-emerald-400/50 text-white' : 'bg-red-500/90 border-red-400/50 text-white'">
        <div class="flex items-center space-x-3"><i :class="sendStatus.success ? 'fas fa-check-circle' : 'fas fa-times-circle'" class="text-xl"></i><span class="font-medium">{{ sendStatus.message }}</span></div>
      </div>
    </Transition>

    <!-- 短信详情对话框 -->
    <Teleport to="body">
      <Transition name="fade">
        <div v-if="showDialog" class="fixed inset-0 z-50 flex items-center justify-center p-4">
          <div class="absolute inset-0 bg-black/60 backdrop-blur-sm" @click="closeDialog"></div>
          <div class="relative w-full max-w-lg bg-white dark:bg-slate-800/95 backdrop-blur-xl rounded-3xl border border-slate-200 dark:border-white/10 shadow-2xl overflow-hidden">
            <div class="p-6 border-b border-slate-200 dark:border-white/10 bg-gradient-to-r from-emerald-500/10 to-teal-500/10">
              <div class="flex items-center justify-between">
                <div class="flex items-center space-x-3">
                  <div class="w-12 h-12 rounded-xl bg-gradient-to-br from-emerald-500 to-teal-500 flex items-center justify-center"><i class="fas fa-envelope text-white text-lg"></i></div>
                  <div><h3 class="text-slate-900 dark:text-white font-bold">短信详情</h3><p class="text-slate-500 dark:text-white/50 text-sm">{{ currentMessage?.sender }}</p></div>
                </div>
                <button @click="closeDialog" class="w-10 h-10 rounded-xl bg-slate-100 dark:bg-white/10 hover:bg-slate-200 dark:hover:bg-white/20 flex items-center justify-center transition-all text-slate-600 dark:text-white/60"><i class="fas fa-times"></i></button>
              </div>
            </div>
            <div class="p-6 space-y-4">
              <div class="flex items-center justify-between text-sm"><span class="text-slate-500 dark:text-white/40">接收时间</span><span class="text-slate-700 dark:text-white/80">{{ currentMessage ? formatTime(currentMessage.timestamp, true) : '' }}</span></div>
              <div class="p-4 bg-slate-50 dark:bg-white/5 rounded-xl border border-slate-200 dark:border-white/10"><p class="text-slate-900 dark:text-white leading-relaxed">{{ currentMessage?.content }}</p></div>
              <div class="pt-4 border-t border-slate-200 dark:border-white/10">
                <label class="text-slate-600 dark:text-white/60 text-sm mb-2 block">快速回复</label>
                <textarea v-model="replyContent" rows="3" placeholder="输入回复内容..." class="w-full px-4 py-3 bg-slate-50 dark:bg-white/5 border border-slate-200 dark:border-white/10 rounded-xl text-slate-900 dark:text-white placeholder-slate-400 dark:placeholder-white/30 focus:border-emerald-500/50 focus:outline-none transition-all resize-none"></textarea>
              </div>
            </div>
            <div class="p-6 border-t border-slate-200 dark:border-white/10 flex justify-end space-x-3">
              <button @click="deleteMessage(currentMessage?.id)" class="px-4 py-2 bg-red-500/20 text-red-600 dark:text-red-400 rounded-xl hover:bg-red-500/30 transition-all border border-red-500/30">删除</button>
              <button @click="sendReply" :disabled="!replyContent.trim()" class="px-6 py-2 bg-gradient-to-r from-emerald-500 to-teal-500 text-white font-medium rounded-xl hover:shadow-lg hover:shadow-emerald-500/30 transition-all disabled:opacity-50 disabled:cursor-not-allowed">发送回复</button>
            </div>
          </div>
        </div>
      </Transition>
    </Teleport>
  </div>
</template>

<style scoped>
.fade-enter-active, .fade-leave-active { transition: all 0.3s ease; }
.fade-enter-from, .fade-leave-to { opacity: 0; }
.slide-enter-active, .slide-leave-active { transition: all 0.3s ease; }
.slide-enter-from, .slide-leave-to { opacity: 0; transform: translateX(100px); }
select option { background-color: #1e293b; color: white; }
input[type="range"]::-webkit-slider-thumb { -webkit-appearance: none; width: 16px; height: 16px; background: #10b981; border-radius: 50%; cursor: pointer; }
</style>
