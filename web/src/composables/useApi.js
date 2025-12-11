/**
 * API调用封装
 * 统一管理所有后端接口调用
 */

const BASE_URL = ''

// 通用请求函数
async function request(url, options = {}) {
  const response = await fetch(`${BASE_URL}${url}`, {
    headers: { 'Content-Type': 'application/json', ...options.headers },
    ...options
  })
  if (!response.ok) {
    throw new Error(`HTTP错误: ${response.status}`)
  }
  return response.json()
}

// ==================== 系统信息API ====================

// 获取系统信息
export async function fetchSystemInfo() {
  return request('/api/info')
}

// 清除系统缓存
export async function clearCache() {
  return request('/api/clear_cache', { method: 'POST' })
}

// 设备控制（重启/关机）
export async function deviceControl(action) {
  return request('/api/device_control', {
    method: 'POST',
    body: JSON.stringify({ action })
  })
}

// 获取设备序列号
export async function getDeviceSerial() {
  return request('/api/serial')
}

// 提交激活码
export async function submitActivationKey(key) {
  return request('/api/key', {
    method: 'POST',
    body: JSON.stringify({ key })
  })
}

// ==================== 网络管理API ====================

// 切换飞行模式
export async function toggleAirplaneMode(enabled) {
  return request('/api/airplane_mode', {
    method: 'POST',
    body: JSON.stringify({ enabled })
  })
}

// 设置网络模式
export async function setNetworkMode(mode) {
  return request('/api/set_network', {
    method: 'POST',
    body: JSON.stringify({ mode })
  })
}

// 切换SIM卡槽
export async function switchSlot(slot) {
  return request('/api/switch', {
    method: 'POST',
    body: JSON.stringify({ slot })
  })
}

// ==================== WiFi控制API ====================

// 获取WiFi状态
export async function getWifiStatus() {
  return request('/api/wifi/status')
}

// 设置WiFi配置
export async function setWifiConfig(config) {
  return request('/api/wifi/config', {
    method: 'POST',
    body: JSON.stringify(config)
  })
}

// 启用WiFi
export async function enableWifi(band = null) {
  return request('/api/wifi/enable', {
    method: 'POST',
    body: JSON.stringify({ band })
  })
}

// 禁用WiFi
export async function disableWifi() {
  return request('/api/wifi/disable', { method: 'POST' })
}

// 切换WiFi频段
export async function setWifiBand(band) {
  return request('/api/wifi/band', {
    method: 'POST',
    body: JSON.stringify({ band })
  })
}

// 兼容旧API
export async function setWifi(ssid, password) {
  return setWifiConfig({ ssid, password })
}

// ==================== 流量统计API ====================

// 获取流量统计
export async function getTrafficTotal() {
  return request('/api/get/Total')
}

// 获取流量配置
export async function getTrafficConfig() {
  return request('/api/get/set')
}

// 设置流量限制
export async function setTrafficLimit(enabled, limitGB) {
  const switchValue = enabled ? 1 : 0
  const muchValue = Math.round(limitGB * 1073741824) // GB转字节
  return request(`/api/set/total?switch=${switchValue}&much=${muchValue}`)
}

// 清除流量统计
export async function clearTrafficStats() {
  return request('/api/set/total')
}

// ==================== 定时重启API ====================

// 获取定时重启配置
export async function getRebootConfig() {
  return request('/api/get/first-reboot')
}

// 设置定时重启
export async function setReboot(days, hour, minute) {
  const dayStr = days.join(',')
  return request(`/api/set/reboot?day=${dayStr}&hour=${hour}&minute=${minute}`)
}

// 清除定时重启
export async function clearReboot() {
  return request('/api/claen/cron')
}

// ==================== 系统时间API ====================

// 获取系统时间
export async function getSystemTime() {
  return request('/api/get/time')
}

// NTP同步系统时间
export async function syncSystemTime() {
  return request('/api/set/time', { method: 'POST' })
}

// ==================== 高级网络API ====================

// 获取频段状态
export async function getBands() {
  return request('/api/bands')
}

// 获取当前连接的频段
export async function getCurrentBand() {
  return request('/api/current_band')
}

// 锁定频段
export async function lockBands(bands) {
  return request('/api/lock_bands', {
    method: 'POST',
    body: JSON.stringify({ bands })
  })
}

// 解锁所有频段
export async function unlockBands() {
  return request('/api/unlock_bands', { method: 'POST' })
}

// 获取小区信息
export async function getCells() {
  return request('/api/cells')
}

// 锁定小区
export async function lockCell(technology, arfcn, pci) {
  return request('/api/lock_cell', {
    method: 'POST',
    body: JSON.stringify({ 
      technology, 
      arfcn: arfcn.toString(), 
      pci: pci.toString() 
    })
  })
}

// 解锁小区
export async function unlockCell() {
  return request('/api/unlock_cell', { method: 'POST' })
}


// ==================== 充电控制API ====================

// 获取充电配置和电池状态
export async function getChargeConfig() {
  return request('/api/charge/config')
}

// 设置充电配置
export async function setChargeConfig(enabled, startThreshold, stopThreshold) {
  return request('/api/charge/config', {
    method: 'POST',
    body: JSON.stringify({ enabled, startThreshold, stopThreshold })
  })
}

// 手动开启充电
export async function chargeOn() {
  return request('/api/charge/on', { method: 'POST' })
}

// 手动停止充电
export async function chargeOff() {
  return request('/api/charge/off', { method: 'POST' })
}


// ==================== AT命令调试API ====================

// 执行AT命令
export async function executeAT(command) {
  return request('/api/at', {
    method: 'POST',
    body: JSON.stringify({ command })
  })
}

// ==================== WiFi客户端管理API ====================

// 获取所有连接的客户端
export async function getWifiClients() {
  return request('/api/wifi/clients')
}

// 获取黑名单列表
export async function getWifiBlacklist() {
  return request('/api/wifi/blacklist')
}

// 添加到黑名单
export async function addToWifiBlacklist(mac) {
  return request('/api/wifi/blacklist', {
    method: 'POST',
    body: JSON.stringify({ mac })
  })
}

// 从黑名单移除
export async function removeFromWifiBlacklist(mac) {
  return request(`/api/wifi/blacklist/${mac}`, { method: 'DELETE' })
}

// 清空黑名单
export async function clearWifiBlacklist() {
  return request('/api/wifi/blacklist', { method: 'DELETE' })
}

// 获取白名单列表
export async function getWifiWhitelist() {
  return request('/api/wifi/whitelist')
}

// 添加到白名单
export async function addToWifiWhitelist(mac) {
  return request('/api/wifi/whitelist', {
    method: 'POST',
    body: JSON.stringify({ mac })
  })
}

// 从白名单移除
export async function removeFromWifiWhitelist(mac) {
  return request(`/api/wifi/whitelist/${mac}`, { method: 'DELETE' })
}

// 清空白名单
export async function clearWifiWhitelist() {
  return request('/api/wifi/whitelist', { method: 'DELETE' })
}


// ==================== 通用API封装 ====================

// 返回一个包含get/post方法的对象，用于组件中调用
export function useApi() {
  return {
    async get(url) {
      return request(url)
    },
    async post(url, data = {}) {
      return request(url, {
        method: 'POST',
        body: JSON.stringify(data)
      })
    }
  }
}
