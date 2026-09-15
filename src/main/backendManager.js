const GameManager = require('./gameManager')

class BackendManager {
  constructor (window) {
    this.gameManager = new GameManager(window)
  }

  launch (version, username) {
    this.gameManager.launch(version, username)
  }
}

module.exports = BackendManager
