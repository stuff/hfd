/*
 * High Fidelity Dreams, native port: window, input and the 50.05 Hz VBL loop
 * (port of ports/web/src/main.ts), plus two headless modes used to verify the port
 * against the web one (ports/web/scripts/render.ts and render-audio.ts).
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "display.h"
#include "mod.h"
#include "music.h"
#include "pacing.h"
#include "scene.h"

static Display display;

static int usage(void) {
  fprintf(stderr,
          "usage: hfd [--authentic] [--menu]\n"
          "       hfd --render <intro|menu|text> <out.rgb> <vbls> [vbl:key ...] [--enigma-ticks n]\n"
          "       hfd --render-audio <module.mod> <out.wav> <seconds> [rate]\n");
  return 2;
}

/* ---- headless rendering ----------------------------------------------------------- */

/** For the text pages: Enigma replayed one tick per VBL (1 sample per tick). */
static TickInfo last_tick;
static bool has_last_tick;
static void remember_tick(void *ctx, const TickInfo *t) { (void)ctx; last_tick = *t; has_last_tick = true; }
static const TickInfo *get_last_tick(void *ctx) { (void)ctx; return has_last_tick ? &last_tick : NULL; }

static bool key_named(const char *name, Key *k) {
  if (!strcmp(name, "ArrowUp")) *k = KEY_UP;
  else if (!strcmp(name, "ArrowDown")) *k = KEY_DOWN;
  else if (!strcmp(name, "Enter") || !strcmp(name, "NumpadEnter")) *k = KEY_ENTER;
  else if (!strcmp(name, "Space")) *k = KEY_SPACE;
  else return false;
  return true;
}

/**
 * Runs a scene without a window and writes raw RGB24 frames (416×276), like
 * ports/web/scripts/render.ts: menu: VBL 0 is the menu's first VBL (reference VBL 9024);
 * intro: VBL 0 is the ear screen's first VBL (reference VBL 546); text: VBL 0 is the
 * text pages' first VBL, after `enigma_ticks` ticks of Enigma.
 */
static int render(int argc, char **argv) {
  if (argc < 3) return usage();
  const char *which = argv[0], *out_path = argv[1];
  int vbls = atoi(argv[2]);
  int key_vbl[64];
  Key keys[64];
  int nkeys = 0;
  long enigma_ticks = 0;
  const char *env = SDL_getenv("ENIGMA_TICKS");
  if (env) enigma_ticks = atol(env);
  for (int i = 3; i < argc; i++) {
    if (!strcmp(argv[i], "--enigma-ticks") && i + 1 < argc) { enigma_ticks = atol(argv[++i]); continue; }
    const char *colon = strchr(argv[i], ':');
    if (!colon || nkeys == 64 || !key_named(colon + 1, &keys[nkeys])) return usage();
    key_vbl[nkeys++] = atoi(argv[i]);
  }

  App app = {.music = NULL, .playing = ENIGMA, .cursor = ENIGMA, .authentic = true};
  Module enigma;
  Replayer replay;
  float one[1];
  bool text = !strcmp(which, "text");
  if (text) {
    mod_parse(&enigma, SONGS[ENIGMA].data, SONGS[ENIGMA].size);
    replayer_init(&replay, &enigma, ST_VBL_HZ, ST_VBL_HZ, remember_tick, NULL);
    for (long i = 0; i < enigma_ticks; i++) replayer_render(&replay, one, one, 0, 1);
  }
  if (!strcmp(which, "intro")) app.scene = intro_new(&app);
  else if (text) app.scene = textpage_new(&app, get_last_tick, NULL);
  else if (!strcmp(which, "menu")) app.scene = menu_new(&app);
  else return usage();

  FILE *f = fopen(out_path, "wb");
  if (!f) { perror(out_path); return 1; }
  display_init(&display);
  static uint8_t rgb[FRAME_W * FRAME_H * 3];
  for (int v = 0; v < vbls; v++) {
    for (int k = 0; k < nkeys; k++)
      if (key_vbl[k] == v && app.scene->key) app.scene->key(app.scene, keys[k]);
    app.scene->draw(app.scene, &display);
    display_render(&display);
    for (int i = 0; i < FRAME_W * FRAME_H; i++) {
      uint32_t p = display.out[i];
      rgb[i * 3] = (uint8_t)(p >> 16);
      rgb[i * 3 + 1] = (uint8_t)(p >> 8);
      rgb[i * 3 + 2] = (uint8_t)p;
    }
    fwrite(rgb, 1, sizeof rgb, f);
    app.scene->vbl(app.scene);
    app_reap(&app);
    if (text) replayer_render(&replay, one, one, 0, 1);
  }
  fclose(f);
  printf("%d frames -> %s\n", vbls, out_path);
  return 0;
}

static void put16(uint8_t *p, int v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, (int)(v & 0xffff)); put16(p + 2, (int)(v >> 16)); }

