const { app, BrowserWindow, ipcMain, net, Menu } = require('electron')
const path = require('node:path')
const BackendManager = require('./backendManager')

let currentUser = ''
let backendManager = null

const createWindow = () => {
  const win = new BrowserWindow({
    width: 1000,
    height: 700,
    minWidth: 800,
    minHeight: 600,
    icon: path.join(__dirname, '..', '..', 'assets', 'logo.png'),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: false,
      contextIsolation: true
    },
    title: "Mine-Launcher",
    autoHideMenuBar: true
  })

  // Remove default menu
  Menu.setApplicationMenu(null)

  win.loadFile('src/renderer/pages/login.html')
  backendManager = new BackendManager(win)
}

app.whenReady().then(() => {
  ipcMain.handle('ping', () => 'pong')

  ipcMain.on('login', (event, username) => {
    currentUser = username
    const webContents = event.sender
    const win = BrowserWindow.fromWebContents(webContents)
    win.loadFile('src/renderer/pages/launcher.html')
  })

  ipcMain.handle('get-username', () => {
    return currentUser
  })

  ipcMain.on('launch-game', (event, version) => {
    if (backendManager) {
      backendManager.launch(version, currentUser)
    }
  })

  ipcMain.handle('get-versions', async () => {
    return new Promise((resolve, reject) => {
      const request = net.request('https://piston-meta.mojang.com/mc/game/version_manifest_v2.json')
      request.on('response', (response) => {
        let data = ''
        response.on('data', (chunk) => {
          data += chunk
        })
        response.on('end', () => {
          try {
            const json = JSON.parse(data)
            const versions = json.versions
              .filter(v => v.type === 'release')
              .filter(v => {
                const parts = v.id.split('.')
                if (parts.length < 2) return false
                const major = parseInt(parts[0])
                const minor = parseInt(parts[1])
                return (major > 1) || (major === 1 && minor >= 16)
              })
              .map(v => v.id)
            resolve(versions)
          } catch (e) {
            reject(e)
          }
        })
      })
      request.on('error', (error) => {
        reject(error)
      })
      request.end()
    })
  })

  createWindow()

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow()
    }
  })
})

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit()
  }
})