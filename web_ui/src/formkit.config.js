import "@formkit/themes/genesis"
import { genesisIcons } from "@formkit/icons"
import { defaultConfig } from "@formkit/vue"

const config = defaultConfig({
  icons: { ...genesisIcons },
  plugins: [],
})

export default config
