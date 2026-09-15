const { spawn, spawnSync } = require('node:child_process')
const fs = require('node:fs')
const fsp = fs.promises
const path = require('node:path')
const os = require('node:os')

const USER_AGENT = 'SJ-Minecraft-Launcher/1.0'
const MANIFEST_URL = 'https://piston-meta.mojang.com/mc/game/version_manifest_v2.json'
const ASSET_BASE_URL = 'https://resources.download.minecraft.net'

const appData = process.env.APPDATA || os.homedir()
const BASE_DIR = path.join(appData, 'MiLauncher')
const VERSIONS_DIR = path.join(BASE_DIR, 'versions')
const LIBRARIES_DIR = path.join(BASE_DIR, 'libraries')
const ASSETS_DIR = path.join(BASE_DIR, 'assets')
const NATIVES_DIR = path.join(BASE_DIR, 'natives')

function makeDirs () {
  for (const dir of [BASE_DIR, VERSIONS_DIR, LIBRARIES_DIR, ASSETS_DIR, NATIVES_DIR]) {
    fs.mkdirSync(dir, { recursive: true })
  }
}

async function fetchJson (url) {
  const res = await fetch(url, { headers: { 'User-Agent': USER_AGENT } })
  if (!res.ok) throw new Error(`HTTP ${res.status} al obtener ${url}`)
  return res.json()
}

async function download (url, destPath, onProgress) {
  const res = await fetch(url, { redirect: 'follow', headers: { 'User-Agent': USER_AGENT } })
  if (!res.ok) throw new Error(`HTTP ${res.status} al descargar ${url}`)
  const total = Number(res.headers.get('content-length') || 0)
  if (!res.body) throw new Error(`No se pudo leer ${url}`)
  await fsp.mkdir(path.dirname(destPath), { recursive: true })
  const writer = fs.createWriteStream(destPath)
  const reader = res.body.getReader()
  let received = 0
  try {
    while (true) {
      const { done, value } = await reader.read()
      if (done) break
      received += value.length
      writer.write(value)
      if (onProgress && total > 0) onProgress(received, total)
    }
    await new Promise((resolve, reject) => {
      writer.once('error', reject)
      writer.end(() => resolve())
    })
  } catch (e) {
    try { fs.unlinkSync(destPath) } catch (_) {}
    throw e
  }
}

async function downloadWithRetry (url, destPath, onProgress, attempts = 3) {
  for (let i = 1; i <= attempts; i++) {
    try {
      return await download(url, destPath, onProgress)
    } catch (e) {
      if (i === attempts) throw e
    }
  }
}

function isRuleAllowed (rules) {
  if (!Array.isArray(rules)) return true
  let allowed = false
  for (const rule of rules) {
    if (!rule || typeof rule.action !== 'string') continue
    let applies = true
    if (rule.os) {
      const osName = rule.os.name
      const arch = rule.os.arch
      if (osName && osName !== 'windows') applies = false
      if (applies && arch && arch !== process.arch) applies = false
    }
    if (applies) allowed = rule.action === 'allow'
  }
  return allowed
}

function findJava () {
  const candidates = []
  const javaHome = process.env.JAVA_HOME
  if (javaHome) candidates.push(path.join(javaHome, 'bin', 'java.exe'))
  const pathEnv = (process.env.Path || process.env.PATH || '').split(path.delimiter)
  for (const dir of pathEnv) {
    if (!dir) continue
    candidates.push(path.join(dir, 'java.exe'))
  }
  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) return candidate
  }
  return candidates[0] || 'java'
}

