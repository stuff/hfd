/*
 * The intro, from the loading screen (the ear) to the last logo. Port of
 * ports/web/src/scenes/intro.ts. Its clock counts VBLs like the reference recording does
 * (the ear appears at VBL 546, after EmuTOS), so every time below can be checked against
 * rip/out/rec1/video.mkv.
 */
#include <stdlib.h>
#include "scene.h"

enum {
  EAR = 546,
  LOADING_TO_DECRUNCHING = 973,
  EAR_OUT = 2801,
  MUSIC = 2836,
  AURA = 3169, AURA_OUT = 3763,
  HFD_RULES = 3798, HFD_WORDS = 3972, HFD_OUT = 4560,
  AEON = 4704, AEON_OUT = 5256,
  INTRO_END = 5295, /* the text pages' first VBL */
};

static const uint16_t BLACK[16] = {0};

static int imax(int a, int b) { return a > b ? a : b; }
static int imin(int a, int b) { return a < b ? a : b; }

/** The short version skips whole pulse cycles of LOADING and DECRUNCHING (56 VBLs each). */
static int short_skip(int v) {
  if (v == LOADING_TO_DECRUNCHING - 280) return 280; /* LOADING stays 147 VBLs instead of 427 */
  if (v == EAR_OUT - 1680) return 1680;              /* DECRUNCHING stays 148 VBLs instead of 1828 */
  return 0;
}

/** Fade in from level 6, one level every 2 VBLs; 7 (black) before `start`. */
static int fade_in2(int v, int start) { return v < start ? 7 : imax(0, 6 - ((v - start) >> 1)); }
/** Fade out to black, one level every 2 VBLs from `start` (level 1 at `start`). */
static int fade_out2(int v, int start) { return v < start ? 0 : imin(7, 1 + ((v - start) >> 1)); }

/**
 * The colour LOADING/DECRUNCHING pulses with: a 56-VBL triangle from 000 to eee and back,
 * one 3-bit level every 4 VBLs, red then green then blue one VBL apart. k counts from 1.
 */
static int pulse(int k) {
  int m = (k - 1) % 56, n = m >> 2, sub = m & 3;
  bool up = n < 7;
  int level = up ? n : 7 - (n - 7);
  int d = up ? 1 : -1;
  int r = level + (sub >= 0 ? d : 0), g = level + (sub >= 1 ? d : 0), b = level + (sub >= 2 ? d : 0);
  return ((r * 2) << 8) | ((g * 2) << 4) | (b * 2);
}

/** The ear screen's loader fade, keeping the STE low bit (measured: it starts on odd VBLs). */
static int ear_fade(int v) {
  if (v < EAR_OUT) return v <= EAR ? 6 : imax(0, 5 - ((v - EAR - 1) >> 1));
  return v >= EAR_OUT + 10 ? 7 : 1 + ((v - EAR_OUT) >> 1);
}

/** CODE / AND / DESIGN / AEON each flash in with these levels, one per VBL. */
static const int FLASH[17] = {0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0};
static int flash(int v, int start) { return v < start ? 7 : FLASH[imin(16, v - start)]; }

typedef struct {
  Scene base;
  /** The reference VBL being shown. */
  int v;
} Intro;

static void ear(Display *d, int v) {
  display_blit(d, &IMG_EAR, 0, 0, 320, 200, 0, 0, MAP_ID, false);
  display_set_picture_colors(d, &META_EAR);
  /* LOADING (colour 15) pulses, then DECRUNCHING (colour 14) takes over the same pulse.
     The pulse is written after the fade, so it doesn't fade (nor does colour 13 there);
     the hidden word has the background colour, which does. */
  int j = ear_fade(v);
  int c = v > EAR ? pulse(v - EAR) : 0, bg = fade_color(0x004, j, true);
  bool decrunching = v >= LOADING_TO_DECRUNCHING;
  display_set_lines(d, 189, 200, 14, decrunching ? c : bg);
  display_set_lines(d, 189, 200, 15, decrunching ? bg : c);
  display_set_lines(d, 189, 200, 0, bg);
  display_no_fade(d, 189, 200);
  d->fade = j;
  d->fade_keeps_lsb = true;
}

