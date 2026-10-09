/**
 * A virtual Atari ST low-resolution display, framed like Hatari's screenshots
 * (416×276, borders included) so frames can be compared with the reference recording.
 *
 * Colours are 12-bit 0xRGB numbers with 4-bit linear components (0 = black, 15 = full),
 * which is how the STE's 4-bit colours come out once the odd STE bit order is undone.
 * Every frame line has its own 16-colour palette: that's how rasters work. The border
 * of a line shows colour 0 of that line's palette.
 */
export const FRAME_W = 416;
export const FRAME_H = 276;
export const SCREEN_X = 48;
export const SCREEN_Y = 29;
export const SCREEN_W = 320;
/** Screen rows that can be shown, the lower border included (the menu opens it). */
export const MAX_ROWS = FRAME_H - SCREEN_Y;

/** The JSON files next to the pictures in rip/graphics. */
export interface PictureMeta {
  palette: string[];
  per_line_palette?: Record<string, string | undefined>[];
}

export interface IndexedImage {
  width: number;
  height: number;
  data: Uint8Array;
}

/**
 * The demo's palette fade: level j removes j from each component on the ST's 3-bit
 * scale. The main program drops the STE's extra low bit as soon as j > 0; the loader
 * (ear screen) keeps it unless the subtraction went below zero (`keepLsb`).
 */
export function fadeColor(c: number, j: number, keepLsb = false): number {
  if (j <= 0) return c;
  let out = 0;
  for (let shift = 8; shift >= 0; shift -= 4) {
    const n = (c >> shift) & 15;
    const v3 = (n >> 1) - j;
    if (v3 >= 0) out |= ((v3 << 1) | (keepLsb ? n & 1 : 0)) << shift;
  }
  return out;
}

export class Display {
  /**
   * Palette indices for the whole frame, FRAME_W per line (use `at` to address them in
   * screen coordinates). Only the screen window of each line is shown: the 320 pixels
   * of the first `rows` screen lines, or the whole line where `wide` opened the side
   * borders. Elsewhere the line shows its colour 0.
   */
  readonly pixels = new Uint8Array(FRAME_W * FRAME_H);
  rows = 200;
  /** Frame lines whose side borders are open (cleared by clear()). */
  readonly wideLines = new Uint8Array(FRAME_H);
  /** FRAME_H × 16 colours. Use setColor/setLines with screen coordinates (y may be negative). */
  readonly palette = new Uint16Array(FRAME_H * 16);
  /** Fade level applied to every colour when rendering (see fadeColor). */
  fade = 0;
  fadeKeepsLsb = false;
  /** Lines whose colours are used as they are, whatever `fade` is (cleared by setPalette). */
  readonly noFade = new Uint8Array(FRAME_H);

  private readonly rgba = new Uint32Array(4096);
  private readonly image: ImageData;
  private readonly out: Uint32Array;
  private lutFade = -1;
  private readonly lut = new Uint32Array(4096);

  constructor(private readonly ctx: CanvasRenderingContext2D) {
    this.image = ctx.createImageData(FRAME_W, FRAME_H);
    this.out = new Uint32Array(this.image.data.buffer);
    for (let c = 0; c < 4096; c++) {
      const r = ((c >> 8) & 15) * 17, g = ((c >> 4) & 15) * 17, b = (c & 15) * 17;
      this.rgba[c] = 0xff000000 | (b << 16) | (g << 8) | r; // little-endian ABGR
    }
  }

  clear(): void {
    this.pixels.fill(0);
    this.wideLines.fill(0);
  }

  /** Index into `pixels` of screen position (x, y); both may lie in the borders. */
  at(x: number, y: number): number {
    return (y + SCREEN_Y) * FRAME_W + x + SCREEN_X;
  }

  /** Opens the side borders on screen lines y0 ≤ y < y1. */
  wide(y0: number, y1: number): void {
    this.wideLines.fill(1, Math.max(0, y0 + SCREEN_Y), Math.min(FRAME_H, y1 + SCREEN_Y));
  }

  /** Same palette on every line. */
  setPalette(colors: ArrayLike<number>): void {
    for (let l = 0; l < FRAME_H; l++) this.palette.set(colors as ArrayLike<number>, l * 16);
    this.noFade.fill(0);
  }

