# High Fidelity Dreams — Amiga port

A bootable disk for the Amiga 500 with 1 MB (OCS, Kickstart 1.3, 512 KB chip + 512 KB slow), in 68000 assembly.

**Status: the whole demo is there** ([SPEC.md](../../rip/SPEC.md) 3 to 6): the intro (the ear screen while the disk loads, AN AURA PRODUCTION, HIGH FIDELITY DREAMS, CODE AND DESIGN AEON), the text pages (the 4 bands, the text-spreader, the channel analysers), the menu (SPACE, the cursor) and the song changes (RETURN: the music fades out, LOADING and DECRUNCHING while the module is read and decompressed, back to the menu with it playing). The 8 modules are on the disk.
- **Every frame of the intro is identical to the demo's**, on the whole 416 × 276 frame borders included, from VBL 546 (the ear appears) to VBL 5298 (the last logo): 4,753 frames, verified against the Hatari reference recording.
- **Every frame of the text pages is identical to the web port's** (`scripts/render.ts`, with the music at the same position), analysers included: the first 6,000 VBLs from 5295, and 500 around the first loop of the raster script (VBL 17488). Against the Hatari recording, everything but the analysers is identical: Hatari doesn't replay the music the same way twice (see the web port's README).
- **Every frame of the menu and of a song change is identical to the demo's**: the reference recording's SPACE, DOWN and RETURN (Just Spank It), VBLs 9024 to 10400 (1,377 frames).
- The music starts on the same VBLs as in the demo (Enigma at 2836, Just Spank It at 10184), and follows the reference recording as closely as the web port's replay does: their 10 ms envelopes correlate at 0.90 to 0.94, without drifting. The 8 modules decompress byte for byte.

## Building

```bash
tools/setup.sh
```

builds the tools into `tools/.build/` (about 1 min): `vasm`, `ptplayer`, `zx0`, vAmiga's core and `capture`, and a Python venv. Then:

```bash
make
```

writes `build/hfd.adf` from `src/` and the data in [`../../rip/`](../../rip/) (the first time, compressing the modules takes a few minutes; `build/packed/` keeps them). `make AUTHENTIC=1` keeps the ear screen and the loading screens as long as in the demo (for the verification); by default they are shorter, as in the web port, but they last until the disk is read and the data decompressed. `make B=build/authentic AUTHENTIC=1` builds it into its own directory, beside the default one (the compressed files stay in `build/packed/`).

## Verifying

The Kickstart 1.3 ROM isn't in the repository (it is copyrighted): give its path to `capture`. The reference video comes from "Reference video" in [rip/README.md](../../rip/README.md).

```bash
tools/.build/cmake/capture ~/Kickstart1.3.rom build/hfd.adf 0 5200 build/frames
```

```bash
tools/.build/venv/bin/python tools/compare.py build/frames ../../rip/out/rec1/video.mkv --from 546 --to 5298
```

`capture` records each frame (5,200 frames: about 2 GB) and, in `vbl.txt`, the VBL it shows: the program writes it at $1F0. `compare.py` compares each frame with the reference frame of that VBL, whichever version was built. Two runs give the same frames. `CAPTURE_KEYS=frame:code,...` presses keys (raw Amiga codes, in hex): the reference recording's SPACE, DOWN and RETURN are, in `make AUTHENTIC=1`'s build:

```bash
CAPTURE_KEYS=8868:40,9017:4d,9167:44 tools/.build/cmake/capture ~/Kickstart1.3.rom build/hfd.adf 8880 1450 build/frames
```

```bash
tools/.build/venv/bin/python tools/compare.py build/frames ../../rip/out/rec1/video.mkv --from 9024 --to 10400
```

For the text pages, the web port renders the reference (`npm ci` in `ports/web` first). `ENIGMA_TICKS=2460`: the Amiga plays Enigma's 2460th tick at VBL 5295.

```bash
(cd ../web && ENIGMA_TICKS=2460 npx vite-node scripts/render.ts -- text /tmp/text.rgb 6000)
```

```bash
tools/.build/venv/bin/python tools/compare.py build/frames /tmp/text.rgb --rgb-vbl 5295 --from 5295
```

```bash
tools/.build/cmake/capture --audio ~/Kickstart1.3.rom build/hfd.adf 2605 40 build/audio.wav
```

records 40 s of sound from frame 2605 (VBL 2800 in `make AUTHENTIC=1`'s build); `CAPTURE_KEYS` works there too.

## How it works

### The disk and the memory

The bootblock reads the program's first part (the code, the Copper templates and the ear: 67 KB) and runs it; the program takes over the hardware and shows the ear while it reads the rest with its own trackloader (`src/loader.s`): its second part (the other screens, the menu, the text pages' RAM), then Enigma. Both are compressed with ZX0 and decompress in place: each file is read at the end of its destination, as the files' table says (`tools/mkdisk.py` writes it into the first part). The 8 modules take 728 KB of the disk's 880 (1.16 MB uncompressed); each is stored as is, or with its samples as differences between successive bytes, whichever compresses better.

The program and its variables live in any memory (the slow memory); only what the custom chips read lives in a block of chip memory (`src/memory.i`): the screens shown, the module (204 KB, the largest one decompressed in place), the Copper lists, the disk's DMA buffer. The screens are copied there when the second part is in.

### The display

Each VBL, the level 3 interrupt plays the music (ptplayer, one tick per VBL like the demo's replay) and computes a frame. The ST's per-line palettes become Copper lists:
- The intro, the menu and the loading screens compute the next frame into the Copper list that isn't shown. `tools/mkintro.py` builds, for each screen, a Copper list template that only sets the colours a line shows (colour 0 for the border, and the indices its pixels use), and a patch table: each patched colour at the 8 fade levels, and the band of lines whose level it follows. The measured exceptions (the menu's rules, its entries' outline) are in these values.
- A line's colours change after the screen's last pixel of the line before, colour 0 in the horizontal blank between the right and left borders.
- HIGH FIDELITY DREAMS' rules are the only lines where the demo opens the side borders. Their moving ends always lie in the screen: the CPU redraws them, shifted, in a double-buffered screen, and the Copper paints their parts in the borders by changing colour 0 in the middle of the line.

The text pages (`src/textpage.s`) are a port of the demo's own 68000 code, as the web port's `scenes/textpage.ts`: they work on a copy of the ST's RAM ($12000-$25B00, `rip/extracted/textpage`), with its data, its state and its self-modified variables at their original addresses. What changes is the display:
- the ST's interleaved screens become Amiga screens: the drawing routines compute Amiga addresses. A letter on the last byte of a line wraps to the start of the next one, as on the ST.
- The chain of 12 Timer B interrupts (the bands) becomes a Copper list, made in the frame it shows, as on the ST: the VBL interrupt rebuilds the rasters first, and the Copper waits for line 45 before reading them.
- The ST's analysers write bitplanes 0 and 3 (colours 1 and 8, which are equal); here, bitplane 0 only, and the screens have 3 bitplanes. The CPU draws a band's first line, the blitter copies it down.
- Each screen keeps the list of what the spreader drew in bitplane 1 (the bubble, the letters in flight), and erases only that two VBLs later, as the ST does.

Between the two ways of computing frames, a black frame serves as the joint (the intro's last one, the fades' last ones).

On a 7 MHz 68000, a VBL of the text pages takes at most 190 of the frame's 313 lines (the rasters are done by line 22). What doesn't change at run time is precomputed: the Copper lists and their colours at the 8 fade levels, the loading screens' words, the STE colours' conversion, the lines' offsets, and, from the ST, the pre-shifted font and bubble.

### The song changes

As the web port's `scenes/loading.ts`, in VBLs from RETURN: the music's master volume falls to 0 over 87 VBLs, then it stops and the main loop reads the new module into its place (LOADING), then decompresses it (DECRUNCHING); the loading screens (both words, both with the song's name) are made while the menu fades out. The screens stay until the work is done; the new song starts 32 VBLs after the last fade-out, and the menu fades in.

| File | Role |
|---|---|
| `src/boot.s` | The bootblock: allocates the program's memory and the chip memory, reads the first part with trackdisk (into chip memory: Kickstart 1.3's trackdisk decodes with the blitter), copies it, runs it. |
| `src/main.s` | Takes over the hardware, the interrupts (VBL, keyboard), the music's tick (and what the analysers read of it), the files, the main loop's work, the variables. |
| `src/intro.s` | The intro, as the web port's `scenes/intro.ts`, and the screens' Copper lists. |
| `src/textpage.s` | The text pages, as the web port's `scenes/textpage.ts`: rasters, spreader, analysers, fades. |
| `src/menu.s` | The menu and the song changes, as the web port's `scenes/menu.ts` and `scenes/loading.ts`. |
| `src/loader.s` | The trackloader: seeks, reads a track by DMA, decodes its MFM sectors (checksums, read again if one is bad). |
| `src/zx0.s` | ZX0's decompressor. |
| `tools/mkintro.py` | The screens' bitmaps (4 interleaved bitplanes), the rules' strips, the Copper templates and patches, the pulse table, the loading screens' words, the STE colours' table. |
| `tools/mkdisk.py` | The 880 KB disk: the bootblock with its checksum, the program's first part, its second part and the modules compressed, the files' table. |
| `tools/capture.cpp` | vAmiga driven frame by frame: frames as PPM (456 × 313, lowres, the whole PAL frame), the VBL shown, keys, sound as WAV. |
| `tools/compare.py` | Our frames (the 416 × 276 frame at (37, 30)) against the reference video or the web port's frames, in 12-bit colours. |

The frame is placed like Hatari's: the ST screen at DIWSTRT $3b79 (lines 59 to 258, a fetch from $30 delayed by 8 pixels), the 416 × 276 frame from line 30.

## Pitfalls

- **Colours**: Hatari renders a 4-bit component n as n × 17, vAmiga as n × 16. `compare.py` compares the 12-bit values.
- **Frame skipping**: in warp mode, vAmiga only renders one frame in 17 (`DENISE_FRAME_SKIPPING`), like Hatari without `--frameskips 0`. `capture` turns it off.
- **Which frame is captured**: vAmiga's end-of-frame trap stops the CPU at the end of line 312, and the finished frame only becomes the visible texture at the start of line 0. If an instruction straddles the two (an interrupt), the capture would get the following frame. `capture` therefore also runs line 0, and checks the texture's frame number.
- **Sound**: vAmiga mutes the sound in warp mode and while paused, its audio buffer must never run dry (it then clears it and changes the sample rate), and leaving warp mode leaves half the buffer as silence. `capture --audio` runs in VSYNC mode, one frame per wakeup, drops that silence, and only copies the samples that are there.
- **The VBL rate**: a non-interlaced PAL frame has 313 lines, 49.92 Hz (the ST's: 50.05 Hz). The music, one tick per VBL, is 0.27 % slower than on the ST, as the rest is.
- **ptplayer** plays a module's first row on its 6th tick, like ProTracker; the demo's replay on the first: `start_music` sets its counter. It ends with `END`, which would stop the assembly of `main.s`: the Makefile copies it without that line.
- **The trackloader**: DMA starts after the first sync word, so a sector may begin with one sync word instead of two. Two sync words are 1,088 bytes apart, a sector's header and data only 1,080.
- **Decompressing in place**: the compressed data must end where the decompressed data ends, plus the compressor's margin; the padding to whole sectors therefore goes before it.
- **Shrinkler** compresses the modules 10 % smaller than ZX0, but decompresses at about 4 KB/s on a 7 MHz 68000: Just Spank It would take 25 s (the demo took 11), Enigma at boot 45 s.
- **The memory**: a 512 KB chip memory can't hold everything; Kickstart 1.3's trackdisk only reads into chip memory.
- **Position independence**: PC-relative addressing reaches 32 KB: the `LEAFAR` and `FARBSR` macros (`hardware.i`) reach further. vasm silently turns a `bsr` out of range into an absolute `jmp`, which crashes a program that isn't where it was assembled: the Makefile makes its warnings errors. Its `rs.w 0` doesn't align: `rseven` does.
- Make can't depend on a file whose name contains spaces (the modules).

## Next steps

- A faster trackloader (it reads a whole revolution per track).
- The demo on a real Amiga 500: everything here was verified in vAmiga.
