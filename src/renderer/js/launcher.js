const welcome = document.getElementById('welcome');
const playBtn = document.getElementById('playBtn');
const versionSelect = document.getElementById('versionSelect');
const progressContainer = document.getElementById('progressContainer');
const progressBar = document.getElementById('progressBar');
const progressText = document.getElementById('progressText');

const init = async () => {
    // Get username
    const username = await window.api.getUsername();
    welcome.innerText = `Bienvenido ${username}`;

    // Fetch versions dynamically
    try {
        const versions = await window.api.getVersions();
        versionSelect.innerHTML = ''; // Clear loading message
        versions.forEach(v => {
            const option = document.createElement('option');
            option.value = v;
            option.innerText = `Minecraft ${v}`;
            versionSelect.appendChild(option);
        });
    } catch (e) {
        console.error('Error fetching versions:', e);
        versionSelect.innerHTML = '<option disabled>Error al cargar versiones</option>';
    }

    // Launch game logic
    playBtn.addEventListener('click', () => {
        const version = versionSelect.value;
        if (!version) return;

        // Show progress bar
        progressContainer.style.display = 'flex';
        progressText.innerText = `Iniciando Minecraft ${version}...`;
        progressBar.style.width = '0%';

        playBtn.disabled = true;
        window.api.launchGame(version);
    });

    // Listen for status updates
    window.api.onStatusUpdate((data) => {
        console.log('Status update:', data);
        if (data.type === 'status') {
            progressText.innerText = data.message;
            // If the backend sends progress, update the bar
            if (data.progress !== undefined && data.progress >= 0) {
                progressBar.style.width = `${data.progress}%`;
            } else {
                // Indeterminate state or just message update
                // We can do a small "fake" progress or just keep it as is
            }
        } else if (data.type === 'error') {
            progressText.innerText = `Error: ${data.message}`;
            progressBar.style.backgroundColor = '#ef4444'; // Red for error
            playBtn.disabled = false;
        } else if (data.type === 'exit') {
            progressText.innerText = 'Juego cerrado.';
            progressBar.style.width = '100%';
            playBtn.disabled = false;
            setTimeout(() => {
                progressContainer.style.display = 'none';
                progressBar.style.backgroundColor = ''; // Reset color
            }, 3000);
        }
    });
}

init();
