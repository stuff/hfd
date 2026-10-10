/* Scenes: one per screen. Everything advances by VBL (1/50 s). */
#ifndef HFD_SCENE_H
#define HFD_SCENE_H

#include <stdbool.h>
#include "display.h"
#include "mod.h"
#include "music.h"

typedef enum { KEY_UP, KEY_DOWN, KEY_ENTER, KEY_SPACE } Key;

typedef struct Scene Scene;
typedef struct App App;

struct App {
  Music *music;
  /** Song being played (menu index). */
  int playing;
  /** Menu cursor, kept between visits. */
  int cursor;
  /** Original disk loading/decrunching durations instead of short ones. */
  bool authentic;
  /** The text pages, kept while in the menu: SPACE in the menu resumes them where they were. */
  Scene *text_page;
  Scene *scene;
  /** Scenes left by app_go, freed by app_reap once nothing runs them any more. */
  Scene *dead[8];
  int dead_count;
};

struct Scene {
  App *app;
  /** Advances one VBL (1/50 s). */
  void (*vbl)(Scene *s);
  /** Draws the current state into the display. */
  void (*draw)(Scene *s, Display *d);
  void (*key)(Scene *s, Key k);
  /** Pointer position in screen coordinates (0,0 = top-left of the 320-wide screen). */
  void (*pointer)(Scene *s, double x, double y, bool click);
  void (*free)(Scene *s);
};

void app_go(App *app, Scene *next);
void app_reap(App *app);

/**
 * Fade level for a palette fade as the demo does it: one level every 3 VBLs.
 * `steps` lists the levels (see fade_color); before the first step the level is
 * `before`, after the last one it stays on the last.
 */
int fade_at(int t, const int *steps, int count, int before);
/** From black (level 7) to full colour, 21 VBLs. */
extern const int FADE_IN[8];
/** From full colour to black, 21 VBLs. */
extern const int FADE_OUT[8];

/* The scenes. */
typedef const TickInfo *(*TickSource)(void *ctx);
Scene *intro_new(App *app);
Scene *menu_new(App *app);
Scene *loading_new(App *app, int song);
/** `tick` gives the replay state the analysers read; NULL: the music being heard. */
Scene *textpage_new(App *app, TickSource tick, void *ctx);
/** Back from the menu: the spreader carries on where it stopped, fading in again. */
Scene *textpage_resume(Scene *s);

/* Shared by the menu and the loading screen. */
#define ENTRY_Y 37
#define ENTRY_STEP 23
#define ENTRY_H 20
void menu_rule(Display *d, int y);
void menu_draw_song_word(Display *d, int k, int y, bool playing);
void menu_draw(Display *d, int cursor, int playing, int fade);

#endif
