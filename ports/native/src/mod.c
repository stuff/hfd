#include "mod.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const int BASE_PERIODS[36] = {
  856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
  428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
  214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
};
/** PERIODS[finetune + 8][note] */
static int PERIODS[16][36];
static const int SINE[32] = {0, 24, 49, 74, 97, 120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
  255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97, 74, 49, 24};

/** JavaScript's Math.round. */
static double js_round(double v) { return floor(v + 0.5); }

static void init_periods(void) {
  static bool done = false;
  if (done) return;
  for (int f = 0; f < 16; f++)
    for (int n = 0; n < 36; n++) PERIODS[f][n] = (int)js_round(BASE_PERIODS[n] * pow(2, -(f - 8) / 96.0));
  done = true;
}

static int word(const uint8_t *d, size_t o) { return (d[o] << 8) | d[o + 1]; }

bool mod_parse(Module *m, const uint8_t *d, size_t size) {
  memset(m, 0, sizeof *m);
  if (size < 1084) return false;
  init_periods();
  m->song_length = d[950];
  m->restart = d[951];
  memcpy(m->order, d + 952, 128);
  int npat = 0;
  for (int i = 0; i < 128; i++) if (m->order[i] + 1 > npat) npat = m->order[i] + 1;
  m->pattern_count = npat;
  m->patterns = calloc((size_t)npat, 1024);
  if (!m->patterns) return false;
  for (size_t i = 0; i < (size_t)npat * 1024; i++)
    if (1084 + i < size) m->patterns[i] = d[1084 + i];
  size_t off = 1084 + (size_t)npat * 1024;
  for (int i = 0; i < 31; i++) {
    size_t h = 20 + (size_t)i * 30;
    int length = word(d, h + 22) * 2;
    int loop_start = word(d, h + 26) * 2, loop_length = word(d, h + 28) * 2;
    size_t a = off < size ? off : size, b = off + length < size ? off + length : size;
    Sample *s = &m->samples[i];
    s->data = (const int8_t *)(d + a);
    s->length = (int)(b - a);
    off += length;
    if (loop_start + loop_length > s->length) loop_length = s->length - loop_start > 0 ? s->length - loop_start : 0;
    int ft = d[h + 24] & 15;
    s->finetune = ft > 7 ? ft - 16 : ft;
    s->volume = d[h + 25] < 64 ? d[h + 25] : 64;
    s->loop_start = loop_start;
    s->loop_length = loop_length;
  }
  return true;
}

void mod_free(Module *m) {
  free(m->patterns);
  m->patterns = NULL;
}

static int note_index(int period) {
  int best = 0;
  for (int i = 1; i < 36; i++)
    if (abs(BASE_PERIODS[i] - period) < abs(BASE_PERIODS[best] - period)) best = i;
  return best;
}

static int waveform(int wave, int pos) {
  int p = pos & 63;
  switch (wave & 3) {
    case 1: return p < 32 ? 255 - p * 8 : -(p - 32) * 8; /* ramp down, as ProTracker plays it */
    case 2: return p < 32 ? 255 : -255;
    default: return p < 32 ? SINE[p] : -SINE[p - 32];
  }
}

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

/** Reached the end of the sample or of its loop. */
static void channel_wrap(Channel *c) {
  if (c->next) { c->sample = c->next; c->next = NULL; }
  const Sample *s = c->sample;
  if (s->loop_length > 2) {
    double over = c->pos - c->end;
    c->end = s->loop_start + s->loop_length;
    c->pos = s->loop_start + fmod(over, s->loop_length);
  } else c->playing = false;
}

static void channel_trigger(Channel *c, const Sample *s, int offset) {
  c->sample = s;
  c->next = NULL;
  if (!s || !s->length) { c->playing = false; return; }
  c->pos = offset;
  c->end = s->length;
  c->playing = true;
  c->triggered = true;
  if (c->pos >= c->end) channel_wrap(c);
  if (c->vib_wave < 4) c->vib_pos = 0;
  if (c->trem_wave < 4) c->trem_pos = 0;
}

void replayer_init(Replayer *r, const Module *mod, double rate, double tick_hz, TickCallback on_tick, void *ctx) {
  memset(r, 0, sizeof *r);
  init_periods();
  r->mod = mod;
  r->rate = rate;
  r->speed = 6;
  r->bpm = 125;
  r->master = 1;
  r->jump_to = r->break_row = r->loop_jump = -1;
  r->tick_hz = tick_hz;
  r->on_tick = on_tick;
  r->on_tick_ctx = ctx;
}

void replayer_fade_out(Replayer *r, int ticks) { r->fade_step = r->master / imax(1, ticks); }

static double samples_per_tick(const Replayer *r) {
  return r->bpm == 125 ? r->rate / r->tick_hz : (r->rate * 2.5) / r->bpm;
}

