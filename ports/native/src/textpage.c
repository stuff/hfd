/*
 * The text pages: a port of the 68000 code that runs them (see the disassembly in
 * rip/extracted/disasm/textpage_4000.s, addresses in comments), by way of
 * ports/web/src/scenes/textpage.ts.
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
#include <stdlib.h>
#include <string.h>
#include "scene.h"

#define RAM_BASE 0x12000u
#define SCREEN_A 0x60000u
#define SCREEN_B 0x68000u
#define SCREEN_SIZE 32000

/* Addresses of variables (mostly self-modified instruction operands). */
enum {
  REC = 0x12b32, REC_END = 0x12ad4, SCRIPT = 0x12adc, SCRIPT_COUNT = 0x12b50, REC_STEP = 0x12ac4,
  COLTAB = 0x12a54, /* colour table in use: 8 tables of 38 bytes, black to full brightness */
  HANDLERS = 0x12b52, HANDLER_SIZE = 0x28,
  NEXT_SCREEN = 0x1e554, DRAW_SCREEN = 0x1e55a,
  BUBBLE_PTR = 0x1e73c, TEXT_PTR = 0x1e884,
  BUBBLE_X = 0x1e9bc, BUBBLE_Y = 0x1e9be, TEXT_X = 0x1e9c0, TEXT_Y = 0x1e9c2, PAUSE = 0x1e9c4,
  PITCH = 0x1e950, CLEAR_COUNT = 0x1e96c,
  SLOTS = 0x1e9c8, SLOT_SIZE = 22, SLOT_COUNT = 33,
  ANALYSERS = 0x1e2e4,
  FONT = 0x1ec9e, BUBBLE = 0x2189e,
  PATH_START = 0x21c9e, PATH_END = 0x2214e, TEXT_START = 0x2214e,
};

/** STE palette word ($RGB, odd STE nibble order) → 0xRGB with linear 4-bit components. */
static int ste_color(int w) {
  int c = 0;
  for (int s = 8; s >= 0; s -= 4) {
    int n = (w >> s) & 15;
    c |= (((n & 7) << 1) | (n >> 3)) << s;
  }
  return c;
}

/** Analyser stripe patterns (d2, d3, d5, d6 at $1E314-$1E3E0), by (level - 1) bits 1-3. */
static const uint32_t PATTERNS[8][4] = {
  {0x1, 0x3777ffff, 0xfffff776, 0x40000000},
  {0x13, 0x777fffff, 0xffffff77, 0x64000000},
  {0x137, 0x77ffffff, 0xfffffff7, 0x76400000},
  {0x1377, 0x7fffffff, 0xffffffff, 0x77640000},
  {0x13777, 0xffffffff, 0xffffffff, 0xf7764000},
  {0x13777f, 0xffffffff, 0xffffffff, 0xff776400},
  {0x13777ff, 0xffffffff, 0xffffffff, 0xfff77640},
  {0x13777fff, 0xffffffff, 0xffffffff, 0xffff7764},
};

typedef struct { int c0, c1, text; } LineColors;

typedef struct {
  uint8_t *ram;
  size_t ram_size;
  uint8_t screens[2][SCREEN_SIZE];
  /** What the original keeps at screen+$7D00: the bubble's and the letters' addresses to erase. */
  uint32_t erase_bubble[2];
  uint32_t erase_bobs[2][SLOT_COUNT];
  int erase_bob_count[2];
  /** Screen being shown (set during the previous VBL). */
  uint32_t shown;
  /** Colours 0/1 (8 = 1) left by the last raster of the previous frame, shown above the first band. */
  int carry[2];
  int first_count;
  int text;
} Machine;

