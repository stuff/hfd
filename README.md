# High Fidelity Dreams

*High Fidelity Dreams* is an Atari ST/STE music disk by **Aura** (code and design: Aeon), released in 1992 at the BMT-Party in Essen.
It contains 8 Amiga modules by Phenomena and Sanity.
This project starts from a study of the original floppy disk and ports the demo to other machines.

## Layout

| Folder | Contents |
|---|---|
| [`rip/`](rip/) | Everything extracted from the disk (`hifidrea.st`): the 8 original .mod files, the graphics and the font, the texts, the disassembled 68000 code, and the tools that drive Hatari. See [rip/README.md](rip/README.md). |
| [`rip/SPEC.md`](rip/SPEC.md) | **The specification of the demo's behaviour**, independent of any platform. It is the starting point for any new port. |
| [`ports/web/`](ports/web/) | The browser port, in Vite + TypeScript. **Complete**: intro, text pages, menu, loading screens, music. Verified frame by frame against the demo running in Hatari. See [ports/web/README.md](ports/web/README.md). |

The ports share no code. What they share is the **data** in `rip/` (modules, bitmaps, palettes, text page RAM) and the specification.
Each port reads this data from `rip/` or copies it at build time, without duplicating it in the repository.

## Next steps
- **Windows / Mac**: the shortest path is to package the web port (Tauri or Electron). A native port (C/C++ + SDL2) will have to take over the MOD player and the 50 Hz pacing from the web port. See SPEC.md, section 9.
- **Amiga**: the most natural target (50 Hz, Paula, Copper for the rasters, blitter for the spreader). See SPEC.md, section 9.
- **Reference video**: it is not versioned (about 390 MB in `rip/out/`). It can be regenerated in a few minutes, see "Reference video" in rip/README.md. It is needed to verify a port.