/** Renders a module into a 16-bit stereo WAV, like ports/web/scripts/render-audio.ts. */
static int render_audio(int argc, char **argv) {
  if (argc < 3) return usage();
  size_t size;
  void *data = SDL_LoadFile(argv[0], &size);
  if (!data) { fprintf(stderr, "%s: %s\n", argv[0], SDL_GetError()); return 1; }
  int rate = argc > 3 ? atoi(argv[3]) : 44100;
  const char *tick_hz = SDL_getenv("TICK_HZ");
  Module mod;
  if (!mod_parse(&mod, data, size)) { fprintf(stderr, "%s: not a module\n", argv[0]); return 1; }
  int n = (int)SDL_round(atof(argv[2]) * rate);
  float *l = malloc(n * sizeof *l), *r = malloc(n * sizeof *r);
  uint8_t *wav = malloc(44 + (size_t)n * 4);
  Replayer replay;
  replayer_init(&replay, &mod, rate, tick_hz ? atof(tick_hz) : ST_VBL_HZ, NULL, NULL);
  replayer_render(&replay, l, r, 0, n);

  memcpy(wav, "RIFF", 4); put32(wav + 4, 36 + n * 4); memcpy(wav + 8, "WAVEfmt ", 8);
  put32(wav + 16, 16); put16(wav + 20, 1); put16(wav + 22, 2);
  put32(wav + 24, rate); put32(wav + 28, rate * 4); put16(wav + 32, 4); put16(wav + 34, 16);
  memcpy(wav + 36, "data", 4); put32(wav + 40, n * 4);
  for (int i = 0; i < n; i++) {
    for (int c = 0; c < 2; c++) {
      double v = SDL_floor((c ? r[i] : l[i]) * 32767.0 + 0.5); /* Math.round */
      put16(wav + 44 + i * 4 + c * 2, (int)(v < -32768 ? -32768 : v > 32767 ? 32767 : v));
    }
  }
  FILE *f = fopen(argv[1], "wb");
  if (!f) { perror(argv[1]); return 1; }
  fwrite(wav, 1, 44 + (size_t)n * 4, f);
  fclose(f);
  printf("%s: %ss -> %s\n", argv[0], argv[2], argv[1]);
  return 0;
}

/* ---- the demo ---------------------------------------------------------------------- */

static App app;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;

/** A double click toggles full screen, so a single click waits for the double-click delay first. */
#define DOUBLE_CLICK_NS (250 * SDL_NS_PER_MS)
static bool click_pending;
static Uint64 click_at;
static double click_x, click_y;
/** In full screen, the mouse pointer hides after 2 s without moving. */
#define CURSOR_HIDE_NS (2000 * SDL_NS_PER_MS)
static Uint64 mouse_moved;

static bool fullscreen(void) { return SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN; }
static void toggle_fullscreen(void) { SDL_SetWindowFullscreen(window, !fullscreen()); }

/** Window position → screen coordinates (0,0 = top-left of the 320-wide screen). */
static void to_screen(float wx, float wy, double *x, double *y) {
  float fx, fy;
  SDL_RenderCoordinatesFromWindow(renderer, wx, wy, &fx, &fy);
  *x = fx - SCREEN_X;
  *y = fy - SCREEN_Y;
}

static void scene_key(Key k) {
  if (app.scene->key) app.scene->key(app.scene, k);
}

static bool handle(const SDL_Event *e) {
  switch (e->type) {
    case SDL_EVENT_QUIT: return false;
    case SDL_EVENT_KEY_DOWN: {
      SDL_Scancode sc = e->key.scancode;
      if (e->key.repeat && sc != SDL_SCANCODE_UP && sc != SDL_SCANCODE_DOWN) break;
      if (sc == SDL_SCANCODE_ESCAPE) return false;
      if (sc == SDL_SCANCODE_F || ((sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) && (e->key.mod & SDL_KMOD_ALT)))
        toggle_fullscreen();
      else if (sc == SDL_SCANCODE_UP) scene_key(KEY_UP);
      else if (sc == SDL_SCANCODE_DOWN) scene_key(KEY_DOWN);
      else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) scene_key(KEY_ENTER);
      else if (sc == SDL_SCANCODE_SPACE) scene_key(KEY_SPACE);
      break;
    }
    case SDL_EVENT_MOUSE_MOTION: {
      mouse_moved = SDL_GetTicksNS();
      SDL_ShowCursor();
      double x, y;
      to_screen(e->motion.x, e->motion.y, &x, &y);
      if (app.scene->pointer) app.scene->pointer(app.scene, x, y, false);
      break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (e->button.button != SDL_BUTTON_LEFT) break;
      if (click_pending) {
        click_pending = false;
        toggle_fullscreen();
        break;
      }
      click_pending = true;
      click_at = SDL_GetTicksNS();
      to_screen(e->button.x, e->button.y, &click_x, &click_y);
      break;
  }
  return true;
}

