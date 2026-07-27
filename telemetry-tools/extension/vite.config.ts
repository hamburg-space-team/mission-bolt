import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// Bundles the React webviews into media/webview/<name>.js (+ .css) with a
// shared vendor chunk. Loaded by the host as ES modules under the webview CSP.
export default defineConfig({
  plugins: [react()],
  build: {
    outDir: "media/webview",
    emptyOutDir: true,
    sourcemap: true,
    cssCodeSplit: true,
    manifest: true,
    minify: false,
    target: "es2020",
    rollupOptions: {
      input: {
        feed: "webview/src/entries/feed.tsx",
        overview: "webview/src/entries/overview.tsx",
        packets: "webview/src/entries/packets.tsx",
        experiment: "webview/src/entries/experiment.tsx",
        selftest: "webview/src/entries/selftest.tsx",
      },
      output: {
        entryFileNames: "[name].js",
        chunkFileNames: "chunk-[hash].js",
        assetFileNames: "[name][extname]",
      },
    },
  },
});
