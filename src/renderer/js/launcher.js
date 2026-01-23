const welcome = document.getElementById('welcome')
const info = document.getElementById('info')

const init = async () => {
    const username = await window.api.getUsername()
    welcome.innerText = `Welcome ${username}`

    info.innerText = `This app is using Chrome (v${window.api.chrome()}), Node.js (v${window.api.node()}), and Electron (v${window.api.electron()})`
}

init()
