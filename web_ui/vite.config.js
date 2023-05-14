import { defineConfig } from "vite"
import vue from "@vitejs/plugin-vue"
import viteCompression from "vite-plugin-compression"

export default defineConfig({
  build: {
    minify: "terser",
    terserOptions: {},
    rollupOptions: {
      output: {
        entryFileNames: `[name].js`,
        chunkFileNames: `[name].js`,
        assetFileNames: `[name].[ext]`,
      },
    },
  },
  plugins: [
    vue(),
    viteCompression({
      filter: "/.(js|css|html|ico|png)$/i",
      threshold: 1,
    }),
  ],
})
