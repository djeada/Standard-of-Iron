#!/usr/bin/env bash
# Films the Steam store screenshots from the real game, with the full HUD, and
# exports the chosen frames at 1920x1080.
#
#   scripts/capture-steam-screenshots.sh [out-dir] [take...]
#
# Every take is a --film run of a fixture in tools/arena/promos/film/steam/
# (regenerate them with generate_steam_fixtures.py). The run is muted: a
# throwaway profile at master volume 0, and SOI_AUDIO_OFFLINE so no audio device
# opens at all. Takes film on :0 behind every other window. Two things spoil a
# take, and both are guarded here:
#   - another game instance rendering on the same GPU and display. Grabs then
#     come back as unrelated GPU memory, so the script refuses to start while
#     one is running;
#   - the X screensaver blanking an idle display, which leaves the window
#     unexposed. The idle timer is reset for as long as the take runs.
#
# The frame numbers in TAKES are the moments that were picked by eye; the
# simulation is deterministic per build, so the same frame shows the same
# moment on a rerun.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly ROOT
readonly FIXTURES="tools/arena/promos/film/steam"
OUT="${1:-${ROOT}/artifacts/steam/screenshots}"
shift || true

# name | mission | action fixture | fps | seconds | start | frames to export
readonly TAKES=(
  "battle|combined.mission.json|combined.action.json|8|14|3|80"
  "duel|duel.mission.json|duel.action.json|10|15|0|87"
  "oasis|oasis.mission.json|oasis.action.json|8|16|0|0 103"
  "construction|construction.mission.json|construction.action.json|8|16|0|80"
  "winter|winter.mission.json|winter.action.json|8|24|0|100"
)

wanted() {
  [[ $# -eq 0 ]] && return 0
  local take="$1"
  shift
  for name in "$@"; do
    [[ "${name}" == "${take}" ]] && return 0
  done
  return 1
}

if pgrep -f 'bin/standard_of_iron' >/dev/null || pgrep -x arena_app >/dev/null; then
  echo "another game or arena instance is running; its GPU work spoils the grabs:" >&2
  pgrep -af 'bin/standard_of_iron|arena_app' >&2 || true
  exit 1
fi

export SOI_AUDIO_OFFLINE=1
mkdir -p "${OUT}"
WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

for take in "${TAKES[@]}"; do
  IFS='|' read -r name mission fixture fps seconds start frames <<<"${take}"
  wanted "${name}" "$@" || continue
  [[ -n "${frames}" ]] || {
    echo "skip ${name}: no frames picked yet"
    continue
  }

  echo "=== ${name}"
  (
    sleep 10
    while pgrep -f 'bin/standard_of_iron' >/dev/null; do
      xset -display :0 s reset 2>/dev/null || true
      sleep 45
    done
  ) &
  keepawake=$!
  xset -display :0 s reset 2>/dev/null || true

  "${ROOT}/scripts/film-game.sh" "${WORK}/${name}.mp4" \
    --mission-file "${ROOT}/${FIXTURES}/${mission}" \
    --action-fixture "${ROOT}/${FIXTURES}/${fixture}" \
    --fps "${fps}" --seconds "${seconds}" --start "${start}" \
    --size 1920x1080 --keep-frames -- --graphics-preset ultra
  kill "${keepawake}" 2>/dev/null || true

  for frame in ${frames}; do
    src="$(printf '%s/%s.frames/frame_%06d.png' "${WORK}" "${name}" "${frame}")"
    dst="$(printf '%s/%s_%04d.png' "${OUT}" "${name}" "${frame}")"
    cp "${src}" "${dst}"
    echo "  ${dst}"
  done
done
