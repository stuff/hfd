/**
 * Renders a module with the web replayer into a 16-bit stereo WAV.
 * usage: vite-node scripts/render-audio.ts -- <module.mod> <out.wav> <seconds> [rate]
 */
import { readFileSync, writeFileSync } from 'node:fs';
import { parseMod, Replayer } from '../src/audio/mod';

const [file, out, seconds, rateArg] = process.argv.slice(2).filter((a) => a !== '--');
const rate = Number(rateArg ?? 44100);
const buf = readFileSync(file);
const mod = parseMod(buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength) as ArrayBuffer);
const n = Math.round(Number(seconds) * rate);
const l = new Float32Array(n), r = new Float32Array(n);
new Replayer(mod, rate, Number(process.env.TICK_HZ ?? 50.0533)).render(l, r, 0, n);

const wav = Buffer.alloc(44 + n * 4);
wav.write('RIFF', 0); wav.writeUInt32LE(36 + n * 4, 4); wav.write('WAVEfmt ', 8);
wav.writeUInt32LE(16, 16); wav.writeUInt16LE(1, 20); wav.writeUInt16LE(2, 22);
wav.writeUInt32LE(rate, 24); wav.writeUInt32LE(rate * 4, 28); wav.writeUInt16LE(4, 32); wav.writeUInt16LE(16, 34);
wav.write('data', 36); wav.writeUInt32LE(n * 4, 40);
const clip = (v: number) => Math.max(-32768, Math.min(32767, Math.round(v * 32767)));
for (let i = 0; i < n; i++) {
  wav.writeInt16LE(clip(l[i]), 44 + i * 4);
  wav.writeInt16LE(clip(r[i]), 46 + i * 4);
}
writeFileSync(out, wav);
console.log(`${mod.title}: ${seconds}s -> ${out}`);
