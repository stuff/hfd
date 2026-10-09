import { type Display, SCREEN_W } from './display';

/** Character → glyph code in the demo's 8×8 font (codes 48 '0' to 91 '['), -1 for none. */
function glyph(ch: string): number {
  const map: Record<string, string> = { '.': ';', '(': '<', '!': '=', ')': '>', '-': '[' };
  const c = (map[ch] ?? ch.toUpperCase()).charCodeAt(0);
  return c >= 48 && c <= 91 ? c - 48 : -1;
}

/** Draws text with the 8×8 font on a 10-pixel pitch (as the text pages do), centred if x is omitted. */
export function drawText(d: Display, font: Uint8Array, text: string, y: number, color: number, x?: number): void {
  const pitch = 10;
  let px = x ?? ((SCREEN_W - text.length * pitch + 2) >> 1);
  for (const ch of text) {
    const g = glyph(ch);
    if (g >= 0)
      for (let r = 0; r < 8; r++) {
        const bits = font[g * 8 + r];
        for (let b = 0; b < 8; b++) if (bits & (0x80 >> b)) d.pixels[d.at(px + b, y + r)] = color;
      }
    px += pitch;
  }
}
