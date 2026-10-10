/*
 * A virtual Atari ST low-resolution display, framed like Hatari's screenshots
 * (416×276, borders included) so frames can be compared with the reference recording.
 * Port of ports/web/src/st/display.ts.
 *
 * Colours are 12-bit 0xRGB numbers with 4-bit linear components (0 = black, 15 = full),
 * which is how the STE's 4-bit colours come out once the odd STE bit order is undone.
 * Every frame line has its own 16-colour palette: that's how rasters work. The border
 * of a line shows colour 0 of that line's palette.
 */
#ifndef HFD_DISPLAY_H
#define HFD_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>
#include "assets.h"

#define FRAME_W 416
#define FRAME_H 276
#define SCREEN_X 48
#define SCREEN_Y 29
#define SCREEN_W 320
/** Screen rows that can be shown, the lower border included (the menu opens it). */
#define MAX_ROWS (FRAME_H - SCREEN_Y)

/** How blit turns source indices into screen indices. */
typedef enum {
  MAP_ID,   /* i */
  MAP_LOW3, /* i & 7 */
  MAP_PINK, /* i & 7, moved to colours 9-15 (bitplane 3) */
} BlitMap;

typedef struct {
  /**
   * Palette indices for the whole frame, FRAME_W per line (use display_at to address
   * them in screen coordinates). Only the screen window of each line is shown: the 320
   * pixels of the first `rows` screen lines, or the whole line where `wide` opened the
   * side borders. Elsewhere the line shows its colour 0.
   */
  uint8_t pixels[FRAME_W * FRAME_H];
  int rows;
  /** Frame lines whose side borders are open (cleared by display_clear). */
  uint8_t wide_lines[FRAME_H];
  /** FRAME_H × 16 colours. Use the setters with screen coordinates (y may be negative). */
  uint16_t palette[FRAME_H * 16];
  /** Fade level applied to every colour when rendering (see fade_color). */
  int fade;
  bool fade_keeps_lsb;
  /** Lines whose colours are used as they are, whatever `fade` is (cleared by display_set_palette). */
  uint8_t no_fade[FRAME_H];
  /** The rendered frame, 0xAARRGGBB. */
  uint32_t out[FRAME_W * FRAME_H];

  uint32_t rgba[4096];
  uint32_t lut[4096];
  int lut_key;
} Display;

/**
 * The demo's palette fade: level j removes j from each component on the ST's 3-bit
 * scale. The main program drops the STE's extra low bit as soon as j > 0; the loader
 * (ear screen) keeps it unless the subtraction went below zero (`keep_lsb`).
 */
uint16_t fade_color(int c, int j, bool keep_lsb);

void display_init(Display *d);
void display_clear(Display *d);
/** Index into `pixels` of screen position (x, y); both may lie in the borders. */
static inline int display_at(int x, int y) { return (y + SCREEN_Y) * FRAME_W + x + SCREEN_X; }
/** Opens the side borders on screen lines y0 ≤ y < y1. */
void display_wide(Display *d, int y0, int y1);
/** Same palette on every line. */
void display_set_palette(Display *d, const uint16_t colors[16]);
/** Sets colour `index` on screen lines y0 ≤ y < y1 (screen coordinates, clipped to the frame). */
void display_set_lines(Display *d, int y0, int y1, int index, int color);
void display_set_color(Display *d, int y, int index, int color);
/** Marks screen lines y0 ≤ y < y1 as not faded. */
void display_no_fade(Display *d, int y0, int y1);
/**
 * Fades the palettes of screen lines y0 ≤ y < y1 to level j now, and exempts them
 * from the global fade: for screens whose parts fade separately.
 */
void display_fade_lines(Display *d, int y0, int y1, int j, bool keep_lsb);
/** Sets the palette of every line from a picture's palette and its per-line colours. */
void display_set_picture_colors(Display *d, const PictureMeta *meta);
/**
 * Copies a rectangle of an indexed image to screen position (dx, dy), clipped to the
 * frame. Index 0 is transparent unless `opaque`.
 */
void display_blit(Display *d, const IndexedImage *img, int sx, int sy, int w, int h, int dx, int dy,
                  BlitMap map, bool opaque);
/** Renders the frame into `out`. */
void display_render(Display *d);

#endif
