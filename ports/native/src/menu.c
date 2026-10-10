/* The SELECT SONG menu. Port of ports/web/src/scenes/menu.ts. */
#include <math.h>
#include <stdlib.h>
#include "scene.h"

#define ROWS 218 /* the menu opens the lower border */

/* Colours measured in the reference recording (0xRGB, 4 bits per component). */
static const uint16_t BASE[16] = {0x000, 0x004, 0x226, 0x448, 0x66a, 0xccc, 0x000, 0x000,
  0x000, 0x626, 0x848, 0xa6a, 0xc8c, 0xfff, 0x000, 0x000};
static const int ORANGE[5] = {0x840, 0xa62, 0xc84, 0xea6, 0xfff}; /* colours 1-5 of SELECT SONG / LOADING / DECRUNCHING */
/** Colours 1-5 of the entry under the cursor. */
static const int CURSOR[5] = {0x226, 0x448, 0x66a, 0x88c, 0xfff};
static const int CURSOR_BAR[6] = {0xfff, 0x226, 0x448, 0x66a, 0x88c, 0xfff}; /* colour 0 on lines 7-12 of that entry */
/**
 * The 3-line horizontal rules (colour 0), per fade level. They don't fade like the
 * palette does (aaa and 666 hold on longer), so here is the measured table.
 */
static const int RULE_FADE[8][3] = {
  {0xfff, 0xaaa, 0x666}, {0xccc, 0x888, 0x666}, {0xaaa, 0x888, 0x444}, {0x888, 0x666, 0x444},
  {0x666, 0x444, 0x222}, {0x444, 0x444, 0x222}, {0x222, 0x222, 0x000}, {0x000, 0x000, 0x000},
};

/** Draws a rule at screen line y. Call after setting d->fade. */
void menu_rule(Display *d, int y) {
  const int *c = RULE_FADE[d->fade < 7 ? d->fade : 7];
  for (int i = 0; i < 3; i++) {
    display_set_color(d, y + i, 0, c[i]);
    d->no_fade[y + i + SCREEN_Y] = 1;
  }
}

/** Copies the word of menu entry k to screen line y; `playing` draws it pink (bitplane 3). */
void menu_draw_song_word(Display *d, int k, int y, bool playing) {
  display_blit(d, &IMG_MENU, 0, ENTRY_Y + k * ENTRY_STEP, SCREEN_W, ENTRY_H, 0, y, playing ? MAP_PINK : MAP_LOW3, false);
}

void menu_draw(Display *d, int cursor, int playing, int fade) {
  d->rows = ROWS;
  d->fade = fade;
  display_clear(d);
  display_blit(d, &IMG_MENU, 0, 0, SCREEN_W, ROWS, 0, 0, MAP_LOW3, true);
  menu_draw_song_word(d, playing, ENTRY_Y + playing * ENTRY_STEP, true);

  display_set_palette(d, BASE);
  /* The entries' white outline (ccc) is fff one fade step down: during a fade it
     follows max(fade, 1), not fade + 1. */
  if (fade > 0) display_set_lines(d, 0, ROWS, 5, 0xfff);
  menu_rule(d, 2);
  menu_rule(d, 30);
  for (int i = 0; i < 5; i++) display_set_lines(d, 7, 27, i + 1, ORANGE[i]);
  int y = ENTRY_Y + cursor * ENTRY_STEP;
  if (cursor != playing)
    for (int i = 0; i < 5; i++) display_set_lines(d, y, y + ENTRY_H, i + 1, CURSOR[i]);
  for (int i = 0; i < 6; i++) {
    display_set_color(d, y + 7 + i, 0, CURSOR_BAR[i]);
    display_set_color(d, y + 7 + i, 7, 0xfff);
    display_set_color(d, y + 7 + i, 15, 0xfff);
  }
}

static int entry_at(double y) {
  int k = (int)floor((y - ENTRY_Y + 1) / ENTRY_STEP);
  return k >= 0 && k < SONG_COUNT ? k : -1;
}

typedef struct {
  Scene base;
  int t;
  /** VBL at which SPACE was pressed (back to the text pages), -1 if not. */
  int leaving;
} Menu;

static void menu_vbl(Scene *s) {
  Menu *m = (Menu *)s;
  m->t++;
  if (m->leaving >= 0 && m->t - m->leaving >= 24) {
    App *app = s->app;
    app_go(app, textpage_resume(app->text_page ? app->text_page : textpage_new(app, NULL, NULL)));
  }
}

static void menu_scene_draw(Scene *s, Display *d) {
  Menu *m = (Menu *)s;
  int fade = m->leaving >= 0 ? fade_at(m->t - m->leaving, FADE_OUT, 8, 0) : fade_at(m->t, FADE_IN, 8, 8);
  menu_draw(d, s->app->cursor, s->app->playing, fade);
}

static bool ready(const Menu *m) { return m->leaving < 0 && m->t >= 8 * 3; }

static void select_song(Scene *s) {
  /* even on the song being played: the demo loads it again */
  app_go(s->app, loading_new(s->app, s->app->cursor));
}

static void menu_key(Scene *s, Key k) {
  Menu *m = (Menu *)s;
  App *app = s->app;
  if (!ready(m)) return;
  if (k == KEY_UP) app->cursor = app->cursor > 0 ? app->cursor - 1 : 0;
  else if (k == KEY_DOWN) app->cursor = app->cursor < SONG_COUNT - 1 ? app->cursor + 1 : SONG_COUNT - 1;
  else if (k == KEY_ENTER) select_song(s);
  else if (k == KEY_SPACE) m->leaving = m->t; /* back to the text pages, as in the demo */
}

static void menu_pointer(Scene *s, double x, double y, bool click) {
  (void)x;
  if (!ready((Menu *)s)) return;
  /* Not in the demo: for mice and fingers, a click on SELECT SONG does what SPACE does. */
  if (click && y < ENTRY_Y - 4) { menu_key(s, KEY_SPACE); return; }
  int k = entry_at(y);
  if (k < 0) return;
  s->app->cursor = k;
  if (click) select_song(s);
}

static void menu_free(Scene *s) { free(s); }

Scene *menu_new(App *app) {
  Menu *m = calloc(1, sizeof *m);
  m->base = (Scene){app, menu_vbl, menu_scene_draw, menu_key, menu_pointer, menu_free};
  m->leaving = -1;
  return &m->base;
}
