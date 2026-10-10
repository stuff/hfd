/*
 * Plays the modules through SDL's audio and tells which replay tick is being heard
 * right now. Replaces ports/web/src/audio/music.ts and worklet.ts.
 * Every function accepts a NULL Music (headless rendering): it then does nothing.
 */
#ifndef HFD_MUSIC_H
#define HFD_MUSIC_H

#include <stdbool.h>
#include "mod.h"

typedef struct Music Music;

/** Opens the audio device (or SDL's dummy driver if there is none). NULL on failure. */
Music *music_open(void);
void music_close(Music *m);
/** Starts song `song` (menu index) from the beginning. */
void music_play(Music *m, int song);
void music_stop(Music *m);
/** Linear fade to silence over `ticks` replay ticks (VBLs). */
void music_fade_out(Music *m, int ticks);
/**
 * The audio clock in ms, at the moment its sound comes out of the speakers. Returns
 * false before the device has asked for any sound. Pictures follow this clock so that
 * they stay in sync.
 */
bool music_output_time_ms(Music *m, double *ms);
/** The replay tick being heard now, for the analysers (NULL if none yet). */
const TickInfo *music_current(Music *m);

#endif
