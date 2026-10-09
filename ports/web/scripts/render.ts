/**
 * Runs the scenes headless and writes raw RGB24 frames (416×276), to compare with
 * the Hatari reference recording (see compare.py).
 * usage: vite-node scripts/render.ts -- <menu|intro> <out.rgb> <vbls> [vbl:key ...]
 * menu: VBL 0 is the first VBL of the menu scene = reference VBL 9024.
 * intro: VBL 0 is the ear screen's first VBL = reference VBL 546.
 * text: VBL 0 is the text pages' first VBL; ENIGMA_TICKS=n replays n ticks of Enigma
 *   before it (the analysers follow the music).
 */
import { writeFileSync } from 'node:fs';
import { Display, FRAME_H, FRAME_W } from '../src/st/display';
import type { App, Scene } from '../src/scenes/scene';
import { MenuScene } from '../src/scenes/menu';
import { IntroScene } from '../src/scenes/intro';
import { TextPageScene } from '../src/scenes/textpage';
import { parseMod, Replayer, ST_VBL_HZ, type TickInfo } from '../src/audio/mod';
import { readFileSync } from 'node:fs';
import { ENIGMA } from '../src/assets';
import type { Music } from '../src/audio/music';

const [which, out, vbls, ...events] = process.argv.slice(2).filter((a) => a !== '--');
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
// For the text pages: Enigma replayed one tick per VBL (1 sample per tick).
let lastTick: TickInfo | null = null;
const enigma = readFileSync(new URL('../../../rip/music/04 - Enigma aka Hyperbased (Firefox & Tip).mod', import.meta.url));
const replay = new Replayer(parseMod(enigma.buffer.slice(enigma.byteOffset, enigma.byteOffset + enigma.byteLength) as ArrayBuffer),
  ST_VBL_HZ, ST_VBL_HZ, (t) => (lastTick = t));
const one = new Float32Array(1);
const replayVbl = () => replay.render(one, one, 0, 1);
for (let i = 0; i < Number(process.env.ENIGMA_TICKS ?? 0); i++) replayVbl();

scene = which === 'intro' ? new IntroScene(app) : which === 'text' ? new TextPageScene(app, () => lastTick) : new MenuScene(app);

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
  if (which === 'text') replayVbl();
  // let the fake music promises resolve
  await Promise.resolve();
}
writeFileSync(out, rgb);
console.log(`${n} frames -> ${out}`);