/* big-endian accessors on the RAM copy (out of range: reads 0, writes are ignored) */
static uint8_t *ram_at(Machine *m, uint32_t a, int n) {
  uint32_t o = a - RAM_BASE;
  return a >= RAM_BASE && o + n <= m->ram_size ? m->ram + o : NULL;
}
static int b(Machine *m, uint32_t a) { uint8_t *p = ram_at(m, a, 1); return p ? p[0] : 0; }
static int w(Machine *m, uint32_t a) { uint8_t *p = ram_at(m, a, 2); return p ? (p[0] << 8) | p[1] : 0; }
static int sw(Machine *m, uint32_t a) { return (int16_t)w(m, a); }
static uint32_t l(Machine *m, uint32_t a) {
  uint8_t *p = ram_at(m, a, 4);
  return p ? (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3] : 0;
}
static int32_t sl(Machine *m, uint32_t a) { return (int32_t)l(m, a); }
static void set_b(Machine *m, uint32_t a, int v) { uint8_t *p = ram_at(m, a, 1); if (p) p[0] = (uint8_t)v; }
static void set_w(Machine *m, uint32_t a, int v) {
  uint8_t *p = ram_at(m, a, 2);
  if (p) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
}
static void set_l(Machine *m, uint32_t a, uint32_t v) {
  uint8_t *p = ram_at(m, a, 4);
  if (p) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
}

/* screen memory accessors (addresses as on the ST; outside the 32,000 bytes: ignored) */
static int screen_index(uint32_t base) { return base >= SCREEN_B ? 1 : 0; }
static uint8_t *scr(Machine *m, uint32_t a) {
  uint32_t base = a >= SCREEN_B ? SCREEN_B : SCREEN_A;
  int64_t o = (int64_t)a - base;
  return o >= 0 && o < SCREEN_SIZE ? &m->screens[screen_index(base)][o] : NULL;
}
static void put_b(Machine *m, uint32_t a, int v) { uint8_t *p = scr(m, a); if (p) *p = (uint8_t)v; }
static void or_b(Machine *m, uint32_t a, int v) { uint8_t *p = scr(m, a); if (p) *p |= (uint8_t)v; }
static void put_w(Machine *m, uint32_t a, int v) { put_b(m, a, v >> 8); put_b(m, a + 1, v); }
static void or_w(Machine *m, uint32_t a, int v) { or_b(m, a, v >> 8); or_b(m, a + 1, v); }

/* ---- $12A36: rasters ---------------------------------------------------------------- */
static void entry(Machine *m, uint32_t a2, int h, int off) {
  uint32_t p = HANDLERS + h * HANDLER_SIZE;
  set_l(m, p + 2, l(m, a2 + off));
  set_w(m, p + 0xa, w(m, a2 + off + 2));
}

static void rasters(Machine *m) {
  uint32_t a0 = l(m, REC) + 8;
  m->first_count = b(m, a0++);
  uint32_t a2 = l(m, COLTAB);
  m->text = ste_color(w(m, a2 + 0x24));
  for (int i = 0; i < 6; i++) {
    if (i > 0) set_b(m, HANDLERS + (2 * i - 1) * HANDLER_SIZE + 0x15, b(m, a0++));
    int d1 = b(m, a0++);
    entry(m, a2, 2 * i, (d1 & 0xf0) >> 2);
    set_b(m, HANDLERS + 2 * i * HANDLER_SIZE + 0x15, b(m, a0++));
    entry(m, a2, 2 * i + 1, (d1 & 0x0f) * 4);
  }
  /* step through the record stream and the script */
  set_l(m, REC, l(m, REC) + (uint32_t)sl(m, REC_STEP));
  if (l(m, REC) == l(m, REC_END)) {
    uint32_t e = l(m, SCRIPT);
    int old = w(m, SCRIPT_COUNT);
    set_w(m, SCRIPT_COUNT, old - 1);
    if (old <= 1) {
      /* subq.w/bhi: the entry has run its count, take the next one (the script loops from $12D48) */
      e += 12;
      if (e >= 0x12f64) e = 0x12d48;
      set_l(m, SCRIPT, e);
      set_w(m, SCRIPT_COUNT, w(m, e + 8));
      set_l(m, REC_STEP, (uint32_t)sw(m, e + 10));
    }
    set_l(m, REC, l(m, e));
    set_l(m, REC_END, l(m, e + 4));
    int32_t step = sl(m, REC_STEP);
    if (step < 0) set_l(m, REC, l(m, REC) + (uint32_t)step);
  }
  uint32_t r = l(m, REC);
  for (int k = 0; k < 4; k++) set_w(m, ANALYSERS + 2 + 4 * k, w(m, r + 2 * k));
}

