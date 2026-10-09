import ramImage from '@rip/extracted/textpage/ram_012000_025b00.bin?bytes';
import type { TickInfo } from '../audio/mod';
import type { Display } from '../st/display';
import type { App, Scene } from './scene';
import { MenuScene } from './menu';

/**
 * The text pages: a port of the 68000 code that runs them (see the disassembly in
 * rip/extracted/disasm/textpage_4000.s, addresses in comments).
 *
 * Their data and state live in a copy of the ST's RAM from $12000 to $25B00, taken just
 * before the text pages start (rip/extracted/textpage): the raster script and records,
 * the colour tables, the pre-shifted font and bubble, the bubble's path, the text and
 * the 33 letter slots. The code below reads and writes it at the original addresses,
 * self-modified operands included, so that it behaves exactly like the original.
 *
 * Each VBL, like the interrupt at $12A22:
 *  - rasters ($12A36): set up the 12 Timer B handlers (band colours and heights) for the
 *    coming frame from the current record, step through the raster script;
 *  - spreader ($1E552): swap the two screens, erase and redraw the bubble and the letters
 *    in flight (bitplane 1), land letters (bitplane 2), clear the page when asked;
 *  - voice analysers ($1E250): vertical stripes in bitplanes 3 and 0, one band per channel.
 */

const RAM_BASE = 0x12000;
const SCREEN_A = 0x60000, SCREEN_B = 0x68000;

// Addresses of variables (mostly self-modified instruction operands).
const REC = 0x12b32, REC_END = 0x12ad4, SCRIPT = 0x12adc, SCRIPT_COUNT = 0x12b50, REC_STEP = 0x12ac4;
const COLTAB = 0x12a54; // colour table in use: 8 tables of 38 bytes, black to full brightness
const HANDLERS = 0x12b52, HANDLER_SIZE = 0x28;
const NEXT_SCREEN = 0x1e554, DRAW_SCREEN = 0x1e55a;
const BUBBLE_PTR = 0x1e73c, TEXT_PTR = 0x1e884;
const BUBBLE_X = 0x1e9bc, BUBBLE_Y = 0x1e9be, TEXT_X = 0x1e9c0, TEXT_Y = 0x1e9c2, PAUSE = 0x1e9c4;
const PITCH = 0x1e950, CLEAR_COUNT = 0x1e96c;
const SLOTS = 0x1e9c8, SLOT_SIZE = 22, SLOT_COUNT = 33;
const ANALYSERS = 0x1e2e4;
const FONT = 0x1ec9e, BUBBLE = 0x2189e;
const PATH_START = 0x21c9e, PATH_END = 0x2214e, TEXT_START = 0x2214e;

/** STE palette word ($RGB, odd STE nibble order) → 0xRGB with linear 4-bit components. */
function steColor(w: number): number {
  let c = 0;
  for (let s = 8; s >= 0; s -= 4) {
    const n = (w >> s) & 15;
    c |= (((n & 7) << 1) | (n >> 3)) << s;
  }
  return c;
}

/** Analyser stripe patterns (d2, d3, d5, d6 at $1E314-$1E3E0), by (level - 1) bits 1-3. */
const PATTERNS = [
  [0x1, 0x3777ffff, 0xfffff776, 0x40000000],
  [0x13, 0x777fffff, 0xffffff77, 0x64000000],
  [0x137, 0x77ffffff, 0xfffffff7, 0x76400000],
  [0x1377, 0x7fffffff, 0xffffffff, 0x77640000],
  [0x13777, 0xffffffff, 0xffffffff, 0xf7764000],
  [0x13777f, 0xffffffff, 0xffffffff, 0xff776400],
  [0x13777ff, 0xffffffff, 0xffffffff, 0xfff77640],
  [0x13777fff, 0xffffffff, 0xffffffff, 0xffff7764],
];

