/**
 * Renders the screenshots shown in the top-level README, headless, at twice the size
 * (832×552, borders included, nearest neighbour).
 * usage: vite-node scripts/screenshots.ts -- <out dir>
 * The VBL numbers count from the start of each scene, as in render.ts.
 */
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { encode } from 'fast-png';
import { Display, FRAME_H, FRAME_W } from '../src/st/display';
import type { App, Scene } from '../src/scenes/scene';
import { MenuScene } from '../src/scenes/menu';
import { IntroScene } from '../src/scenes/intro';
import { TextPageScene } from '../src/scenes/textpage';
import { parseMod, Replayer, ST_VBL_HZ, type TickInfo } from '../src/audio/mod';
import { ENIGMA } from '../src/assets';
import type { Music } from '../src/audio/music';

const SHOTS: Record<'intro' | 'text' | 'menu', Record<number, string>> = {
  intro: { 200: 'loading', 3600: 'logo', 4400: 'aeon' },
  text: { 1000: 'welcome', 4625: 'credits' },
  menu: { 200: 'menu' },
};
// Enigma's ticks before the text pages, as in the checks (see README.md).
const ENIGMA_TICKS = 2471;
const SCALE = 2;

const [out] = process.argv.slice(2).filter((a) => a !== '--');
mkdirSync(out, { recursive: true });

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

const enigma = readFileSync(new URL('../../../rip/music/04 - Enigma aka Hyperbased (Firefox & Tip).mod', import.meta.url));

function save(name: string) {
  const src = image!.data;
  const w = FRAME_W * SCALE;
  const data = new Uint8Array(w * FRAME_H * SCALE * 3);
  for (let y = 0; y < FRAME_H * SCALE; y++) {
    for (let x = 0; x < w; x++) {
      const i = (((y / SCALE) | 0) * FRAME_W + ((x / SCALE) | 0)) * 4;
      const o = (y * w + x) * 3;
      data[o] = src[i];
      data[o + 1] = src[i + 1];
      data[o + 2] = src[i + 2];
    }
  }
  writeFileSync(`${out}/${name}.png`, encode({ width: w, height: FRAME_H * SCALE, data, channels: 3 }));
  console.log(`${out}/${name}.png`);
}

for (const [which, shots] of Object.entries(SHOTS)) {
  let scene: Scene;
  const app: App = {
    display: new Display(ctx),
    music,
    playing: ENIGMA,
    cursor: ENIGMA,
    authentic: true,
    go: (s) => (scene = s),
  };
  // For the text pages: Enigma replayed one tick per VBL (the analysers follow it).
  let lastTick: TickInfo | null = null;
  const replay = new Replayer(parseMod(enigma.buffer.slice(enigma.byteOffset, enigma.byteOffset + enigma.byteLength) as ArrayBuffer),
    ST_VBL_HZ, ST_VBL_HZ, (t) => (lastTick = t));
  const one = new Float32Array(1);
  const replayVbl = () => replay.render(one, one, 0, 1);
  if (which === 'text') for (let i = 0; i < ENIGMA_TICKS; i++) replayVbl();

  scene = which === 'intro' ? new IntroScene(app) : which === 'text' ? new TextPageScene(app, () => lastTick) : new MenuScene(app);
  const last = Math.max(...Object.keys(shots).map(Number));
  for (let v = 0; v <= last; v++) {
    scene.draw(app.display);
    app.display.render();
    if (shots[v]) save(shots[v]);
    scene.vbl();
    if (which === 'text') replayVbl();
    // let the fake music promises resolve
    await Promise.resolve();
  }
}
