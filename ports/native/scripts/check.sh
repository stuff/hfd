#!/usr/bin/env bash
# Checks that the native port renders exactly what the web port renders: the same
# frames (intro, menu, text pages) and the same audio (the 8 modules), byte for byte.
# The web port is itself checked against the Hatari recording (see ports/web/README.md).
#
# usage: scripts/check.sh <hfd executable> [seconds of audio per module, default 30]
# Needs node with ports/web's dependencies installed (npm ci in ports/web).
set -euo pipefail

HFD=$(realpath "$1")
SECONDS_AUDIO=${2:-30}
HERE=$(cd "$(dirname "$0")/.." && pwd)
WEB=$HERE/../web
RIP=$HERE/../../rip
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fail=0

compare() { # name, web file, native file
  if cmp -s "$2" "$3"; then echo "ok    $1"
  else echo "FAIL  $1: $(cmp "$2" "$3" 2>&1 | head -1)"; fail=1; fi
}

frames() { # name, vbls, extra args...
  local name=$1 vbls=$2; shift 2
  (cd "$WEB" && npx vite-node scripts/render.ts -- "$name" "$TMP/web-$name.rgb" "$vbls" "$@" > /dev/null)
  "$HFD" --render "$name" "$TMP/native-$name.rgb" "$vbls" "$@" > /dev/null
  compare "$name ($vbls frames)" "$TMP/web-$name.rgb" "$TMP/native-$name.rgb"
}

frames intro 4753
frames menu 1396 127:ArrowDown 276:Enter
ENIGMA_TICKS=2471 frames text 600

for mod in "$RIP"/music/*.mod; do
  base=$(basename "$mod" .mod)
  (cd "$WEB" && npx vite-node scripts/render-audio.ts -- "$mod" "$TMP/web.wav" "$SECONDS_AUDIO" > /dev/null)
  "$HFD" --render-audio "$mod" "$TMP/native.wav" "$SECONDS_AUDIO" > /dev/null
  compare "$base (${SECONDS_AUDIO} s)" "$TMP/web.wav" "$TMP/native.wav"
done

exit $fail
