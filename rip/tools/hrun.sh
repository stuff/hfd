#!/bin/bash
# Runs the demo headless in Hatari (STE, 1 MB, EmuTOS) and takes screenshots /
# RAM dumps at given VBL counts.
# usage: hrun.sh <outdir> <run_vbls> "<screenshot vbls>" "<ram dump vbls>" [extra hatari args]
# If <outdir>/pre.ini exists (e.g. key presses from keys.py) it is prepended.
# Example (select "Chinese Dream" and dump its module):
#   mkdir -p out/cd && tools/keys.py out/cd "9000:0x39,9130:0x48,9160:0x48,9400:0x1c"
#   tools/hrun.sh out/cd 13000 "9390 10000" "12990"
T=$(cd "$(dirname "$0")" && pwd)
B=$T/.build
DISK=${DISK:-$(dirname "$T")/hifidrea.st}
D=$(mkdir -p "$1" && cd "$1" && pwd); V=$2; SH=$3; MD=$4; shift 4
: > "$D/run.ini"
[ -f "$D/pre.ini" ] && cat "$D/pre.ini" >> "$D/run.ini"
for v in $SH; do echo "screenshot $D/s_$v.bmp" > "$D/shot_$v.ini"; echo "b VBL=$v :quiet :once :file $D/shot_$v.ini" >> "$D/run.ini"; done
for v in $MD; do echo "savebin $D/m_$v.bin 0 \$100000" > "$D/mem_$v.ini"; echo "b VBL=$v :quiet :once :file $D/mem_$v.ini" >> "$D/run.ini"; done
# SDL "disk" audio driver with no delay = sound is emulated (the demo needs it) but not throttled to real time
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=disk SDL_DISKAUDIODELAY=0 SDL_DISKAUDIOFILE=/dev/null LD_LIBRARY_PATH=$B/local/lib
"$B/src/hatari-2.5.0/build/src/hatari" --confirm-quit 0 --machine ste --memsize 1024 \
  --tos "$B/src/emutos-256k-1.3/etos256uk.img" --disk-a "$DISK" --statusbar off --fast-forward on \
  --sound 44100 --run-vbls "$V" --parse "$D/run.ini" "$@" < /dev/null > "$D/log.txt" 2>&1
