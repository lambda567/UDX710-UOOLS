import { createApp } from 'vue'
import App from './App.vue'
import './style.css'
import { FontAwesomeIcon } from './plugins/fontawesome'
import { dom } from '@fortawesome/fontawesome-svg-core'

const app = createApp(App)
app.component('font-awesome-icon', FontAwesomeIcon)
app.mount('#app')

// 确保 Vue 挂载后重新扫描 DOM 中的图标
setTimeout(() => dom.i2svg(), 100)
