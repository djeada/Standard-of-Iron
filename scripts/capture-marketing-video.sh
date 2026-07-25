#!/usr/bin/env bash
# Renders the settlement and fortification Arena scenarios through the real
# renderer and cuts them into a promo reel with the game's own soundtrack.
#
#   scripts/capture-marketing-video.sh [output-dir] [--recapture]
#
# Needs a working display (the Arena renders through OpenGL, not offscreen) and
# ffmpeg. Frames are kept in <output-dir>/frames so the edit can be rebuilt
# without re-rendering; pass --recapture to render them again.
set -euo pipefail

readonly ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly ARENA="${ROOT}/build/bin/arena_app"

OUT_DIR="${ROOT}/artifacts/marketing"
RECAPTURE=0
for arg in "$@"; do
  case "${arg}" in
  --recapture) RECAPTURE=1 ;;
  *) OUT_DIR="${arg}" ;;
  esac
done

readonly FRAME_DIR="${OUT_DIR}/frames"
readonly SHOT_DIR="${OUT_DIR}/shots"
readonly PROMO="${OUT_DIR}/standard_of_iron_promo.mp4"
readonly WORK="${OUT_DIR}/.work"

readonly FPS=30
readonly CAPTURE_INTERVAL=0.0333
readonly SCENARIO_SECONDS=6
readonly TIME_OF_DAY=afternoon

# Each shot holds for SHOT_SECONDS and dissolves into the next over
# TRANSITION_SECONDS, so the reel runs
# n*SHOT_SECONDS - (n-1)*TRANSITION_SECONDS plus the cards.
readonly SHOT_SECONDS=3.6
readonly TRANSITION_SECONDS=0.6
readonly TITLE_SECONDS=2.6
readonly END_SECONDS=3.0

# The menu theme swells from roughly its thirtieth second, which puts the peak
# on the closing shots. Trimmed, loudness-normalised and faded in the mix below.
readonly MUSIC="${ROOT}/assets/audio/music/menu/main_theme_rome_vs_carthage.ogg"
readonly MUSIC_START=30

