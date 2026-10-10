#include "display.h"

#include <string.h>

uint16_t fade_color(int c, int j, bool keep_lsb) {
  if (j <= 0) return (uint16_t)c;
  int out = 0;
  for (int shift = 8; shift >= 0; shift -= 4) {
    int n = (c >> shift) & 15;
    int v3 = (n >> 1) - j;
    if (v3 >= 0) out |= ((v3 << 1) | (keep_lsb ? n & 1 : 0)) << shift;
  }
  return (uint16_t)out;
}

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void display_init(Display *d) {
  memset(d, 0, sizeof *d);
  d->rows = 200;
  d->lut_key = -1;
  for (int c = 0; c < 4096; c++) {
    uint32_t r = ((c >> 8) & 15) * 17, g = ((c >> 4) & 15) * 17, b = (c & 15) * 17;
    d->rgba[c] = 0xff000000u | (r << 16) | (g << 8) | b;
  }
}

void display_clear(Display *d) {
  memset(d->pixels, 0, sizeof d->pixels);
  memset(d->wide_lines, 0, sizeof d->wide_lines);
}

void display_wide(Display *d, int y0, int y1) {
  for (int l = clamp(y0 + SCREEN_Y, 0, FRAME_H); l < clamp(y1 + SCREEN_Y, 0, FRAME_H); l++) d->wide_lines[l] = 1;
}

void display_set_palette(Display *d, const uint16_t colors[16]) {
  for (int l = 0; l < FRAME_H; l++) memcpy(d->palette + l * 16, colors, 16 * sizeof *colors);
  memset(d->no_fade, 0, sizeof d->no_fade);
}

void display_set_lines(Display *d, int y0, int y1, int index, int color) {
  int a = clamp(y0 + SCREEN_Y, 0, FRAME_H), b = clamp(y1 + SCREEN_Y, 0, FRAME_H);
  for (int l = a; l < b; l++) d->palette[l * 16 + index] = (uint16_t)color;
}

void display_set_color(Display *d, int y, int index, int color) { display_set_lines(d, y, y + 1, index, color); }

void display_no_fade(Display *d, int y0, int y1) {
  for (int l = clamp(y0 + SCREEN_Y, 0, FRAME_H); l < clamp(y1 + SCREEN_Y, 0, FRAME_H); l++) d->no_fade[l] = 1;
}

void display_fade_lines(Display *d, int y0, int y1, int j, bool keep_lsb) {
  int a = clamp(y0 + SCREEN_Y, 0, FRAME_H), b = clamp(y1 + SCREEN_Y, 0, FRAME_H);
  for (int l = a; l < b; l++) {
    for (int i = 0; i < 16; i++) d->palette[l * 16 + i] = fade_color(d->palette[l * 16 + i], j, keep_lsb);
    d->no_fade[l] = 1;
  }
}

void display_set_picture_colors(Display *d, const PictureMeta *meta) {
  display_set_palette(d, meta->palette);
  for (int k = 0; k < meta->line_count; k++)
    display_set_color(d, meta->lines[k][0], meta->lines[k][1], meta->lines[k][2]);
}

void display_blit(Display *d, const IndexedImage *img, int sx, int sy, int w, int h, int dx, int dy,
                  BlitMap map, bool opaque) {
  for (int y = 0; y < h; y++) {
    int fy = dy + y + SCREEN_Y;
    if (fy < 0 || fy >= FRAME_H || sy + y < 0 || sy + y >= img->height) continue;
    for (int x = 0; x < w; x++) {
      int fx = dx + x + SCREEN_X;
      if (fx < 0 || fx >= FRAME_W || sx + x < 0 || sx + x >= img->width) continue;
      int i = img->data[(sy + y) * img->width + sx + x];
      if (!i && !opaque) continue;
      if (map == MAP_LOW3) i &= 7;
      else if (map == MAP_PINK) i = i & 7 ? (i & 7) | 8 : 0;
      d->pixels[fy * FRAME_W + fx] = (uint8_t)i;
    }
  }
}

void display_render(Display *d) {
  int key = d->fade * 2 + (d->fade_keeps_lsb ? 1 : 0);
  if (d->lut_key != key) {
    for (int c = 0; c < 4096; c++) d->lut[c] = d->rgba[fade_color(c, d->fade, d->fade_keeps_lsb)];
    d->lut_key = key;
  }
  uint32_t line[16];
  for (int l = 0; l < FRAME_H; l++) {
    const uint32_t *map = d->no_fade[l] ? d->rgba : d->lut;
    for (int i = 0; i < 16; i++) line[i] = map[d->palette[l * 16 + i]];
    int o = l * FRAME_W, y = l - SCREEN_Y;
    int x0 = SCREEN_X, x1 = SCREEN_X + SCREEN_W;
    if (d->wide_lines[l]) { x0 = 0; x1 = FRAME_W; }
    else if (y < 0 || y >= d->rows) x0 = x1 = FRAME_W;
    for (int x = 0; x < x0; x++) d->out[o + x] = line[0];
    for (int x = x0; x < x1; x++) d->out[o + x] = line[d->pixels[o + x]];
    for (int x = x1; x < FRAME_W; x++) d->out[o + x] = line[0];
  }
}