export class TextPageMachine {
  private readonly ram = new Uint8Array(ramImage);
  private readonly mem = new DataView(this.ram.buffer);
  private readonly screens = new Map([[SCREEN_A, new Uint8Array(32000)], [SCREEN_B, new Uint8Array(32000)]]);
  /** What the original keeps at screen+$7D00: the bubble's and the letters' addresses to erase. */
  private readonly eraseBubble = new Map([[SCREEN_A, SCREEN_A], [SCREEN_B, SCREEN_B]]);
  private readonly eraseBobs = new Map<number, number[]>([[SCREEN_A, []], [SCREEN_B, []]]);
  /** Screen being shown (set during the previous VBL). */
  shown = SCREEN_A;
  /** Colours 0/1 (8 = 1) left by the last raster of the previous frame, shown above the first band. */
  private carry: [number, number] = [0, 0];

  // big-endian accessors on the RAM copy
  private b(a: number): number { return this.ram[a - RAM_BASE]; }
  private w(a: number): number { return this.mem.getUint16(a - RAM_BASE); }
  private sw(a: number): number { return this.mem.getInt16(a - RAM_BASE); }
  private l(a: number): number { return this.mem.getUint32(a - RAM_BASE); }
  private sl(a: number): number { return this.mem.getInt32(a - RAM_BASE); }
  private setB(a: number, v: number): void { this.ram[a - RAM_BASE] = v; }
  private setW(a: number, v: number): void { this.mem.setUint16(a - RAM_BASE, v & 0xffff); }
  private setL(a: number, v: number): void { this.mem.setUint32(a - RAM_BASE, v >>> 0); }

  // screen memory accessors (addresses as on the ST)
  private scr(a: number): [Uint8Array, number] {
    const base = a >= SCREEN_B ? SCREEN_B : SCREEN_A;
    return [this.screens.get(base)!, a - base];
  }
  private putW(a: number, v: number): void { const [s, o] = this.scr(a); s[o] = v >> 8; s[o + 1] = v; }
  private orW(a: number, v: number): void { const [s, o] = this.scr(a); s[o] |= v >> 8; s[o + 1] |= v; }
  private putB(a: number, v: number): void { const [s, o] = this.scr(a); s[o] = v; }
  private orB(a: number, v: number): void { const [s, o] = this.scr(a); s[o] |= v; }

  /** Colour table pointer: the fades step it by ±38 bytes. */
  get colorTable(): number { return this.l(COLTAB); }
  set colorTable(a: number) { this.setL(COLTAB, a); }

  /** One VBL of the text pages. `tick` is the replay state the analysers read (previous VBL's). */
  vbl(tick: TickInfo | null): void {
    const last = this.lineColors()[199];
    this.carry = [last.c0, last.c1];
    this.rasters();
    this.spreader();
    this.analysers(tick);
  }

  // ---- $12A36: rasters ----------------------------------------------------------------
  private rasters(): void {
    let a0 = this.l(REC) + 8;
    this.firstCount = this.b(a0++);
    const a2 = this.l(COLTAB);
    this.text = steColor(this.w(a2 + 0x24));
    const entry = (h: number, off: number) => {
      const p = HANDLERS + h * HANDLER_SIZE;
      this.setL(p + 2, this.l(a2 + off));
      this.setW(p + 0xa, this.w(a2 + off + 2));
    };
    for (let i = 0; i < 6; i++) {
      if (i > 0) this.setB(HANDLERS + (2 * i - 1) * HANDLER_SIZE + 0x15, this.b(a0++));
      const d1 = this.b(a0++);
      entry(2 * i, (d1 & 0xf0) >> 2);
      this.setB(HANDLERS + 2 * i * HANDLER_SIZE + 0x15, this.b(a0++));
      entry(2 * i + 1, (d1 & 0x0f) * 4);
    }
    // step through the record stream and the script
    this.setL(REC, this.l(REC) + this.sl(REC_STEP));
    if (this.l(REC) === this.l(REC_END)) {
      let e = this.l(SCRIPT);
      const old = this.w(SCRIPT_COUNT);
      this.setW(SCRIPT_COUNT, old - 1);
      if (old <= 1) {
        // subq.w/bhi: the entry has run its count, take the next one (the script loops from $12D48)
        e += 12;
        if (e >= 0x12f64) e = 0x12d48;
        this.setL(SCRIPT, e);
        this.setW(SCRIPT_COUNT, this.w(e + 8));
        this.setL(REC_STEP, this.sw(e + 10));
      }
      this.setL(REC, this.l(e));
      this.setL(REC_END, this.l(e + 4));
      const step = this.sl(REC_STEP);
      if (step < 0) this.setL(REC, this.l(REC) + step);
    }
    const r = this.l(REC);
    for (let k = 0; k < 4; k++) this.setW(ANALYSERS + 2 + 4 * k, this.w(r + 2 * k));
  }

