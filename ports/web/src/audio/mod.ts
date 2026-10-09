/**
 * ProTracker module parser and replayer, mixing like Paula: no interpolation,
 * channels 1+4 left and 2+3 right. It has no browser dependency, so it runs in the
 * AudioWorklet and in Node alike.
 *
 * Ticks run at the ST's VBL rate (the demo's replay is VBL-driven) and none of the
 * 8 modules uses CIA tempo, though Fxx ≥ 32 is handled anyway.
 */

export interface Sample {
  name: string;
  length: number;
  finetune: number; // -8..7
  volume: number;
  loopStart: number;
  loopLength: number; // ≤ 2 means no loop
  data: Int8Array;
}

export interface Module {
  title: string;
  samples: Sample[];
  songLength: number;
  restart: number;
  order: Uint8Array;
  patterns: Uint8Array[]; // 64 rows × 4 channels × 4 bytes
}

export const PAULA_CLOCK = 3546895; // PAL
export const ST_VBL_HZ = 50.0533;

export function parseMod(buf: ArrayBuffer): Module {
  const d = new Uint8Array(buf);
  const str = (o: number, n: number) => String.fromCharCode(...d.subarray(o, o + n)).replace(/\0.*$/s, '');
  const word = (o: number) => (d[o] << 8) | d[o + 1];
  const songLength = d[950];
  const order = d.slice(952, 952 + 128);
  let npat = 0;
  for (let i = 0; i < 128; i++) npat = Math.max(npat, order[i] + 1);
  const patterns: Uint8Array[] = [];
  for (let p = 0; p < npat; p++) patterns.push(d.slice(1084 + p * 1024, 1084 + (p + 1) * 1024));
  let off = 1084 + npat * 1024;
  const samples: Sample[] = [];
  for (let i = 0; i < 31; i++) {
    const h = 20 + i * 30;
    const length = word(h + 22) * 2;
    let loopStart = word(h + 26) * 2, loopLength = word(h + 28) * 2;
    const data = new Int8Array(buf.slice(Math.min(off, d.length), Math.min(off + length, d.length)));
    off += length;
    if (loopStart + loopLength > data.length) loopLength = Math.max(0, data.length - loopStart);
    const ft = d[h + 24] & 15;
    samples.push({ name: str(h, 22), length: data.length, finetune: ft > 7 ? ft - 16 : ft,
      volume: Math.min(64, d[h + 25]), loopStart, loopLength, data });
  }
  return { title: str(0, 20), samples, songLength, restart: d[951], order, patterns };
}

const BASE_PERIODS = [
  856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
  428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
  214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
];
/** PERIODS[finetune + 8][note] */
const PERIODS = Array.from({ length: 16 }, (_, f) =>
  BASE_PERIODS.map((p) => Math.round(p * Math.pow(2, -(f - 8) / 96))));
const SINE = [0, 24, 49, 74, 97, 120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
  255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97, 74, 49, 24];

function noteIndex(period: number): number {
  let best = 0;
  for (let i = 1; i < 36; i++) if (Math.abs(BASE_PERIODS[i] - period) < Math.abs(BASE_PERIODS[best] - period)) best = i;
  return best;
}

function waveform(wave: number, pos: number): number {
  const p = pos & 63;
  switch (wave & 3) {
    case 1: return p < 32 ? 255 - p * 8 : -(p - 32) * 8; // ramp down, as ProTracker plays it
    case 2: return p < 32 ? 255 : -255;
    default: return p < 32 ? SINE[p] : -SINE[p - 32];
  }
}

class Channel {
  sample: Sample | null = null;
  next: Sample | null = null; // instrument given without a note: swapped in at the loop
  pos = 0;
  end = 0;
  playing = false;
  instrument: Sample | null = null;
  period = 0;
  outPeriod = 0;
  volume = 0;
  outVolume = 0;
  finetune = 0;
  note = 0;
  cmd = 0;
  param = 0;
  /** The current row has a note or an instrument for this channel. */
  noteOrIns = false;
  portaTarget = 0;
  portaSpeed = 0;
  vibPos = 0; vibSpeed = 0; vibDepth = 0; vibWave = 0;
  tremPos = 0; tremSpeed = 0; tremDepth = 0; tremWave = 0;
  offset = 0;
  loopRow = 0;
  loopCount = 0;
  delayedPeriod = 0;
  // analyser
  peak = 0;
  triggered = false;