static void mix(Replayer *r, float *L, float *R, int o, int n) {
  memset(L + o, 0, n * sizeof *L);
  if (R != L) memset(R + o, 0, n * sizeof *R);
  double gain = r->master / (128 * 64 * 2);
  for (int c = 0; c < 4; c++) {
    Channel *ch = &r->ch[c];
    if (!ch->playing || ch->out_period <= 0) continue;
    float *out = c == 0 || c == 3 ? L : R;
    double step = (double)PAULA_CLOCK / ch->out_period / r->rate;
    double vol = ch->out_volume * gain;
    double pos = ch->pos;
    int peak = ch->peak;
    for (int i = 0; i < n; i++) {
      if (pos >= ch->end) {
        ch->pos = pos;
        channel_wrap(ch);
        if (!ch->playing) break;
        pos = ch->pos;
      }
      int s = ch->sample->data[(int)pos];
      out[o + i] = (float)((double)out[o + i] + s * vol); /* as Float32Array += */
      int a = s < 0 ? -s : s;
      if (a > peak) peak = a;
      pos += step;
    }
    ch->pos = pos;
    ch->peak = peak;
  }
}

static void vol_slide(Channel *ch, int x, int y) {
  ch->volume = x ? imin(64, ch->volume + x) : imax(0, ch->volume - y);
}

static void tone_porta(Channel *ch) {
  if (!ch->porta_target) return;
  if (ch->period < ch->porta_target) ch->period = imin(ch->porta_target, ch->period + ch->porta_speed);
  else ch->period = imax(ch->porta_target, ch->period - ch->porta_speed);
  if (ch->period == ch->porta_target) ch->porta_target = 0;
  ch->out_period = ch->period;
}

static void vibrato(Channel *ch) {
  ch->out_period = ch->period + ((waveform(ch->vib_wave, ch->vib_pos) * ch->vib_depth) >> 7);
  ch->vib_pos = (ch->vib_pos + ch->vib_speed) & 63;
}

static void read_row(Replayer *r) {
  const uint8_t *pat = r->mod->patterns + r->mod->order[r->position] * 1024;
  for (int c = 0; c < 4; c++) {
    Channel *ch = &r->ch[c];
    int o = r->row * 16 + c * 4;
    int b0 = pat[o], b1 = pat[o + 1], b2 = pat[o + 2], b3 = pat[o + 3];
    int ins = (b0 & 0xf0) | (b2 >> 4);
    int period = ((b0 & 0x0f) << 8) | b1;
    ch->cmd = b2 & 0x0f;
    ch->param = b3;
    ch->note_or_ins = ins > 0 || period > 0;
    int x = ch->param >> 4, y = ch->param & 15;

    if (ins > 0 && ins <= 31) {
      const Sample *s = &r->mod->samples[ins - 1];
      ch->instrument = s;
      ch->volume = s->volume;
      ch->finetune = s->finetune;
      if (!period && ch->sample != s) ch->next = s;
    }
    if (period) {
      if (ch->cmd == 0xe && x == 5) ch->finetune = y > 7 ? y - 16 : y;
      int note = note_index(period);
      int p = PERIODS[ch->finetune + 8][note];
      if (ch->cmd == 3 || ch->cmd == 5) {
        ch->porta_target = p;
      } else {
        ch->note = note;
        if (ch->cmd == 0xe && x == 0xd && y > 0) {
          ch->delayed_period = p;
        } else {
          ch->period = p;
          int off = 0;
          if (ch->cmd == 9) {
            if (ch->param) ch->offset = ch->param << 8;
            off = ch->offset;
          }
          channel_trigger(ch, ch->instrument, off);
        }
      }
    }
    ch->out_period = ch->period;
    ch->out_volume = ch->volume;

    switch (ch->cmd) {
      case 0x3: if (ch->param) ch->porta_speed = ch->param; break;
      case 0x4: if (x) ch->vib_speed = x; if (y) ch->vib_depth = y; break;
      case 0x7: if (x) ch->trem_speed = x; if (y) ch->trem_depth = y; break;
      case 0xb: r->jump_to = ch->param; r->break_row = imax(r->break_row, 0); break;
      case 0xc: ch->volume = ch->out_volume = imin(64, ch->param); break;
      case 0xd: {
        int row = x * 10 + y;
        r->break_row = row > 63 ? 0 : row;
        break;
      }
      case 0xe:
        switch (x) {
          case 0x1: ch->period = ch->out_period = imax(113, ch->period - y); break;
          case 0x2: ch->period = ch->out_period = imin(856, ch->period + y); break;
          case 0x4: ch->vib_wave = y; break;
          case 0x6:
            if (y == 0) ch->loop_row = r->row;
            else if (ch->loop_count == 0) { ch->loop_count = y; r->loop_jump = ch->loop_row; }
            else if (--ch->loop_count > 0) r->loop_jump = ch->loop_row;
            break;
          case 0x7: ch->trem_wave = y; break;
          case 0xa: ch->volume = ch->out_volume = imin(64, ch->volume + y); break;
          case 0xb: ch->volume = ch->out_volume = imax(0, ch->volume - y); break;
          case 0xc: if (y == 0) ch->volume = ch->out_volume = 0; break;
          case 0xe: if (r->delay_repeat == 0) r->pattern_delay = y; break;
        }
        break;
      case 0xf:
        if (ch->param == 0) break;
        if (ch->param < 32) r->speed = ch->param;
        else r->bpm = ch->param;
        break;
    }
  }
}