  private firstCount = 0;
  private text = 0;

  /**
   * Colours of each screen line for the frame being shown: [colour 0, colour 1 (= 8),
   * the other colours]. The Timer B chain changes 0/1/8 after `count` lines each time.
   */
  lineColors(): { c0: number; c1: number; text: number }[] {
    const out: { c0: number; c1: number; text: number }[] = [];
    let [c0, c1] = this.carry;
    let next = this.firstCount || 256, h = 0;
    for (let y = 0; y < 200; y++) {
      while (h < 12 && y === next) {
        const p = HANDLERS + h * HANDLER_SIZE;
        c0 = steColor(this.w(p + 2));
        c1 = steColor(this.w(p + 4));
        const count = this.b(p + 0x15);
        h++;
        next = h < 12 && count ? y + count : 999;
      }
      out.push({ c0, c1, text: this.text });
    }
    return out;
  }

  // ---- $1E552: the text spreader ------------------------------------------------------
  private spreader(): void {
    // the frame about to be shown is the screen drawn during the previous VBL
    this.shown = this.l(DRAW_SCREEN);
    const d0 = this.l(NEXT_SCREEN);
    this.setL(NEXT_SCREEN, this.l(DRAW_SCREEN));
    this.setL(DRAW_SCREEN, d0);
    const buf = d0;

    // erase the bubble (2 words × 16 lines of bitplane 1)
    const ob = this.eraseBubble.get(buf)!;
    for (let r = 0; r < 16; r++) { this.putW(ob + r * 160, 0); this.putW(ob + r * 160 + 8, 0); }
    // erase the letters in flight
    for (const a of this.eraseBobs.get(buf)!) {
      for (let r = 0; r < 8; r++) {
        if (a & 1) { this.putB(a + r * 160, 0); this.putB(a + r * 160 + 7, 0); }
        else this.putW(a + r * 160, 0);
      }
    }

    // draw the bubble at its current position (MOVE: it overwrites bitplane 1)
    let x = this.w(BUBBLE_X) - 4, y = this.w(BUBBLE_Y) - 4;
    const col = (x & 0xfff0) >> 1;
    x &= 15;
    const ba = buf + 2 + col + y * 160;
    this.eraseBubble.set(buf, ba);
    let s = BUBBLE + x * 64;
    for (let r = 0; r < 16; r++) {
      this.putW(ba + r * 160, this.w(s)); this.putW(ba + r * 160 + 8, this.w(s + 2)); s += 4;
    }
    // next position on the path
    let p = this.l(BUBBLE_PTR);
    this.setW(BUBBLE_X, this.b(p) + 0x20);
    this.setW(BUBBLE_Y, this.b(p + 1));
    p += 2;
    this.setL(BUBBLE_PTR, p >= PATH_END ? PATH_START : p);

    // the 33 letter slots: ptr.l, dx.l, x.l, dy.l, y.l, counter.w
    const bobs: number[] = [];
    for (let k = 0; k < SLOT_COUNT; k++) {
      const S = SLOTS + k * SLOT_SIZE;
      const glyph = this.l(S);
      if (!(glyph & 0x80000000)) {
        const bx = this.w(S + 8), by = this.w(S + 16);
        let a = buf + 2 + (((bx >> 1) & 0xfff8) + by * 160);
        if (bx & 8) a += 1;
        bobs.push(a);
        const g = glyph + (bx & 7) * 16;
        if (this.sw(S + 20) <= 0) a += 2; // landed: bitplane 2, which nothing erases
        if (!(bx & 8)) {
          for (let r = 0; r < 8; r++) this.orW(a + r * 160, this.w(g + 2 * r));
        } else {
          for (let r = 0; r < 8; r++) this.orB(a + r * 160, this.w(g + 0x1600 + 2 * r) & 0xff);
          for (let r = 0; r < 8; r++) this.orB(a + 7 + r * 160, this.w(g + 2 * r) & 0xff);
        }
      }
      // move, count down, and give the slot the next letter when it is done
      if (this.w(S + 20) !== 0) {
        this.setL(S + 8, this.l(S + 8) + this.l(S + 4));
        this.setL(S + 16, this.l(S + 16) + this.l(S + 12));
      }
      const c = (this.w(S + 20) - 1) & 0xffff;
      this.setW(S + 20, c);
      if (c !== 0xfffe) continue;
      this.setW(S + 20, 0x40);
      const pause = this.sl(PAUSE) - 1;
      this.setL(PAUSE, pause);
      if (pause >= 0) {
        this.setL(S + 16, 0); this.setL(S + 12, 0); this.setL(S + 8, 0); this.setL(S + 4, 0);
        this.setL(S, 0xffffffff);
        continue;
      }
      this.nextLetter(S);
    }
    this.eraseBobs.set(buf, bobs);

    // page clear: 2 lines of bitplane 2 per VBL, from line 198 - n down
    const n = this.sw(CLEAR_COUNT);
    if (n >= 0) {
      let a = buf + (0xc6 - n) * 160 + 4;
      for (let i = 0; i < 4; i++, a += 0x50) for (let k = 0; k < 10; k++) this.putW(a + 8 * k, 0);
      this.setW(CLEAR_COUNT, n - 1);
    }
  }

