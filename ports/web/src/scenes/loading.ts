import { images, SONGS } from '../assets';
import { type Display, fadeColor } from '../st/display';
import { type App, type Scene, fadeAt, FADE_IN, FADE_IN_EVEN, FADE_OUT, FADE_OUT_EVEN } from './scene';
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
const MENU_FADE = 108; // menu fades out (the music fade takes 95 VBLs)
const SCREEN_IN = 132; // loading screen fades in
const AUTHENTIC = { loading: 306, decrunching: 870 }; // the real disk took that long
const MIN_LOADING = 60; // VBLs on screen in the short version
const MIN_DECRUNCHING = 60;

export class LoadingScene implements Scene {
  private t = 0;
  private loaded = false;
  private loadingEnd = Infinity;
  private decrunchingEnd = Infinity;

  constructor(private readonly app: App, private readonly song: number) {
    app.music.fadeOut(95);
    app.music.load(SONGS[song].url).then(() => (this.loaded = true), (e) => console.error(e));
  }

  vbl(): void {
    const t = ++this.t;
    if (this.loadingEnd === Infinity && t >= SCREEN_IN + 18 && this.loaded) {
      this.loadingEnd = Math.max(t, this.app.authentic ? AUTHENTIC.loading : SCREEN_IN + 18 + MIN_LOADING);
    }
    if (this.loadingEnd !== Infinity && this.decrunchingEnd === Infinity) {
      this.decrunchingEnd = this.app.authentic ? AUTHENTIC.decrunching : this.loadingEnd + 36 + MIN_DECRUNCHING;
    }
    if (t === MENU_FADE + 24) this.app.music.stop();
    if (t >= this.decrunchingEnd + 30) {
      this.app.playing = this.song;
      void this.app.music.play(SONGS[this.song].url);
      this.app.go(new MenuScene(this.app, FADE_IN_EVEN));
    }
  }

  draw(d: Display): void {
    const t = this.t;
    if (t < SCREEN_IN) {
      drawMenu(d, this.app.cursor, this.app.playing, fadeAt(t - MENU_FADE, FADE_OUT, 0));
      return;
    }
    d.rows = 200;
    d.fade = t >= this.decrunchingEnd ? fadeAt(t - this.decrunchingEnd, FADE_OUT_EVEN) : fadeAt(t - SCREEN_IN, FADE_IN);
    d.clear();
    d.setPalette([0, ...ORANGE, 0, 0, 0, ...PINK, 0, 0]);
    rule(d, 67);
    rule(d, 135);
    drawSongWord(d, this.song, NAME_Y, true);

    // LOADING, then a fade on its lines only, then DECRUNCHING.
    const sinceLoad = t - this.loadingEnd;
    const decrunching = sinceLoad >= 18;
    const w = decrunching ? images.wordDecrunching : images.wordLoading;
    d.blit(w, 0, 0, w.width, w.height, (320 - w.width) >> 1, WORD_Y, (i) => i & 7);
    let wordFade = 0;
    if (sinceLoad >= 0) wordFade = decrunching ? fadeAt(sinceLoad - 18, FADE_IN_EVEN) : fadeAt(sinceLoad, [2, 4, 6]);
    if (wordFade) ORANGE.forEach((c, i) => d.setLines(WORD_Y, WORD_Y + 20, i + 1, fadeColor(c, wordFade)));

  }
}
