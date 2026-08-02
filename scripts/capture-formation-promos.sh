#!/usr/bin/env bash
# Record and cut the three formation showcase reels.
#
#   scripts/capture-formation-promos.sh [output-dir] [--edit-only]
#
# Each reel is one faction's army put through the army formation layer's own
# vocabulary -- column, line, defensive, assault, encirclement -- rather than a
# battle. See docs/PROMO_CAPTURE.md.
#
# Needs a working display (the Arena renders through OpenGL, not offscreen) and
# ffmpeg. Raw clips are kept under <output-dir>/clips so the edit can be redone
# without re-rendering; pass --edit-only to skip the capture entirely.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly ROOT
readonly ARENA="${ROOT}/build/bin/arena_app"

OUT_DIR="${ROOT}/artifacts/promo-formations"
EDIT_ONLY=0
for arg in "$@"; do
  case "${arg}" in
    --edit-only) EDIT_ONLY=1 ;;
    *) OUT_DIR="${arg}" ;;
  esac
done
readonly CLIP_DIR="${OUT_DIR}/clips"

readonly REELS=(
  rome_iron_line
  carthage_crescent
  rome_hill_drill
)

if [[ ${EDIT_ONLY} -eq 0 && ! -x "${ARENA}" ]]; then
  echo "capture-formation-promos: build arena_app first (cmake --build build --target arena_app)" >&2
  exit 1
fi

mkdir -p "${CLIP_DIR}"
for reel in "${REELS[@]}"; do
  spec="${ROOT}/tools/arena/promos/${reel}.json"
  if [[ ${EDIT_ONLY} -eq 0 ]]; then
    echo "== recording ${reel}"
    DISPLAY="${DISPLAY:-:0}" "${ARENA}" --promo-spec "${spec}" --promo-out "${CLIP_DIR}"
  fi
  echo "== cutting ${reel}"
  # Invoked through python3 rather than the shebang: promo-edit.py is tracked
  # without its executable bit.
  python3 "${ROOT}/scripts/promo-edit.py" \
    --spec "${spec}" \
    --clips "${CLIP_DIR}/${reel}" \
    --out "${OUT_DIR}/${reel}.mp4"
done

echo "capture-formation-promos: wrote ${#REELS[@]} reel(s) to ${OUT_DIR}"