  private nextLetter(S: number): void {
    let a2 = this.l(TEXT_PTR);
    while (this.b(a2) === 0xff) a2 = TEXT_START;
    if (this.b(a2) === 0) {
      // new line: x, y, signed pitch; a negative pitch writes right to left
      a2++;
      this.setW(TEXT_X, this.b(a2++));
      this.setW(TEXT_Y, this.b(a2++));
      const pitch = (this.b(a2++) << 24) >> 24;
      if (pitch < 0) this.setW(TEXT_X, -this.w(TEXT_X) + 0x142 + pitch);
      this.setW(PITCH, pitch);
    }
    // fly from the bubble to the letter's place in 64 VBLs (16.16 fixed point)
    const from = (src: number, dst: number) => {
      const start = this.w(src), diff = ((this.w(dst) - start) << 16) >> 16;
      return [start << 16, (diff << 16) >> 6];
    };
    const [y, dy] = from(BUBBLE_Y, TEXT_Y);
    const [x, dx] = from(BUBBLE_X, TEXT_X);
    this.setL(S + 16, y); this.setL(S + 12, dy); this.setL(S + 8, x); this.setL(S + 4, dx);
    const ch = this.b(a2++);
    if (ch === 0xbd) {
      // clear the page over n VBLs, and wait
      const n = this.b(a2++);
      this.setW(CLEAR_COUNT, n);
      this.setL(PAUSE, n >> 1);
      this.setL(S, 0xffffffff);
    } else if (ch === 9) {
      this.setL(PAUSE, this.b(a2++) * 30);
      this.setL(S, 0xffffffff);
    } else {
      const g = ch - 0x30;
      this.setL(S, g < 0 || g > 0x4f ? 0xffffffff : FONT + g * 128);
      this.setW(TEXT_X, this.w(TEXT_X) + this.sw(PITCH));
    }
    this.setL(TEXT_PTR, a2);
  }

