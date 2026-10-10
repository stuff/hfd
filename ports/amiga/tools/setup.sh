#!/bin/bash
# Fetches and builds the Amiga port's tools inside tools/.build (no root needed):
# - vasm (68000 assembler; tested with 2.0f, the site only serves the latest release);
# - ptplayer 6.4 (Frank Wille's ProTracker replay, public domain);
# - ZX0 (Einar Saukas' compressor) at a fixed commit, for the disk's files;
# - vAmiga's core at a fixed commit, and capture (capture.cpp) linked to it;
# - a Python venv with numpy, Pillow and PyAV for compare.py.
# Needs: a C/C++ compiler (C++20), make, cmake, curl, git, bsdtar (for the .lha), python3.
set -e
T=$(cd "$(dirname "$0")" && pwd)
B=$T/.build
VAMIGA_COMMIT=899da5ddd872dfb2e9e35e80247cbb2b36086524
ZX0_COMMIT=ecde3a2ae05061fe06469ed46df81a33b7de7d86
mkdir -p "$B/src" && cd "$B/src"

[ -f vasm.tar.gz ]  || curl -sfL -o vasm.tar.gz http://sun.hasenbraten.de/vasm/release/vasm.tar.gz
[ -f ptplayer.lha ] || curl -sfL -o ptplayer.lha https://aminet.net/mus/play/ptplayer.lha

if [ ! -x vasm/vasmm68k_mot ]; then
  tar xzf vasm.tar.gz
  make -C vasm CPU=m68k SYNTAX=mot -j8 > /dev/null 2>&1  # quiet: its sources warn a lot
fi

[ -f ptplayer/ptplayer.asm ] || (mkdir -p ptplayer && cd ptplayer && bsdtar -xf ../ptplayer.lha)

if [ ! -d ZX0 ]; then
  git clone -q https://github.com/einar-saukas/ZX0.git
fi
git -C ZX0 checkout -q "$ZX0_COMMIT"
[ -x ZX0/src/zx0 ] || cc -O2 -o ZX0/src/zx0 ZX0/src/zx0.c ZX0/src/optimize.c ZX0/src/compress.c ZX0/src/memory.c

if [ ! -d vAmiga ]; then
  git clone -q https://github.com/dirkwhoffmann/vAmiga.git
fi
git -C vAmiga checkout -q "$VAMIGA_COMMIT"

cmake -S "$T" -B "$B/cmake" -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$B/cmake" --target capture -j8 > /dev/null

[ -x "$B/venv/bin/python" ] || python3 -m venv "$B/venv"
"$B/venv/bin/pip" install -q numpy pillow av

echo "OK: $B/cmake/capture"
