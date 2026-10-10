# High Fidelity Dreams

*High Fidelity Dreams* is an Atari ST/STE music disk by **Aura** (code and design: Aeon), released in 1992 at the BMT-Party in Essen.
It contains 8 Amiga modules by Phenomena and Sanity.
This project starts from a study of the original floppy disk and ports the demo to other machines.

**Run it in your browser: <https://stuff.github.io/hfd/>**

## Layout

| Folder | Contents |
|---|---|
| [`rip/`](rip/) | Everything extracted from the disk (`hifidrea.st`): the 8 original .mod files, the graphics and the font, the texts, the disassembled 68000 code, and the tools that drive Hatari. See [rip/README.md](rip/README.md). |
| [`rip/SPEC.md`](rip/SPEC.md) | **The specification of the demo's behaviour**, independent of any platform. It is the starting point for any new port. |
| [`ports/web/`](ports/web/) | The browser port, in Vite + TypeScript. **Complete**: intro, text pages, menu, loading screens, music. Verified frame by frame against the demo running in Hatari. Published on [GitHub Pages](https://stuff.github.io/hfd/) on each push to `main`. See [ports/web/README.md](ports/web/README.md). |
| [`ports/native/`](ports/native/) | The native port for Windows, Linux and macOS, in C + SDL3: a transcription of the web port, identical to it byte for byte (frames and sound). A single executable per OS, built by GitHub Actions and published on each `v*` tag. See [ports/native/README.md](ports/native/README.md). |

The ports share no code. What they share is the **data** in `rip/` (modules, bitmaps, palettes, text page RAM) and the specification.
Each port reads this data from `rip/` or copies it at build time, without duplicating it in the repository.

## Next steps
- **Amiga**: the most natural target (50 Hz, Paula, Copper for the rasters, blitter for the spreader). See SPEC.md, section 9. In [`ports/amiga/`](ports/amiga/) (68000 assembly, a bootable disk for an A500 with 1 MB, built and verified on macOS with vasm and vAmiga): the whole demo, identical to it frame by frame (to the web port for the text pages' analysers); still to be tried on a real Amiga.
- **Reference video**: it is not versioned (about 390 MB in `rip/out/`). It can be regenerated in a few minutes, see "Reference video" in rip/README.md. It is needed to verify a port.
