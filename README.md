# SJ Minecraft Launcher

Launcher de Minecraft (Electron) que descarga las versiones oficiales desde Mojang y las lanza con Java, sin depender de binarios compilados.

## Requisitos

- [Node.js](https://nodejs.org) 18+ (probado con 24)
- [Java](https://www.oracle.com/java/technologies/downloads/) según la versión de Minecraft:
  - 1.16 – 1.16.5: Java 8
  - 1.17.x: Java 16+
  - 1.18 y superior: Java 17+
- Windows (usa funcionalidades del proceso principal de Electron)

## Ejecución

```bash
npm install
npm run dev
```

Al abrir la app: inicia sesión con tu nombre de usuario, elige una versión en el desplegable y pulsa **Iniciar**. El launcher descargará el manifest, el `client.jar`, las librerías, los assets y las natives, y lanzará el juego.

Las descargas se guardan en `%APPDATA%\MiLauncher` (solo se re-descarga lo que falte).

## Funcionamiento

Aunque el proyecto original usaba un backend en C (`src/backend/backend.exe`), ese binario nunca se compiló, por lo que el lanzador no funcionaba. Actualmente toda la lógica de descarga y lanzamiento vive en Node.js:

- `src/main/gameManager.js`: descarga manifest → JSON de versión → client jar → librerías (reglas de OS) → assets → natives, arma el classpath e invoca Java.
- `src/main/backendManager.js`: delega en `GameManager`.
- `src/main/main.js`: IPC entre la UI y el gestor del juego.

El backend en C (`src/backend/`) quedó deprecado y no se usa.

## Build (opcional)

```bash
npm run build
```

Genera el empaquetado con [electron-builder](https://www.electron.build/) en `dist/`.