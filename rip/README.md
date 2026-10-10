# High Fidelity Dreams (Aura, 1992) — study of the floppy disk (`rip/`)

This folder holds everything extracted from the disk: music, graphics, texts, disassembled code and extraction tools.
The ports themselves are in [`../ports/`](../ports/).

*High Fidelity Dreams* is an Atari ST/STE music disk by **Aura** (code and design: Aeon, protection: Chris),
released on 5.6.1992 at the BMT-Party in Essen. It contains 8 Amiga modules
(Phenomena / Sanity) played through a software emulation of the Amiga's Paula sound chip ("HiFi-Paula",
16 kHz on ST and 50 kHz DMA sound on STE). The modules are stored in NoisePacker 2 format and compressed.

The disk `hifidrea.st` (800 KB, 80 tracks, 2 sides, 10 sectors) has **no file system**.
It boots from an encrypted boot sector that loads the following stages by driving the floppy controller directly.
Everything was extracted by running the demo in Hatari (STE, 1 MB, EmuTOS), headless and script-driven.

## 1. The music (`music/`)

As agreed, these are the **original .mod files** (Modland / ModArchive), not reconversions of the disk's data.
For each song, I had the demo play it, then compared the module unpacked in RAM ($26000) with the candidates found online:
notes of every pattern, play order and samples compared byte by byte (`tools/np2_compare.py`).

