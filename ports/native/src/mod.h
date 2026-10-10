/*
 * ProTracker module parser and replayer, mixing like Paula: no interpolation,
 * channels 1+4 left and 2+3 right. Port of ports/web/src/audio/mod.ts, kept
 * bit-exact with it (same double-precision arithmetic, float mix buffers).
 *
 * Ticks run at the ST's VBL rate (the demo's replay is VBL-driven) and none of the
 * 8 modules uses CIA tempo, though Fxx ≥ 32 is handled anyway.
 */
#ifndef HFD_MOD_H
#define HFD_MOD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PAULA_CLOCK 3546895 /* PAL */
#define ST_VBL_HZ 50.0533

typedef struct {
  int length;
  int finetune; /* -8..7 */
  int volume;
  int loop_start;
  int loop_length; /* ≤ 2 means no loop */
  const int8_t *data;
} Sample;

typedef struct {
  Sample samples[31];
  int song_length;
  int restart;
  uint8_t order[128];
  int pattern_count;
  uint8_t *patterns; /* pattern_count × (64 rows × 4 channels × 4 bytes) */
} Module;

/** Parses a module; its samples point into `data`, which must outlive it. */
bool mod_parse(Module *m, const uint8_t *data, size_t size);
void mod_free(Module *m);

typedef struct {
  /** Volume heard (tremolo included), 0 when silent. */
  int volume;
  /** Channel volume as the effects set it (what the demo's voice analysers read). */
  int base_volume;
  int period;
  int peak;
  bool triggered;
  int sample;
  /** Effect command and whether the row has a note or instrument, for the current row. */
  int cmd;
  bool note_or_ins;
} TickChannel;

typedef struct {
  /** Output frame at which this tick starts. */
  int64_t frame;
  int position;
  int row;
  /** This tick read a new row (tick 0). */
  bool row_read;
  TickChannel channels[4];
} TickInfo;

typedef struct {
  const Sample *sample;
  const Sample *next; /* instrument given without a note: swapped in at the loop */
  double pos;
  int end;
  bool playing;
  const Sample *instrument;
  int period, out_period;
  int volume, out_volume;
  int finetune;
  int note;
  int cmd, param;
  /** The current row has a note or an instrument for this channel. */
  bool note_or_ins;
  int porta_target, porta_speed;
  int vib_pos, vib_speed, vib_depth, vib_wave;
  int trem_pos, trem_speed, trem_depth, trem_wave;
  int offset;
  int loop_row, loop_count;
  int delayed_period;
  /* analyser */
  int peak;
  bool triggered;
} Channel;

typedef void (*TickCallback)(void *ctx, const TickInfo *t);

typedef struct {
  const Module *mod;
  double rate;
  Channel ch[4];
  int speed, bpm, tick, row, position;
  /** Master volume 0..1, and a linear fade towards 0. */
  double master;
  double fade_step;
  int pattern_delay, delay_repeat;
  int jump_to, break_row, loop_jump;
  double samples_left;
  int64_t frame;
  double tick_hz;
  TickCallback on_tick;
  void *on_tick_ctx;
} Replayer;

void replayer_init(Replayer *r, const Module *mod, double rate, double tick_hz, TickCallback on_tick, void *ctx);
/** Linear fade to silence over `ticks` replay ticks. */
void replayer_fade_out(Replayer *r, int ticks);
/** Renders `n` stereo frames into l/r starting at `o`. */
void replayer_render(Replayer *r, float *l, float *rr, int o, int n);

#endif