  trigger(s: Sample | null, offset = 0): void {
    this.sample = s;
    this.next = null;
    if (!s || !s.length) { this.playing = false; return; }
    this.pos = offset;
    this.end = s.length;
    this.playing = true;
    this.triggered = true;
    if (this.pos >= this.end) this.wrap();
    if (this.vibWave < 4) this.vibPos = 0;
    if (this.tremWave < 4) this.tremPos = 0;
  }

  /** Reached the end of the sample or of its loop. */
  wrap(): void {
    if (this.next) { this.sample = this.next; this.next = null; }
    const s = this.sample!;
    if (s.loopLength > 2) {
      const over = this.pos - this.end;
      this.end = s.loopStart + s.loopLength;
      this.pos = s.loopStart + (over % s.loopLength);
    } else this.playing = false;
  }
}

export interface TickInfo {
  /** Output frame at which this tick starts. */
  frame: number;
  position: number;
  row: number;
  /** This tick read a new row (tick 0). */
  rowRead: boolean;
  channels: {
    /** Volume heard (tremolo included), 0 when silent. */
    volume: number;
    /** Channel volume as the effects set it (what the demo's voice analysers read). */
    baseVolume: number;
    period: number;
    peak: number;
    triggered: boolean;
    sample: number;
    /** Effect command and whether the row has a note or instrument, for the current row. */
    cmd: number;
    noteOrIns: boolean;
  }[];
}

export class Replayer {
  readonly ch = [new Channel(), new Channel(), new Channel(), new Channel()];
  speed = 6;
  bpm = 125;
  tick = 0;
  row = 0;
  position = 0;
  /** Master volume 0..1, and a linear fade towards 0 over `fadeTicks`. */
  master = 1;
  private fadeStep = 0;
  private patternDelay = 0;
  private delayRepeat = 0;
  private jumpTo = -1;
  private breakRow = -1;
  private loopJump = -1;
  private samplesLeft = 0;
  private frame = 0;
  private readonly tickHz: number;

  constructor(readonly mod: Module, readonly rate: number, tickHz = ST_VBL_HZ,
    private readonly onTick?: (t: TickInfo) => void) {
    this.tickHz = tickHz;
  }

  fadeOut(ticks: number): void {
    this.fadeStep = this.master / Math.max(1, ticks);
  }

  private samplesPerTick(): number {
    return this.bpm === 125 ? this.rate / this.tickHz : (this.rate * 2.5) / this.bpm;
  }

  /** Renders `n` stereo frames into l/r starting at `o`. */
  render(l: Float32Array, r: Float32Array, o: number, n: number): void {
    while (n > 0) {
      if (this.samplesLeft <= 0) {
        this.doTick();
        this.samplesLeft += this.samplesPerTick();
      }
      const k = Math.min(n, Math.ceil(this.samplesLeft));
      this.mix(l, r, o, k);
      this.samplesLeft -= k;
      this.frame += k;
      o += k;
      n -= k;
    }
  }

  private mix(L: Float32Array, R: Float32Array, o: number, n: number): void {
    L.fill(0, o, o + n);
    R.fill(0, o, o + n);
    const gain = this.master / (128 * 64 * 2);
    for (let c = 0; c < 4; c++) {
      const ch = this.ch[c];
      if (!ch.playing || ch.outPeriod <= 0) continue;
      const out = c === 0 || c === 3 ? L : R;
      const step = PAULA_CLOCK / ch.outPeriod / this.rate;
      const vol = ch.outVolume * gain;
      let pos = ch.pos, peak = ch.peak;
      for (let i = 0; i < n; i++) {
        if (pos >= ch.end) {
          ch.pos = pos;
          ch.wrap();
          if (!ch.playing) break;
          pos = ch.pos;
        }
        const s = ch.sample!.data[pos | 0];
        out[o + i] += s * vol;
        const a = s < 0 ? -s : s;
        if (a > peak) peak = a;
        pos += step;
      }
      ch.pos = pos;
      ch.peak = peak;
    }
  }

