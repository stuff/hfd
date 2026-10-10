# High Fidelity Dreams — Amiga port

A bootable disk for the Amiga 500 (OCS, Kickstart 1.3, 512 KB chip + 512 KB slow), in 68000 assembly.

**Status: the intro is done** ([SPEC.md](../../rip/SPEC.md) 3): the ear screen (LOADING then DECRUNCHING, while the disk loads), AN AURA PRODUCTION, HIGH FIDELITY DREAMS, CODE AND DESIGN AEON, and Enigma from VBL 2836.
- **Every frame of the intro is identical to the demo's**, on the whole 416 × 276 frame borders included, from VBL 546 (the ear appears) to VBL 5298 (the last logo): 4,753 frames, verified against the Hatari reference recording.
- The music starts on the same VBL as in the demo, and follows the reference recording as closely as the web port's replay does: their 10 ms envelopes correlate at 0.92 to 0.94 over 40 s, without drifting.
- After the intro, the screen stays black: the text pages, the menu and the song changes remain to be done.

## Building

```bash
tools/setup.sh
```

builds the tools into `tools/.build/` (about 1 min): `vasm`, `ptplayer`, vAmiga's core and `capture`, and a Python venv. Then:

```bash
make
```

writes `build/hfd.adf` from `src/` and the data in [`../../rip/`](../../rip/). `make AUTHENTIC=1` keeps the ear screen as long as in the demo (for the verification); by default it skips whole cycles of LOADING and DECRUNCHING's pulse, as the web port does, but it stays until the disk is loaded.

## Verifying

The Kickstart 1.3 ROM isn't in the repository (it is copyrighted): give its path to `capture`. The reference video comes from "Reference video" in [rip/README.md](../../rip/README.md).

```bash
tools/.build/cmake/capture ~/Kickstart1.3.rom build/hfd.adf 0 5200 build/frames
```

```bash
tools/.build/venv/bin/python tools/compare.py build/frames ../../rip/out/rec1/video.mkv --from 546 --to 5298
```

`capture` records each frame (5,200 frames: about 2 GB) and, in `vbl.txt`, the VBL it shows: the program writes it at $1F0. `compare.py` compares each frame with the reference frame of that VBL, whichever version was built. Two runs give the same frames.

```bash
tools/.build/cmake/capture --audio ~/Kickstart1.3.rom build/hfd.adf 2605 40 build/audio.wav
```

records 40 s of sound from frame 2605 (VBL 2800 in `make AUTHENTIC=1`'s build).

## How it works

The bootblock loads the program's first part (the code and the ear: 51 KB) and runs it; the program takes over the hardware and shows the ear while it loads its second part (the other screens and the module: 286 KB, 13 s) with its own trackloader.

Each VBL, the level 3 interrupt plays the music (ptplayer, one tick per VBL like the demo's replay), then computes the next frame: it advances the intro's VBL, sets each band of lines' fade level, and patches the Copper list that isn't shown. The ST's per-line palettes become Copper lists:
- `tools/mkintro.py` builds, for each screen, a Copper list template that only sets the colours a line shows (colour 0 for the border, and the indices its pixels use), and a patch table: each patched colour faded to the 8 levels, and the band whose level it follows.
- A line's colours change after the screen's last pixel of the line before, colour 0 in the horizontal blank between the right and left borders.
- HIGH FIDELITY DREAMS' rules are the only lines where the demo opens the side borders. Their moving ends always lie in the screen: the CPU redraws them, shifted, in a double-buffered screen, and the Copper paints their parts in the borders by changing colour 0 in the middle of the line.

| File | Role |
|---|---|
| `src/boot.s` | The bootblock: allocates chip memory for the whole program and its BSS, reads the first part with trackdisk, runs it. |
| `src/main.s` | Takes over the hardware, the VBL interrupt, loads the second part, the variables. |
| `src/intro.s` | The intro, as the web port's `scenes/intro.ts`: the VBL, the screens, the bands' levels, the Copper lists' patches, the rules. |
| `src/loader.s` | The trackloader: seeks, reads a track by DMA, decodes its MFM sectors (checksums, read again if one is bad). |
| `tools/mkintro.py` | The screens' bitmaps (4 interleaved bitplanes), the rules' strips, the Copper templates and patches, the pulse table. |
| `tools/mkadf.py` | The 880 KB disk: the bootblock with its checksum, then the program. |
| `tools/capture.cpp` | vAmiga driven frame by frame: frames as PPM (456 × 313, lowres, the whole PAL frame), the VBL shown, sound as WAV. |
| `tools/compare.py` | Our frames (the 416 × 276 frame at (37, 30)) against the reference, in 12-bit colours. |

The frame is placed like Hatari's: the ST screen at DIWSTRT $3b79 (lines 59 to 258, a fetch from $30 delayed by 8 pixels), the 416 × 276 frame from line 30.

## Pitfalls

- **Colours**: Hatari renders a 4-bit component n as n × 17, vAmiga as n × 16. `compare.py` compares the 12-bit values.
- **Frame skipping**: in warp mode, vAmiga only renders one frame in 17 (`DENISE_FRAME_SKIPPING`), like Hatari without `--frameskips 0`. `capture` turns it off.
- **Which frame is captured**: vAmiga's end-of-frame trap stops the CPU at the end of line 312, and the finished frame only becomes the visible texture at the start of line 0. If an instruction straddles the two (an interrupt), the capture would get the following frame. `capture` therefore also runs line 0, and checks the texture's frame number.
- **Sound**: vAmiga mutes the sound in warp mode and while paused, its audio buffer must never run dry (it then clears it and changes the sample rate), and leaving warp mode leaves half the buffer as silence. `capture --audio` runs in VSYNC mode, one frame per wakeup, drops that silence, and only copies the samples that are there.
- **The VBL rate**: a non-interlaced PAL frame has 313 lines, 49.92 Hz (the ST's: 50.05 Hz). The music, one tick per VBL, is 0.27 % slower than on the ST, as the intro is.
- **ptplayer** plays a module's first row on its 6th tick, like ProTracker; the demo's replay on the first: `start_music` sets its counter. It ends with `END`, which would stop the assembly of `main.s`: the Makefile copies it without that line.
- **The trackloader**: DMA starts after the first sync word, so a sector may begin with one sync word instead of two. Two sync words are 1,088 bytes apart, a sector's header and data only 1,080.
- PC-relative addressing reaches 32 KB: the `LEAFAR` macro (`hardware.i`) reaches the data further away.
- Make can't depend on a file whose name contains spaces (the modules).

## Next steps

- The text pages (SPEC.md 4): the 68000 code in `rip/extracted/disasm/` can largely be reused (the screen format changes: interleaved ST planes to Amiga bitplanes). The analysers read the replay's channels in reverse order.
- The menu and the song changes (SPEC.md 5 and 6): the other modules, compressed to fit on the disk (1.16 MB of modules for 880 KB), loaded and decrunched behind the loading screens; the music's master volume for the fade.
- A faster trackloader (it reads a whole revolution per track).
