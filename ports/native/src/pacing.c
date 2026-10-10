#include "pacing.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void pacer_init(Pacer *p, double vbl_ms, int max_catch_up) {
  memset(p, 0, sizeof *p);
  p->refresh = 1000.0 / 60;
  p->vbl_ms = vbl_ms;
  p->max_catch_up = max_catch_up;
  p->model = p->last_now = p->vsync = NAN;
  p->source = -1;
}

static int compare(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return x < y ? -1 : x > y;
}

static int catch_up(Pacer *p) {
  long long target = (long long)floor(p->model / p->vbl_ms);
  long long n = target - p->done;
  p->done = target;
  return n < 0 ? 0 : n > p->max_catch_up ? p->max_catch_up : (int)n;
}

int pacer_step(Pacer *p, double now, double reference, int source) {
  double dt = now - p->last_now;
  p->last_now = now;
  if (dt > 4 && dt < 50) {
    /* mean of the recent periods (jitter cancels out in a mean), leaving out dropped frames */
    if (p->delta_count == PACER_HISTORY) {
      memmove(p->deltas, p->deltas + 1, (PACER_HISTORY - 1) * sizeof *p->deltas);
      p->delta_count--;
    }
    p->deltas[p->delta_count++] = dt;
    double sorted[PACER_HISTORY];
    memcpy(sorted, p->deltas, p->delta_count * sizeof *sorted);
    qsort(sorted, p->delta_count, sizeof *sorted, compare);
    double median = sorted[p->delta_count >> 1], sum = 0;
    int ok = 0;
    for (int i = 0; i < p->delta_count; i++)
      if (p->deltas[i] < median * 1.5) { sum += p->deltas[i]; ok++; }
    p->refresh = sum / ok;
  }
  int new_clock = source != p->source || !(dt > 0);
  if (new_clock || dt > 250) {
    /* new clock: continue from here; back from a pause: catch up (up to max_catch_up) */
    p->source = source;
    p->model = reference;
    p->vsync = now;
    if (new_clock) p->done = (long long)floor(reference / p->vbl_ms);
    return new_clock ? 0 : catch_up(p);
  }
  double refreshes = fmax(1, round((now - p->vsync) / p->refresh));
  p->vsync += refreshes * p->refresh;
  p->vsync += (now - p->vsync) * 0.05;
  p->model += refreshes * p->refresh;
  double err = reference - p->model;
  if (fabs(err) > 100) p->model = reference; /* lost track (window hidden, audio hiccup) */
  else p->model += err * 0.02;
  return catch_up(p);
}