  private doTick(): void {
    if (this.fadeStep) this.master = Math.max(0, this.master - this.fadeStep);
    const rowRead = this.tick === 0 && this.delayRepeat === 0;
    if (rowRead) this.readRow();
    else for (const ch of this.ch) this.tickEffects(ch);
    for (const ch of this.ch) {
      if (ch.outPeriod > 0 && ch.outPeriod < 113) ch.outPeriod = 113;
    }
    if (this.onTick) {
      this.onTick({
        frame: this.frame, position: this.position, row: this.row, rowRead,
        channels: this.ch.map((ch) => ({
          volume: ch.playing ? ch.outVolume : 0, baseVolume: ch.volume, period: ch.outPeriod, peak: ch.peak,
          triggered: ch.triggered, sample: ch.sample ? this.mod.samples.indexOf(ch.sample) + 1 : 0,
          cmd: ch.cmd, noteOrIns: ch.noteOrIns,
        })),
      });
    }
    for (const ch of this.ch) { ch.peak = 0; ch.triggered = false; }
    if (++this.tick >= this.speed) {
      this.tick = 0;
      if (this.delayRepeat > 0) { this.delayRepeat--; if (this.delayRepeat > 0) return; }
      else if (this.patternDelay > 0) { this.delayRepeat = this.patternDelay; this.patternDelay = 0; return; }
      this.nextRow();
    }
  }

  private nextRow(): void {
    const m = this.mod;
    if (this.loopJump >= 0) {
      this.row = this.loopJump;
    } else if (this.jumpTo >= 0 || this.breakRow >= 0) {
      this.position = this.jumpTo >= 0 ? this.jumpTo : this.position + 1;
      this.row = this.breakRow >= 0 ? this.breakRow : 0;
    } else if (++this.row >= 64) {
      this.row = 0;
      this.position++;
    }
    this.loopJump = this.jumpTo = this.breakRow = -1;
    if (this.position >= m.songLength) this.position = m.restart < m.songLength ? m.restart : 0;
  }

  private readRow(): void {
    const pat = this.mod.patterns[this.mod.order[this.position]];
    for (let c = 0; c < 4; c++) {
      const ch = this.ch[c];
      const o = this.row * 16 + c * 4;
      const b0 = pat[o], b1 = pat[o + 1], b2 = pat[o + 2], b3 = pat[o + 3];
      const ins = (b0 & 0xf0) | (b2 >> 4);
      const period = ((b0 & 0x0f) << 8) | b1;
      ch.cmd = b2 & 0x0f;
      ch.param = b3;
      ch.noteOrIns = ins > 0 || period > 0;
      const x = ch.param >> 4, y = ch.param & 15;

      if (ins > 0 && ins <= 31) {
        const s = this.mod.samples[ins - 1];
        ch.instrument = s;
        ch.volume = s.volume;
        ch.finetune = s.finetune;
        if (!period && ch.sample !== s) ch.next = s;
      }
      if (period) {
        if (ch.cmd === 0xe && x === 5) ch.finetune = y > 7 ? y - 16 : y;
        const note = noteIndex(period);
        const p = PERIODS[ch.finetune + 8][note];
        if (ch.cmd === 3 || ch.cmd === 5) {
          ch.portaTarget = p;
        } else {
          ch.note = note;
          if (ch.cmd === 0xe && x === 0xd && y > 0) {
            ch.delayedPeriod = p;
          } else {
            ch.period = p;
            let off = 0;
            if (ch.cmd === 9) {
              if (ch.param) ch.offset = ch.param << 8;
              off = ch.offset;
            }
            ch.trigger(ch.instrument, off);
          }
        }
      }
      ch.outPeriod = ch.period;
      ch.outVolume = ch.volume;

      switch (ch.cmd) {
        case 0x3: if (ch.param) ch.portaSpeed = ch.param; break;
        case 0x4: if (x) ch.vibSpeed = x; if (y) ch.vibDepth = y; break;
        case 0x7: if (x) ch.tremSpeed = x; if (y) ch.tremDepth = y; break;
        case 0xb: this.jumpTo = ch.param; this.breakRow = Math.max(this.breakRow, 0); break;
        case 0xc: ch.volume = ch.outVolume = Math.min(64, ch.param); break;
        case 0xd: {
          const r = x * 10 + y;
          this.breakRow = r > 63 ? 0 : r;
          break;
        }
        case 0xe:
          switch (x) {
            case 0x1: ch.period = ch.outPeriod = Math.max(113, ch.period - y); break;
            case 0x2: ch.period = ch.outPeriod = Math.min(856, ch.period + y); break;
            case 0x4: ch.vibWave = y; break;
            case 0x6:
              if (y === 0) ch.loopRow = this.row;
              else if (ch.loopCount === 0) { ch.loopCount = y; this.loopJump = ch.loopRow; }
              else if (--ch.loopCount > 0) this.loopJump = ch.loopRow;
              break;
            case 0x7: ch.tremWave = y; break;
            case 0xa: ch.volume = ch.outVolume = Math.min(64, ch.volume + y); break;
            case 0xb: ch.volume = ch.outVolume = Math.max(0, ch.volume - y); break;
            case 0xc: if (y === 0) ch.volume = ch.outVolume = 0; break;
            case 0xe: if (this.delayRepeat === 0) this.patternDelay = y; break;
          }
          break;
        case 0xf:
          if (ch.param === 0) break;
          if (ch.param < 32) this.speed = ch.param;
          else this.bpm = ch.param;
          break;
      }
    }
  }

