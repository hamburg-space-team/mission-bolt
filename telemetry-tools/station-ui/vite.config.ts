import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// Relative base, so the bundle survives being moved under a sub-path.
export default defineConfig({
  plugins: [react()],
  base: "./",
  build: {
    outDir: "dist",
    emptyOutDir: true,
    // one file each: fewer requests, faster first paint on a Pi
    rollupOptions: {
      output: {
        entryFileNames: "app.js",
        chunkFileNames: "app-[hash].js",
        assetFileNames: "app[extname]",
      },
    },
  },
  server: {
    // `npm run dev` behaves like the station's own single origin
    proxy: {
      "/api": { target: "http://bolt-station.local:8080", changeOrigin: true },
    },
  },
});
