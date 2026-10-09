/**
 * Runs the scenes headless and writes raw RGB24 frames (416×276), to compare with
 * the Hatari reference recording (see compare.py).
 * usage: vite-node scripts/render.ts -- <out.rgb> <vbls> [vbl:key ...]
 * VBL 0 is the first VBL of the menu scene (it fades in from black after 6 VBLs).
 */
import { writeFileSync } from 'node:fs';
import { Display, FRAME_H, FRAME_W } from '../src/st/display';
import type { App, Scene } from '../src/scenes/scene';
import { MenuScene } from '../src/scenes/menu';
import { ENIGMA } from '../src/assets';
import type { Music } from '../src/audio/music';

const [out, vbls, ...events] = process.argv.slice(2).filter((a) => a !== '--');
const keys = new Map<number, string>();
for (const e of events) {
  const [v, k] = e.split(':');
  keys.set(Number(v), k);
}

let image: { data: Uint8ClampedArray } | null = null;
const ctx = {
  createImageData: (w: number, h: number) => ({ width: w, height: h, data: new Uint8ClampedArray(w * h * 4) }),
  putImageData: (img: { data: Uint8ClampedArray }) => (image = img),
} as unknown as CanvasRenderingContext2D;

const music = {
  load: () => Promise.resolve(new ArrayBuffer(0)),
  play: () => Promise.resolve(),
  stop() {},
  fadeOut() {},
  current: () => null,
} as unknown as Music;

let scene: Scene;
const app: App = {
  display: new Display(ctx),
  music,
  playing: ENIGMA,
  cursor: ENIGMA,
  authentic: true,
  go: (s) => (scene = s),
};
scene = new MenuScene(app, undefined, 6);

const n = Number(vbls);
const rgb = Buffer.alloc(n * FRAME_W * FRAME_H * 3);
for (let v = 0; v < n; v++) {
  const k = keys.get(v);
  if (k) scene.key?.(k);
  scene.draw(app.display);
  app.display.render();
  const src = image!.data;
  for (let i = 0, o = v * FRAME_W * FRAME_H * 3; i < FRAME_W * FRAME_H; i++, o += 3) {
    rgb[o] = src[i * 4];
    rgb[o + 1] = src[i * 4 + 1];
    rgb[o + 2] = src[i * 4 + 2];
  }
  scene.vbl();
  // let the fake music promises resolve
  await Promise.resolve();
}
writeFileSync(out, rgb);
console.log(`${n} frames -> ${out}`);