/**
 * Colours of each screen line for the frame being shown: colour 0, colour 1 (= 8),
 * the other colours. The Timer B chain changes 0/1/8 after `count` lines each time.
 */
static void line_colors(Machine *m, LineColors out[200]) {
  int c0 = m->carry[0], c1 = m->carry[1];
  int next = m->first_count ? m->first_count : 256, h = 0;
  for (int y = 0; y < 200; y++) {
    while (h < 12 && y == next) {
      uint32_t p = HANDLERS + h * HANDLER_SIZE;
      c0 = ste_color(w(m, p + 2));
      c1 = ste_color(w(m, p + 4));
      int count = b(m, p + 0x15);
      h++;
      next = h < 12 && count ? y + count : 999;
    }
    out[y] = (LineColors){c0, c1, m->text};
  }
}

/* ---- $1E552: the text spreader ------------------------------------------------------ */
static void next_letter(Machine *m, uint32_t S);

static void spreader(Machine *m) {
  /* the frame about to be shown is the screen drawn during the previous VBL */
  m->shown = l(m, DRAW_SCREEN);
  uint32_t d0 = l(m, NEXT_SCREEN);
  set_l(m, NEXT_SCREEN, l(m, DRAW_SCREEN));
  set_l(m, DRAW_SCREEN, d0);
  uint32_t buf = d0;
  int bi = screen_index(buf);

  /* erase the bubble (2 words × 16 lines of bitplane 1) */
  uint32_t ob = m->erase_bubble[bi];
  for (int r = 0; r < 16; r++) { put_w(m, ob + r * 160, 0); put_w(m, ob + r * 160 + 8, 0); }
  /* erase the letters in flight */
  for (int i = 0; i < m->erase_bob_count[bi]; i++) {
    uint32_t a = m->erase_bobs[bi][i];
    for (int r = 0; r < 8; r++) {
      if (a & 1) { put_b(m, a + r * 160, 0); put_b(m, a + r * 160 + 7, 0); }
      else put_w(m, a + r * 160, 0);
    }
  }

  /* draw the bubble at its current position (MOVE: it overwrites bitplane 1) */
  int x = w(m, BUBBLE_X) - 4, y = w(m, BUBBLE_Y) - 4;
  int col = (x & 0xfff0) >> 1;
  x &= 15;
  uint32_t ba = buf + 2 + col + y * 160;
  m->erase_bubble[bi] = ba;
  uint32_t s = BUBBLE + x * 64;
  for (int r = 0; r < 16; r++) {
    put_w(m, ba + r * 160, w(m, s));
    put_w(m, ba + r * 160 + 8, w(m, s + 2));
    s += 4;
  }
  /* next position on the path */
  uint32_t p = l(m, BUBBLE_PTR);
  set_w(m, BUBBLE_X, b(m, p) + 0x20);
  set_w(m, BUBBLE_Y, b(m, p + 1));
  p += 2;
  set_l(m, BUBBLE_PTR, p >= PATH_END ? PATH_START : p);

  /* the 33 letter slots: ptr.l, dx.l, x.l, dy.l, y.l, counter.w */
  int bobs = 0;
  for (int k = 0; k < SLOT_COUNT; k++) {
    uint32_t S = SLOTS + k * SLOT_SIZE;
    uint32_t glyph = l(m, S);
    if (!(glyph & 0x80000000u)) {
      int bx = w(m, S + 8), by = w(m, S + 16);
      uint32_t a = buf + 2 + (((bx >> 1) & 0xfff8) + by * 160);
      if (bx & 8) a += 1;
      m->erase_bobs[bi][bobs++] = a;
      uint32_t g = glyph + (bx & 7) * 16;
      if (sw(m, S + 20) <= 0) a += 2; /* landed: bitplane 2, which nothing erases */
      if (!(bx & 8)) {
        for (int r = 0; r < 8; r++) or_w(m, a + r * 160, w(m, g + 2 * r));
      } else {
        for (int r = 0; r < 8; r++) or_b(m, a + r * 160, w(m, g + 0x1600 + 2 * r) & 0xff);
        for (int r = 0; r < 8; r++) or_b(m, a + 7 + r * 160, w(m, g + 2 * r) & 0xff);
      }
    }
    /* move, count down, and give the slot the next letter when it is done */
    if (w(m, S + 20) != 0) {
      set_l(m, S + 8, l(m, S + 8) + l(m, S + 4));
      set_l(m, S + 16, l(m, S + 16) + l(m, S + 12));
    }
    int c = (w(m, S + 20) - 1) & 0xffff;
    set_w(m, S + 20, c);
    if (c != 0xfffe) continue;
    set_w(m, S + 20, 0x40);
    int32_t pause = sl(m, PAUSE) - 1;
    set_l(m, PAUSE, (uint32_t)pause);
    if (pause >= 0) {
      set_l(m, S + 16, 0); set_l(m, S + 12, 0); set_l(m, S + 8, 0); set_l(m, S + 4, 0);
      set_l(m, S, 0xffffffffu);
      continue;
    }
    next_letter(m, S);
  }
  m->erase_bob_count[bi] = bobs;

  /* page clear: 2 lines of bitplane 2 per VBL, from line 198 - n down */
  int n = sw(m, CLEAR_COUNT);
  if (n >= 0) {
    uint32_t a = buf + (0xc6 - n) * 160 + 4;
    for (int i = 0; i < 4; i++, a += 0x50)
      for (int k = 0; k < 10; k++) put_w(m, a + 8 * k, 0);
    set_w(m, CLEAR_COUNT, n - 1);
  }
}

