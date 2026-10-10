# Original graphics and fonts

General overview: `overview.png`.

## Where these images come from
The main program keeps its images **packed** and unpacks them when it displays them ("realtime picture depacking" in the demo's text).
So the demo was stopped in Hatari at each screen (`tools/schedule.py` + `tools/hrun.sh`), and the pixels were read back **directly from video memory**.
These are the exact colour indices, in low resolution, 320 px wide and 4 planes, not a recoloured screenshot.
The palette is derived from Hatari's screenshot, **line by line**, because almost every screen changes colours mid-scan (rasters).
`tools/stscreen.py` aligns the video RAM with the screenshot: 0 inconsistent pixels for the logos and the menu.

## Formats
For each image `<name>`:
| File | Contents |
|---|---|
| `<name>_indexed.png` | **original pixels** (16-colour indexed PNG). The palette is the most frequent colour of each index over the image. |
| `<name>.png` | rendering **as displayed**, rasters included |
| `<name>.json` | source RAM address, crop box, palette (`#rrggbb` + STE colour words `$RGB`), "raster" indices, and `per_line_palette` (colours of the raster indices for each line) |
| `<name>.pi1` | full screens only: **Degas** file (palette + 32,000 planar bytes), readable by ST tools |

## Contents
- `pictures/`
  - `loading_screen*`: the loading screen (the ear, "by Aeon"), 16 colours. The words LOADING and DECRUNCHING are in the same image, on indices 13 to 15, revealed by raster.
  - `logo_an_aura_production`, `logo_high_fidelity_dreams`, `logo_code_and_design_aeon`: the three cropped logos.
    The gold gradient of "AEON" and the blue of "CODE AND DESIGN" are rasters on the same indices, see the JSON.
  - `fullscreen/`: the matching full screens (with the rules), as PNG and `.pi1`.
- `menu/`
  - `menu_select_song`: the whole menu, **218 lines**, because the demo opens the bottom border.
    The entry currently playing is drawn with plane 3 added (index +8), which gives it the pink palette.
    The cursor (the two rules) is a raster on colour 0, not pixels.
  - `words/`: each word separately (SELECT SONG, the 8 titles, ENIGMA in normal and "playing" versions, LOADING, DECRUNCHING).
    The "LOADING / DECRUNCHING + song name" screens are composed from these same words (example: `screen_*_act_of_impulse`).
- `fonts/font8x8.*`: the text-spreader's font, **44 8×8 glyphs on 1 plane**, in ASCII order from 48 `0` to 91 `[`.
  - `font8x8.bin`: 8 bytes per glyph.
  - `font8x8_strip.png`: 352×8 pixel strip.
  - `font8x8_sheet.png`: enlarged sheet, with each code and the character displayed.
  - In RAM, the font is stored **pre-shifted**: at $1EC9E for shifts 0 to 7 and at $2029E for shifts 8 to 15, i.e. 128 bytes per character.
    The big titles CREDITS, FEATURES, AURA and LET'S WRAP are not images: they are composed with this font, from the dot art present in the text (`texts/hfd_texts.txt`).
- `sprites/`
  - `bubble16.*`: the 16×16 bubble on 1 plane, pre-shifted in RAM at $2189E.
  - `disconnect_module_msg.*`: the 128×6 "DISCONNECT MODULE!" message, shown by boot stage 2 if a cartridge is plugged in.
- `textpage/quads_frame_*`: one frame of the text pages' background.
  The "4 big vector-quaders with voice-analysers" announced in the text are these **4 bands**.
  They are quads rotating around the horizontal axis, drawn in real time in planes 0 and 3.
  The vertical stripes are the analysers of the 4 channels and the colours are rasters. It's code, not an image: the frame serves as a visual reference.

## Taking a new capture
```bash
tools/schedule.py out/x "9000:0x39" "9300"
```
This command schedules a SPACE press at VBL 9000 (entering the menu) and a capture at VBL 9300.
```bash
tools/hrun.sh out/x 9400 "" ""
```
This one runs the demo. Then see `tools/export_pic.py` (function `export`) to produce the PNGs, the JSON and the `.pi1`.