static void aura(Display *d, int v) {
  static const int bands[3][2] = {{53, 73}, {84, 116}, {127, 147}};
  display_blit(d, &IMG_AURA, 0, 0, 320, 200, 0, 0, MAP_ID, false);
  display_set_picture_colors(d, &META_AURA);
  for (int w = 0; w < 3; w++)
    display_fade_lines(d, bands[w][0], bands[w][1], imax(fade_in2(v, AURA + 24 * w), fade_out2(v, AURA_OUT)), false);
}

static void hfd(Display *d, int v) {
  static const int words[3][2] = {{37, 69}, {84, 116}, {131, 163}};
  const IndexedImage *img = &IMG_HFD; /* 416 wide: x = 0 is the left edge of the frame */
  display_set_picture_colors(d, &META_HFD);
  int out = fade_out2(v, HFD_OUT);
  /* The two rules slide in from the sides, 2 pixels per VBL, fading in. */
  int slide = imin(142, v - HFD_RULES);
  int top = 350 - 2 * slide, bottom = 65 + 2 * slide; /* left end of the top rule, right end of the bottom one */
  display_wide(d, 14, 22);
  display_wide(d, 178, 186);
  display_blit(d, img, 66, 14, 416 - 66, 8, top - 48, 14, MAP_ID, false);
  display_blit(d, img, 0, 178, 350, 8, bottom - 349 - 48, 178, MAP_ID, false);
  int rules = imax(fade_in2(v, HFD_RULES), out);
  display_fade_lines(d, 14, 22, rules, false);
  display_fade_lines(d, 178, 186, rules, false);
  /* Then HIGH, FIDELITY, DREAMS, 6 VBLs apart. */
  for (int w = 0; w < 3; w++) {
    int y0 = words[w][0], y1 = words[w][1];
    display_blit(d, img, 0, y0, 416, y1 - y0, -48, y0, MAP_ID, false);
    display_fade_lines(d, y0, y1, imax(fade_in2(v, HFD_WORDS + 6 * w), out), false);
  }
}

static void aeon(Display *d, int v) {
  static const int parts[4][2] = {{7, 39}, {42, 62}, {65, 97}, {128, 192}};
  display_blit(d, &IMG_AEON, 0, 0, 320, 200, 0, 0, MAP_ID, false);
  display_set_picture_colors(d, &META_AEON);
  int out = fade_out2(v, AEON_OUT);
  for (int w = 0; w < 4; w++)
    display_fade_lines(d, parts[w][0], parts[w][1], imax(flash(v, AEON + 12 * w), out), false);
}

static void intro_vbl(Scene *s) {
  Intro *in = (Intro *)s;
  in->v++;
  if (!s->app->authentic) in->v += short_skip(in->v);
  if (in->v == MUSIC) music_play(s->app->music, ENIGMA);
  if (in->v >= INTRO_END) app_go(s->app, textpage_new(s->app, NULL, NULL));
}

static void intro_draw(Scene *s, Display *d) {
  int v = ((Intro *)s)->v;
  d->rows = 200;
  d->fade = 0;
  d->fade_keeps_lsb = false;
  display_clear(d);
  display_set_palette(d, BLACK);
  if (v >= EAR && v < EAR_OUT + 10) ear(d, v);
  else if (v >= AURA && v < AURA_OUT + 14) aura(d, v);
  else if (v >= HFD_RULES && v < HFD_OUT + 14) hfd(d, v);
  else if (v >= AEON && v < AEON_OUT + 14) aeon(d, v);
}

static void intro_free(Scene *s) { free(s); }

Scene *intro_new(App *app) {
  Intro *in = calloc(1, sizeof *in);
  in->base = (Scene){app, intro_vbl, intro_draw, NULL, NULL, intro_free};
  in->v = EAR;
  return &in->base;
}
