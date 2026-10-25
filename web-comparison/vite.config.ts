import { defineConfig } from 'vitest/config';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import tailwindcss from '@tailwindcss/vite';

export default defineConfig({
  plugins: [svelte(), tailwindcss()],
  base: './',
  build: { outDir: '../.tools/web-comparison/frontend', emptyOutDir: true },
  resolve: { conditions: ['browser'] },
  test: { environment: 'jsdom', setupFiles: ['tests/setup.ts'], include: ['tests/**/*.test.ts'] },
});
