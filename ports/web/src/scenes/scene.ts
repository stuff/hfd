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
 * Fade level for a palette fade made of steps of 6 VBLs, as the demo does it.
 * `steps` lists the levels (see fadeColor); before the first step the level is `before`,
 * after the last one it stays on the last.
 */
export function fadeAt(t: number, steps: number[], before = steps[0]): number {
  if (t < 0) return before;
  return steps[Math.min(steps.length - 1, Math.floor(t / 6))];
}

export const FADE_IN = [5, 3, 1, 0];
export const FADE_IN_EVEN = [6, 4, 2, 0];
export const FADE_OUT = [1, 3, 5, 7];
export const FADE_OUT_EVEN = [2, 4, 6, 8];