/** Largest whole scale of the 416×276 frame that fits comfortably on the display. */
static int window_scale(void) {
  SDL_Rect r;
  int scale = 3;
  if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &r)) {
    scale = SDL_min((r.w * 9 / 10) / FRAME_W, (r.h * 9 / 10) / FRAME_H);
    if (scale < 1) scale = 1;
  }
  return scale;
}

static int run(bool authentic, bool menu) {
  if (!SDL_Init(SDL_INIT_VIDEO)) { SDL_Log("SDL_Init: %s", SDL_GetError()); return 1; }
  SDL_SetAppMetadata("High Fidelity Dreams", NULL, "org.aura.hifidreams");
  int scale = window_scale();
  if (!SDL_CreateWindowAndRenderer("High Fidelity Dreams", FRAME_W * scale, FRAME_H * scale, SDL_WINDOW_RESIZABLE,
                                   &window, &renderer)) {
    SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
    return 1;
  }
  SDL_SetWindowMinimumSize(window, FRAME_W, FRAME_H);
  /* Windows takes the icon from the executable's resources. On macOS, the Finder takes
     it from the bundle's .icns, but the Dock and Cmd-Tab show the running app's icon,
     which AppKit didn't pick up from it: it is set from the bundle's icon.png. */
#if defined(SDL_PLATFORM_MACOS)
  char icon_path[4096];
  SDL_snprintf(icon_path, sizeof icon_path, "%sicon.png", SDL_GetBasePath());
  SDL_Surface *icon = SDL_LoadPNG(icon_path);
#elif !defined(SDL_PLATFORM_WINDOWS)
  SDL_Surface *icon = SDL_CreateSurfaceFrom(ICON_SIZE, ICON_SIZE, SDL_PIXELFORMAT_RGBA32, (void *)ICON_RGBA, ICON_SIZE * 4);
#else
  SDL_Surface *icon = NULL;
#endif
  if (icon) {
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
  }
  SDL_SetRenderVSync(renderer, 1);
  int vsync = 0;
  SDL_GetRenderVSync(renderer, &vsync);
  /* The 416×276 ST frame, scaled to fit with square pixels. */
  SDL_SetRenderLogicalPresentation(renderer, FRAME_W, FRAME_H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, FRAME_W, FRAME_H);
  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);

  display_init(&display);
  app.music = music_open();
  app.playing = app.cursor = ENIGMA;
  app.authentic = authentic;
  if (menu) {
    /* Straight to the menu (development), with Enigma playing as after the intro. */
    music_play(app.music, ENIGMA);
    app.scene = menu_new(&app);
  } else app.scene = intro_new(&app);

  /* The ST's 50.05 Hz VBLs, run on display refreshes (see Pacer), locked to the sound.
     The picture is only redrawn when a VBL ran. */
  Pacer pacer;
  pacer_init(&pacer, 1000 / ST_VBL_HZ, 500);
  bool dirty = true;
  for (;;) {
    SDL_Event e;
    bool running = true;
    while (running && SDL_PollEvent(&e)) running = handle(&e);
    if (!running) break;
    Uint64 now_ns = SDL_GetTicksNS();
    if (click_pending && now_ns - click_at >= DOUBLE_CLICK_NS) {
      click_pending = false;
      if (app.scene->pointer) app.scene->pointer(app.scene, click_x, click_y, true);
    }
    if (fullscreen() && now_ns - mouse_moved > CURSOR_HIDE_NS) SDL_HideCursor();

    double audio;
    bool has_audio = music_output_time_ms(app.music, &audio);
    double now = now_ns / 1e6;
    int n = pacer_step(&pacer, now, has_audio ? audio : now, has_audio ? 1 : 0);
    for (int i = 0; i < n; i++) app.scene->vbl(app.scene);
    app_reap(&app);
    if (n > 0 || dirty) {
      app.scene->draw(app.scene, &display);
      display_render(&display);
      SDL_UpdateTexture(texture, NULL, display.out, FRAME_W * 4);
      dirty = false;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    /* Without vsync (or while hidden), wait for the display's next refresh ourselves. */
    if (!vsync || (SDL_GetWindowFlags(window) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_OCCLUDED))) {
      const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
      double hz = mode && mode->refresh_rate > 0 ? mode->refresh_rate : 60;
      Uint64 period = (Uint64)(1e9 / hz), next = (SDL_GetTicksNS() / period + 1) * period;
      SDL_DelayPrecise(next - SDL_GetTicksNS());
    }
  }

  music_close(app.music);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

int main(int argc, char *argv[]) {
  bool authentic = false, menu = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--render")) return render(argc - i - 1, argv + i + 1);
    if (!strcmp(argv[i], "--render-audio")) return render_audio(argc - i - 1, argv + i + 1);
    if (!strcmp(argv[i], "--authentic")) authentic = true;
    else if (!strcmp(argv[i], "--menu")) menu = true;
    else if (!strncmp(argv[i], "-psn_", 5)) continue; /* macOS Finder */
    else return usage();
  }
  return run(authentic, menu);
}
