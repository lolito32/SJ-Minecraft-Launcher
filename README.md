# SJ-Minecraft-Launcher

Un launcher de Minecraft moderno, rápido y profesional construido con **Electron** y un potente backend en **C**.

## 🚀 Características

- **Rendimiento Extremo**: Backend escrito en C con descargas paralelas (`curl_multi`) para una velocidad inigualable.
- **Optimización de Juego**: Incluye flags de JVM avanzados (`G1GC`) para minimizar el stuttering y maximizar los FPS.
- **Interfaz Moderna**: Diseño minimalista, elegante y fluido con animaciones premium.
- **Versiones Dinámicas**: Obtiene automáticamente todas las versiones de Minecraft desde la 1.16 en adelante.
- **Ligero**: Consumo mínimo de recursos gracias a su arquitectura híbrida.

## 🛠️ Requisitos

- **Node.js** (v18 o superior)
- **GCC** (para compilar el backend)
- **libcurl** (dependencia del backend)
- **Java** (recomendado Java 17+ para versiones modernas de Minecraft)

## 📦 Instalación y Uso

1. **Clonar el repositorio**:
   ```bash
   git clone https://github.com/lolito32/SJ-Minecraft-Launcher.git
   cd SJ-Minecraft-Launcher
   ```

2. **Instalar dependencias de Electron**:
   ```bash
   npm install
   ```

3. **Compilar el Backend**:
   Navega a la carpeta del backend y ejecuta el script de compilación:
   ```bash
   cd src/backend
   ./build.bat
   ```

4. **Iniciar el Launcher**:
   ```bash
   npm start
   ```

## 🏗️ Estructura del Proyecto

- `src/main`: Proceso principal de Electron y gestión del backend.
- `src/renderer`: Interfaz de usuario (HTML, CSS, JS).
- `src/backend`: Código fuente en C para la lógica de descarga y lanzamiento.
- `assets`: Recursos visuales y logos.

## 📄 Licencia

Este proyecto está bajo la Licencia MIT. Consulta el archivo [LICENSE](LICENSE) para más detalles.

---
Desarrollado con ❤️ para la comunidad de Minecraft.
