# High Fidelity Dreams — behaviour specification

This document describes what the demo does, independently of any platform. It is the reference for writing a port (web, Windows/Mac, Amiga…).
Everything was measured in the reference video (Hatari, VBL by VBL) or read from the 68000 code. The web port (`ports/web/`) implements everything below, and can be consulted when in doubt.

## 0. Conventions

- **Clock**: everything advances by ST **VBL**, i.e. 50.0533 Hz (PAL). The VBL numbers quoted are those of the reference video `out/rec1/video.mkv`: frame n corresponds to VBL n, with EmuTOS at startup. See "Reference video" in [README.md](README.md).
- **Frame**: 416×276 pixels, like Hatari's captures, borders included. The 320×200 ST screen starts at (48, 29). "Line y" means a line of the ST screen (y = 0 at the top of the 200 lines). It can be negative (top border) or ≥ 200 (bottom border).
- **Colours**: written `0xRGB` with **linear 4-bit** components (0 = black, 15 = full), i.e. as displayed.
  STE palette words have a peculiar bit order (bit 3 = low bit). To convert an STE word `$RGB` to linear, each nibble n becomes `((n & 7) << 1) | (n >> 3)`.
  The JSON files in `graphics/` use `#rrggbb` where each component is 17 × the 4-bit value.
- **Rasters**: the palette changes from one line to the next. Each line therefore has its own 16-colour palette. The left/right border of a line shows colour 0 of that line.
- **Planes**: ST low resolution, 4 interleaved planes (160 bytes per line, 8 bytes per group of 16 pixels). Colour index = plane 0 bit | plane 1 << 1 | plane 2 << 2 | plane 3 << 3.

## 1. Fades

A fade level j goes from 0 (full colours) to 7 (black).

