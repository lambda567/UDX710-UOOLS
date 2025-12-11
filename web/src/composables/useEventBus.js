// 简单事件总线 - 用于跨组件通信
const listeners = new Map()

export function emit(event, data) {
  const callbacks = listeners.get(event)
  if (callbacks) {
    callbacks.forEach(fn => fn(data))
  }
}

export function on(event, fn) {
  if (!listeners.has(event)) {
    listeners.set(event, new Set())
  }
  listeners.get(event).add(fn)
}

export function off(event, fn) {
  const callbacks = listeners.get(event)
  if (callbacks) {
    callbacks.delete(fn)
  }
}
