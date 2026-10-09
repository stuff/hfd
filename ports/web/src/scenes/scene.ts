import type { Display } from '../st/display';
import type { Music } from '../audio/music';

export interface App {
  display: Display;
  music: Music;
  /** Song being played (menu index). */
  playing: number;
  /** Menu cursor, kept between visits. */
  cursor: number;
  /** Original disk loading/decrunching durations instead of short ones. */
  authentic: boolean;
  /** The text pages, kept while in the menu: SPACE in the menu resumes them where they were. */
  textPage?: Scene & { resume(): Scene };
  go(scene: Scene): void;
}

export interface Scene {
  /** Advances one VBL (1/50 s). */
  vbl(): void;
  /** Draws the current state into the display. */
  draw(d: Display): void;
  key?(code: string): void;
  /** Pointer position in screen coordinates (0,0 = top-left of the 320-wide screen). */
  pointer?(x: number, y: number, click: boolean): void;
}

/**
 * Fade level for a palette fade as the demo does it: one level every 3 VBLs.
 * `steps` lists the levels (see fadeColor); before the first step the level is `before`,
 * after the last one it stays on the last.
 */
export function fadeAt(t: number, steps: number[], before = steps[0]): number {
  if (t < 0) return before;
  return steps[Math.min(steps.length - 1, Math.floor(t / 3))];
}

/** From black (level 7) to full colour, 21 VBLs. */
export const FADE_IN = [7, 6, 5, 4, 3, 2, 1, 0];
/** From full colour to black, 21 VBLs. */
export const FADE_OUT = [0, 1, 2, 3, 4, 5, 6, 7];
