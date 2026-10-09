#!/bin/bash
# Builds a headless Hatari 2.5.0 (+ "key" debugger command patch), SDL2 and
# fetches EmuTOS 1.3, all inside tools/.build (no root needed).
# Needs: gcc, make, curl, unzip, python3 + pip (for cmake).
set -e
T=$(cd "$(dirname "$0")" && pwd)
B=$T/.build
mkdir -p "$B/src" && cd "$B/src"

[ -f sdl2.tar.gz ]   || curl -sL -o sdl2.tar.gz   https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz
[ -f hatari.tar.gz ] || curl -sL -o hatari.tar.gz https://github.com/hatari/hatari/archive/refs/tags/v2.5.0.tar.gz
[ -f emutos.zip ]    || curl -sL -o emutos.zip    https://sourceforge.net/projects/emutos/files/emutos/1.3/emutos-256k-1.3.zip/download

if [ ! -f "$B/local/lib/libSDL2.so" ]; then
  tar xzf sdl2.tar.gz && cd SDL2-2.30.9
  ./configure --prefix="$B/local" --disable-video-x11 --disable-video-wayland --disable-pulseaudio \
    --disable-alsa --disable-pipewire --disable-jack --disable-video-opengl --disable-video-vulkan \
    --disable-video-kmsdrm > /dev/null
  make -j"$(nproc)" > /dev/null && make install > /dev/null
  cd ..
fi

[ -x "$B/pycmake/bin/cmake" ] || pip install -q --target "$B/pycmake" cmake

if [ ! -x "$B/src/hatari-2.5.0/build/src/hatari" ]; then
  rm -rf hatari-2.5.0 && tar xzf hatari.tar.gz
  patch -p1 < "$T/hatari-debugger-key.patch"
  mkdir -p hatari-2.5.0/build && cd hatari-2.5.0/build
  PATH="$B/pycmake/bin:$PATH" PYTHONPATH="$B/pycmake" cmake .. -DCMAKE_PREFIX_PATH="$B/local" -DCMAKE_BUILD_TYPE=Release > /dev/null
  make -j"$(nproc)" hatari > /dev/null
  cd ../..
fi

[ -f emutos-256k-1.3/etos256uk.img ] || unzip -qo emutos.zip
echo "OK: $B/src/hatari-2.5.0/build/src/hatari"
