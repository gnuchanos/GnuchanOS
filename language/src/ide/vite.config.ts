import { fileURLToPath, URL } from "node:url";
import react from "@vitejs/plugin-react";
import { defineConfig } from "vite";

/* root: "." yerine config'in bulundugu dizini mutlak olarak ver —
 * boylece build hem IDE dizininden (npm run build) hem de disaridan
 * `vite build --config path/to/vite.config.ts` ile cagrildiginda
 * index.html'i dogru yerde bulur. */
const ideRoot = fileURLToPath(new URL(".", import.meta.url));

export default defineConfig({
  plugins: [react()],
  root: ideRoot,
  base: "./",
  build: {
    outDir: fileURLToPath(new URL("dist", import.meta.url)),
    emptyOutDir: true,
  },
  server: {
    port: 5173,
  },
});

