// Generates the app icons and favicon from icons/icon.svg into two places from
// this single source, so they never drift:
//   - web_ui/public/  (firmware-served web UI: PWA manifest icons + favicon)
//   - docs/           (GitHub Pages installer page: favicon + apple-touch icon)
// Run with `npm run icons` after editing icon.svg. Uses sharp (dev-only) so it
// works cross-platform without ImageMagick/Chrome.
import sharp from "sharp"
import { readFileSync, writeFileSync, copyFileSync } from "node:fs"
import { fileURLToPath } from "node:url"
import { dirname, join } from "node:path"

const here = dirname(fileURLToPath(import.meta.url))
const svgPath = join(here, "icon.svg")
const svg = readFileSync(svgPath)
const pub = join(here, "..", "public")
const docs = join(here, "..", "..", "docs")

const render = (size) => sharp(svg).resize(size, size).png().toBuffer()

// Assemble a PNG-in-ICO (widely supported) from one or more PNG buffers.
function buildIco(images) {
  const header = Buffer.alloc(6)
  header.writeUInt16LE(0, 0) // reserved
  header.writeUInt16LE(1, 2) // type: icon
  header.writeUInt16LE(images.length, 4)
  let offset = 6 + 16 * images.length
  const dir = Buffer.concat(
    images.map(({ size, data }) => {
      const e = Buffer.alloc(16)
      e.writeUInt8(size >= 256 ? 0 : size, 0) // width
      e.writeUInt8(size >= 256 ? 0 : size, 1) // height
      e.writeUInt16LE(1, 4) // color planes
      e.writeUInt16LE(32, 6) // bits per pixel
      e.writeUInt32LE(data.length, 8) // image size
      e.writeUInt32LE(offset, 12) // image offset
      offset += data.length
      return e
    })
  )
  return Buffer.concat([header, dir, ...images.map((i) => i.data)])
}

async function main() {
  const png192 = await render(192)
  const png512 = await render(512)
  const favicon = buildIco([
    { size: 16, data: await render(16) },
    { size: 32, data: await render(32) },
  ])

  // Firmware-served web UI (PWA manifest icons + favicon).
  writeFileSync(join(pub, "icon-192.png"), png192)
  writeFileSync(join(pub, "icon-512.png"), png512)
  writeFileSync(join(pub, "favicon.ico"), favicon)

  // GitHub Pages installer page (docs/) is served as committed static files,
  // so regenerate its copies from the same source here to avoid drift.
  writeFileSync(join(docs, "icon-192.png"), png192)
  writeFileSync(join(docs, "favicon.ico"), favicon)
  copyFileSync(svgPath, join(docs, "icon.svg"))

  console.log("Generated icons into web_ui/public/ and docs/")
}

main()
