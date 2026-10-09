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

export interface IndexedImage {
  width: number;
  height: number;
  data: Uint8Array;
}

/**
 * The demo's palette fade: level j removes j from each component on the ST's 3-bit
 * scale (the STE's extra low bit is dropped as soon as j > 0).
 */
export function fadeColor(c: number, j: number): number {
  if (j <= 0) return c;
  let out = 0;
  for (let shift = 8; shift >= 0; shift -= 4) {
    const v3 = Math.max(0, (((c >> shift) & 15) >> 1) - j);
    out |= (v3 << 1) << shift;
  }
  return out;
}

export class Display {
  /** Palette indices, SCREEN_W per row. Rows at or after `rows` show the border colour. */
  readonly pixels = new Uint8Array(SCREEN_W * MAX_ROWS);
  rows = 200;
  /** FRAME_H × 16 colours. Use setColor/setLines with screen coordinates (y may be negative). */
  readonly palette = new Uint16Array(FRAME_H * 16);
  /** Fade level applied to every colour when rendering (see fadeColor). */
  fade = 0;
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
   * Copies a rectangle of an indexed image to the screen. `map` turns source indices
   * into screen indices; index 0 is transparent unless `opaque`.
   */
  blit(img: IndexedImage, sx: number, sy: number, w: number, h: number, dx: number, dy: number,
    map: (i: number) => number = (i) => i, opaque = false): void {
    for (let y = 0; y < h; y++) {
      const ty = dy + y;
      if (ty < 0 || ty >= MAX_ROWS) continue;
      for (let x = 0; x < w; x++) {
        const tx = dx + x;
        if (tx < 0 || tx >= SCREEN_W) continue;
        const i = img.data[(sy + y) * img.width + sx + x];
        if (i || opaque) this.pixels[ty * SCREEN_W + tx] = map(i);
      }
    }
  }

  render(): void {
    if (this.lutFade !== this.fade) {
      for (let c = 0; c < 4096; c++) this.lut[c] = this.rgba[fadeColor(c, this.fade)];
      this.lutFade = this.fade;
    }
    const { out, lut, palette, pixels } = this;
    const line = new Uint32Array(16);
    for (let l = 0; l < FRAME_H; l++) {
      const map = this.noFade[l] ? this.rgba : lut;
      for (let i = 0; i < 16; i++) line[i] = map[palette[l * 16 + i]];
      const o = l * FRAME_W, y = l - SCREEN_Y;
      if (y < 0 || y >= this.rows) {
        out.fill(line[0], o, o + FRAME_W);
        continue;
      }
      out.fill(line[0], o, o + SCREEN_X);
      const p = y * SCREEN_W;
      for (let x = 0; x < SCREEN_W; x++) out[o + SCREEN_X + x] = line[pixels[p + x]];
      out.fill(line[0], o + SCREEN_X + SCREEN_W, o + FRAME_W);
    }
    this.ctx.putImageData(this.image, 0, 0);
  }
}

/** '#aa8800' (as in the rip's JSON files, where every component is a multiple of 17) → 0xa80. */
export function hex(c: string): number {
  const v = parseInt(c.slice(1), 16);
  return ((((v >> 16) & 255) / 17) << 8) | ((((v >> 8) & 255) / 17) << 4) | ((v & 255) / 17);
}
