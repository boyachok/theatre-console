import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
export default defineConfig({
  plugins: [react()],
  server: {
    host: '0.0.0.0',
    port: 5173,
    strictPort: true,
    proxy: process.env.CONSOLE_TARGET
      ? { '/api/console': { target: process.env.CONSOLE_TARGET, changeOrigin: true } }
      : undefined,
  },
});
