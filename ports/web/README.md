# High Fidelity Dreams — web port

An exact replica of the music disk in the browser, in Vite + TypeScript, with no runtime dependencies.
The original data (modules, graphics) is read straight from [`../../rip/`](../../rip/) at build time.

```bash
npm install
npm run dev        # http://localhost:5173
npm run build      # static site in dist/
```

URL parameters:
- `?authentic`: original loading times, as on the floppy disk. The ear screen then lasts 45 s instead of 6 s, and each song change 18 s instead of 5 s. The fades are the same either way.
- `?menu`: skips the intro and goes straight to the menu (for development).
- `?debug`: exposes the application in the console as `hfd`.

Controls, as in the demo:
- SPACE switches from the text pages to the menu, and from the menu to the text pages. The text pages resume where they left off, and the music keeps playing.
- In the menu, ↑ / ↓ to choose, RETURN to start the song.

For mouse and touch (not in the demo): a click on a text page, or on SELECT SONG in the menu, does what SPACE does. Hovering moves the cursor, and a click or tap on a title starts it.

Full screen: double-click (or double tap), or the F key. So that a double-click doesn't also count as a click, a single click's action waits 250 ms.

## Status
- [x] SELECT SONG menu, cursor, now-playing entry, fades
- [x] LOADING / DECRUNCHING + song name, sound fade-out
- [x] MOD player (AudioWorklet)
- [x] Intro: ear screen (pulsing LOADING / DECRUNCHING), AN AURA PRODUCTION / HIGH FIDELITY DREAMS (rules sliding into the borders) / CODE AND DESIGN AEON logos
- [x] Text pages: text-spreader, 4 "vector" bands (rasters), channel analysers, fades

## How it works
- **`src/st/display.ts`**: a virtual ST screen framed like Hatari's captures (416×276, borders included, screen at (48, 29)).
  Pixels are colour indices. Each line has its own 16-colour palette (that is how rasters work), and the border takes colour 0 of its line.
  The fade reproduces the demo's: at level j, each component loses j on the ST's 3-bit scale.
  The main program loses the STE's fine bit along the way, whereas the loader (ear screen) keeps it. `fadeLines` gives a band of lines its own fade level, and `wide` opens a line's side borders.
- **`src/audio/mod.ts`**: a dependency-free ProTracker replay (it runs in the AudioWorklet as well as in Node).
  It mixes like Paula, without interpolation, with channels 1+4 on the left and 2+3 on the right. Ticks run at 50.05 Hz, the ST's VBL rate, like the demo's replay.
  It reports each channel's state on every tick (volume, period, peak, trigger), for the analysers.
- **`src/scenes/textpage.ts`**: the port of the text pages' 68000 code (rasters at $12A36, spreader at $1E552, analysers at $1E250). It emulates the ST's two bitplane screens and the chain of 12 Timer B interrupts. It works on a copy of the ST's RAM ($12000–$25B00, in `rip/extracted/textpage/`), read and modified at the original addresses: raster script and records, colour tables, pre-shifted font and bubble, bubble path, text, 33 letters in flight.
- **`src/pacing.ts`**: the display pacing. The demo advances by ST VBLs (50.05 Hz), whatever the display.
  On each refresh, we run as many VBLs as the clock asks for, and only redraw if at least one ran. This clock advances by a whole number of refreshes, whose period is measured: the pacing therefore stays regular despite timestamp jitter (at 60 Hz, one frame in five stays up for two refreshes, which is unavoidable). It is also gently pulled towards the clock of the sound actually being heard, so that picture and music never drift apart.
- **`src/scenes/`**: one scene per screen. Everything advances by VBL (1/50 s), with durations measured in the reference recording.
- The Vite plugin in `vite.config.ts` turns `…_indexed.png?indexed` into raw palette indices at build time.

## Verification against the original demo
The reference is the frame-by-frame recording of the demo in Hatari (see "Reference video" in [`rip/README.md`](../../rip/README.md)).
`scripts/render.ts` runs the scenes without a browser, and `scripts/compare.py` compares them pixel by pixel with the video:

```bash
npx vite-node scripts/render.ts -- intro /tmp/intro.rgb 4753
python3 scripts/compare.py /tmp/intro.rgb ../../rip/out/rec1/video.mkv 546
npx vite-node scripts/render.ts -- menu /tmp/menu.rgb 1396 127:ArrowDown 276:Enter
python3 scripts/compare.py /tmp/menu.rgb ../../rip/out/rec1/video.mkv 9024
```
- **Intro**: VBL 0 of the script corresponds to VBL 546 of the reference (the ear appearing, after EmuTOS). Result: **0 pixels of difference over 4,753 frames**, from the ear to the end of CODE AND DESIGN AEON.
- **Menu**: VBL 0 corresponds to VBL 9024 (SPACE at VBL 9000). The menu, cursor movement, RETURN, LOADING, DECRUNCHING and the return to the menu give **0 pixels of difference over 1,396 frames**.

Both comparisons are done VBL by VBL.

- **Text pages**:
  ```bash
  ENIGMA_TICKS=2471 npx vite-node scripts/render.ts -- text /tmp/text.rgb 600
  python3 scripts/compare.py /tmp/text.rgb ../../rip/out/rec1/video.mkv 5295
  ```
  The text, the bubble, the letters in flight and the bands are identical to the reference. The analysers, however, depend on the exact timing of the music.
  And Hatari doesn't replay it at exactly the same moment from one run to the next: two runs showed a 2-row pattern offset. The analysers' logic was therefore validated against the levels read from RAM (same values as the ST), not to the frame.

For sound, `scripts/render-audio.ts` renders a module to WAV. Compared with libopenmpt at the same tempo, the envelope correlates between 0.94 and 0.99. Only a few passages of *Act of Impulse* (vibrato and instrument change without a note) drop to 0.7–0.85.

`scripts/smoke.mjs` runs the build in headless Chrome (puppeteer-core), clicks, changes songs, takes screenshots and collects console errors.
