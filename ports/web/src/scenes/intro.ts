import { ENIGMA, images, pictureMeta, SONGS } from '../assets';
import { type Display, fadeColor, SCREEN_Y } from '../st/display';
import type { App, Scene } from './scene';
import { MenuScene } from './menu';

/**
 * The intro, from the loading screen (the ear) to the last logo. Its clock counts VBLs
 * like the reference recording does (the ear appears at VBL 546, after EmuTOS), so every
 * time below can be checked against rip/out/rec1/video.mkv.
 */
const EAR = 546;
const LOADING_TO_DECRUNCHING = 973;
const EAR_OUT = 2801;
const MUSIC = 2836;
const AURA = 3169, AURA_OUT = 3763;
const HFD_RULES = 3798, HFD_WORDS = 3972, HFD_OUT = 4560;
const AEON = 4704, AEON_OUT = 5256;
export const INTRO_END = 5299; // the text pages start here

const BLACK = new Array(16).fill(0);

/** The short version skips whole pulse cycles of LOADING and DECRUNCHING (56 VBLs each). */
const SHORT_SKIPS = new Map([
  [LOADING_TO_DECRUNCHING - 280, 280], // LOADING stays 147 VBLs instead of 427
  [EAR_OUT - 1680, 1680], // DECRUNCHING stays 148 VBLs instead of 1828
]);

/** Fade in from level 6, one level every 2 VBLs; 7 (black) before `start`. */
const fadeIn2 = (v: number, start: number) => (v < start ? 7 : Math.max(0, 6 - ((v - start) >> 1)));
/** Fade out to black, one level every 2 VBLs from `start` (level 1 at `start`). */
const fadeOut2 = (v: number, start: number) => (v < start ? 0 : Math.min(7, 1 + ((v - start) >> 1)));

/**
 * The colour LOADING/DECRUNCHING pulses with: a 56-VBL triangle from 000 to eee and back,
 * one 3-bit level every 4 VBLs, red then green then blue one VBL apart. k counts from 1.
 */
function pulse(k: number): number {
  const m = (k - 1) % 56, n = m >> 2, sub = m & 3;
  const up = n < 7;
  const level = up ? n : 7 - (n - 7);
  const step = (i: number) => (sub >= i ? (up ? 1 : -1) : 0);
  const r = level + step(0), g = level + step(1), b = level + step(2);
  return ((r * 2) << 8) | ((g * 2) << 4) | (b * 2);
}

/** The ear screen's loader fade, keeping the STE low bit (measured: it starts on odd VBLs). */
function earFade(v: number): number {
  if (v < EAR_OUT) return v <= EAR ? 6 : Math.max(0, 5 - ((v - EAR - 1) >> 1));
  return v >= EAR_OUT + 10 ? 7 : 1 + ((v - EAR_OUT) >> 1);
}

/** CODE / AND / DESIGN / AEON each flash in with these levels, one per VBL. */
const FLASH = [0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0];
const flash = (v: number, start: number) => (v < start ? 7 : FLASH[Math.min(FLASH.length - 1, v - start)]);

export class IntroScene implements Scene {
  /** The reference VBL being shown. */
  v = EAR;

  constructor(private readonly app: App) {
    void app.music.load(SONGS[ENIGMA].url); // ready when the music starts
  }

  vbl(): void {
    this.v++;
    if (!this.app.authentic) this.v += SHORT_SKIPS.get(this.v) ?? 0;
    if (this.v === MUSIC) void this.app.music.play(SONGS[ENIGMA].url);
    if (this.v >= INTRO_END) this.app.go(new MenuScene(this.app));
  }

  draw(d: Display): void {
    const v = this.v;
    d.rows = 200;
    d.fade = 0;
    d.fadeKeepsLsb = false;
    d.clear();
    d.setPalette(BLACK);
    if (v >= EAR && v < EAR_OUT + 10) this.ear(d, v);
    else if (v >= AURA && v < AURA_OUT + 14) this.aura(d, v);
    else if (v >= HFD_RULES && v < HFD_OUT + 14) this.hfd(d, v);
    else if (v >= AEON && v < AEON_OUT + 14) this.aeon(d, v);
  }

  private ear(d: Display, v: number): void {
    d.blit(images.ear, 0, 0, 320, 200, 0, 0);
    d.setPictureColors(pictureMeta.ear);
    // LOADING (colour 15) pulses, then DECRUNCHING (colour 14) takes over the same pulse.
    // The pulse is written after the fade, so it doesn't fade (nor does colour 13 there);
    // the hidden word has the background colour, which does.
    const j = earFade(v);
    const c = v > EAR ? pulse(v - EAR) : 0, bg = fadeColor(0x004, j, true);
    const decrunching = v >= LOADING_TO_DECRUNCHING;
    d.setLines(189, 200, 14, decrunching ? c : bg);
    d.setLines(189, 200, 15, decrunching ? bg : c);
    d.setLines(189, 200, 0, bg);
    d.noFade.fill(1, 189 + SCREEN_Y, 200 + SCREEN_Y);
    d.fade = j;
    d.fadeKeepsLsb = true;
  }

  private aura(d: Display, v: number): void {
    d.blit(images.aura, 0, 0, 320, 200, 0, 0);
    d.setPictureColors(pictureMeta.aura);
    [[53, 73], [84, 116], [127, 147]].forEach(([y0, y1], w) =>
      d.fadeLines(y0, y1, Math.max(fadeIn2(v, AURA + 24 * w), fadeOut2(v, AURA_OUT))));
  }

  private hfd(d: Display, v: number): void {
    const img = images.hfd; // 416 wide: x = 0 is the left edge of the frame
    d.setPictureColors(pictureMeta.hfd);
    const out = fadeOut2(v, HFD_OUT);
    // The two rules slide in from the sides, 2 pixels per VBL, fading in.
    const slide = Math.min(142, v - HFD_RULES);
    const top = 350 - 2 * slide, bottom = 65 + 2 * slide; // left end of the top rule, right end of the bottom one
    d.wide(14, 22);
    d.wide(178, 186);
    d.blit(img, 66, 14, 416 - 66, 8, top - 48, 14, (i) => i);
    d.blit(img, 0, 178, 350, 8, bottom - 349 - 48, 178, (i) => i);
    const rules = Math.max(fadeIn2(v, HFD_RULES), out);
    d.fadeLines(14, 22, rules);
    d.fadeLines(178, 186, rules);
    // Then HIGH, FIDELITY, DREAMS, 6 VBLs apart.
    [[37, 69], [84, 116], [131, 163]].forEach(([y0, y1], w) => {
      d.blit(img, 0, y0, 416, y1 - y0, -48, y0);
      d.fadeLines(y0, y1, Math.max(fadeIn2(v, HFD_WORDS + 6 * w), out));
    });
  }

  private aeon(d: Display, v: number): void {
    d.blit(images.aeon, 0, 0, 320, 200, 0, 0);
    d.setPictureColors(pictureMeta.aeon);
    const out = fadeOut2(v, AEON_OUT);
    [[7, 39], [42, 62], [65, 97], [128, 192]].forEach(([y0, y1], w) =>
      d.fadeLines(y0, y1, Math.max(flash(v, AEON + 12 * w), out)));
  }
}
