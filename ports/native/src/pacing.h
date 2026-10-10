/*
 * Decides how many ST VBLs (50.05 Hz) to run on each display refresh.
 * Port of ports/web/src/pacing.ts.
 *
 * A display at 60 Hz can't show 50 frames per second evenly: one frame in five has to
 * stay up for two refreshes. What we can avoid is making it worse: if each refresh's
 * timestamp is compared with the VBL clock directly, their jitter sometimes gives 0 then
 * 2 VBLs where 1 and 1 were due. So the clock used here advances by whole refresh periods
 * (measured), which makes the pattern exactly regular, and it is pulled gently towards a
 * reference clock (the sound being heard, once there is sound) so that pictures and music
 * never drift apart.
 */
#ifndef HFD_PACING_H
#define HFD_PACING_H

#define PACER_HISTORY 120

typedef struct {
  /** Display refresh period, estimated from recent frames (ms). */
  double refresh;
  /** VBLs run so far. */
  long long done;
  double vbl_ms;
  int max_catch_up;
  double deltas[PACER_HISTORY];
  int delta_count;
  double model;   /* the paced clock (ms) */
  double last_now;
  double vsync;   /* modelled time of the last refresh: counting from it ignores timestamp jitter */
  int source;     /* -1 before the first step */
} Pacer;

void pacer_init(Pacer *p, double vbl_ms, int max_catch_up);
/**
 * Called once per display refresh with the refresh timestamp and the reference clock
 * (both ms). Returns how many VBLs to run now. `source` names the reference clock: when
 * it changes (e.g. from the wall clock to the audio clock) the paced clock is re-based.
 */
int pacer_step(Pacer *p, double now, double reference, int source);

#endif
