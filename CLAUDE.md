# High Fidelity Dreams — notes for picking the work back up

Study and ports of the Atari ST music disk *High Fidelity Dreams* (Aura, 1992). All documentation, code comments and commit messages are in English.

## Where to look
- `README.md`: overview and next steps.
- `rip/SPEC.md`: **the demo's behaviour**, measured and platform-independent (VBL timings, fades, menu, text pages, replay, Amiga and Windows/Mac notes). Read it before any port.
- `rip/README.md`: the extraction (disk, music, graphics, 68000 code) and the Hatari tools.
- `ports/web/README.md`: the web port (complete), its architecture and its tests.
- `ports/amiga/README.md`: the Amiga port (68000 assembly): the whole demo (intro, text pages, menu, song changes, the 8 modules on the disk), verified frame by frame in vAmiga against the reference video (and the web port's render for the analysers). The Kickstart 1.3 ROM isn't versioned.
- `ports/native/README.md`: the native port (C + SDL3, Windows/Linux/macOS), a transcription of the web port. `ports/native/scripts/check.sh` checks it byte for byte against the web port: any change to the web port's logic must be carried over to it.

## Method
- The reference is the demo itself, recorded frame by frame in Hatari: `rip/out/rec1/video.mkv` (frame n = VBL n, 416×276 frame, ST screen at (48, 29)). This file is not versioned. To recreate it:
  1. `rip/tools/setup_hatari.sh` (builds Hatari, about 2 min 30 s);
  2. then the three commands under "Reference video" in `rip/README.md`.
- To verify a port, render the scenes offscreen and compare them frame by frame with the video. In the web port: `ports/web/scripts/render.ts` then `scripts/compare.py`, which masks Hatari's drive LED. The intro (4,753 frames) and the menu (1,396 frames) give a 0-pixel difference.
- To understand a routine, read the code rather than guess from the video: Hatari's profiler (`profile on` then `profile save`) gives every executed address, and `rip/tools/rdis.py` disassembles them.

## Known pitfalls
- To record a video, Hatari must run with `--frameskips 0`. Otherwise, in fast-forward mode, it only refreshes the picture once every 6 VBLs.
- Hatari is not deterministic from one run to the next for the music (a 2-row pattern offset was observed). The text page analysers therefore can't be compared frame by frame; everything else can.
- This Hatari build has no libpng: record as BMP AVI (about 21 GB), then convert with `rip/tools/avisplit.py`.
- The demo needs emulated sound, otherwise it hangs while decrunching (`hrun.sh` takes care of it).
- The demo's NoisePacker replay stores its channels in reverse order (analyser band k follows channel 3 − k), and its effect 7 is ProTracker's A.
