#!/usr/bin/env bash
# Rebuild the cinematic trailer end to end.
#
#   scripts/trailer/build.sh [capture|picture|sound|master|all]   (default: all)
#
# capture  records every capture spec in tools/arena/promos/cinematic/ with the
#          arena on the real GPU at Ultra (DISPLAY must be a hardware display);
# picture  conforms the graded, matted picture from the clips (conform.py);
# sound    builds score, effects and master mix (mix.py);
# master   muxes and encodes the delivery MP4 and runs the delivery checks.
#
# Environment: ARENA (default build/bin/arena_app), PYTHON (needs numpy, scipy,
# soundfile, pyloudnorm, pillow; see scripts/trailer/requirements.txt),
# OUT (default artifacts/trailer).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARENA="${ARENA:-$ROOT/build/bin/arena_app}"
PYTHON="${PYTHON:-python3}"
OUT="${OUT:-$ROOT/artifacts/trailer}"
SPECS="$ROOT/tools/arena/promos/cinematic"
CUT="$SPECS/cut.json"
STAGE="${1:-all}"

capture() {
  for spec in "$SPECS"/capture_*.json; do
    name="$(basename "$spec" .json)"
    echo "== capture $name"
    PULSE_SERVER="${PULSE_SERVER:-unix:/nonexistent}" \
      "$ARENA" --promo-spec "$spec" --promo-out "$OUT/clips" || {
        # the arena dumps core after a completed capture; trust the manifest
        id="$("$PYTHON" -c 'import json,sys; print(json.load(open(sys.argv[1]))["id"])' "$spec")"
        test -s "$OUT/clips/$id/shots.json" || exit 1
      }
  done
}

picture() {
  "$PYTHON" "$ROOT/scripts/trailer/conform.py" --cut "$CUT" --clips "$OUT/clips" \
    --work "$OUT/work" --out "$OUT/picture.mov"
}

sound() {
  "$PYTHON" "$ROOT/scripts/trailer/mix.py" --cut "$CUT" --clips "$OUT/clips" \
    --out "$OUT/mix.wav" --stems
}

master() {
  "$PYTHON" "$ROOT/scripts/trailer/deliver.py" --picture "$OUT/picture.mov" \
    --mix "$OUT/mix.wav" --cut "$CUT" --out "$OUT/standard_of_iron_trailer.mp4"
}

case "$STAGE" in
  capture) capture ;;
  picture) picture ;;
  sound) sound ;;
  master) master ;;
  all) capture; picture; sound; master ;;
  *) echo "unknown stage $STAGE" >&2; exit 2 ;;
esac