/** Start and step (16.16 fixed point) of a flight from the bubble to the letter's place in 64 VBLs. */
static void flight(Machine *m, uint32_t src, uint32_t dst, uint32_t *start, uint32_t *step) {
  int s = w(m, src);
  int diff = (int16_t)(w(m, dst) - s);
  *start = (uint32_t)s << 16;
  *step = (uint32_t)((int32_t)((uint32_t)diff << 16) >> 6);
}

static void next_letter(Machine *m, uint32_t S) {
  uint32_t a2 = l(m, TEXT_PTR);
  while (b(m, a2) == 0xff) a2 = TEXT_START;
  if (b(m, a2) == 0) {
    /* new line: x, y, signed pitch; a negative pitch writes right to left */
    a2++;
    set_w(m, TEXT_X, b(m, a2++));
    set_w(m, TEXT_Y, b(m, a2++));
    int pitch = (int8_t)b(m, a2++);
    if (pitch < 0) set_w(m, TEXT_X, -w(m, TEXT_X) + 0x142 + pitch);
    set_w(m, PITCH, pitch);
  }
  uint32_t y, dy, x, dx;
  flight(m, BUBBLE_Y, TEXT_Y, &y, &dy);
  flight(m, BUBBLE_X, TEXT_X, &x, &dx);
  set_l(m, S + 16, y); set_l(m, S + 12, dy); set_l(m, S + 8, x); set_l(m, S + 4, dx);
  int ch = b(m, a2++);
  if (ch == 0xbd) {
    /* clear the page over n VBLs, and wait */
    int n = b(m, a2++);
    set_w(m, CLEAR_COUNT, n);
    set_l(m, PAUSE, (uint32_t)(n >> 1));
    set_l(m, S, 0xffffffffu);
  } else if (ch == 9) {
    set_l(m, PAUSE, (uint32_t)(b(m, a2++) * 30));
    set_l(m, S, 0xffffffffu);
  } else {
    int g = ch - 0x30;
    set_l(m, S, g < 0 || g > 0x4f ? 0xffffffffu : (uint32_t)(FONT + g * 128));
    set_w(m, TEXT_X, w(m, TEXT_X) + sw(m, PITCH));
  }
  set_l(m, TEXT_PTR, a2);
}

