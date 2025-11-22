<script setup>
import { ref, onMounted, onUnmounted, nextTick, computed } from 'vue'
import { decode } from '@msgpack/msgpack'
import logo from '@/assets/awakelion_logo.jpg'

const logs = ref([])
const isConnected = ref(false)
const logContainer = ref(null)
const filterText = ref('')
const autoScroll = ref(true)
const selectedLevel = ref('DEBUG')
const toast = ref({ show: false, message: '', type: 'info' })
const fontSize = ref(parseInt(localStorage.getItem('logViewerFontSize')) || 14)
let socket = null

const showToast = (msg, type = 'info') => {
  toast.value = { show: true, message: msg, type }
  setTimeout(() => {
    toast.value.show = false
  }, 3000)
}

const connect = () => {
  // Connect to the C++ WebSocket server
  socket = new WebSocket('ws://127.0.0.1:1234')
  socket.binaryType = 'arraybuffer'

  socket.onopen = () => {
    isConnected.value = true
    addLog({
      type: 'system',
      level: 'NOTICE',
      tid: 'SYSTEM',
      timestamp: Date.now(),
      msg: 'Connected to AwakeLion Logger Server'
    })
  }

  socket.onmessage = (event) => {
    try {
      let data
      if (event.data instanceof ArrayBuffer) {
        data = decode(new Uint8Array(event.data))
      } else {
        data = JSON.parse(event.data)
      }
      addLog({ type: 'log', ...data })
    } catch (e) {
      addLog({
        type: 'raw',
        level: 'ERROR',
        tid: 'SYSTEM',
        timestamp: Date.now(),
        msg: typeof event.data === 'string' ? event.data : 'Binary data received'
      })
    }
  }

  socket.onclose = () => {
    isConnected.value = false
    addLog({
      type: 'system',
      level: 'NOTICE',
      tid: 'SYSTEM',
      timestamp: Date.now(),
      msg: 'Disconnected from server'
    })
  }

  socket.onerror = (error) => {
    console.error('WebSocket error:', error)
    addLog({
      type: 'error',
      level: 'ERROR',
      tid: 'SYSTEM',
      timestamp: Date.now(),
      msg: 'WebSocket error occurred'
    })
  }
}

const setLogLevel = () => {
  if (!socket || socket.readyState !== WebSocket.OPEN) return

  const cmd = {
    command: "SET_LEVEL",
    level: selectedLevel.value
  }
  socket.send(JSON.stringify(cmd))
  showToast(`Requesting to set log level to ${selectedLevel.value}...`, 'info')
}

const addLog = (log) => {
  logs.value.push(log)

  if (autoScroll.value) {
    nextTick(() => {
      scrollToBottom()
    })
  }
}

const scrollToBottom = () => {
  if (logContainer.value) {
    logContainer.value.scrollTop = logContainer.value.scrollHeight
  }
}

const clearLogs = () => {
  logs.value = []
}

const refreshConnection = () => {
  if (socket) {
    socket.close()
  }
  clearLogs()
  // Small delay to ensure socket is closed before reconnecting
  setTimeout(() => {
    connect()
    showToast('Refreshing connection...', 'info')
  }, 100)
}

const increaseFontSize = () => {
  if (fontSize.value < 24) {
    fontSize.value += 1
    localStorage.setItem('logViewerFontSize', fontSize.value)
  }
}

const decreaseFontSize = () => {
  if (fontSize.value > 10) {
    fontSize.value -= 1
    localStorage.setItem('logViewerFontSize', fontSize.value)
  }
}

const toggleAutoScroll = () => {
  autoScroll.value = !autoScroll.value
  if (autoScroll.value) scrollToBottom()
}

const filteredLogs = computed(() => {
  if (!filterText.value) return logs.value
  const lowerFilter = filterText.value.toLowerCase()
  return logs.value.filter(log => {
    return (log.msg && log.msg.toLowerCase().includes(lowerFilter)) ||
           (log.level && log.level.toLowerCase().includes(lowerFilter)) ||
           (log.file_name && log.file_name.toLowerCase().includes(lowerFilter))
  })
})

// Helper to format timestamp
const formatTime = (ts) => {
  if (!ts) return ''
  // If ts is already a formatted string (from backend), return it directly
  if (typeof ts === 'string') return ts

  let ms = ts
  if (ts > 1000000000000000000) ms = ts / 1000000
  else if (ts > 1000000000000000) ms = ts / 1000

  const date = new Date(Number(ms))
  return date.toLocaleTimeString('en-GB') + '.' + String(Math.floor(ms % 1000)).padStart(3, '0')
}

onMounted(() => {
  connect()
})

onUnmounted(() => {
  if (socket) socket.close()
})
</script>

