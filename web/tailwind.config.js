/** @type {import('tailwindcss').Config} */
export default {
  // 使用class策略切换深色/浅色模式
  darkMode: 'class',
  content: [
    "./index.html",
    "./src/**/*.{vue,js,ts,jsx,tsx}",
  ],
  theme: {
    // 扩展screens断点，支持小屏手机
    screens: {
      'xs': '320px',   // 小屏手机 (iPhone SE等)
      'sm': '375px',   // 标准手机 (iPhone 6/7/8)
      'md': '768px',   // 平板
      'lg': '1024px',  // 桌面
      'xl': '1280px',  // 大屏桌面
    },
    extend: {
      colors: {
        primary: '#165DFF',
        secondary: '#F5F7FA',
        success: '#00B42A',
        warning: '#FF7D00',
        danger: '#F53F3F',
        info: '#86909C'
      },
      // 安全区域间距
      spacing: {
        'safe-top': 'env(safe-area-inset-top)',
        'safe-bottom': 'env(safe-area-inset-bottom)',
        'safe-left': 'env(safe-area-inset-left)',
        'safe-right': 'env(safe-area-inset-right)',
      },
      // 最小触控尺寸
      minHeight: {
        'touch': '44px',
      },
      minWidth: {
        'touch': '44px',
      },
      animation: {
        'fade-in': 'fadeIn 0.3s ease-out',
        'slide-up': 'slideUp 0.3s ease-out',
        'spin-slow': 'spin 2s linear infinite'
      },
      keyframes: {
        fadeIn: {
          '0%': { opacity: '0' },
          '100%': { opacity: '1' }
        },
        slideUp: {
          '0%': { opacity: '0', transform: 'translateY(10px)' },
          '100%': { opacity: '1', transform: 'translateY(0)' }
        }
      }
    },
  },
  plugins: [],
}