/* ---- $1E250: voice analysers -------------------------------------------------------- */
static void analysers(Machine *m, const TickInfo *tick) {
  uint32_t buf = l(m, DRAW_SCREEN);
  for (int ch = 0; ch < 4; ch++) {
    uint32_t E = ANALYSERS + 4 * ch;
    if (tick) {
      const TickChannel *c = &tick->channels[3 - ch]; /* the NoisePacker replay keeps its channels in reverse order */
      /* a note, an instrument or a set-volume on the row just read; or a volume
         slide (with tone portamento or vibrato) on the other ticks: back to the volume */
      bool trigger = tick->row_read ? c->note_or_ins || c->cmd == 0xc : c->cmd == 5 || c->cmd == 6 || c->cmd == 0xa;
      if (trigger) set_w(m, E, c->base_volume);
    }
    int level = w(m, E);
    if (level) { level -= 2; if ((int16_t)level < 2) level = 2; }
    set_w(m, E, level);
    int d1 = (level - 1) & 0xffff;
    int y = b(m, E + 2), h = b(m, E + 3);
    if (h & 0x80) continue;
    const uint32_t *pat = PATTERNS[((d1 * 2) & 0x1c) >> 2];
    uint32_t p2 = pat[0], p3 = pat[1], p5 = pat[2], p6 = pat[3];
    int mask = d1 & 0x8030;
    int variant = mask >> 4;
    if (mask & 0x8000) { p2 = 0; p6 = 0; variant = 0; }
    const uint32_t F = 0xffffffffu;
    const uint32_t rows[4][8] = {
      {0, 0, 0, p2, p6, 0, 0, 0},
      {0, 0, p2, p3, p5, p6, 0, 0},
      {0, p2, p3, F, F, p5, p6, 0},
      {p2, p3, F, F, F, F, p5, p6},
    };
    const uint32_t *row = rows[variant];
    for (int ln = 0; ln <= h; ln++) {
      uint32_t a = buf + 0x16 + (y + ln) * 160;
      for (int k = 0; k < 8; k++) {
        put_w(m, a + 16 * k, (int)(row[k] >> 16));
        put_w(m, a + 16 * k + 2, (int)(row[k] & 0xffff));
      }
    }
  }
}

/** One VBL of the text pages. `tick` is the replay state the analysers read (previous VBL's). */
static void machine_vbl(Machine *m, const TickInfo *tick) {
  LineColors lines[200];
  line_colors(m, lines);
  m->carry[0] = lines[199].c0;
  m->carry[1] = lines[199].c1;
  rasters(m);
  spreader(m);
  analysers(m, tick);
}

/** Pixel indices of the screen being shown (4 bitplanes, ST interleaved). */
static void pixels_into(Machine *m, Display *d) {
  const uint8_t *s = m->screens[screen_index(m->shown)];
  for (int y = 0; y < 200; y++) {
    uint8_t *o = d->pixels + display_at(0, y);
    for (int col = 0; col < 20; col++) {
      const uint8_t *q = s + y * 160 + col * 8;
      int p0 = (q[0] << 8) | q[1], p1 = (q[2] << 8) | q[3], p2 = (q[4] << 8) | q[5], p3 = (q[6] << 8) | q[7];
      for (int bit = 0; bit < 16; bit++) {
        int mk = 0x8000 >> bit;
        o[col * 16 + bit] = (uint8_t)((p0 & mk ? 1 : 0) | (p1 & mk ? 2 : 0) | (p2 & mk ? 4 : 0) | (p3 & mk ? 8 : 0));
      }
    }
  }
}