<template>
  <div class="viewer">
    <header class="header">
      <div class="brand">
        <img :src="logo" class="logo-icon" alt="Logo" />
        <h1>Awakelion <span class="subtitle">Viewer</span></h1>
      </div>

      <div class="controls">
        <div class="level-selector">
          <select v-model="selectedLevel" @change="setLogLevel" class="level-select" title="Set Remote Log Level">
            <option value="DEBUG">DEBUG</option>
            <option value="INFO">INFO</option>
            <option value="NOTICE">NOTICE</option>
            <option value="WARN">WARN</option>
            <option value="ERROR">ERROR</option>
            <option value="FATAL">FATAL</option>
          </select>
        </div>        <div class="search-box">
          <span class="search-icon">🔍</span>
          <input
            v-model="filterText"
            type="text"
            placeholder="Filter logs..."
            class="filter-input"
          />
        </div>

        <div class="actions">
          <div class="font-controls">
            <button @click="increaseFontSize" class="action-btn mini" title="Increase Font Size">▲</button>
            <button @click="decreaseFontSize" class="action-btn mini" title="Decrease Font Size">▼</button>
          </div>
          <button
            @click="refreshConnection"
            class="action-btn"
            title="Refresh Connection"
          >
            🔄
          </button>
          <button
            @click="toggleAutoScroll"
            class="action-btn"
            :class="{ active: autoScroll }"
            title="Auto Scroll"
          >
            ⬇
          </button>
          <button @click="clearLogs" class="action-btn" title="Clear Logs">
            🗑
          </button>
        </div>

        <div class="status">
          <span :class="['indicator', isConnected ? 'connected' : 'disconnected']"></span>
          <span class="status-text">{{ isConnected ? 'ONLINE' : 'OFFLINE' }}</span>
        </div>
      </div>
    </header>

    <div class="log-container" ref="logContainer" :style="{ fontSize: fontSize + 'px' }">
      <div v-if="logs.length === 0" class="empty-state">
        Waiting for logs...
      </div>
      <div
        v-for="(log, index) in filteredLogs"
        :key="index"
        class="log-entry"
        :class="log.level ? log.level.toLowerCase() : log.type"
      >
        <span v-if="log.timestamp" class="timestamp">{{ formatTime(log.timestamp) }}</span>
        <span v-if="log.level" class="level">{{ log.level }}</span>
        <span v-if="log.tid" class="tid">TID:{{ log.tid }}</span>
        <span v-if="log.file_name" class="location">[{{ log.file_name }}:{{ log.line }}]</span>
        <span class="message">{{ log.msg }}</span>
      </div>
    </div>

    <div class="toast-container">
      <transition name="fade">
        <div v-if="toast.show" class="toast" :class="toast.type">
          {{ toast.message }}
        </div>
      </transition>
    </div>
  </div>
</template>

<style scoped>
@import url('https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;500;700&display=swap');

.viewer {
  display: flex;
  flex-direction: column;
  height: 100vh;
  width: 100vw;
  background: #1e1e1e;
  color: #d4d4d4;
  font-family: 'JetBrains Mono', 'Consolas', monospace;
}

.header {
  height: 60px;
  padding: 0 1.5rem;
  background-color: #252526;
  border-bottom: 1px solid #333;
  display: flex;
  align-items: center;
  justify-content: space-between;
  box-shadow: 0 4px 6px rgba(0,0,0,0.3);
  z-index: 10;
}

.brand {
  display: flex;
  align-items: center;
  gap: 10px;
}

.logo-icon {
  height: 32px;
  width: auto;
  border-radius: 4px;
}

.header h1 {
  margin: 0;
  font-size: 1.1rem;
  font-weight: 700;
  color: #e0e0e0;
  letter-spacing: -0.5px;
}

.subtitle {
  color: #4caf50;
  font-weight: 400;
  opacity: 0.8;
}

.controls {
  display: flex;
  gap: 15px;
  align-items: center;
}

.level-selector {
  display: flex;
  align-items: center;
  gap: 8px;
}

.control-label {
  font-size: 0.8rem;
  color: #aaa;
  font-weight: 500;
}

.level-select {
  background: #3c3c3c;
  border: 1px solid #444;
  color: #fff;
  padding: 6px 10px;
  border-radius: 4px;
  font-family: inherit;
  font-size: 0.85rem;
  cursor: pointer;
  transition: all 0.2s;
}

.level-select:focus {
  outline: none;
  border-color: #4caf50;
  background: #444;
}

.search-box {
  position: relative;
  display: flex;
  align-items: center;
}

.search-icon {
  position: absolute;
  left: 8px;
  font-size: 0.8rem;
  opacity: 0.5;
}

.filter-input {
  background: #3c3c3c;
  border: 1px solid #444;
  color: #fff;
  padding: 6px 10px 6px 30px;
  border-radius: 4px;
  font-family: inherit;
  font-size: 0.85rem;
  width: 200px;
  transition: all 0.2s;
}

.filter-input:focus {
  outline: none;
  border-color: #4caf50;
  background: #444;
}