| # | Demo menu | Authors* | Chosen file | Source | Match with the disk |
|---|---|---|---|---|---|
| 1 | Act of Impulse | Mantronix & Tip | `01 - Act of Impulse …mod` | ModArchive #65381 (`actofimp.mod`) | identical (the Modland version differs: 7 patterns and 1 sample change) |
| 2 | Chinese Dream | Firefox & Tip | `02 - Chinese Dream …mod` | Modland `Firefox/chinese dream.mod` | identical |
| 3 | Elysium | Jester (Sanity) | `03 - Elysium (Jester).mod` | Modland `Jester/elysium.mod` (FLT4/StarTrekker format) | identical |
| 4 | Enigma | Firefox & Tip | `04 - Enigma aka Hyperbased …mod` | Modland `Firefox/coop-Tip/hyperbased.mod` | identical; it is the music of Phenomena's *Enigma* demo (it is also used as the intro music) |
| 5 | Just Spank It | Firefox & Tip | `05 - Just Spank It …mod` | Modland `Firefox/coop-Tip/yeaah just spank it.mod` | identical (Modland's `just spank it.mod` is another version) |
| 6 | Macrocosm | Firefox | `06 - Macrocosm (Firefox) [repaired].mod` | Modland `Firefox/macrocosm.mod`, **repaired** | see below |
| 7 | Overload | Mantronix & Tip | `07 - Overload …mod` | Modland `Mantronix/coop-Tip/overload.mod` | identical, except 6 bytes in sample 29 |
| 8 | The Sweat Shop | Mantronix & Tip | `08 - The Sweat Shop …mod` | Modland `Mantronix/coop-Tip/the sweat shop.mod` | identical |

\* The authors come from Demozoo and Modland. The credits shown by the demo sometimes differ (it puts "by Tip and Mantronix" on almost everything).

"Identical" means that the notes of every pattern used, the play order and all the samples are the same.
The only remaining differences come from the NoisePacker encoding: renumbered patterns, effect 8 removed, volume-slide parameters rewritten.

**Macrocosm**: the Modland file is a faulty rip. Each sample in it is shifted by 2 bytes more than the previous one,
up to 52 bytes for the last one, so that each sample starts with the end of the previous one.
The repaired version realigns the samples of the Modland file itself and fills in the 52 missing bytes at the end of the last sample.
These 52 bytes are the only data taken from the disk.
The unmodified original is in `music/sources/modland/`.

`music/sources/` contains all the downloaded candidates, including the rejected variants
(a final hyperbase, the final hyperbase, just spank it, the Modland version of act of impulse, another "overload").

## 2. Graphics, fonts, texts and screens

- `graphics/` contains the **original graphics**, read back pixel by pixel from the emulated demo's video memory.
  It has the loading screen (the ear), the 3 logos, the whole menu and each menu word, the text-spreader's 8×8 font, the bubble and the anti-cartridge message.
  Each image exists as an indexed PNG (original pixels), as an "as displayed" PNG (rasters included) and as JSON (palettes, including line by line).
  The full screens are also available as Degas `.pi1`.
  Details are in [graphics/README.md](graphics/README.md) and the overview in `graphics/overview.png`.

- `texts/hfd_texts.txt` contains the demo's full text, decoded. The "text-spreader" stores every other line reversed, and the font uses `[`→`-`, `;`→`.`, `=`→`!`, `@`→block.
  It contains the credits, the members of Aura, the greetings, two party reports (Shining 8, Black Sausages) and technical info:
  1,041,122 bytes of modules compressed into 696,357 bytes, 37:16 of music, a "Twins of Phenomena" NoisePacker replay rewritten by Aeon (5 scanlines at most),
  4 "vector-quaders" with voice analysers (these are the 4 bands rotating behind the text), and 32 8×8 bobs for the spreader.
  The boot also holds a hidden message: "HI INTRUDER ! … DARE TO CRACK IT … SPECIALLY DEDICATED TO TINA M.".
- `screens/*.png` contains 13 screenshots at native resolution (borders included): loading (the ear, "by Aeon"), decrunching, AURA logos, title, CODE AND DESIGN AEON,
  text pages with rasters, credits, song selection menu and decrunching of a song.
  The screenshots don't hold the per-line palettes (rasters) separately: they show the final picture.

## 3. Code (`extracted/`)

### Boot chain and protection
1. **Boot sector** (`T0 S0 s1`). The code at $1E XOR-decrypts $132 bytes written **over the exception vectors** ($8–$13A), then jumps to $10.
   It sets the palette to black and sends the "pause" command to the keyboard.
   For the address computations to work out, the code assumes that `$4CE` (vbl_list) contains `$01F8` / `$0000`.
   `tools/decrypt_boot.py` reproduces this decryption.
2. **Stage 1** (at $10). It fills RAM up to 4 MB, programs the DMA and the WD1772 controller by hand, reads `T0 S0 s8` to $500, decrypts it and jumps to $400.
3. **Stage 2** ($400). It checks for a cartridge at $FA0000 (anti-debugger) and shows "DISCONNECT MODULE!" if it finds one, then reads `T0 S0 s7` to $200.
4. **Stage 3** ($200). It redirects **all** exceptions to $272 (trap), reads 36 sectors to $70000 and jumps there.
5. **Resident** ($70000–$74800). This is the loader and depacker ("JAM and Aeon-pre-pack"), with the ear screen and the Timer B at $70564.
   It loads the packed main program to $3E00 and the Enigma module to $25E00, then unpacks.
6. **Main program**, entry point at **$4000**. VBL at $4532, HBL at $4282, TRAP #0 at $44D8.
   The current module is unpacked at **$26000** in NoisePacker 2 format, with its sample pointers relocated and its loops pre-unrolled for the mixer.
   The screens are around $58000–$7FFFF and the mixer tables at $74000/$7C000.

### Disk map (recorded with an FDC trace in Hatari)
| Contents | Track/side/sector | Offset in the .st | Sectors | Loaded at |
|---|---|---|---|---|
| boot sector | T0 S0 s1 | $000000 | 1 | (TOS buffer) |
| stage 3 | T0 S0 s7 | $000C00 | 1 | $200 |
| stage 2 | T0 S0 s8 | $000E00 | 1 | $500 → $400 |
| Act of Impulse | T0 S1 s5 | $001C00 | 102 | $25E00 |
| Chinese Dream | T5 S1 s7 | $00E800 | 104 | $25E00 |
| Elysium | T11 S0 s1 | $01B800 | ~179 | $25E00 |
| Enigma | T19 S1 s9 | $031C00 | 234 | $25E00 |
| Just Spank It | T31 S1 s3 | $04F000 | ~119 | $25E00 |
| Macrocosm | T37 S1 s1 | $05DC00 | ~203 | $25E00 |
| Overload | T47 S1 s3 | $077000 | ~236 | $25E00 |
| The Sweat Shop | T59 S0 s7 | $094400 | 200 | $25E00 |
| main program (packed) | T69 S0 s7 | $0AD400 | 173 | $3E00 |
| resident loader / depacker | T77 S1 s9 | $0C2C00 | 36 | $70000 |

The raw log of every sector read is in `extracted/fdc_reads_raw.txt`.

### Files
- `extracted/memory/` contains raw RAM images (address 0 = start of file):
  decrypted boot, stage 2, RAM at the resident's entry ($70000), RAM at the main program's entry ($4000, unpacked code), and RAM in the menu (VBL 9000).
- `extracted/disasm/*.s` contains recursive 68000 **disassemblies**, with labels, named hardware registers (`mfp_*`, `snd_*`, `dma_*`…) and data areas as `dc.b`:
  `stage1_bootsector.s`, `stage2_0400.s`, `stage3_0200.s`, `resident_70000.s` (557 instructions), `main_4000.s` (1438 instructions),
  and `textpage_4000.s`, the main program as it sits in RAM during the text pages, disassembled from every executed address (see `extracted/textpage/`).
  This is **not** a decompilation to C. Coverage of the main program is partial:
  routines called through tables (`jmp (a0)`) or by self-generated code (the Paula mixer generates its code at $74000) remain as `dc.b`.
  Entry points can be added by rerunning `tools/rdis.py`.

## 4. Reproducing / continuing (`tools/`)

All the commands below are run from the `rip/` folder.

```bash
tools/setup_hatari.sh
```
This script builds SDL2 and Hatari 2.5.0 and downloads EmuTOS 1.3 into `tools/.build`, without root (about 2 min 30 s).
It applies `hatari-debugger-key.patch`, which adds a `key <scancode> d|u` command to Hatari's debugger to drive the menu to the exact VBL.

```bash
mkdir -p out/cd && tools/keys.py out/cd "9000:0x39,9130:0x48,9160:0x48,9400:0x1c"
```
These presses are SPACE, UP twice, then RETURN, which selects Chinese Dream.

```bash
tools/hrun.sh out/cd 13000 "9390 10000" "12990"
```
This command takes screenshots at VBLs 9390 and 10000 and a RAM dump at VBL 12990, in about 30 s.

```bash
python3 tools/np2_compare.py out/cd/m_12990.bin music/*.mod
```

Other tools:
- `schedule.py`: schedules key presses and captures (video address, BMP, RAM) by chaining breakpoints, which keeps the emulation fast.
- `stscreen.py`, `export_pic.py` and `memview.py`: graphics extraction.
- `fdcmap.py`: map of disk reads from `--trace fdc`.
- `rdis.py`: recursive disassembler, using capstone from `tools/.build/pylib`.
- `modinfo.py`.
- `widegrab.py`: rebuilds the full-width version (416 px, borders included) of a screen that draws into the side borders, from a reference frame and the indexed image.
  That is how the `graphics/pictures/fullscreen/*_wide_indexed.png` files were produced: the HIGH FIDELITY DREAMS rules spill into the borders.
- `fadescan.py`: gives, VBL by VBL, the fade level of each band of lines of a screen. That is how the intro's fade-in effects were measured.
- `palscan.py`: reads back, VBL by VBL, the palette of each line from a known indexed image.
- `extracted/textpage/`: RAM from $12000 to $25B00 just before the text pages (VBL 5280), used as is by the web port.
  It also holds Hatari's CPU profile over 300 VBLs of text pages: every executed instruction, with its execution count.
  `extracted/disasm/textpage_4000.s` is the disassembly obtained by taking all these addresses as entry points.
  The text pages work as follows:
  - the 4 bands are precomputed rasters: 12 Timer B interrupts, whose colours and heights are read from 26-byte records, walked by a script;
  - the text-spreader flies 33 letters from the bubble to their place, in 64 VBLs;
  - the analysers show stripes on planes 3 and 0, retriggered at the channel's volume.
- `avisplit.py`: converts Hatari's raw AVI (see below) into a compact lossless video (`video.mkv`, FFV1) and an `audio.wav`.

### Reference video (for verifying ports)
Hatari can record the demo frame by frame, sound included. This build has no libpng, so the BMP codec is needed: the raw AVI weighs about 21 GB for 16,000 VBLs. `avisplit.py` reads it even when its header is broken beyond 4 GB.
```bash
tools/schedule.py out/rec1 "9000:0x39,9150:0x50,9300:0x1c" "15990"
tools/hrun.sh out/rec1 16000 "" "" --frameskips 0 --avirecord --avi-vcodec bmp --avi-file $PWD/out/rec1/demo.avi
tools/avisplit.py out/rec1/demo.avi out/rec1 && rm out/rec1/demo.avi
```
This gives the complete intro, the menu (SPACE at VBL 9000), DOWN at VBL 9150 and RETURN at VBL 9300 (loading Just Spank It), at 50 Hz.
Frame n of `video.mkv` corresponds to VBL n. `--frameskips 0` is essential: otherwise, in fast-forward mode, Hatari only refreshes the picture once every 6 VBLs.
`palscan.py` reads back, VBL by VBL, the palette of each line from a known indexed image. The frame is 416×276, and the ST screen starts at (48, 29).
The `out/` folder is not versioned.

Note: the demo needs sound to be emulated. With `--sound off`, it hangs while decrunching.
`hrun.sh` therefore uses SDL's "disk" audio driver with no delay, which keeps sound emulated without locking the speed to real time.

## 5. For a port
The demo's complete behaviour (VBL timings, fades, menu colours, how the text pages work, replay quirks, Amiga and Windows/Mac notes) is described in **[SPEC.md](SPEC.md)**.
The web port (`../ports/web/`) is a complete implementation of it, verified against the reference video.
