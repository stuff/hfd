import './style.css';
import { ENIGMA, loadFont, SONGS } from './assets';
import { Music } from './audio/music';
import { ST_VBL_HZ } from './audio/mod';
import { Display, FRAME_H, FRAME_W, SCREEN_X, SCREEN_Y } from './st/display';
import { drawText } from './st/text';
import type { App, Scene } from './scenes/scene';
import { IntroScene } from './scenes/intro';
import { MenuScene } from './scenes/menu';

const canvas = document.querySelector<HTMLCanvasElement>('#screen')!;
canvas.width = FRAME_W;
canvas.height = FRAME_H;
const display = new Display(canvas.getContext('2d')!);
const params = new URLSearchParams(location.search);

let scene: Scene;
const app: App = {
  display,
  music: new Music(),
  playing: ENIGMA,
  cursor: ENIGMA,
  authentic: params.has('authentic'),
  go(s) {
    scene = s;
  },
};

// ?debug exposes the app to the console (and to scripts/smoke.mjs).
if (params.has('debug')) Object.assign(window, { hfd: app });

/** Browsers only allow sound after a user gesture, hence this first screen. */
class StartScene implements Scene {
  private starting = false;
  constructor(private readonly font: Uint8Array) {}
  vbl(): void {}
  draw(d: Display): void {
    d.rows = 200;
    d.clear();
    d.setPalette([0x000, 0xfff]);
    drawText(d, this.font, 'HIGH FIDELITY DREAMS', 92, 1);
    drawText(d, this.font, 'CLICK OR PRESS A KEY', 108, 1);
    d.fade = 0;
  }
  key(): void {
    void this.start();
  }
  pointer(_x: number, _y: number, click: boolean): void {
    if (click) void this.start();
  }
  private async start(): Promise<void> {
    if (this.starting) return;
    this.starting = true;
    await app.music.start();
    if (params.has('menu')) {
      // Straight to the menu (development), with Enigma playing as after the intro.
      await app.music.play(SONGS[ENIGMA].url);
      app.go(new MenuScene(app));
    } else app.go(new IntroScene(app));
  }
}

// Fixed 50 Hz VBL clock, rendered on animation frames.
const VBL_MS = 1000 / ST_VBL_HZ;
let last = performance.now();
let acc = 0;
function frame(now: number): void {
  acc = Math.min(acc + now - last, 250);
  last = now;
  while (acc >= VBL_MS) {
    scene.vbl();
    acc -= VBL_MS;
  }
  scene.draw(display);
  display.render();
  requestAnimationFrame(frame);
}

addEventListener('keydown', (e) => {
  if (e.repeat && e.code !== 'ArrowUp' && e.code !== 'ArrowDown') return;
  if (['ArrowUp', 'ArrowDown', 'Space', 'Enter'].includes(e.code)) e.preventDefault();
  scene.key?.(e.code);
});

function toScreen(e: PointerEvent): [number, number] {
  const r = canvas.getBoundingClientRect();
  return [((e.clientX - r.left) / r.width) * FRAME_W - SCREEN_X, ((e.clientY - r.top) / r.height) * FRAME_H - SCREEN_Y];
}
canvas.addEventListener('pointermove', (e) => {
  if (e.pointerType === 'mouse') scene.pointer?.(...toScreen(e), false);
});
canvas.addEventListener('pointerup', (e) => scene.pointer?.(...toScreen(e), true));

loadFont().then((font) => {
  scene = new StartScene(font);
  requestAnimationFrame(frame);
});
