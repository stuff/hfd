/* The demo's data, embedded at build time from rip/ by tools/embed.py. */
#ifndef HFD_ASSETS_H
#define HFD_ASSETS_H

#include <stddef.h>
#include <stdint.h>

/** An indexed picture: one palette index per byte. */
typedef struct {
  int width, height;
  const uint8_t *data;
} IndexedImage;

/** A picture's palette, then the colours measured line by line (rasters): {y, index, 0xRGB}. */
typedef struct {
  uint16_t palette[16];
  const uint16_t (*lines)[3];
  int line_count;
} PictureMeta;

typedef struct {
  const char *title;
  const uint8_t *data;
  size_t size;
} Song;

extern const IndexedImage IMG_MENU, IMG_WORD_LOADING, IMG_WORD_DECRUNCHING;
/** The ear (loading screen) and the intro's three screens; IMG_HFD is 416 wide. */
extern const IndexedImage IMG_EAR, IMG_AURA, IMG_HFD, IMG_AEON;
extern const PictureMeta META_EAR, META_AURA, META_HFD, META_AEON;

/** Menu order. */
#define SONG_COUNT 8
#define ENIGMA 3
extern const Song SONGS[SONG_COUNT];

/** The ST's RAM from $12000 to $25B00 just before the text pages start. */
extern const uint8_t *const TEXTPAGE_RAM;
extern const size_t TEXTPAGE_RAM_SIZE;

#endif
