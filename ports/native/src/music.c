#include "music.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include "assets.h"

#define RATE 48000
#define TICK_QUEUE 1024 /* about 20 s of ticks: far more than any audio buffer holds */
#define CHUNK 1024

struct Music {
  SDL_AudioStream *stream;
  Module mods[SONG_COUNT];
  bool parsed[SONG_COUNT];
  Replayer replayer;
  bool playing;
  /** Frames handed to SDL so far, and the frame at which the current song started. */
  int64_t produced;
  int64_t start_frame;
  /** Ticks rendered but not heard yet (ring buffer), and the last one heard. */
  TickInfo queue[TICK_QUEUE];
  int head, count;
  TickInfo heard;
  bool has_heard;
  /** Frames the device holds beyond the stream (its buffer). */
  int device_frames;
  /** The audio clock: frames heard at `anchor_ns`, interpolated in between. */
  int64_t anchor_frames;
  Uint64 anchor_ns;
  bool started;
  float l[CHUNK], r[CHUNK], mix[CHUNK * 2];
};

static void on_tick(void *ctx, const TickInfo *t) {
  Music *m = ctx;
  if (m->count == TICK_QUEUE) { m->head = (m->head + 1) % TICK_QUEUE; m->count--; }
  TickInfo *q = &m->queue[(m->head + m->count++) % TICK_QUEUE];
  *q = *t;
  q->frame += m->start_frame; /* tick frames are relative to the start of the song */
}

/* Runs on SDL's audio thread, with the stream locked. */
static void SDLCALL feed(void *userdata, SDL_AudioStream *stream, int additional, int total) {
  (void)total;
  Music *m = userdata;
  int frames = additional / (int)(2 * sizeof(float));
  while (frames > 0) {
    int n = frames < CHUNK ? frames : CHUNK;
    if (m->playing) {
      replayer_render(&m->replayer, m->l, m->r, 0, n);
      for (int i = 0; i < n; i++) { m->mix[2 * i] = m->l[i]; m->mix[2 * i + 1] = m->r[i]; }
    } else memset(m->mix, 0, n * 2 * sizeof(float));
    SDL_PutAudioStreamData(stream, m->mix, n * 2 * (int)sizeof(float));
    m->produced += n;
    frames -= n;
  }
  m->started = true;
}

static bool open_stream(Music *m) {
  const SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, RATE};
  m->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, m);
  return m->stream != NULL;
}

Music *music_open(void) {
  Music *m = calloc(1, sizeof *m);
  if (!m) return NULL;
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO) || !open_stream(m)) {
    /* No sound device: SDL's dummy driver keeps the music's clock running in silence. */
    SDL_Log("No audio (%s): playing silently", SDL_GetError());
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO) || !open_stream(m)) {
      free(m);
      return NULL;
    }
  }
  SDL_AudioSpec dev;
  int frames = 0;
  if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(m->stream), &dev, &frames) && dev.freq > 0)
    m->device_frames = (int)((int64_t)frames * RATE / dev.freq);
  SDL_ResumeAudioStreamDevice(m->stream);
  return m;
}

void music_close(Music *m) {
  if (!m) return;
  SDL_DestroyAudioStream(m->stream);
  for (int i = 0; i < SONG_COUNT; i++) if (m->parsed[i]) mod_free(&m->mods[i]);
  free(m);
}

void music_play(Music *m, int song) {
  if (!m) return;
  SDL_LockAudioStream(m->stream);
  if (!m->parsed[song]) m->parsed[song] = mod_parse(&m->mods[song], SONGS[song].data, SONGS[song].size);
  if (m->parsed[song]) {
    replayer_init(&m->replayer, &m->mods[song], RATE, ST_VBL_HZ, on_tick, m);
    m->start_frame = m->produced;
    m->playing = true;
  }
  m->count = 0;
  m->has_heard = false;
  SDL_UnlockAudioStream(m->stream);
}

void music_stop(Music *m) {
  if (!m) return;
  SDL_LockAudioStream(m->stream);
  m->playing = false;
  m->count = 0;
  m->has_heard = false;
  SDL_UnlockAudioStream(m->stream);
}

void music_fade_out(Music *m, int ticks) {
  if (!m) return;
  SDL_LockAudioStream(m->stream);
  if (m->playing) replayer_fade_out(&m->replayer, ticks);
  SDL_UnlockAudioStream(m->stream);
}

/** Frames coming out of the speakers now: produced, minus what SDL and the device still hold. Call locked. */
static int64_t heard_frames(Music *m) {
  int queued = SDL_GetAudioStreamQueued(m->stream);
  return m->produced - (queued > 0 ? queued / (int)(2 * sizeof(float)) : 0) - m->device_frames;
}

bool music_output_time_ms(Music *m, double *ms) {
  if (!m) return false;
  SDL_LockAudioStream(m->stream);
  bool started = m->started;
  int64_t frames = heard_frames(m);
  SDL_UnlockAudioStream(m->stream);
  if (!started) return false;
  /* The count moves by whole device buffers: between two moves, let the clock run on
     the wall clock, never further than one buffer ahead. */
  Uint64 now = SDL_GetTicksNS();
  if (frames != m->anchor_frames) { m->anchor_frames = frames; m->anchor_ns = now; }
  double ahead = (now - m->anchor_ns) / 1e6, limit = (m->device_frames > 0 ? m->device_frames : RATE / 50) * 1000.0 / RATE;
  *ms = m->anchor_frames * 1000.0 / RATE + (ahead < limit ? ahead : limit);
  return true;
}

const TickInfo *music_current(Music *m) {
  if (!m) return NULL;
  SDL_LockAudioStream(m->stream);
  int64_t now = heard_frames(m);
  while (m->count && m->queue[m->head].frame <= now) {
    m->heard = m->queue[m->head];
    m->has_heard = true;
    m->head = (m->head + 1) % TICK_QUEUE;
    m->count--;
  }
  SDL_UnlockAudioStream(m->stream);
  return m->has_heard ? &m->heard : NULL;
}