.actions {
  display: flex;
  gap: 5px;
}

.action-btn {
  background: #3c3c3c;
  border: 1px solid #444;
  color: #ccc;
  width: 32px;
  height: 32px;
  border-radius: 4px;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.2s;
}

.action-btn:hover {
  background: #505050;
  color: #fff;
}

.font-controls {
  display: flex;
  flex-direction: column;
  gap: 2px;
  margin-right: 5px;
}

.action-btn.mini {
  width: 24px;
  height: 14px;
  font-size: 8px;
  line-height: 1;
  padding: 0;
  border-radius: 2px;
  display: flex;
  align-items: center;
  justify-content: center;
}

.action-btn.active {
  background: #4caf50;
  color: #fff;
  border-color: #4caf50;
}

.status {
  display: flex;
  align-items: center;
  font-size: 0.75rem;
  background: #111;
  padding: 4px 8px;
  border-radius: 4px;
  border: 1px solid #333;
}

.indicator {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  margin-right: 6px;
  background-color: #555;
  box-shadow: 0 0 5px rgba(0,0,0,0.5);
}

.indicator.connected {
  background-color: #4caf50;
  box-shadow: 0 0 8px #4caf50;
}
.indicator.disconnected {
  background-color: #f44336;
  box-shadow: 0 0 8px #f44336;
}

.status-text {
  font-weight: 600;
  letter-spacing: 0.5px;
}

.log-container {
  flex: 1;
  overflow-y: auto;
  padding: 0.5rem 0;
  font-size: 0.85rem;
  background: #1e1e1e;
  scroll-behavior: smooth;
}

.empty-state {
  display: flex;
  align-items: center;
  justify-content: center;
  height: 100%;
  color: #555;
  font-style: italic;
}

.log-entry {
  padding: 2px 1.5rem;
  display: flex;
  align-items: baseline;
  gap: 10px;
  border-left: 3px solid transparent;
  transition: background 0.1s;
  font-family: 'JetBrains Mono', monospace;
}

.log-entry:hover {
  background: #2a2d2e;
}

.timestamp { color: #858585; font-size: 0.8em; white-space: nowrap; }
.level { font-weight: 700; font-size: 0.8em; padding: 1px 4px; border-radius: 2px; min-width: 45px; text-align: center; white-space: nowrap; }
.tid { color: #569cd6; font-size: 0.8em; white-space: nowrap; }
.location { color: #6a9955; font-size: 0.8em; white-space: nowrap; }
.message { color: #cccccc; white-space: pre-wrap; word-break: break-all; flex: 1; }

/* Log Levels Styling - Matching aw_logger_settings.json */
.info .level { color: #00ffff; background: rgba(0, 255, 255, 0.1); } /* cyan */
.info { border-left-color: #00ffff; }

.warn .level { color: #ffff00; background: rgba(255, 255, 0, 0.1); } /* yellow */
.warn { border-left-color: #ffff00; }

.error .level { color: #ff0000; background: rgba(255, 0, 0, 0.1); } /* red */
.error { border-left-color: #ff0000; }

.fatal .level { color: #ff00ff; background: rgba(255, 0, 255, 0.1); } /* magenta */
.fatal { border-left-color: #ff00ff; background: rgba(255, 0, 255, 0.05); }

.debug .level { color: #ffffff; background: rgba(255, 255, 255, 0.1); } /* white */
.debug { border-left-color: #ffffff; }

.notice .level { color: #448aff; background: rgba(68, 138, 255, 0.1); } /* blue (adjusted for dark mode) */
.notice { border-left-color: #448aff; }

.system {
  color: #808080;
  font-style: italic;
  padding: 5px 1.5rem;
  justify-content: center;
  border-bottom: 1px dashed #333;
}

/* Toast Styling */
.toast-container {
  position: fixed;
  top: 80px;
  left: 50%;
  transform: translateX(-50%);
  z-index: 100;
  pointer-events: none;
}

.toast {
  background: #333;
  color: #fff;
  padding: 10px 20px;
  border-radius: 4px;
  box-shadow: 0 4px 12px rgba(0,0,0,0.5);
  font-size: 0.9rem;
  border: 1px solid #444;
  display: flex;
  align-items: center;
  justify-content: center;
}

.toast.info { border-color: #4caf50; color: #4caf50; }

.fade-enter-active, .fade-leave-active {
  transition: opacity 0.3s, transform 0.3s;
}
.fade-enter-from, .fade-leave-to {
  opacity: 0;
  transform: translateY(-10px);
}

/* Scrollbar styling */
::-webkit-scrollbar {
  width: 10px;
}

::-webkit-scrollbar-track {
  background: #1e1e1e;
}

::-webkit-scrollbar-thumb {
  background: #424242;
  border-radius: 5px;
}

::-webkit-scrollbar-thumb:hover {
  background: #4f4f4f;
}
</style>
