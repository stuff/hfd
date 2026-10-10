/*
 * After RETURN in the menu: the music fades out, then "LOADING <song>", then
 * "DECRUNCHING <song>", then back to the menu with the new song playing.
 * Port of ports/web/src/scenes/loading.ts. Times in VBLs from the key press, as
 * measured in the reference recording.
 */
#include <stdlib.h>
#include "scene.h"

enum {
  WORD_Y = 78,
  NAME_Y = 106,
  MUSIC_FADE = 87, /* the music fades out over 87 VBLs... */
  MENU_OUT = 87,   /* ...then the menu fades out */
  SCREEN_IN = 108, /* the loading screen fades in */
  WORD_SWAP = 21,  /* LOADING fades out, DECRUNCHING fades in, 21 VBLs each */
  MENU_IN = 32,    /* from the start of the last fade-out to the menu fading in with the new song */
  AUTHENTIC_LOADING = 288, AUTHENTIC_DECRUNCHING = 852, /* the real disk took that long */
  MIN_LOADING = 60, /* VBLs on screen in the short version */
  MIN_DECRUNCHING = 60,
  NEVER = 1 << 28,
};

static const int ORANGE[5] = {0x840, 0xa62, 0xc84, 0xea6, 0xfff};
static const uint16_t PALETTE[16] = {0, 0x840, 0xa62, 0xc84, 0xea6, 0xfff, 0, 0,
  0, 0x626, 0x848, 0xa6a, 0xc8c, 0xfff, 0, 0}; /* orange in 1-5, pink in 9-13 */

static int imax(int a, int b) { return a > b ? a : b; }

typedef struct {
  Scene base;
  int song;
  int t;
  int loading_end;
  int decrunching_end;
} Loading;

static void loading_vbl(Scene *s) {
  Loading *l = (Loading *)s;
  App *app = s->app;
  int t = ++l->t;
  /* The modules are in the executable: always loaded. */
  if (l->loading_end == NEVER && t >= SCREEN_IN + 21) {
    l->loading_end = imax(t, app->authentic ? AUTHENTIC_LOADING : SCREEN_IN + 21 + MIN_LOADING);
    l->decrunching_end = app->authentic ? imax(AUTHENTIC_DECRUNCHING, l->loading_end + 2 * WORD_SWAP)
                                        : l->loading_end + 2 * WORD_SWAP + MIN_DECRUNCHING;
  }
  if (t == MUSIC_FADE) music_stop(app->music);
  if (t >= l->decrunching_end + MENU_IN) {
    app->playing = l->song;
    music_play(app->music, l->song);
    app_go(app, menu_new(app));
  }
}

static void loading_draw(Scene *s, Display *d) {
  Loading *l = (Loading *)s;
  int t = l->t;
  if (t < SCREEN_IN) {
    menu_draw(d, s->app->cursor, s->app->playing, fade_at(t - MENU_OUT, FADE_OUT, 8, 0));
    return;
  }
  d->rows = 200;
  d->fade = t >= l->decrunching_end ? fade_at(t - l->decrunching_end, FADE_OUT, 8, FADE_OUT[0])
                                    : fade_at(t - SCREEN_IN, FADE_IN, 8, FADE_IN[0]);
  display_clear(d);
  display_set_palette(d, PALETTE);
  menu_rule(d, 67);
  menu_rule(d, 135);
  menu_draw_song_word(d, l->song, NAME_Y, true);

  /* LOADING, then a fade on its lines only, then DECRUNCHING. */
  int since = t - l->loading_end;
  bool decrunching = since >= WORD_SWAP;
  const IndexedImage *w = decrunching ? &IMG_WORD_DECRUNCHING : &IMG_WORD_LOADING;
  display_blit(d, w, 0, 0, w->width, w->height, (320 - w->width) >> 1, WORD_Y, MAP_LOW3, false);
  if (since >= 0) {
    int j = decrunching ? fade_at(since - WORD_SWAP, FADE_IN, 8, FADE_IN[0]) : fade_at(since, FADE_OUT, 8, FADE_OUT[0]);
    for (int i = 0; i < 5; i++) display_set_lines(d, WORD_Y, WORD_Y + 20, i + 1, fade_color(ORANGE[i], j, false));
  }
}

static void loading_free(Scene *s) { free(s); }

Scene *loading_new(App *app, int song) {
  Loading *l = calloc(1, sizeof *l);
  l->base = (Scene){app, loading_vbl, loading_draw, NULL, NULL, loading_free};
  l->song = song;
  l->loading_end = l->decrunching_end = NEVER;
  music_fade_out(app->music, MUSIC_FADE);
  return &l->base;
}
