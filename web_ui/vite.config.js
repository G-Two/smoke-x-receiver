import { defineConfig } from "vite"
import vue from "@vitejs/plugin-vue"
import viteCompression from "vite-plugin-compression"

export default defineConfig({
  build: {
    minify: "terser",
    terserOptions: {},
    rollupOptions: {
      output: {
        entryFileNames: `[name]-[hash].js`,
        chunkFileNames: `[name]-[hash].js`,
        assetFileNames: `[name]-[hash].[ext]`,
      },
    },
  },
  plugins: [
    vue(),
    viteCompression({
      // The firmware only serves pre-gzipped (.gz) files, so every asset it
      // needs to serve must be compressed here — including the PWA manifest.
      filter: "/.(js|css|html|ico|png|webmanifest)$/i",
      threshold: 1,
    }),
  ],
})
