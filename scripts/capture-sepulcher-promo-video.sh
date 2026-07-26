#!/usr/bin/env bash
# Renders an Iron Sepulcher promo through the real Arena renderer and edits the
# resulting frames with the game's own soundtrack.
#
#   scripts/capture-sepulcher-promo-video.sh [output-dir] [--recapture]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly ROOT
readonly ARENA="${ROOT}/build/bin/arena_app"

OUT_DIR="${ROOT}/artifacts/sepulcher-promo"
RECAPTURE=0
for arg in "$@"; do
  case "${arg}" in
  --recapture) RECAPTURE=1 ;;
  *) OUT_DIR="${arg}" ;;
  esac
done

readonly FRAME_DIR="${OUT_DIR}/frames"
readonly SHOT_DIR="${OUT_DIR}/shots"
readonly WORK="${OUT_DIR}/.work"
readonly PROMO="${OUT_DIR}/iron_sepulcher_promo.mp4"

readonly FPS=24
# Arena capture is intentionally paced at 20 source frames per second. Delivery
# stays at 24 fps, allowing ffmpeg to interpolate timing without accelerating
# a 5.5 second simulation into a sub-three-second shot.
readonly SOURCE_FPS=20
readonly CAPTURE_INTERVAL=0.0417
readonly SCENARIO_SECONDS=5.5
readonly TIME_OF_DAY=afternoon
readonly SHOT_SECONDS=3.2
readonly TRANSITION_SECONDS=0.45
readonly TITLE_SECONDS=2.2
readonly END_SECONDS=2.6

readonly MUSIC="${ROOT}/assets/audio/music/combat/combat_battlefield_escalation.ogg"
readonly MUSIC_START=8

# Scenario id, title, and camera orbit in degrees per second.
readonly SHOTS=(
  "sepulcher_roster_lineup|THE DEAD MUSTER|2"
  "sepulcher_spell_fx_showcase|THE GRAVE PRIEST|1"
  "sepulcher_vs_rome_ranged|CURSED VOLLEY|2"
  "sepulcher_vs_rome_infantry|THE LEGION ADVANCES|2"
  "sepulcher_vs_carthage_cavalry|BREAK THE CHARGE|2"
)

font_file() {
  local candidate
  for candidate in \
    /usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf \
    /usr/share/fonts/dejavu/DejaVuSerif-Bold.ttf \
    /Library/Fonts/Georgia.ttf; do
    if [[ -f "${candidate}" ]]; then
      printf '%s' "${candidate}"
      return 0
    fi
  done
  return 1
}

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ffmpeg is required" >&2
  exit 1
fi
if ! command -v ffprobe >/dev/null 2>&1; then
  echo "ffprobe is required" >&2
  exit 1
fi
FONT="$(font_file)" || {
  echo "no serif font found for titles" >&2
  exit 1
}

capture_frames() {
  if [[ ! -x "${ARENA}" ]]; then
    echo "arena_app not built; build the arena_app target first" >&2
    exit 1
  fi

  rm -rf "${FRAME_DIR}"
  mkdir -p "${FRAME_DIR}"

  local index=0
  local shot scenario title orbit
  for shot in "${SHOTS[@]}"; do
    IFS='|' read -r scenario title orbit <<<"${shot}"
    index=$((index + 1))
    echo "[${index}/${#SHOTS[@]}] Arena capture: ${scenario}"
    # Some combat scenarios can finish their acceptance audit before the
    # capture timer. Any frames already produced remain valid promo footage.
    "${ARENA}" --batch --clean-capture \
      --scenario "${scenario}" \
      --capture-orbit "${orbit}" \
      --duration "${SCENARIO_SECONDS}" \
      --capture-interval "${CAPTURE_INTERVAL}" \
      --time-of-day "${TIME_OF_DAY}" \
      --artifact-dir "${FRAME_DIR}" >/dev/null 2>&1 || true
  done
}

if [[ ${RECAPTURE} -eq 1 || ! -d "${FRAME_DIR}" ]]; then
  capture_frames
else
  echo "reusing ${FRAME_DIR} (pass --recapture to render again)"
fi

rm -rf "${SHOT_DIR}" "${WORK}"
mkdir -p "${SHOT_DIR}" "${WORK}"

# Crop the Arena's instrumentation strip, then use a restrained grade that
# preserves the ivory bones and ember-red spell effects.
readonly SHOT_FILTER_BASE="crop=1236:696:0:70,scale=1920:1080:flags=lanczos,eq=contrast=1.09:saturation=1.04:gamma=0.95,vignette=PI/7"

