import { parseMod, Replayer, type TickInfo } from './mod';

export type WorkletCommand =
  | { type: 'play'; data: ArrayBuffer }
  | { type: 'stop' }
  | { type: 'fadeOut'; ticks: number };

export type WorkletEvent = { type: 'ticks'; ticks: TickInfo[] } | { type: 'ended' };

class ModProcessor extends AudioWorkletProcessor {
  private player: Replayer | null = null;
  private pending: TickInfo[] = [];
  private startFrame = 0;

  constructor() {
    super();
    this.port.onmessage = (e: MessageEvent<WorkletCommand>) => {
      const m = e.data;
      if (m.type === 'play') {
        this.player = new Replayer(parseMod(m.data), sampleRate, undefined, (t) => {
          // Tick frames are relative to the start of the song; make them absolute.
          this.pending.push({ ...t, frame: t.frame + this.startFrame });
        });
        this.startFrame = currentFrame;
      } else if (m.type === 'stop') {
        this.player = null;
      } else if (m.type === 'fadeOut') {
        this.player?.fadeOut(m.ticks);
      }
    };
  }

  process(_in: Float32Array[][], outputs: Float32Array[][]): boolean {
    const [l, r] = outputs[0];
    if (this.player) this.player.render(l, r ?? l, 0, l.length);
    if (this.pending.length) {
      this.port.postMessage({ type: 'ticks', ticks: this.pending } satisfies WorkletEvent);
      this.pending = [];
    }
    return true;
  }
}

registerProcessor('mod-processor', ModProcessor);
