const init = async () => {
    const username = await window.api.getUsername()
    welcome.innerText = `Bienvenido ${username}`
}

init()