# Scenario id, on-screen title, and orbit speed in degrees per second. Wide
# settlements drift more slowly so they stay inside the frame.
readonly SHOTS=(
  "roman_fortification_showcase|Roman Timber Fortification|5"
  "carthage_fortification_showcase|Carthaginian Dread Palisade|5"
  "roman_marching_camp|Roman Marching Camp|5"
  "carthage_trade_town|Carthaginian Trade Town|5"
  "architecture_and_props_showcase|Architecture and Props|5"
  "rival_economies|Rival Economies|2"
  "wall_corner_showcase|Fortifications That Join Cleanly|2"
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
FONT="$(font_file)" || {
  echo "no serif font found for shot titles" >&2
  exit 1
}

capture_frames() {
  if [[ ! -x "${ARENA}" ]]; then
    echo "arena_app not built; run: make build" >&2
    exit 1
  fi
  rm -rf "${FRAME_DIR}"
  mkdir -p "${FRAME_DIR}"

  local index=0
  local shot scenario title orbit
  for shot in "${SHOTS[@]}"; do
    IFS='|' read -r scenario title orbit <<<"${shot}"
    index=$((index + 1))
    echo "[${index}/${#SHOTS[@]}] rendering ${scenario}"
    # The capture timer outruns the scenario watchdog, which reports a timeout
    # after the frames are already on disk; the footage is unaffected.
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
  echo "reusing frames in ${FRAME_DIR} (pass --recapture to re-render)"
fi

rm -rf "${SHOT_DIR}" "${WORK}"
mkdir -p "${SHOT_DIR}" "${WORK}"

# Crop the arena viewport to 16:9, lift contrast a touch so the flat-shaded art
# reads on a compressed upload, and letter the shot. No fades: the reel
# dissolves between shots instead.
readonly SHOT_FILTER_BASE="crop=1236:696:0:70,scale=1920:1080:flags=lanczos,eq=contrast=1.06:saturation=1.10"

clips=()
index=0
for shot in "${SHOTS[@]}"; do
  IFS='|' read -r scenario title orbit <<<"${shot}"
  index=$((index + 1))
  frames="${FRAME_DIR}/${scenario}"
  if ! compgen -G "${frames}/frame_*.png" >/dev/null; then
    echo "  no frames for ${scenario}; skipping" >&2
    continue
  fi

  clip="${SHOT_DIR}/$(printf '%02d' "${index}")_${scenario}.mp4"
  ffmpeg -y -v error -framerate "${FPS}" \
    -pattern_type glob -i "${frames}/frame_*.png" \
    -vf "${SHOT_FILTER_BASE},\
drawtext=fontfile=${FONT}:text='${title}':fontcolor=white@0.90:fontsize=42:\
x=76:y=h-132:shadowcolor=black@0.65:shadowx=2:shadowy=2:\
alpha='min(1,min(max(0,t-0.35)/0.5,max(0,${SHOT_SECONDS}-0.35-t)/0.5))'" \
    -t "${SHOT_SECONDS}" -an -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow \
    -r "${FPS}" "${clip}"
  clips+=("${clip}")
done

if [[ ${#clips[@]} -eq 0 ]]; then
  echo "no clips produced" >&2
  exit 1
fi

# Opening and closing cards, generated rather than authored so the reel rebuilds
# from nothing but the repository.
ffmpeg -y -v error -f lavfi -i "color=c=black:s=1920x1080:r=${FPS}:d=${TITLE_SECONDS}" \
  -vf "drawtext=fontfile=${FONT}:text='STANDARD OF IRON':fontcolor=white:fontsize=96:\
x=(w-text_w)/2:y=(h-text_h)/2-40:alpha='min(1,min(max(0,t-0.3)/0.7,max(0,${TITLE_SECONDS}-0.2-t)/0.6))',\
drawtext=fontfile=${FONT}:text='Rome and Carthage - settlements, walls and war':\
fontcolor=white@0.72:fontsize=36:x=(w-text_w)/2:y=(h-text_h)/2+70:\
alpha='min(1,min(max(0,t-0.7)/0.7,max(0,${TITLE_SECONDS}-0.2-t)/0.6))'" \
  -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow "${WORK}/title.mp4"

ffmpeg -y -v error -f lavfi -i "color=c=black:s=1920x1080:r=${FPS}:d=${END_SECONDS}" \
  -vf "drawtext=fontfile=${FONT}:text='STANDARD OF IRON':fontcolor=white:fontsize=72:\
x=(w-text_w)/2:y=(h-text_h)/2-20:alpha='min(1,min(max(0,t-0.4)/0.8,max(0,${END_SECONDS}-0.6-t)/0.8))',\
drawtext=fontfile=${FONT}:text='A historical real-time strategy engine':\
fontcolor=white@0.70:fontsize=32:x=(w-text_w)/2:y=(h-text_h)/2+70:\
alpha='min(1,min(max(0,t-0.8)/0.8,max(0,${END_SECONDS}-0.6-t)/0.8))'" \
  -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow "${WORK}/end.mp4"

# Chain title, shots and end card with dissolves. xfade takes one pair at a
# time, so the graph is built up segment by segment with running offsets.
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
  offset=$(LC_ALL=C awk -v o="${offset}" -v t="${TRANSITION_SECONDS}" 'BEGIN{printf "%.3f", o-t}')
  label="[x${i}]"
  filter+="${previous}[${i}:v]xfade=transition=fade:duration=${TRANSITION_SECONDS}:offset=${offset}${label};"
  previous="${label}"
  offset=$(LC_ALL=C awk -v o="${offset}" -v d="${durations[${i}]}" -v t="${TRANSITION_SECONDS}" \
    'BEGIN{printf "%.3f", o+t+d-t}')
done
filter+="${previous}format=yuv420p[v]"

ffmpeg -y -v error "${inputs[@]}" -filter_complex "${filter}" -map "[v]" \
  -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow -r "${FPS}" "${WORK}/silent.mp4"

total=$(ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 "${WORK}/silent.mp4")
fade_out_start=$(LC_ALL=C awk -v d="${total}" 'BEGIN{printf "%.2f", d-2.2}')

# The soundtrack is mono at 32 kHz; upmix and resample for delivery, level it to
# the -14 LUFS most platforms target, and duck the ends under the cards.
ffmpeg -y -v error -i "${WORK}/silent.mp4" -ss "${MUSIC_START}" -i "${MUSIC}" \
  -filter_complex "[1:a]aformat=channel_layouts=mono,aresample=48000,\
pan=stereo|c0=c0|c1=c0,loudnorm=I=-14:TP=-1.5:LRA=11,\
afade=t=in:st=0:d=1.6,afade=t=out:st=${fade_out_start}:d=2.2[a]" \
  -map 0:v -map "[a]" -t "${total}" \
  -c:v copy -c:a aac -b:a 192k -movflags +faststart "${PROMO}"

rm -rf "${WORK}"

echo "promo: ${PROMO}"
ffprobe -v error -show_entries format=duration:stream=width,height,codec_name \
  -of default=nw=1 "${PROMO}"