  private tickEffects(ch: Channel): void {
    const x = ch.param >> 4, y = ch.param & 15;
    ch.outPeriod = ch.period;
    switch (ch.cmd) {
      case 0x0:
        if (ch.param) {
          const k = this.tick % 3;
          const n = Math.min(35, ch.note + (k === 1 ? x : k === 2 ? y : 0));
          ch.outPeriod = k ? PERIODS[ch.finetune + 8][n] : ch.period;
        }
        break;
      case 0x1: ch.period = ch.outPeriod = Math.max(113, ch.period - ch.param); break;
      case 0x2: ch.period = ch.outPeriod = Math.min(856, ch.period + ch.param); break;
      case 0x3: this.tonePorta(ch); break;
      case 0x4: this.vibrato(ch); break;
      case 0x5: this.tonePorta(ch); this.volSlide(ch, x, y); break;
      case 0x6: this.vibrato(ch); this.volSlide(ch, x, y); break;
      case 0x7: {
        const d = (waveform(ch.tremWave, ch.tremPos) * ch.tremDepth) >> 6;
        ch.outVolume = Math.max(0, Math.min(64, ch.volume + d));
        ch.tremPos = (ch.tremPos + ch.tremSpeed) & 63;
        return;
      }
      case 0xa: this.volSlide(ch, x, y); break;
      case 0xe:
        if (x === 0x9 && y && this.tick % y === 0) ch.trigger(ch.sample ?? ch.instrument);
        if (x === 0xc && this.tick === y) ch.volume = 0;
        if (x === 0xd && this.tick === y && ch.delayedPeriod) {
          ch.period = ch.outPeriod = ch.delayedPeriod;
          ch.delayedPeriod = 0;
          ch.trigger(ch.instrument);
        }
        break;
    }
    ch.outVolume = ch.volume;
  }

  private volSlide(ch: Channel, x: number, y: number): void {
    ch.volume = x ? Math.min(64, ch.volume + x) : Math.max(0, ch.volume - y);
  }

  private tonePorta(ch: Channel): void {
    if (!ch.portaTarget) return;
    if (ch.period < ch.portaTarget) ch.period = Math.min(ch.portaTarget, ch.period + ch.portaSpeed);
    else ch.period = Math.max(ch.portaTarget, ch.period - ch.portaSpeed);
    if (ch.period === ch.portaTarget) ch.portaTarget = 0;
    ch.outPeriod = ch.period;
  }

  private vibrato(ch: Channel): void {
    ch.outPeriod = ch.period + ((waveform(ch.vibWave, ch.vibPos) * ch.vibDepth) >> 7);
    ch.vibPos = (ch.vibPos + ch.vibSpeed) & 63;
  }
}
