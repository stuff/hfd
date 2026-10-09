import workletUrl from './worklet.ts?worker&url';
import type { TickInfo } from './mod';
import type { WorkletCommand, WorkletEvent } from './worklet';

/** Plays modules in an AudioWorklet and tells which replay tick is being heard right now. */
export class Music {
  private ctx!: AudioContext;
  private node!: AudioWorkletNode;
  private queue: TickInfo[] = [];
  private heard: TickInfo | null = null;
  private readonly cache = new Map<string, Promise<ArrayBuffer>>();

  /** Must be called from a user gesture (browsers keep audio muted until then). */
  async start(): Promise<void> {
    this.ctx = new AudioContext({ latencyHint: 'interactive' });
    await this.ctx.audioWorklet.addModule(workletUrl);
    this.node = new AudioWorkletNode(this.ctx, 'mod-processor', { numberOfInputs: 0, outputChannelCount: [2] });
    this.node.connect(this.ctx.destination);
    this.node.port.onmessage = (e: MessageEvent<WorkletEvent>) => {
      if (e.data.type === 'ticks') this.queue.push(...e.data.ticks);
    };
    await this.ctx.resume();
  }

  load(url: string): Promise<ArrayBuffer> {
    let p = this.cache.get(url);
    if (!p) {
      p = fetch(url).then((r) => {
        if (!r.ok) throw new Error(`${url}: ${r.status}`);
        return r.arrayBuffer();
      });
      this.cache.set(url, p);
    }
    return p;
  }

  async play(url: string): Promise<void> {
    const data = (await this.load(url)).slice(0);
    this.queue = [];
    this.heard = null;
    this.send({ type: 'play', data }, [data]);
  }

  stop(): void {
    this.send({ type: 'stop' });
    this.queue = [];
    this.heard = null;
  }

  /** Linear fade to silence over `ticks` replay ticks (VBLs). */
  fadeOut(ticks: number): void {
    this.send({ type: 'fadeOut', ticks });
  }

  /** The replay tick being heard now, for the analysers. */
  current(): TickInfo | null {
    if (!this.ctx) return null;
    const latency = (this.ctx.outputLatency || 0) + (this.ctx.baseLatency || 0);
    const now = (this.ctx.currentTime - latency) * this.ctx.sampleRate;
    while (this.queue.length && this.queue[0].frame <= now) this.heard = this.queue.shift()!;
    return this.heard;
  }

  private send(m: WorkletCommand, transfer: Transferable[] = []): void {
    this.node?.port.postMessage(m, transfer);
  }
}