function getJavaMajorVersion (javaExe) {
  try {
    const res = spawnSync(javaExe, ['-version'], { encoding: 'utf8' })
    const out = `${res.stderr || ''}\n${res.stdout || ''}`
    const m = out.match(/version "([0-9._]+)/)
    if (!m) return 0
    const parts = m[1].split('.')
    const major = parseInt(parts[0], 10)
    return major === 1 ? parseInt(parts[1], 10) : major
  } catch (e) {
    return 0
  }
}

function requiredJavaMajor (mcVersionId) {
  const parts = mcVersionId.split('.')
  const major = parseInt(parts[0], 10)
  const minor = parts[1] ? parseInt(parts[1], 10) : 0
  if (major < 1 || (major === 1 && minor < 17)) return 8
  if (major === 1 && minor === 17) return 16
  return 17
}

function extractJar (jarPath, destDir) {
  fs.mkdirSync(destDir, { recursive: true })
  const tar = spawnSync('tar', ['-xf', jarPath, '-C', destDir], { stdio: 'pipe' })
  if (tar.status === 0) return
  const zipTmp = `${jarPath}.zip`
  fs.copyFileSync(jarPath, zipTmp)
  const ps = spawnSync('powershell', ['-NoProfile', '-Command',
    `Expand-Archive -LiteralPath '${zipTmp}' -DestinationPath '${destDir}' -Force`], { stdio: 'pipe' })
  try { fs.unlinkSync(zipTmp) } catch (_) {}
  if (ps.status !== 0) throw new Error(`No se pudo extraer ${jarPath}`)
}

class GameManager {
  constructor (window) {
    this.window = window
  }

  send (type, message, progress) {
    if (this.window) {
      this.window.webContents.send('backend-status', { type, message, progress })
    }
  }

  error (message) {
    console.error('GameManager error:', message)
    this.send('error', message, -1)
  }

  async launch (versionId, username) {
    try {
      makeDirs()

      const javaExe = findJava()
      const javaMajor = getJavaMajorVersion(javaExe)
      const reqJava = requiredJavaMajor(versionId)
      if (javaMajor < reqJava) {
        this.error(`Minecraft ${versionId} requiere Java ${reqJava}+ y el launcher encontró Java ${javaMajor || 'desconocido'}. Instala una versión compatible.`)
        return
      }

      this.send('status', 'Obteniendo manifiesto de versiones...', 0)
      const manifest = await fetchJson(MANIFEST_URL)
      const entry = manifest.versions.find((v) => v.id === versionId)
      if (!entry) {
        this.error(`La versión ${versionId} no existe en Mojang.`)
        return
      }

      this.send('status', `Descargando información de Minecraft ${versionId}...`, 0)
      const versionJsonPath = path.join(VERSIONS_DIR, versionId, `${versionId}.json`)
      await downloadWithRetry(entry.url, versionJsonPath)
      const versionJson = JSON.parse(await fsp.readFile(versionJsonPath, 'utf8'))

      const clientUrl = versionJson?.downloads?.client?.url
      const clientJarPath = path.join(VERSIONS_DIR, versionId, `${versionId}.jar`)
      if (clientUrl) {
        this.send('status', `Descargando Minecraft ${versionId}...`, 2)
        await downloadWithRetry(clientUrl, clientJarPath)
      }

      const { classpath, nativeJars } = await this.downloadLibraries(versionJson, versionId)
      await this.downloadAssets(versionJson, versionId)
      this.extractNatives(nativeJars)

      const fullClasspath = classpath + path.delimiter + clientJarPath
      this.launchMinecraft({ versionJson, versionId, username, classpath: fullClasspath, javaExe, javaMajor })
    } catch (e) {
      this.error(e.message || 'Error inesperado al lanzar el juego')
    }
  }

  async downloadLibraries (versionJson, versionId) {
    const libraries = versionJson?.libraries || []
    const jobs = []
    for (const lib of libraries) {
      if (!isRuleAllowed(lib.rules)) continue
      const downloads = lib.downloads
      if (!downloads) continue
      const artifact = downloads.artifact
      if (artifact?.url && artifact?.path) {
        jobs.push({ url: artifact.url, rel: artifact.path })
      }
      const natives = lib.natives
      const classifiers = downloads.classifiers
      if (natives && classifiers) {
        const key = natives.windows
        const nativeArtifact = key ? classifiers[key] : null
        if (nativeArtifact?.url && nativeArtifact?.path) {
          jobs.push({ url: nativeArtifact.url, rel: nativeArtifact.path, native: true })
        }
      }
    }

    const total = jobs.length
    let done = 0
    const nativeJars = []
    for (const job of jobs) {
      const dest = path.join(LIBRARIES_DIR, job.rel)
      if (!fs.existsSync(dest)) {
        this.send('status', `Descargando librerías (${done + 1}/${total})`, 5 + Math.floor((done / total) * 50))
        await downloadWithRetry(job.url, dest)
      }
      if (job.native === true) nativeJars.push(dest)
      done++
    }
    this.send('status', 'Librerías listas', 60)

    const classpath = jobs
      .filter((job) => job.native === undefined)
      .map((job) => path.join(LIBRARIES_DIR, job.rel))
      .join(path.delimiter)

    return { classpath, nativeJars }
  }

  async downloadAssets (versionJson, versionId) {
    const assetIndex = versionJson?.assetIndex
    if (!assetIndex?.url || !assetIndex?.id) return

    const indexPath = path.join(ASSETS_DIR, 'indexes', `${assetIndex.id}.json`)
    this.send('status', 'Descargando índice de assets...', 62)
    await downloadWithRetry(assetIndex.url, indexPath)

    const indexData = JSON.parse(await fsp.readFile(indexPath, 'utf8'))
    const objects = indexData.objects || {}
    const entries = Object.entries(objects)
    const total = entries.length
    let done = 0
    for (const [, obj] of entries) {
      if (!obj || typeof obj.hash !== 'string') continue
      const first2 = obj.hash.slice(0, 2)
      const rel = path.join(first2, obj.hash)
      const dest = path.join(ASSETS_DIR, 'objects', rel)
      if (!fs.existsSync(dest)) {
        this.send('status', `Descargando assets (${done + 1}/${total})...`, 65 + Math.floor((done / total) * 30))
        await downloadWithRetry(`${ASSET_BASE_URL}/${first2}/${obj.hash}`, dest)
      }
      done++
    }
    this.send('status', 'Assets listos', 95)
  }

  extractNatives (nativeJars) {
    fs.rmSync(NATIVES_DIR, { recursive: true, force: true })
    fs.mkdirSync(NATIVES_DIR, { recursive: true })
    for (const jar of nativeJars) {
      if (fs.existsSync(jar)) extractJar(jar, NATIVES_DIR)
    }
  }

  launchMinecraft ({ versionJson, versionId, username, classpath, javaExe, javaMajor }) {
    const assetIndex = versionJson?.assetIndex
    const assetIndexId = assetIndex?.id || 'legacy'
    const mainClass = versionJson?.mainClass || 'net.minecraft.client.main.Main'

    const args = [
      '-Xmx2G', '-Xms1G',
      '-XX:+UnlockExperimentalVMOptions',
      '-XX:+UseG1GC',
      '-XX:G1NewSizePercent=20',
      '-XX:G1ReservePercent=20',
      '-XX:MaxGCPauseMillis=50',
      '-XX:G1HeapRegionSize=32M',
      `-Djava.library.path=${NATIVES_DIR}`,
      `-Dorg.lwjgl.system.librarypath=${NATIVES_DIR}`,
      `-Dlwjgl.librarypath=${NATIVES_DIR}`
    ]

    if (javaMajor >= 9) {
      args.push(
        '--add-opens', 'java.base/java.lang=ALL-UNNAMED',
        '--add-opens', 'java.base/java.util=ALL-UNNAMED',
        '--add-opens', 'java.base/java.io=ALL-UNNAMED',
        '--add-opens', 'java.base/java.nio=ALL-UNNAMED',
        '--add-opens', 'java.base/sun.nio.ch=ALL-UNNAMED',
        '--add-opens', 'java.base/jdk.internal.loader=ALL-UNNAMED',
        '--add-opens', 'java.base/java.net=ALL-UNNAMED'
      )
    }

    args.push(
      '-cp', classpath,
      mainClass,
      '--username', username || 'Player',
      '--version', versionId,
      '--gameDir', BASE_DIR,
      '--assetsDir', ASSETS_DIR,
      '--assetIndex', assetIndexId,
      '--uuid', '00000000-0000-0000-0000-000000000000',
      '--accessToken', 'null',
      '--userType', 'mojang',
      '--versionType', 'release'
    )

    this.send('status', `Iniciando Minecraft ${versionId}...`, 100)

    const proc = spawn(javaExe, args, { cwd: BASE_DIR, windowsHide: false })
    console.log(`Launching java (${javaExe})`)
    let outTail = ''
    let errTail = ''
    proc.stdout.on('data', (d) => {
      outTail = (outTail + d).slice(-8000)
      console.log(`[game] ${d}`)
    })
    proc.stderr.on('data', (d) => {
      errTail = (errTail + d).slice(-8000)
      console.log(`[game] ${d}`)
    })
    proc.on('error', (err) => this.error(`No se pudo iniciar Java: ${err.message}`))
    proc.on('close', (code) => {
      console.log(`Game exited with code ${code}`)
      if (code !== 0) {
        const detail = (errTail.trim() + '\n' + outTail.trim()).trim().split('\n').slice(-5).join(' | ')
        this.send('error', detail ? `El juego falló (código ${code}): ${detail}` : `El juego falló (código ${code})`, 100)
      } else {
        this.send('exit', `Juego cerrado (código ${code})`, 100)
      }
    })
  }
}

module.exports = GameManager
