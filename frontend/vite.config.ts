import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// In development the page is served by Vite (port 5173) and /api is forwarded to the C++ backend (port 8080)
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      "/api": "http://localhost:8080",
    },
  },
});
