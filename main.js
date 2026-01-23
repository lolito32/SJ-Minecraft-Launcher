const { app, BrowserWindow, ipcMain } = require('electron/main')
const path = require('node:path')

let currentUser = ''

const createWindow = () => {
  const win = new BrowserWindow({
    width: 500,
    height: 650,
    minWidth: 400,
    minHeight: 600,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js')
    }
  })

  win.loadFile('login.html')
}

app.whenReady().then(() => {
  ipcMain.handle('ping', () => 'pong')

  ipcMain.on('login', (event, username) => {
    currentUser = username
    const webContents = event.sender
    const win = BrowserWindow.fromWebContents(webContents)
    win.loadFile('launcher.html')
  })

  ipcMain.handle('get-username', () => {
    return currentUser
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