clips=()
index=0
for shot in "${SHOTS[@]}"; do
  IFS='|' read -r scenario title orbit <<<"${shot}"
  index=$((index + 1))
  frames="${FRAME_DIR}/${scenario}"
  if ! compgen -G "${frames}/frame_*.png" >/dev/null; then
    echo "no frames for ${scenario}; skipping" >&2
    continue
  fi

  clip="${SHOT_DIR}/$(printf '%02d' "${index}")_${scenario}.mp4"
  ffmpeg -y -v error -framerate "${SOURCE_FPS}" \
    -pattern_type glob -i "${frames}/frame_*.png" \
    -vf "${SHOT_FILTER_BASE},tpad=stop_mode=clone:stop_duration=${SHOT_SECONDS},\
drawtext=fontfile=${FONT}:text='${title}':fontcolor=white@0.92:fontsize=44:\
x=76:y=h-132:shadowcolor=black@0.78:shadowx=2:shadowy=2:\
alpha='min(1,min(max(0,t-0.25)/0.45,max(0,${SHOT_SECONDS}-0.25-t)/0.45))'" \
    -t "${SHOT_SECONDS}" -an -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow \
    -r "${FPS}" "${clip}"
  clips+=("${clip}")
done

if [[ ${#clips[@]} -eq 0 ]]; then
  echo "no clips produced" >&2
  exit 1
fi

ffmpeg -y -v error -f lavfi \
  -i "color=c=0x050607:s=1920x1080:r=${FPS}:d=${TITLE_SECONDS}" \
  -vf "drawtext=fontfile=${FONT}:text='THE IRON SEPULCHER':fontcolor=0xDDD3B7:fontsize=94:\
x=(w-text_w)/2:y=(h-text_h)/2-45:alpha='min(1,min(max(0,t-0.25)/0.65,max(0,${TITLE_SECONDS}-0.15-t)/0.55))',\
drawtext=fontfile=${FONT}:text='THE DEAD MARCH FOR CARTHAGE':\
fontcolor=0xA79B80:fontsize=34:x=(w-text_w)/2:y=(h-text_h)/2+70:\
alpha='min(1,min(max(0,t-0.65)/0.55,max(0,${TITLE_SECONDS}-0.15-t)/0.55))'" \
  -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow "${WORK}/title.mp4"

ffmpeg -y -v error -f lavfi \
  -i "color=c=0x050607:s=1920x1080:r=${FPS}:d=${END_SECONDS}" \
  -vf "drawtext=fontfile=${FONT}:text='STANDARD OF IRON':fontcolor=0xDDD3B7:fontsize=76:\
x=(w-text_w)/2:y=(h-text_h)/2-30:alpha='min(1,min(max(0,t-0.35)/0.7,max(0,${END_SECONDS}-0.4-t)/0.7))',\
drawtext=fontfile=${FONT}:text='COMMAND THE IRON SEPULCHER':\
fontcolor=0xA79B80:fontsize=34:x=(w-text_w)/2:y=(h-text_h)/2+72:\
alpha='min(1,min(max(0,t-0.75)/0.7,max(0,${END_SECONDS}-0.4-t)/0.7))'" \
  -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow "${WORK}/end.mp4"

segments=("${WORK}/title.mp4" "${clips[@]}" "${WORK}/end.mp4")
durations=("${TITLE_SECONDS}")
for _ in "${clips[@]}"; do durations+=("${SHOT_SECONDS}"); done
durations+=("${END_SECONDS}")

inputs=()
for segment in "${segments[@]}"; do inputs+=(-i "${segment}"); done

filter=""
previous="[0:v]"
offset="${durations[0]}"
for ((i = 1; i < ${#segments[@]}; ++i)); do
  offset=$(LC_ALL=C awk -v o="${offset}" -v t="${TRANSITION_SECONDS}" \
    'BEGIN{printf "%.3f", o-t}')
  label="[x${i}]"
  filter+="${previous}[${i}:v]xfade=transition=fade:duration=${TRANSITION_SECONDS}:offset=${offset}${label};"
  previous="${label}"
  offset=$(LC_ALL=C awk -v o="${offset}" -v d="${durations[${i}]}" \
    'BEGIN{printf "%.3f", o+d}')
done
filter+="${previous}format=yuv420p[v]"

ffmpeg -y -v error "${inputs[@]}" -filter_complex "${filter}" -map "[v]" \
  -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow -r "${FPS}" \
  "${WORK}/silent.mp4"

total=$(ffprobe -v error -show_entries format=duration \
  -of default=nw=1:nk=1 "${WORK}/silent.mp4")
fade_out_start=$(LC_ALL=C awk -v d="${total}" 'BEGIN{printf "%.2f", d-2.0}')

ffmpeg -y -v error -i "${WORK}/silent.mp4" -ss "${MUSIC_START}" -i "${MUSIC}" \
  -filter_complex "[1:a]aformat=channel_layouts=mono,aresample=48000,\
pan=stereo|c0=c0|c1=c0,loudnorm=I=-14:TP=-1.5:LRA=11,afade=t=in:st=0:d=1.3,\
afade=t=out:st=${fade_out_start}:d=2.0[a]" \
  -map 0:v -map "[a]" -t "${total}" \
  -c:v copy -c:a aac -b:a 192k -movflags +faststart "${PROMO}"

rm -rf "${WORK}"

echo "promo: ${PROMO}"
ffprobe -v error -show_entries format=duration,size:stream=width,height,codec_name \
  -of default=nw=1 "${PROMO}"
