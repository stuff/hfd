/**
 * Decides how many ST VBLs (50.05 Hz) to run on each display refresh.
 *
 * A display at 60 Hz can't show 50 frames per second evenly: one frame in five has to
 * stay up for two refreshes. What we can avoid is making it worse: if each refresh's
 * timestamp is compared with the VBL clock directly, their jitter sometimes gives 0 then
 * 2 VBLs where 1 and 1 were due. So the clock used here advances by whole refresh periods
 * (measured), which makes the pattern exactly regular, and it is pulled gently towards a
 * reference clock (the sound being heard, once there is sound) so that pictures and music
 * never drift apart.
 */
export class Pacer {
  /** Display refresh period, estimated from recent frames (ms). */
  refresh = 1000 / 60;
  /** VBLs run so far. */
  done = 0;
  private readonly deltas: number[] = [];
  private model = NaN; // the paced clock (ms)
  private lastNow = NaN;
  private vsync = NaN; // modelled time of the last refresh: counting from it ignores timestamp jitter
  private source = '';

  constructor(private readonly vblMs: number, private readonly maxCatchUp = 500) {}

  /**
   * Called once per display refresh with the refresh timestamp and the reference clock
   * (both ms). Returns how many VBLs to run now. `source` names the reference clock: when
   * it changes (e.g. from the page clock to the audio clock) the paced clock is re-based.
   */
  step(now: number, reference: number, source: string): number {
    const dt = now - this.lastNow;
    this.lastNow = now;
    if (dt > 4 && dt < 50) {
      // mean of the recent periods (jitter cancels out in a mean), leaving out dropped frames
      this.deltas.push(dt);
      if (this.deltas.length > 120) this.deltas.shift();
      const median = [...this.deltas].sort((a, b) => a - b)[this.deltas.length >> 1];
      const ok = this.deltas.filter((d) => d < median * 1.5);
      this.refresh = ok.reduce((a, b) => a + b, 0) / ok.length;
    }
    const newClock = source !== this.source || !(dt > 0);
    if (newClock || dt > 250) {
      // new clock: continue from here; back from the background: catch up (up to maxCatchUp)
      this.source = source;
      this.model = reference;
      this.vsync = now;
      if (newClock) this.done = Math.floor(reference / this.vblMs);
      return newClock ? 0 : this.catchUp();
    }
    const refreshes = Math.max(1, Math.round((now - this.vsync) / this.refresh));
    this.vsync += refreshes * this.refresh;
    this.vsync += (now - this.vsync) * 0.05;
    this.model += refreshes * this.refresh;
    const err = reference - this.model;
    if (Math.abs(err) > 100) this.model = reference; // lost track (tab in the background, audio hiccup)
    else this.model += err * 0.02;
    return this.catchUp();
  }

  private catchUp(): number {
    const target = Math.floor(this.model / this.vblMs);
    const n = target - this.done;
    this.done = target;
    return Math.max(0, Math.min(n, this.maxCatchUp));
  }
}
