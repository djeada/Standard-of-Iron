#!/usr/bin/env bash
# Film the real game -- world and HUD -- headlessly and encode the result.
#
#   scripts/film-game.sh <out.mp4> --mission-file <mission.json> \
#       [--action-fixture <fixture.json>] [--fps 60] [--seconds 8] [--start 0] \
#       [--size 1920x1080] [--display :0|xvfb] [--ui-scale 1.0] [--keep-frames] \
#       [-- <extra game args>]
#
# By default the game films on the current display's GPU in a frameless window
# pinned to the bottom of the window stack that never takes focus, so it stays
# behind whatever else is open; pass --display xvfb for a fully headless
# software-GL run (about 20 s a frame at 1080p on llvmpipe, so only for short
# clips). Frame rate is irrelevant either way because --film steps the
# simulation exactly one frame per grab. A throwaway XDG_CONFIG_HOME keeps the
# run muted, off the user's profile, and -- because the camera otherwise drifts
# for the whole take when the pointer happens to rest against a screen edge --
# with edge scrolling off, and with vsync off: the film steps one frame per grab,
# so vsync buys nothing, and a vsynced swap blocks for good once the monitor
# has gone to sleep, which leaves an unattended take stuck on its loading
# screen. --ui-scale writes the accessibility UI scale into it, which is how a
# HUD panel is filmed big enough to read once the clip is three seconds of a
# trailer rather than a screen someone is sitting in front of.
set -euo pipefail
export LC_ALL=C

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly ROOT
readonly GAME="${ROOT}/build/bin/standard_of_iron"

OUT=""
FPS=60
SECONDS_=8
START=0
SIZE=1920x1080
DISPLAY_MODE="${DISPLAY:-:0}"
KEEP_FRAMES=0
UI_SCALE=1.0
FIXTURE=""
MISSION=()
EXTRA=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --mission-file | --campaign-mission | --observe)
      MISSION+=("$1" "$2")
      shift 2
      ;;
    --action-fixture)
      FIXTURE="$2"
      shift 2
      ;;
    --fps)
      FPS="$2"
      shift 2
      ;;
    --seconds)
      SECONDS_="$2"
      shift 2
      ;;
    --start)
      START="$2"
      shift 2
      ;;
    --size)
      SIZE="$2"
      shift 2
      ;;
    --display)
      DISPLAY_MODE="$2"
      shift 2
      ;;
    --ui-scale)
      UI_SCALE="$2"
      shift 2
      ;;
    --keep-frames)
      KEEP_FRAMES=1
      shift
      ;;
    --)
      shift
      EXTRA=("$@")
      break
      ;;
    -*)
      echo "unknown option $1" >&2
      exit 2
      ;;
    *)
      OUT="$1"
      shift
      ;;
  esac
done
[[ -n "${OUT}" ]] || {
  echo "usage: $0 <out.mp4> --mission-file <file> [...]" >&2
  exit 2
}
[[ ${#MISSION[@]} -gt 0 ]] || {
  echo "a --mission-file, --campaign-mission or --observe is required" >&2
  exit 2
}
[[ -x "${GAME}" ]] || {
  echo "${GAME} is not built" >&2
  exit 2
}
command -v ffmpeg >/dev/null || {
  echo "ffmpeg is required" >&2
  exit 2
}

absolute() { python3 -c 'import os,sys;print(os.path.abspath(sys.argv[1]))' "$1"; }
OUT="$(absolute "${OUT}")"
# The game runs from build/bin, so every path handed to it must be absolute.
# A --campaign-mission is a campaign_id/mission_id pair, not a path.
[[ -n "${FIXTURE}" ]] && FIXTURE="$(absolute "${FIXTURE}")"
for index in "${!MISSION[@]}"; do
  if ((index % 2 == 1)) && [[ "${MISSION[index - 1]}" == "--mission-file" ]]; then
    MISSION[index]="$(absolute "${MISSION[index]}")"
  fi
done
readonly WORK="${OUT%.mp4}.frames"
readonly CFG="${WORK}/.cfg"
rm -rf "${WORK}"
mkdir -p "${WORK}" "${CFG}/djeada"
printf '[audio]\nmaster_volume=0\n[display]\nvsync=false\n[ui]\ncamera_legend_seen=true\neconomy_coach=false\nformation_hints=false\nedge_scroll_enabled=false\nscale=%s\n' \
  "${UI_SCALE}" >"${CFG}/djeada/StandardOfIron.ini"

ARGS=("${MISSION[@]}" --skip-briefing --film "${WORK}" --film-fps "${FPS}"
  --film-seconds "${SECONDS_}" --film-start "${START}" --film-size "${SIZE}")
[[ -n "${FIXTURE}" ]] && ARGS+=(--action-fixture "${FIXTURE}")
ARGS+=("${EXTRA[@]}")

cd "${ROOT}/build/bin"
if [[ "${DISPLAY_MODE}" == "xvfb" ]]; then
  XDG_CONFIG_HOME="${CFG}" xvfb-run -a -s "-screen 0 ${SIZE/x/x}x24" \
    "${GAME}" "${ARGS[@]}" >"${WORK}/game.log" 2>&1 || {
    echo "game exited non-zero; see ${WORK}/game.log" >&2
    grep -E "SOI_FILM|FAIL|CRITICAL" "${WORK}/game.log" | tail -5 >&2
    exit 1
  }
else
  DISPLAY="${DISPLAY_MODE}" XDG_CONFIG_HOME="${CFG}" \
    "${GAME}" "${ARGS[@]}" >"${WORK}/game.log" 2>&1 || {
    echo "game exited non-zero; see ${WORK}/game.log" >&2
    exit 1
  }
fi
cd "${ROOT}"

FRAMES=$(ls "${WORK}"/frame_*.png 2>/dev/null | wc -l)
[[ "${FRAMES}" -gt 0 ]] || {
  echo "no frames written; see ${WORK}/game.log" >&2
  exit 1
}
ffmpeg -v error -y -framerate "${FPS}" -i "${WORK}/frame_%06d.png" \
  -c:v libx264 -preset slow -crf 16 -pix_fmt yuv420p -movflags +faststart "${OUT}"
echo "film-game: wrote ${OUT} (${FRAMES} frames at ${FPS} fps)"
if [[ "${KEEP_FRAMES}" -eq 0 ]]; then
  rm -rf "${WORK}"
fi
