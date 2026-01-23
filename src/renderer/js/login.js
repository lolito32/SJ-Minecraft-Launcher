const loginBtn = document.getElementById('loginBtn')
const usernameInput = document.getElementById('username')

loginBtn.addEventListener('click', () => {
  const username = usernameInput.value
  if (username) {
    window.api.login(username)
  }
})