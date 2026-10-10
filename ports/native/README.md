# High Fidelity Dreams — native port

The music disk as a native program for Windows, Linux and macOS, in C11 with [SDL3](https://libsdl.org).
It is a transcription of the [web port](../web/), and it renders exactly what the web port renders: the same frames and the same sound, byte for byte (see [Verification](#verification)).
The data (modules, graphics, the text pages' RAM) is read from [`../../rip/`](../../rip/) at build time and embedded in the executable. The program is a single file with no dependencies.

## Download
Every `v*` tag publishes a GitHub release with the three builds (see [`.github/workflows/native.yml`](../../.github/workflows/native.yml)):
- `hfd-windows-x64.zip`: `hfd.exe`;
- `hfd-linux-x86_64.tar.gz`: `hfd`, for X11 or Wayland, with PipeWire, PulseAudio or ALSA;
- `hfd-macos-universal.zip`: `High Fidelity Dreams.app`, for Apple silicon and Intel, macOS 11 or later. It isn't notarized: the first time, open it with right click > Open.

## Controls
As in the demo:
- SPACE switches from the text pages to the menu, and from the menu to the text pages.
- In the menu, ↑ / ↓ to choose, RETURN to start the song.

Not in the demo:
- The mouse works as in the web port: a click on a text page, or on SELECT SONG, does what SPACE does. Hovering moves the cursor, and a click on a title starts it.
- Full screen: F, Alt+RETURN or a double click. ESC quits.

Options:
- `--authentic`: original loading times, as on the floppy disk (the ear screen then lasts 45 s instead of 6 s, and each song change 18 s instead of 5 s).
- `--menu`: skips the intro and goes straight to the menu (for development).

## Building
You need CMake ≥ 3.21, a C compiler and Python 3 (it converts the data in `rip/`).

```bash
cmake -S . -B build -DHFD_VENDOR_SDL=ON
cmake --build build --config Release
```

`HFD_VENDOR_SDL=ON` downloads SDL 3.4.18 and links it statically, as the release builds do. Without it, CMake uses the system's SDL3 if it finds one.
The executable ends up in `build/` (`build/Release/` with Visual Studio, `build/High Fidelity Dreams.app` on macOS).

On Linux, SDL needs the development packages of the display and sound systems. On Debian or Ubuntu (WSL included):

```bash
sudo apt install cmake ninja-build libasound2-dev libpulse-dev libpipewire-0.3-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev libdbus-1-dev libibus-1.0-dev libudev-dev libwayland-dev wayland-protocols libdecor-0-dev
```

On macOS, a universal build: `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0`.

## How it works
Each file is the port of its web counterpart and keeps its names and comments. See [the web port's README](../web/README.md) for how each part works.

| Native | Web |
|---|---|
| `src/display.c` | `src/st/display.ts`: the virtual ST screen (416×276), per-line palettes, the demo's fade |
| `src/mod.c` | `src/audio/mod.ts`: the ProTracker replay |
| `src/music.c` | `src/audio/music.ts` + `worklet.ts`: an SDL audio stream fed by the replay, the clock of the sound being heard, the ticks heard for the analysers |
| `src/pacing.c` | `src/pacing.ts`: 50.05 Hz VBLs on the display's refreshes, locked to the audio clock |
| `src/intro.c`, `menu.c`, `loading.c`, `textpage.c` | `src/scenes/*.ts` |
| `src/main.c` | `src/main.ts`: window, input, VBL loop |
| `tools/embed.py` | the `?indexed` plugin in `vite.config.ts`: PNG → palette indices, JSON palettes, modules |

Differences with the web port:
- There is no "click or press a key" screen (browsers need a gesture before playing sound): the program starts on the intro.
- The modules are in the executable, so they are always loaded.
- To keep the sound bit-exact, `mod.c` computes in double precision, like JavaScript, and rounds each sample addition to a float, like a `Float32Array`.
- The window keeps the frame's proportions with square pixels (`SDL_SCALEMODE_PIXELART`). It renders at the display's refresh rate with vsync. Without vsync, it waits for the next refresh itself.
- If there is no sound device, SDL's dummy driver runs the music clock in silence.

## Verification
The program has two headless modes, which take the same arguments as the web port's scripts:

```bash
build/hfd --render <intro|menu|text> out.rgb <vbls> [vbl:key ...] [--enigma-ticks n]   # as scripts/render.ts
build/hfd --render-audio module.mod out.wav <seconds> [rate]                            # as scripts/render-audio.ts
```

`scripts/check.sh build/hfd` renders the same things with both ports and compares them byte for byte: the intro (4,753 frames), the menu with a song change (1,396 frames), the text pages (600 frames) and 30 s of each module. It needs the web port's dependencies (`npm ci` in `ports/web`). The Linux CI job runs it.

Results:
- Frames and sound are **identical** to the web port's. This also holds for 5,000 text page frames and 400 s of four modules.
- Against the Hatari recording (`../web/scripts/compare.py`), the intro and the menu show **0 pixels of difference**, as with the web port.
