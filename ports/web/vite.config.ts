import { defineConfig, type Plugin } from 'vite';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { decode } from 'fast-png';

const RIP = fileURLToPath(new URL('../../rip', import.meta.url));

/**
 * `import img from '@rip/graphics/x_indexed.png?indexed'` gives the raw palette
 * indices of an indexed PNG ({ width, height, data: Uint8Array }), one byte per pixel.
 * The browser can't give us indices (canvas decodes to RGB), so we do it at build time.
 * `import b from '…/file.bin?bytes'` inlines a binary file as a Uint8Array.
 */
function indexedPng(): Plugin {
  return {
    name: 'indexed-png',
    enforce: 'pre',
    async resolveId(source, importer) {
      const q = ['?indexed', '?bytes'].find((s) => source.endsWith(s));
      if (!q) return null;
      const r = await this.resolve(source.slice(0, -q.length), importer, { skipSelf: true });
      return r ? r.id + q : null;
    },
    load(id) {
      if (id.endsWith('?bytes')) {
        const file = id.slice(0, -'?bytes'.length);
        this.addWatchFile(file);
        const b64 = readFileSync(file).toString('base64');
        return `const s = atob(${JSON.stringify(b64)}); const d = new Uint8Array(s.length);
for (let i = 0; i < s.length; i++) d[i] = s.charCodeAt(i);
export default d;`;
      }
      if (!id.endsWith('?indexed')) return null;
      const file = id.slice(0, -'?indexed'.length);
      this.addWatchFile(file);
      const png = decode(readFileSync(file));
      if (!png.palette) throw new Error(`${file}: not an indexed PNG`);
      const { width, height, depth } = png;
      const src = png.data as Uint8Array;
      const out = new Uint8Array(width * height);
      const stride = Math.ceil((width * depth) / 8);
      const mask = (1 << depth) - 1;
      for (let y = 0; y < height; y++)
        for (let x = 0; x < width; x++) {
          const bit = x * depth;
          out[y * width + x] = (src[y * stride + (bit >> 3)] >> (8 - depth - (bit & 7))) & mask;
        }
      const b64 = Buffer.from(out).toString('base64');
      return `const s = atob(${JSON.stringify(b64)}); const d = new Uint8Array(s.length);
for (let i = 0; i < s.length; i++) d[i] = s.charCodeAt(i);
export default { width: ${width}, height: ${height}, data: d };`;
    },
  };
}

export default defineConfig({
  base: './',
  resolve: { alias: { '@rip': RIP } },
  server: { fs: { allow: ['.', RIP] } },
  plugins: [indexedPng()],
  build: { target: 'es2022', assetsInlineLimit: 0 },
});