  // ---- $1E250: voice analysers --------------------------------------------------------
  private analysers(tick: TickInfo | null): void {
    const buf = this.l(DRAW_SCREEN);
    for (let ch = 0; ch < 4; ch++) {
      const E = ANALYSERS + 4 * ch;
      const c = tick?.channels[3 - ch]; // the NoisePacker replay keeps its channels in reverse order
      if (c) {
        // a note, an instrument or a set-volume on the row just read; or a volume
        // slide (with tone portamento or vibrato) on the other ticks: back to the volume
        const trigger = tick!.rowRead ? c.noteOrIns || c.cmd === 0xc : c.cmd === 5 || c.cmd === 6 || c.cmd === 0xa;
        if (trigger) this.setW(E, c.baseVolume);
      }
      let level = this.w(E);
      if (level) { level -= 2; if ((level << 16 >> 16) < 2) level = 2; }
      this.setW(E, level);
      const d1 = (level - 1) & 0xffff;
      const y = this.b(E + 2), h = this.b(E + 3);
      if (h & 0x80) continue;
      let [p2, p3, p5, p6] = PATTERNS[(d1 * 2 & 0x1c) >> 2];
      const m = d1 & 0x8030;
      let variant = m >> 4;
      if (m & 0x8000) { p2 = 0; p6 = 0; variant = 0; }
      const F = 0xffffffff;
      const row = [
        [0, 0, 0, p2, p6, 0, 0, 0],
        [0, 0, p2, p3, p5, p6, 0, 0],
        [0, p2, p3, F, F, p5, p6, 0],
        [p2, p3, F, F, F, F, p5, p6],
      ][variant];
      for (let l = 0; l <= h; l++) {
        const a = buf + 0x16 + (y + l) * 160;
        for (let k = 0; k < 8; k++) { this.putW(a + 16 * k, row[k] >>> 16); this.putW(a + 16 * k + 2, row[k] & 0xffff); }
      }
    }
  }

  /** Colours above the first raster. */
  top(): { c0: number; c1: number } {
    return { c0: this.carry[0], c1: this.carry[1] };
  }

  /** Pixel indices of the screen being shown (4 bitplanes, ST interleaved). */
  pixelsInto(d: Display): void {
    const s = this.screens.get(this.shown)!;
    for (let y = 0; y < 200; y++) {
      const o = d.at(0, y);
      for (let col = 0; col < 20; col++) {
        const q = y * 160 + col * 8;
        const p0 = (s[q] << 8) | s[q + 1], p1 = (s[q + 2] << 8) | s[q + 3];
        const p2 = (s[q + 4] << 8) | s[q + 5], p3 = (s[q + 6] << 8) | s[q + 7];
        for (let b = 0; b < 16; b++) {
          const m = 0x8000 >> b;
          d.pixels[o + col * 16 + b] = (p0 & m ? 1 : 0) | (p1 & m ? 2 : 0) | (p2 & m ? 4 : 0) | (p3 & m ? 8 : 0);
        }
      }
    }
  }
}

/** Fade steps, as the main loop at $1298C does them: ±38 bytes of colour table every 3 VBLs. */
const FADE_STEP = 38, FADE_STEPS = 7;

export class TextPageScene implements Scene {
  readonly m = new TextPageMachine();
  private t = 0;
  private leaving = -1;

  constructor(private readonly app: App, private readonly tick: () => TickInfo | null = () => app.music.current()) {
    app.textPage = this;
  }

  /** Back from the menu: the spreader carries on where it stopped, fading in again. */
  resume(): Scene {
    this.t = 0;
    this.leaving = -1;
    return this;
  }

  vbl(): void {
    this.t++;
    this.m.vbl(this.tick());
    // fade in after the VBL is installed, out after SPACE
    if (this.leaving < 0 && this.t % 3 === 0 && this.t <= 3 * FADE_STEPS) this.m.colorTable += FADE_STEP;
    if (this.leaving >= 0) {
      const k = this.t - this.leaving;
      if (k > 0 && k % 3 === 0 && k <= 3 * FADE_STEPS) this.m.colorTable -= FADE_STEP;
      if (k >= 3 * FADE_STEPS + 3) this.app.go(new MenuScene(this.app));
    }
  }

  draw(d: Display): void {
    d.rows = 200;
    d.fade = 0;
    d.fadeKeepsLsb = false;
    d.clear();
    this.m.pixelsInto(d);
    const lines = this.m.lineColors();
    const pal = new Array(16);
    for (let y = -29; y < 247; y++) {
      const c = y < 0 ? { ...lines[0], ...this.m.top() } : lines[Math.min(199, y)];
      pal.fill(c.text);
      pal[0] = c.c0; pal[1] = c.c1; pal[8] = c.c1;
      for (let i = 0; i < 16; i++) d.setColor(y, i, pal[i]);
    }
  }

  key(code: string): void {
    if (code === 'Space' && this.leaving < 0 && this.t > 3 * FADE_STEPS) this.leaving = this.t;
  }

  pointer(_x: number, _y: number, click: boolean): void {
    if (click) this.key('Space');
  }
}
