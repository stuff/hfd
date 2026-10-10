import './style.css';
import { ENIGMA, loadFont, SONGS } from './assets';
import { Music } from './audio/music';
import { ST_VBL_HZ } from './audio/mod';
import { Pacer } from './pacing';
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

// The ST's 50.05 Hz VBLs, run on display refreshes (see Pacer), locked to the sound
// once it plays. The picture is only redrawn when a VBL ran.
const pacer = new Pacer(1000 / ST_VBL_HZ);
let dirty = true;
function frame(now: number): void {
  const audio = app.music.outputTimeMs();
  const n = pacer.step(now, audio ?? now, audio === null ? 'page' : 'audio');
  for (let i = 0; i < n; i++) scene.vbl();
  if (n > 0 || dirty) {
    scene.draw(display);
    display.render();
    dirty = false;
  }
  requestAnimationFrame(frame);
}

addEventListener('keydown', (e) => {
  if (e.repeat && e.code !== 'ArrowUp' && e.code !== 'ArrowDown') return;
  if (['ArrowUp', 'ArrowDown', 'Space', 'Enter'].includes(e.code)) e.preventDefault();
  if (e.code === 'KeyF' && !(scene instanceof StartScene)) toggleFullscreen();
  else scene.key?.(e.code);
});

function toggleFullscreen(): void {
  if (document.fullscreenElement) void document.exitFullscreen();
  else document.documentElement.requestFullscreen?.().catch(() => {});
}

function toScreen(e: PointerEvent): [number, number] {
  const r = canvas.getBoundingClientRect();
  return [((e.clientX - r.left) / r.width) * FRAME_W - SCREEN_X, ((e.clientY - r.top) / r.height) * FRAME_H - SCREEN_Y];
}
canvas.addEventListener('pointermove', (e) => {
  if (e.pointerType === 'mouse') scene.pointer?.(...toScreen(e), false);
});
// A double click (or double tap) toggles full screen. A single click waits for the
// double-click delay first, so that a double click doesn't also act as a click.
const DOUBLE_CLICK_MS = 250;
let pendingClick: number | undefined;
canvas.addEventListener('pointerup', (e) => {
  if (e.button !== 0) return;
  const pos = toScreen(e);
  if (scene instanceof StartScene) {
    scene.pointer(...pos, true); // must run inside the gesture: it starts the audio
    return;
  }
  if (pendingClick !== undefined) {
    clearTimeout(pendingClick);
    pendingClick = undefined;
    toggleFullscreen();
    return;
  }
  pendingClick = window.setTimeout(() => {
    pendingClick = undefined;
    scene.pointer?.(...pos, true);
  }, DOUBLE_CLICK_MS);
});
canvas.addEventListener('dblclick', (e) => e.preventDefault());

loadFont().then((font) => {
  scene = new StartScene(font);
  requestAnimationFrame(frame);
});
