const { spawn } = require('child_process');
const path = require('path');

class BackendManager {
    constructor(window) {
        this.window = window;
        this.backendProcess = null;
    }

    launch(version, username) {
        const backendPath = path.join(__dirname, '..', 'backend', 'backend.exe');
        console.log(`Launching backend from: ${backendPath}`);

        this.backendProcess = spawn(backendPath, [
            '--version', version,
            '--username', username
        ], {
            cwd: path.join(__dirname, '..', 'backend')
        });

        this.backendProcess.stdout.on('data', (data) => {
            const lines = data.toString().split('\n');
            for (const line of lines) {
                if (!line.trim()) continue;
                try {
                    const json = JSON.parse(line);
                    console.log('Backend message:', json);
                    this.window.webContents.send('backend-status', json);
                } catch (e) {
                    console.log('Non-JSON backend output:', line);
                }
            }
        });

        this.backendProcess.stderr.on('data', (data) => {
            console.error(`Backend error: ${data}`);
        });

        this.backendProcess.on('close', (code) => {
            console.log(`Backend process exited with code ${code}`);
            this.window.webContents.send('backend-status', {
                type: 'exit',
                message: `Process exited with code ${code}`,
                progress: 100
            });
        });
    }
}

module.exports = BackendManager;
