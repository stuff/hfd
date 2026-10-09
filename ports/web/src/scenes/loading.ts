import { images, SONGS } from '../assets';
import { type Display, fadeColor } from '../st/display';
import { type App, type Scene, fadeAt, FADE_IN, FADE_OUT } from './scene';
import { drawMenu, drawSongWord, MenuScene, rule } from './menu';

const WORD_Y = 78;
const NAME_Y = 106;
const ORANGE = [0x840, 0xa62, 0xc84, 0xea6, 0xfff];
const PINK = [0x626, 0x848, 0xa6a, 0xc8c, 0xfff];

/**
 * After RETURN in the menu: the music fades out, then "LOADING <song>", then
 * "DECRUNCHING <song>", then back to the menu with the new song playing.
 * Times in VBLs from the key press, as measured in the reference recording.
 */
const MUSIC_FADE = 87; // the music fades out over 87 VBLs...
const MENU_OUT = 87; // ...then the menu fades out
const SCREEN_IN = 108; // the loading screen fades in
const WORD_SWAP = 21; // LOADING fades out, DECRUNCHING fades in, 21 VBLs each
const MENU_IN = 32; // from the start of the last fade-out to the menu fading in with the new song
const AUTHENTIC = { loading: 288, decrunching: 852 }; // the real disk took that long
const MIN_LOADING = 60; // VBLs on screen in the short version
const MIN_DECRUNCHING = 60;

export class LoadingScene implements Scene {
  private t = 0;
  private loaded = false;
  private loadingEnd = Infinity;
  private decrunchingEnd = Infinity;

  constructor(private readonly app: App, private readonly song: number) {
    app.music.fadeOut(MUSIC_FADE);
    app.music.load(SONGS[song].url).then(() => (this.loaded = true), (e) => console.error(e));
  }

  vbl(): void {
    const t = ++this.t;
    if (this.loadingEnd === Infinity && t >= SCREEN_IN + 21 && this.loaded) {
      this.loadingEnd = Math.max(t, this.app.authentic ? AUTHENTIC.loading : SCREEN_IN + 21 + MIN_LOADING);
      this.decrunchingEnd = this.app.authentic
        ? Math.max(AUTHENTIC.decrunching, this.loadingEnd + 2 * WORD_SWAP)
        : this.loadingEnd + 2 * WORD_SWAP + MIN_DECRUNCHING;
    }
    if (t === MUSIC_FADE) this.app.music.stop();
    if (t >= this.decrunchingEnd + MENU_IN) {
      this.app.playing = this.song;
      void this.app.music.play(SONGS[this.song].url);
      this.app.go(new MenuScene(this.app));
    }
  }

  draw(d: Display): void {
    const t = this.t;
    if (t < SCREEN_IN) {
      drawMenu(d, this.app.cursor, this.app.playing, fadeAt(t - MENU_OUT, FADE_OUT, 0));
      return;
    }
    d.rows = 200;
    d.fade = t >= this.decrunchingEnd ? fadeAt(t - this.decrunchingEnd, FADE_OUT) : fadeAt(t - SCREEN_IN, FADE_IN);
    d.clear();
    d.setPalette([0, ...ORANGE, 0, 0, 0, ...PINK, 0, 0]);
    rule(d, 67);
    rule(d, 135);
    drawSongWord(d, this.song, NAME_Y, true);

    // LOADING, then a fade on its lines only, then DECRUNCHING.
    const since = t - this.loadingEnd;
    const decrunching = since >= WORD_SWAP;
    const w = decrunching ? images.wordDecrunching : images.wordLoading;
    d.blit(w, 0, 0, w.width, w.height, (320 - w.width) >> 1, WORD_Y, (i) => i & 7);
    if (since >= 0) {
      const j = decrunching ? fadeAt(since - WORD_SWAP, FADE_IN) : fadeAt(since, FADE_OUT);
      ORANGE.forEach((c, i) => d.setLines(WORD_Y, WORD_Y + 20, i + 1, fadeColor(c, j)));
    }
  }
}