  /** Sets colour `index` on screen lines y0 ≤ y < y1 (screen coordinates, clipped to the frame). */
  setLines(y0: number, y1: number, index: number, color: number): void {
    const a = Math.max(0, y0 + SCREEN_Y), b = Math.min(FRAME_H, y1 + SCREEN_Y);
    for (let l = a; l < b; l++) this.palette[l * 16 + index] = color;
  }

  setColor(y: number, index: number, color: number): void {
    this.setLines(y, y + 1, index, color);
  }

  /**
   * Fades the palettes of screen lines y0 ≤ y < y1 to level j now, and exempts them
   * from the global fade: for screens whose parts fade separately.
   */
  fadeLines(y0: number, y1: number, j: number, keepLsb = false): void {
    const a = Math.max(0, y0 + SCREEN_Y), b = Math.min(FRAME_H, y1 + SCREEN_Y);
    for (let l = a; l < b; l++) {
      for (let i = 0; i < 16; i++) this.palette[l * 16 + i] = fadeColor(this.palette[l * 16 + i], j, keepLsb);
      this.noFade[l] = 1;
    }
  }

  /**
   * Sets the palette of every line from a picture's JSON (see rip/graphics): its
   * palette, then the colours it measured line by line (rasters).
   */
  setPictureColors(meta: PictureMeta): void {
    this.setPalette(meta.palette.map(hex));
    meta.per_line_palette?.forEach((p, y) => {
      for (const [i, c] of Object.entries(p)) if (c) this.setColor(y, Number(i), hex(c));
    });
  }

  /**
   * Copies a rectangle of an indexed image to screen position (dx, dy), clipped to the
   * frame. `map` turns source indices into screen indices; index 0 is transparent
   * unless `opaque`.
   */
  blit(img: IndexedImage, sx: number, sy: number, w: number, h: number, dx: number, dy: number,
    map: (i: number) => number = (i) => i, opaque = false): void {
    for (let y = 0; y < h; y++) {
      const fy = dy + y + SCREEN_Y;
      if (fy < 0 || fy >= FRAME_H || sy + y < 0 || sy + y >= img.height) continue;
      for (let x = 0; x < w; x++) {
        const fx = dx + x + SCREEN_X;
        if (fx < 0 || fx >= FRAME_W || sx + x < 0 || sx + x >= img.width) continue;
        const i = img.data[(sy + y) * img.width + sx + x];
        if (i || opaque) this.pixels[fy * FRAME_W + fx] = map(i);
      }
    }
  }

  render(): void {
    const key = this.fade * 2 + (this.fadeKeepsLsb ? 1 : 0);
    if (this.lutFade !== key) {
      for (let c = 0; c < 4096; c++) this.lut[c] = this.rgba[fadeColor(c, this.fade, this.fadeKeepsLsb)];
      this.lutFade = key;
    }
    const { out, lut, palette, pixels } = this;
    const line = new Uint32Array(16);
    for (let l = 0; l < FRAME_H; l++) {
      const map = this.noFade[l] ? this.rgba : lut;
      for (let i = 0; i < 16; i++) line[i] = map[palette[l * 16 + i]];
      const o = l * FRAME_W, y = l - SCREEN_Y;
      let x0 = SCREEN_X, x1 = SCREEN_X + SCREEN_W;
      if (this.wideLines[l]) { x0 = 0; x1 = FRAME_W; }
      else if (y < 0 || y >= this.rows) x0 = x1 = FRAME_W;
      out.fill(line[0], o, o + x0);
      for (let x = x0; x < x1; x++) out[o + x] = line[pixels[o + x]];
      out.fill(line[0], o + x1, o + FRAME_W);
    }
    this.ctx.putImageData(this.image, 0, 0);
  }
}

/** '#aa8800' (as in the rip's JSON files, where every component is a multiple of 17) → 0xa80. */
export function hex(c: string): number {
  const v = parseInt(c.slice(1), 16);
  return ((((v >> 16) & 255) / 17) << 8) | ((((v >> 8) & 255) / 17) << 4) | ((v & 255) / 17);
}