static void tick_effects(Replayer *r, Channel *ch) {
  int x = ch->param >> 4, y = ch->param & 15;
  ch->out_period = ch->period;
  switch (ch->cmd) {
    case 0x0:
      if (ch->param) {
        int k = r->tick % 3;
        int n = imin(35, ch->note + (k == 1 ? x : k == 2 ? y : 0));
        ch->out_period = k ? PERIODS[ch->finetune + 8][n] : ch->period;
      }
      break;
    case 0x1: ch->period = ch->out_period = imax(113, ch->period - ch->param); break;
    case 0x2: ch->period = ch->out_period = imin(856, ch->period + ch->param); break;
    case 0x3: tone_porta(ch); break;
    case 0x4: vibrato(ch); break;
    case 0x5: tone_porta(ch); vol_slide(ch, x, y); break;
    case 0x6: vibrato(ch); vol_slide(ch, x, y); break;
    case 0x7: {
      int d = (waveform(ch->trem_wave, ch->trem_pos) * ch->trem_depth) >> 6;
      ch->out_volume = imax(0, imin(64, ch->volume + d));
      ch->trem_pos = (ch->trem_pos + ch->trem_speed) & 63;
      return;
    }
    case 0xa: vol_slide(ch, x, y); break;
    case 0xe:
      if (x == 0x9 && y && r->tick % y == 0) channel_trigger(ch, ch->sample ? ch->sample : ch->instrument, 0);
      if (x == 0xc && r->tick == y) ch->volume = 0;
      if (x == 0xd && r->tick == y && ch->delayed_period) {
        ch->period = ch->out_period = ch->delayed_period;
        ch->delayed_period = 0;
        channel_trigger(ch, ch->instrument, 0);
      }
      break;
  }
  ch->out_volume = ch->volume;
}

static void next_row(Replayer *r) {
  const Module *m = r->mod;
  if (r->loop_jump >= 0) {
    r->row = r->loop_jump;
  } else if (r->jump_to >= 0 || r->break_row >= 0) {
    r->position = r->jump_to >= 0 ? r->jump_to : r->position + 1;
    r->row = r->break_row >= 0 ? r->break_row : 0;
  } else if (++r->row >= 64) {
    r->row = 0;
    r->position++;
  }
  r->loop_jump = r->jump_to = r->break_row = -1;
  if (r->position >= m->song_length) r->position = m->restart < m->song_length ? m->restart : 0;
}

static void do_tick(Replayer *r) {
  if (r->fade_step) r->master = fmax(0, r->master - r->fade_step);
  bool row_read = r->tick == 0 && r->delay_repeat == 0;
  if (row_read) read_row(r);
  else for (int c = 0; c < 4; c++) tick_effects(r, &r->ch[c]);
  for (int c = 0; c < 4; c++) {
    Channel *ch = &r->ch[c];
    if (ch->out_period > 0 && ch->out_period < 113) ch->out_period = 113;
  }
  if (r->on_tick) {
    TickInfo t = {r->frame, r->position, r->row, row_read, {{0}}};
    for (int c = 0; c < 4; c++) {
      const Channel *ch = &r->ch[c];
      t.channels[c] = (TickChannel){
        .volume = ch->playing ? ch->out_volume : 0, .base_volume = ch->volume, .period = ch->out_period,
        .peak = ch->peak, .triggered = ch->triggered,
        .sample = ch->sample ? (int)(ch->sample - r->mod->samples) + 1 : 0,
        .cmd = ch->cmd, .note_or_ins = ch->note_or_ins,
      };
    }
    r->on_tick(r->on_tick_ctx, &t);
  }
  for (int c = 0; c < 4; c++) { r->ch[c].peak = 0; r->ch[c].triggered = false; }
  if (++r->tick >= r->speed) {
    r->tick = 0;
    if (r->delay_repeat > 0) { r->delay_repeat--; if (r->delay_repeat > 0) return; }
    else if (r->pattern_delay > 0) { r->delay_repeat = r->pattern_delay; r->pattern_delay = 0; return; }
    next_row(r);
  }
}

void replayer_render(Replayer *r, float *l, float *rr, int o, int n) {
  while (n > 0) {
    if (r->samples_left <= 0) {
      do_tick(r);
      r->samples_left += samples_per_tick(r);
    }
    int k = (int)fmin(n, ceil(r->samples_left));
    mix(r, l, rr, o, k);
    r->samples_left -= k;
    r->frame += k;
    o += k;
    n -= k;
  }
}
