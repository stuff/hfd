# High Fidelity Dreams — Amiga port

**Status: proof of concept.** A bootable disk for the Amiga 500 (OCS, Kickstart 1.3) that shows the ear screen with LOADING pulsing ([SPEC.md](../../rip/SPEC.md) 3.1) and plays *Act of Impulse* on Paula. Its purpose is to validate the tools and the verification method before the actual port:

- The disk is built on macOS (or Linux) with `vasm`, and boots in [vAmiga](https://github.com/dirkwhoffmann/vAmiga) without any OS: a bootblock loads the program, which takes over the hardware.
- `tools/capture` records it frame by frame, and `tools/compare.py` compares the frames with the Hatari reference recording: **372 frames out of 372 identical** to the demo (reference VBLs 600 to 971, the whole LOADING phase after the fade-in), music playing.
- The music (ProTracker replay [ptplayer](https://aminet.net/package/mus/play/ptplayer), CIA-B timer) is recorded too, and follows the web port's replay: their 10 ms envelopes correlate at 0.93 over 15 s, without drifting.

## Building

```bash
tools/setup.sh
```

builds the tools into `tools/.build/` (about 1 min): `vasm`, `ptplayer`, vAmiga's core and `capture`, and a Python venv for `compare.py`. Then:

```bash
make
```

writes `build/hfd.adf`, from `src/boot.s`, `src/main.s` and the data in [`../../rip/`](../../rip/).

## Verifying

The Kickstart 1.3 ROM isn't in the repository (it is copyrighted): give its path to `capture`.
Then capture the frames and compare them with the reference video (see "Reference video" in [rip/README.md](../../rip/README.md)):

```bash
tools/.build/cmake/capture ~/Kickstart1.3.rom build/hfd.adf 619 372 build/frames
```

```bash
tools/.build/venv/bin/python tools/compare.py build/frames 619 ../../rip/out/rec1/video.mkv 600 372
```

Frame n is VBL n after power-on. The program shows its first frame at VBL 566 (the boot and loading take 11 s); 619 is the frame that matches the reference's VBL 600. Two runs give the same frames.

```bash
tools/.build/cmake/capture --audio ~/Kickstart1.3.rom build/hfd.adf 600 20 build/audio.wav
```

records 20 s of sound from VBL 600.

## How it works

| File | Role |
|---|---|
| `src/boot.s` | The bootblock: allocates chip memory, reads `main.bin` from the following sectors with trackdisk, stops the motor and jumps to it. |
| `src/main.s` | The program, position independent. It cuts off the system (interrupts, DMA), sets up a Copper list (the 4 bitplanes, the palette and, on lines 189 to 199, colours 13 to 15 for LOADING), starts ptplayer, then each VBL writes the next colour of the pulse into the Copper list. |
| `tools/mkdata.py` | The ear (`loading_screen_indexed.png`) as 4 bitplanes, its palette and the pulse table (the web port's formula, `intro.ts`). |
| `tools/mkadf.py` | The 880 KB disk: the bootblock with its checksum, then the program. |
| `tools/capture.cpp` | vAmiga driven frame by frame: A500 OCS, 512 KB chip + 512 KB slow, frames as PPM (456 × 313, lowres, the whole PAL frame), sound as WAV. |
| `tools/compare.py` | The ST screen (320 × 200) compared with the same area of our frames, in 12-bit colours. |

## Pitfalls

- **Colours**: Hatari renders a 4-bit component n as n × 17, vAmiga as n × 16. `compare.py` compares the 12-bit values.
- **Frame skipping**: in warp mode, vAmiga only renders one frame in 17 (`DENISE_FRAME_SKIPPING`), like Hatari without `--frameskips 0`. `capture` turns it off.
- **Which frame is captured**: vAmiga's end-of-frame trap stops the CPU at the end of line 312, and the finished frame only becomes the visible texture at the start of line 0. If an instruction straddles the two (an interrupt, here the music's), the capture would get the following frame. `capture` therefore also runs line 0, and checks the texture's frame number.
- **Sound**: vAmiga mutes the sound in warp mode and while paused, and its audio buffer must never run dry (it then clears it and changes the sample rate). `capture --audio` runs in VSYNC mode, one frame per wakeup, and only copies the samples that are there. The first 0.2 s fade in.
- `ptplayer.asm` ends with `END`, which would stop the assembly of `main.s`: the Makefile copies it without that line.
- Make can't depend on a file whose name contains spaces (the modules).

## Next steps

The tools and the method are ready; the port itself remains (SPEC.md 9): the intro's screens, the menu, the loading screens, the text pages with the analysers (the replay's channel state, in reverse order), song changes, and the HIGH FIDELITY DREAMS rules, which need an overscan display. The music should then be played from the VBL, like the demo does, so that the analysers stay in step with the frames.
