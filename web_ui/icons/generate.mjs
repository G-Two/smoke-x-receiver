// Generates the PWA/app icons and favicon from icons/icon.svg into public/.
// Run with `npm run icons` after editing icon.svg. Uses sharp (dev-only) so it
// works cross-platform without ImageMagick/Chrome.
import sharp from "sharp"
import { readFileSync, writeFileSync } from "node:fs"
import { fileURLToPath } from "node:url"
import { dirname, join } from "node:path"

const here = dirname(fileURLToPath(import.meta.url))
const svg = readFileSync(join(here, "icon.svg"))
const pub = join(here, "..", "public")

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
  await sharp(svg).resize(192, 192).png().toFile(join(pub, "icon-192.png"))
  await sharp(svg).resize(512, 512).png().toFile(join(pub, "icon-512.png"))

  const favicon = buildIco([
    { size: 16, data: await render(16) },
    { size: 32, data: await render(32) },
  ])
  writeFileSync(join(pub, "favicon.ico"), favicon)

  console.log("Generated icon-192.png, icon-512.png, favicon.ico")
}

main()
