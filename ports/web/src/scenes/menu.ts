import { images, SONGS } from '../assets';
import { type Display, SCREEN_W, SCREEN_Y } from '../st/display';
import { type App, type Scene, fadeAt, FADE_IN } from './scene';
import { LoadingScene } from './loading';

/** Menu layout, in screen lines: entry k is 20 lines tall at ENTRY_Y + k × ENTRY_STEP. */
export const ENTRY_Y = 37;
export const ENTRY_STEP = 23;
export const ENTRY_H = 20;
const ROWS = 218; // the menu opens the lower border

// Colours measured in the reference recording (0xRGB, 4 bits per component).
const BASE = [0x000, 0x004, 0x226, 0x448, 0x66a, 0xccc, 0x000, 0x000,
  0x000, 0x626, 0x848, 0xa6a, 0xc8c, 0xfff, 0x000, 0x000];
const ORANGE = [0x840, 0xa62, 0xc84, 0xea6, 0xfff]; // colours 1-5 of SELECT SONG / LOADING / DECRUNCHING
/** Colours 1-5 of the entry under the cursor. */
const CURSOR = [0x226, 0x448, 0x66a, 0x88c, 0xfff];
const CURSOR_BAR = [0xfff, 0x226, 0x448, 0x66a, 0x88c, 0xfff]; // colour 0 on lines 7-12 of that entry
/**
 * The 3-line horizontal rules (colour 0), per fade level. They don't fade like the
 * palette does (aaa and 666 hold on longer), so here is the measured table.
 */
const RULE_FADE = [
  [0xfff, 0xaaa, 0x666], [0xccc, 0x888, 0x666], [0xaaa, 0x888, 0x444], [0x888, 0x666, 0x444],
  [0x666, 0x444, 0x222], [0x444, 0x444, 0x222], [0x222, 0x222, 0x000], [0x000, 0x000, 0x000],
];

/** Draws a rule at screen line y. Call after setting d.fade. */
export function rule(d: Display, y: number): void {
  RULE_FADE[Math.min(7, d.fade)].forEach((c, i) => {
    d.setColor(y + i, 0, c);
    d.noFade[y + i + SCREEN_Y] = 1;
  });
}

/** Copies the word of menu entry k to screen line y; `playing` draws it pink (bitplane 3). */
export function drawSongWord(d: Display, k: number, y: number, playing: boolean): void {
  d.blit(images.menu, 0, ENTRY_Y + k * ENTRY_STEP, SCREEN_W, ENTRY_H, 0, y,
    playing ? (i) => (i & 7 ? (i & 7) | 8 : 0) : (i) => i & 7);
}

export function drawMenu(d: Display, cursor: number, playing: number, fade: number): void {
  d.rows = ROWS;
  d.fade = fade;
  d.clear();
  d.blit(images.menu, 0, 0, SCREEN_W, ROWS, 0, 0, (i) => i & 7, true);
  drawSongWord(d, playing, ENTRY_Y + playing * ENTRY_STEP, true);

  d.setPalette(BASE);
  // The entries' white outline (ccc) is fff one fade step down: during a fade it
  // follows max(fade, 1), not fade + 1.
  if (fade > 0) d.setLines(0, ROWS, 5, 0xfff);
  rule(d, 2);
  rule(d, 30);
  ORANGE.forEach((c, i) => d.setLines(7, 27, i + 1, c));
  const y = ENTRY_Y + cursor * ENTRY_STEP;
  if (cursor !== playing) CURSOR.forEach((c, i) => d.setLines(y, y + ENTRY_H, i + 1, c));
  CURSOR_BAR.forEach((c, i) => {
    d.setColor(y + 7 + i, 0, c);
    d.setColor(y + 7 + i, 7, 0xfff);
    d.setColor(y + 7 + i, 15, 0xfff);
  });
}

export function entryAt(y: number): number {
  const k = Math.floor((y - ENTRY_Y + 1) / ENTRY_STEP);
  return k >= 0 && k < SONGS.length ? k : -1;
}

export class MenuScene implements Scene {
  private t = 0;

  constructor(private readonly app: App, private readonly fadeIn: number[] = FADE_IN, private readonly delay = 0) {}

  vbl(): void {
    this.t++;
  }

  draw(d: Display): void {
    drawMenu(d, this.app.cursor, this.app.playing, fadeAt(this.t - this.delay, this.fadeIn, 8));
  }

  private get ready(): boolean {
    return this.t - this.delay >= this.fadeIn.length * 6;
  }

  key(code: string): void {
    if (!this.ready) return;
    const n = SONGS.length;
    if (code === 'ArrowUp') this.app.cursor = Math.max(0, this.app.cursor - 1);
    else if (code === 'ArrowDown') this.app.cursor = Math.min(n - 1, this.app.cursor + 1);
    else if (code === 'Enter' || code === 'NumpadEnter') this.select();
  }

  pointer(_x: number, y: number, click: boolean): void {
    if (!this.ready) return;
    const k = entryAt(y);
    if (k < 0) return;
    this.app.cursor = k;
    if (click) this.select();
  }

  private select(): void {
    if (this.app.cursor === this.app.playing) return;
    this.app.go(new LoadingScene(this.app, this.app.cursor));
  }
}
