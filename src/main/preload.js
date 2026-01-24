const { contextBridge, ipcRenderer } = require('electron')

contextBridge.exposeInMainWorld('api', {
  node: () => process.versions.node,
  chrome: () => process.versions.chrome,
  electron: () => process.versions.electron,
  ping: () => ipcRenderer.invoke('ping'),
  login: (username) => ipcRenderer.send('login', username),
  getUsername: () => ipcRenderer.invoke('get-username'),
  launchGame: (version) => ipcRenderer.send('launch-game', version),
  onStatusUpdate: (callback) => ipcRenderer.on('backend-status', (event, data) => callback(data)),
  getVersions: () => ipcRenderer.invoke('get-versions')
})