/** Fade steps, as the main loop at $1298C does them: ±38 bytes of colour table every 3 VBLs. */
enum { FADE_STEP = 38, FADE_STEPS = 7 };

typedef struct {
  Scene base;
  Machine m;
  int t;
  int leaving;
  TickSource tick;
  void *tick_ctx;
} TextPage;

static void textpage_vbl(Scene *s) {
  TextPage *tp = (TextPage *)s;
  tp->t++;
  machine_vbl(&tp->m, tp->tick ? tp->tick(tp->tick_ctx) : music_current(s->app->music));
  /* fade in after the VBL is installed, out after SPACE */
  if (tp->leaving < 0 && tp->t % 3 == 0 && tp->t <= 3 * FADE_STEPS) set_l(&tp->m, COLTAB, l(&tp->m, COLTAB) + FADE_STEP);
  if (tp->leaving >= 0) {
    int k = tp->t - tp->leaving;
    if (k > 0 && k % 3 == 0 && k <= 3 * FADE_STEPS) set_l(&tp->m, COLTAB, l(&tp->m, COLTAB) - FADE_STEP);
    if (k >= 3 * FADE_STEPS + 3) app_go(s->app, menu_new(s->app));
  }
}

static void textpage_draw(Scene *s, Display *d) {
  Machine *m = &((TextPage *)s)->m;
  d->rows = 200;
  d->fade = 0;
  d->fade_keeps_lsb = false;
  display_clear(d);
  pixels_into(m, d);
  LineColors lines[200];
  line_colors(m, lines);
  for (int y = -29; y < 247; y++) {
    LineColors c = y < 0 ? (LineColors){m->carry[0], m->carry[1], lines[0].text} : lines[y < 199 ? y : 199];
    for (int i = 0; i < 16; i++) display_set_color(d, y, i, c.text);
    display_set_color(d, y, 0, c.c0);
    display_set_color(d, y, 1, c.c1);
    display_set_color(d, y, 8, c.c1);
  }
}

static void textpage_key(Scene *s, Key k) {
  TextPage *tp = (TextPage *)s;
  if (k == KEY_SPACE && tp->leaving < 0 && tp->t > 3 * FADE_STEPS) tp->leaving = tp->t;
}

static void textpage_pointer(Scene *s, double x, double y, bool click) {
  (void)x; (void)y;
  if (click) textpage_key(s, KEY_SPACE);
}

static void textpage_free(Scene *s) {
  TextPage *tp = (TextPage *)s;
  if (s->app->text_page == s) s->app->text_page = NULL;
  free(tp->m.ram);
  free(tp);
}

Scene *textpage_new(App *app, TickSource tick, void *ctx) {
  TextPage *tp = calloc(1, sizeof *tp);
  tp->base = (Scene){app, textpage_vbl, textpage_draw, textpage_key, textpage_pointer, textpage_free};
  tp->leaving = -1;
  tp->tick = tick;
  tp->tick_ctx = ctx;
  Machine *m = &tp->m;
  m->ram_size = TEXTPAGE_RAM_SIZE;
  m->ram = malloc(m->ram_size);
  memcpy(m->ram, TEXTPAGE_RAM, m->ram_size);
  m->erase_bubble[0] = SCREEN_A;
  m->erase_bubble[1] = SCREEN_B;
  m->shown = SCREEN_A;
  app->text_page = &tp->base;
  return &tp->base;
}

Scene *textpage_resume(Scene *s) {
  TextPage *tp = (TextPage *)s;
  tp->t = 0;
  tp->leaving = -1;
  return s;
}