- **Main program** (logos, menu, loading screens, text pages): each component loses j on the ST's 3-bit scale, `v3 = max(0, (n >> 1) − j)`, then `n' = v3 << 1`. The STE's fine bit is therefore lost as soon as j > 0.
- **Loader** (ear screen only): same subtraction, but the fine bit is kept if `(n >> 1) − j ≥ 0`. If the subtraction goes below zero, the component drops to 0.
- **Rate**:
  - menu, loading screens, text pages: one level every **3 VBLs** (7 → 0 or 0 → 7, so 21 VBLs);
  - intro logos: one level every **2 VBLs**, from 6 to 0 on the way in, and from 1 to 7 on the way out;
  - ear: see 3.1.
- **Measured exceptions**:
  - The **horizontal rules** of the menu and loading screens (3 lines `fff aaa 666` on colour 0) don't follow the formula. They follow this table, by level j from 0 to 7:
    `fff aaa 666 | ccc 888 666 | aaa 888 444 | 888 666 444 | 666 444 222 | 444 444 222 | 222 222 000 | 000 000 000`.
  - The **white outline of the menu entries** (colour 5 = `ccc`) behaves like `fff` darkened by one step. During a fade it is therefore `fade(fff, max(j, 1))`, not `fade(ccc, j)`.
- The text pages don't compute their fade: they step from one precomputed colour table to the next (see 4).

## 2. Sequence and controls

| VBL (reference) | Screen |
|---|---|
| 0–545 | EmuTOS, then loading (nothing to port) |
| 546–2811 | Ear screen: LOADING (until 972) then DECRUNCHING |
| 2836 | Music starts (Enigma) |
| 3169–3776 | AN AURA PRODUCTION |
| 3798–4573 | HIGH FIDELITY DREAMS |
| 4704–5269 | CODE AND DESIGN AEON |
| 5295 → | Text pages, looping |

The ear's durations correspond to the actual loading from the disk. The web port shortens them by default, by skipping whole pulse cycles (see 3.1).

**Controls** (this is all the demo handles; ESC, TAB, HELP and the other keys do nothing):
- **SPACE** switches from the text pages to the menu, and from the menu to the text pages.
  - In the pages: SPACE is only read once the fade-in is over. 21-VBL fade-out, 3 VBLs of black, then the menu fades in (21 VBLs).
  - In the menu: fade-out (21 VBLs), then the text pages resume **exactly where they left off**. The text and the letters in flight are frozen during the menu, and the music keeps playing.
- **Menu**: ↑ / ↓ move the cursor, which reacts 1 VBL after the press. RETURN starts the song under the cursor. If it is the one already playing, the demo reloads it anyway (LOADING, DECRUNCHING, then it restarts from the beginning).

## 3. The intro

The images are in `graphics/pictures/`: indexed PNGs, with a JSON that gives the palette and the colours measured line by line (`per_line_palette`).

### 3.1 The ear (`loading_screen_indexed.png`, palette `loading_screen.json`)
- Appears at VBL 546. Loader fade-in (fine bit kept): level 6 at 546, 5 at 547, then −1 every 2 VBLs (4 at 549, … 0 at 557).
- Fade-out: level 1 at 2801, +1 every 2 VBLs up to 5 at 2809, then black at 2811.
- The words LOADING and DECRUNCHING are overlaid in the image, on lines 189 to 199: LOADING in colour 15, DECRUNCHING in colour 14. The small frame on the right is in colour 13 (`008`).
  - The displayed word **pulses**: a 56-VBL triangular ramp, from `000` to `eee` and back, one 3-bit level every 4 VBLs, with red, green then blue offset by one VBL. Formula for k = VBL − 546 (k ≥ 1): `m = (k−1) mod 56`, `n = m >> 2`, `sub = m & 3`. On the way up (n < 7), the level is n and each component i is level + (sub ≥ i ? 1 : 0). On the way down, the level is 7 − (n − 7) and each component i is level − (sub ≥ i ? 1 : 0). The colour is each component × 2.
  - At VBL 973, the pulse moves from colour 15 to colour 14 without losing its phase. The hidden word takes the background colour (`004`).
  - Colours 13 to 15 of these lines **are not affected by the fade** (the pulse is written after it). Only the hidden word, which equals the background, is.

### 3.2 AN AURA PRODUCTION (`fullscreen/screen_an_aura_production_indexed.png`)
- Three bands of lines, one per word: 53–72, 84–115 and 127–146. Word k fades in from VBL 3169 + 24k (6 → 0, one level every 2 VBLs).
- Common fade-out from 3763 (level 1, +1 every 2 VBLs).

### 3.3 HIGH FIDELITY DREAMS (`fullscreen/screen_high_fidelity_dreams_wide_indexed.png`, 416 px wide)
- **These are the only lines of the intro where the demo opens the side borders**: the rules on lines 14–21 and 178–185. The "wide" image was rebuilt borders included (x = 0 at the left edge of the 416 px frame).
- The rules slide by 2 px per VBL from 3798, for 142 VBLs:
  - the left edge of the top rule (pixels x 66 to 415 of the image) is at `350 − 2t`;
  - the right edge of the bottom rule (x 0 to 349) is at `65 + 2t`.
  - They fade in while sliding (6 → 0 from 3798, every 2 VBLs).
- The words HIGH (lines 37–68), FIDELITY (84–115) and DREAMS (131–162) fade in from 3972 + 6k.
- Common fade-out from 4560.
- Colour 8: `000` on the rule lines, `888` elsewhere (see the JSON).

### 3.4 CODE AND DESIGN AEON (`fullscreen/screen_code_and_design_aeon_indexed.png`)
- Four bands: CODE (7–38), AND (42–61, with the raster rule of lines 48–55 on colour 0), DESIGN (65–96) and AEON (128–191, gold gradient).
- Each band appears with a "flash", from 4704 + 12k. The levels, one per VBL, are `0 1 2 3 4 5 0 1 2 3 4 5 4 3 2 1 0`. Before the flash, the band is black.
- Common fade-out from 5256.

For each screen, each band has its own fade level, applied to its lines.

## 4. The text pages

The 68000 code is in `extracted/disasm/textpage_4000.s`. RAM from $12000 to $25B00, taken just before the pages (VBL 5280), is in `extracted/textpage/`: all the data and the initial state are there, at the addresses below. The web port reads it as is, which is the safest approach.

Order of operations on each VBL (interrupt at $12A22):
1. rasters ($12A36);
2. spreader ($1E552);
3. analysers ($1E250);
4. music replay.

**Double buffering**: two screens at $60000 and $68000. During a frame, the screen drawn at the previous VBL is shown, with the rasters set at the current VBL.

### 4.1 The 4 bands (rasters)
- These are not polygons but **precomputed rasters**. A chain of 12 Timer B interrupts (templates at $12B52, 40 bytes each) changes colours 0, 1 and 8 (8 = 1) after a given number of lines. The other colours (2–7, 9–15) all take the text colour.
- **Record** (26 bytes, pointer at $12B32):
  - 4 × (y, height) for the analysers;
  - 1 byte: number of lines before the first interrupt;
  - then 6 × [colour byte (high nibble = hue of the even interrupt, low nibble = of the odd one), line counter of the even one, counter of the odd one]. The last odd counter is not in the record.
  - A counter of 0 means "never" (256 lines). The 12th interrupt stops the timer.
- **Hues**: a 38-byte table = 9 pairs of STE words (colour 0, colour 1), then the text colour at +$24. There are 8 tables, from black ($1E120) to full ($1E22A). The table pointer is at $12A54.
- **Script** ($12D30 to $12F64, 12-byte entries): start, end, repeat count, step (signed, in bytes).
  - The records are walked from start to end by step. At the end, it starts over as long as the counter isn't exhausted (`subq.w` / `bhi`), then moves to the next entry.
  - An entry with step 0 freezes a record (pauses of 100 to 800 VBLs).
  - After the last entry, the script loops back to $12D48.
- **Fades**: the main loop adds 38 to the table pointer every 3 VBLs, 7 times (fade-in), and subtracts it the same way on the way out.

### 4.2 The text-spreader
- **Bubble**: 16×16 sprite on plane 1, pre-shifted at $2189E (16 shifts × 64 bytes), written with MOVE at (x − 4, y − 4).
  - Position read from a 600-point path: $21C9E to $2214E, 2 bytes per point (x − 32, y). The pointer is at $1E73C and advances by one point per VBL.
  - It is drawn at the previous VBL's position.
- **33 letter slots** at $1E9C8, 22 bytes each: glyph.l (−1 = empty), dx.l, x.l, dy.l, y.l (16.16 fixed point), counter.w.
  - On each VBL:
    - if the counter ≠ 0, x += dx and y += dy;
    - then counter −1.
  - At −2, the slot receives the next letter, with the counter set to 64. It starts from the bubble's current position with `d = ((target − start) << 16) >> 6`, so it arrives in 64 VBLs.
  - In flight, the letter is OR-ed onto plane 1, which is cleared on every VBL. With a counter ≤ 0, it is drawn on plane 2, which nothing clears: that is the settled text.
  - At the start, the counters are 0, 2, 4…, which gives a new letter every 2 VBLs.
  - A pause (long counter at $1E9C4) leaves slots empty. It starts at 10.
- **Font**: 8×8, pre-shifted at $1EC9E (44 characters × 128 bytes, codes 48 to 91) and at $2029E (byte-split version). What matters: the glyph is OR-ed with pixel precision in x.
- **Text stream** (pointer at $1E884, start at $2214E, 14,765 bytes):
  - `0, x, y, p`: new line. p is the character step, signed (10 normally). If p < 0, the line is stored reversed and written right to left, with x = 322 − x + p.
  - `9, n`: pause of 30 × n.
  - `$BD, n`: page clear (plane 2), 2 lines per VBL from line 198 − n, and a pause of n / 2.
  - `$FF`: back to the start of the text.
  - A byte < $30 is a space (no glyph, but x advances). Otherwise, it is glyph byte − $30.
  - The font shows `;` as ".", `<` as "(", `=` as "!", `>` as ")", `@` as a block and `[` as "-".

### 4.3 The channel analysers
- Each analyser band has a level (word at $1E2E4 + 4k, followed by y and the height, copied from the record).
- **Band k follows channel 3 − k of the module**: the NoisePacker replay stores its channels in reverse order.
- On each VBL, based on the replay's state at the previous VBL:
  - if a pattern row has just been read and it contains a note, an instrument or a C effect, the level takes the channel's volume;
  - on the other ticks, a 5, 6 or A effect (volume slides) also resets the level to the channel's volume;
  - then, if the level ≠ 0, it loses 2 without going below 2.
- **Drawing**: on lines y..y + height, 8 long words per line from byte 22 of the line, written with MOVE. The high word goes to plane 3, the low word to plane 0 of the next group: this makes a continuous 32-pixel block.
  - The pattern depends on `level − 1`: bits 4–5 choose the width (4 variants), bits 1–3 choose one of 8 edge patterns. The constants are at $1E314–$1E3E0, and in `ports/web/src/scenes/textpage.ts`.

## 5. The menu

- Image: `graphics/menu/menu_select_song_indexed.png`, 218 lines (the demo opens the bottom border).
- **Entries**: entry k occupies lines 37 + 23k to 37 + 23k + 19.
- **Base palette**:
  - colours 1–5 of the entries: `004 226 448 66a ccc`, colour 7 = `000`;
  - SELECT SONG header (lines 7–26): `840 a62 c84 ea6 fff`;
  - rules on lines 2–4 and 30–32 (colour 0): `fff aaa 666`.
- **Entry under the cursor**: colours 1–5 = `226 448 66a 88c fff`. On its lines +7 to +12, colour 0 is `fff 226 448 66a 88c fff` (the cursor rule, which crosses the borders), and colours 7 and 15 are `fff`.
- **Entry playing**: drawn with plane 3 added (index + 8), i.e. colours 9–13 = `626 848 a6a c8c fff`. It is pink, whether or not it is under the cursor. The cursor only changes colour 0 and colour 15 on it.
- The entries are all centred. Their words are reused as is for the loading screens.

## 6. Changing song (RETURN)

Time in VBLs since the press:

| t | Event |
|---|---|
| 0 | The music fades linearly to silence over 87 VBLs |
| 87 | Menu fade-out |
| 108 | Loading screen (fade-in): rules on lines 67 and 135, LOADING (orange, line 78, centred), song name in pink on line 106 |
| 288 | LOADING fades out (21 VBLs, only on its lines), then DECRUNCHING appears (21 VBLs) |
| 852 | Fade-out |
| 884 | The new song starts, and the menu fades back in with it in pink |

Times 288 and 852 come from the actual loading of Just Spank It from the disk. They vary with the song's position on the disk. The web port shortens them by default (`?authentic` to get them back).

## 7. The music

- The 8 files in `music/` are the original modules (see README.md, section 1). The demo plays them with a NoisePacker 2 replay, driven by the VBL: **one tick per VBL, at 50.05 Hz**. No module uses CIA tempo.
- Original sound: software emulation of Paula ("HiFi-Paula", 50 kHz DMA on STE). Amiga-style hard stereo: channels 1 + 4 on the left, 2 + 3 on the right. No interpolation.
- Effect numbering in the NoisePacker replay: 7 = volume slide (ProTracker's A). A D (pattern break) always restarts at row 0; none of the 8 modules has a D with a parameter.
- Enigma starts at VBL 2836 of the reference, during the black that follows the ear, and only stops at the first song change.
- **Warning**: from one Hatari run to the next, the music doesn't land on exactly the same VBL (a 2-row pattern offset was observed). The analysers therefore can't be compared frame by frame with the reference.

## 8. Where to find the data

| For | Files |
|---|---|
| Music | `music/*.mod` |
| Ear | `graphics/pictures/loading_screen_indexed.png` + `loading_screen.json` |
| Logos | `graphics/pictures/fullscreen/*_indexed.png` (HIGH FIDELITY DREAMS: `*_wide_indexed.png`, 416 px) + `*.json` |
| Menu and loading screens | `graphics/menu/menu_select_song_indexed.png`, `graphics/menu/words/word_loading_indexed.png`, `word_decrunching_indexed.png` |
| Text pages | `extracted/textpage/ram_012000_025b00.bin` (everything: tables, text, pre-shifted font and bubble), or separately `graphics/fonts/font8x8.bin`, `graphics/sprites/bubble16.bin`, `texts/hfd_texts.txt` (readable text, without the layout codes) |

## 9. Platform notes

- **Amiga**: the ideal machine for this port.
  - The PAL display runs at 50 Hz, so pacing is no problem.
  - Paula plays the modules in hardware: a ProTracker replay is all it takes. Only the analysers need the channels' state, in reverse order.
  - The Copper naturally does the rasters, one palette per line. It can also change colour 0 in the borders, which covers the rules and the bands that cross the screen.
  - The Amiga's 12-bit colours map directly to this document's `0xRGB` notation.
  - For the HIGH FIDELITY DREAMS rules, an overscan display is needed, or they must be cropped.
  - The blitter can draw the letters (OR onto a plane) and clear them.
- **Windows / Mac**:
  - Done in `ports/native/` (C + SDL3, also for Linux): a transcription of the web port, MOD player included.
  - As on the web, the display rarely runs at 50 Hz. The logic must stay at 50.05 Hz and be shown at the display's refresh rate (see `ports/web/src/pacing.ts`). With a VRR or 100 Hz display, the pacing is perfect.
- **In every case**: the most effective verification is to render the scenes offscreen and compare them frame by frame with `out/rec1/video.mkv`, as the web port does (`scripts/render.ts` and `scripts/compare.py`).
