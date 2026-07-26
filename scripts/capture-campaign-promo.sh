#!/usr/bin/env bash
# Renders every campaign battlefield through the real renderer and cuts them
# into a promo reel.
#
#   scripts/capture-campaign-promo.sh [output-dir] [--recapture]
#
# The reel opens straight on a walled town at full brightness. There is no fade
# from black and no title card in front of the footage: a feed autoplays muted
# and a dark first frame reads as "nothing is happening", so the hook has to be
# the battlefield itself.
#
# Needs a working display (the Arena renders through OpenGL, not offscreen) and
# ffmpeg. Frames are kept in <output-dir>/frames so the edit can be rebuilt
# without re-rendering; pass --recapture to render them again.
set -euo pipefail

# ffmpeg filter graphs only parse a dot as the decimal separator, and awk and
# ffprobe both follow the locale. Pin it so a comma locale cannot corrupt the
# transition offsets.
export LC_ALL=C

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly ROOT
readonly ARENA="${ROOT}/build/bin/arena_app"

OUT_DIR="${ROOT}/artifacts/promo"
RECAPTURE=0
for arg in "$@"; do
  case "${arg}" in
    --recapture) RECAPTURE=1 ;;
    *) OUT_DIR="${arg}" ;;
  esac
done

readonly FRAME_DIR="${OUT_DIR}/frames"
readonly SHOT_DIR="${OUT_DIR}/shots"
readonly PROMO="${OUT_DIR}/standard_of_iron_campaign.mp4"
readonly WORK="${OUT_DIR}/.work"

readonly FPS=30
readonly CAPTURE_INTERVAL=0.0333
readonly CAPTURE_SECONDS=5
readonly ORBIT_SPEED=7
readonly TIME_OF_DAY=morning
# The camera frames the densest built-up area on each map, so these control how
# close it sits to that town and how far it leans over.
readonly PROMO_DISTANCE=0.30
readonly PROMO_TILT=34

readonly HOOK_SECONDS=2.4
readonly SHOT_SECONDS=2.6
readonly TRANSITION_SECONDS=0.34
readonly END_SECONDS=3.4

readonly MUSIC="${ROOT}/assets/audio/music/campaign/campaign_hannibal_march.ogg"

# Campaign order. id | on-screen title | the line under it.
readonly SHOTS=(
  "crossing_the_rhone|Crossing the Rhône|218 BC — take the river forts before Scipio lands"
  "crossing_the_alps|Crossing the Alps|Cut timber, break stone, drag an army over the pass"
  "battle_of_ticino|Ticino|First blood on Italian soil"
  "battle_of_trebia|Trebia|Hold the frozen river. Let the crossings kill them"
  "battle_of_trasimene|Lake Trasimene|Twenty minutes of mist. Then Rome knows"
  "battle_of_cannae|Cannae|Give them the centre. Close both wings"
  "campania_campaign|The Campanian Vigil|Three consular roads. One walled quarter"
  "battle_of_zama|Zama|Four camps, two risings of the dead, no way back"
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

font_file_regular() {
  local candidate
  for candidate in \
    /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \
    /usr/share/fonts/dejavu/DejaVuSans.ttf \
    /Library/Fonts/Georgia.ttf; do
    if [[ -f "${candidate}" ]]; then
      printf '%s' "${candidate}"
      return 0
    fi
  done
  return 1
}

command -v ffmpeg >/dev/null 2>&1 || {
  echo "ffmpeg is required" >&2
  exit 1
}
FONT="$(font_file)" || {
  echo "no serif font found for titles" >&2
  exit 1
}
FONT_BODY="$(font_file_regular)" || FONT_BODY="${FONT}"
readonly FONT FONT_BODY

capture_frames() {
  [[ -x "${ARENA}" ]] || {
    echo "arena_app not built; run: make build" >&2
    exit 1
  }
  rm -rf "${FRAME_DIR}"
  mkdir -p "${FRAME_DIR}"
  echo "rendering 8 campaign battlefields (this opens a render window)"
  "${ARENA}" --batch --campaign-terrain --map-preview-content --clean-capture \
    --capture-interval "${CAPTURE_INTERVAL}" \
    --duration "${CAPTURE_SECONDS}" \
    --capture-orbit "${ORBIT_SPEED}" \
    --promo-distance "${PROMO_DISTANCE}" \
    --promo-tilt "${PROMO_TILT}" \
    --time-of-day "${TIME_OF_DAY}" \
    --artifact-dir "${FRAME_DIR}"
}

if [[ ${RECAPTURE} -eq 1 || ! -d "${FRAME_DIR}" ]]; then
  capture_frames
else
  echo "reusing frames in ${FRAME_DIR} (pass --recapture to re-render)"
fi

rm -rf "${SHOT_DIR}" "${WORK}"
mkdir -p "${SHOT_DIR}" "${WORK}"

# Crop to 16:9 and grade. The renderer's flat shading goes muddy through video
# compression, so lift contrast and warm the midtones before encoding.
readonly GRADE="crop=1236:695:0:60,scale=1920:1080:flags=lanczos,\
eq=contrast=1.16:saturation=1.24:brightness=0.028:gamma=1.04,\
colorbalance=rm=0.06:gm=0.015:bm=-0.05,unsharp=5:5:0.7"

# --- hook -------------------------------------------------------------------
# Opens on the densest town already in motion, at full brightness, with a slow
# push-in. The words arrive over live footage, never over black.
HOOK_SOURCE="${FRAME_DIR}/battle_of_trasimene"
[[ -d "${HOOK_SOURCE}" ]] || HOOK_SOURCE="${FRAME_DIR}/campania_campaign"

hook_clip="${SHOT_DIR}/00_hook.mp4"
ffmpeg -y -v error -framerate "${FPS}" \
  -pattern_type glob -i "${HOOK_SOURCE}/frame_*.png" \
  -vf "${GRADE},zoompan=z='min(1.0+0.055*on/(${FPS}*${HOOK_SECONDS}),1.06)':\
x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':d=1:s=1920x1080:fps=${FPS},\
drawtext=fontfile=${FONT}:text='THEY BUILT A CITY HERE':fontcolor=white:fontsize=86:\
x=(w-tw)/2:y=h*0.40:shadowcolor=black@0.8:shadowx=3:shadowy=3:\
alpha='min(1,max(0,t-0.18)/0.32)',\
drawtext=fontfile=${FONT_BODY}:text='You are going to take it apart.':fontcolor=white@0.94:fontsize=46:\
x=(w-tw)/2:y=h*0.53:shadowcolor=black@0.8:shadowx=2:shadowy=2:\
alpha='min(1,max(0,t-0.75)/0.32)'" \
  -t "${HOOK_SECONDS}" -an -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow \
  -r "${FPS}" "${hook_clip}"

clips=("${hook_clip}")

# --- one shot per battlefield ----------------------------------------------
index=0
for shot in "${SHOTS[@]}"; do
  IFS='|' read -r id title strap <<<"${shot}"
  index=$((index + 1))
  frames="${FRAME_DIR}/${id}"
  if ! compgen -G "${frames}/frame_*.png" >/dev/null; then
    echo "  no frames for ${id}; skipping" >&2
    continue
  fi

  clip="${SHOT_DIR}/$(printf '%02d' "${index}")_${id}.mp4"
  ffmpeg -y -v error -framerate "${FPS}" \
    -pattern_type glob -i "${frames}/frame_*.png" \
    -vf "${GRADE},\
drawtext=fontfile=${FONT}:text='${title}':fontcolor=white:fontsize=58:\
x=84:y=h-186:shadowcolor=black@0.75:shadowx=3:shadowy=3:\
alpha='min(1,min(max(0,t-0.20)/0.28,max(0,${SHOT_SECONDS}-0.20-t)/0.30))',\
drawtext=fontfile=${FONT_BODY}:text='${strap}':fontcolor=white@0.88:fontsize=34:\
x=88:y=h-116:shadowcolor=black@0.75:shadowx=2:shadowy=2:\
alpha='min(1,min(max(0,t-0.40)/0.28,max(0,${SHOT_SECONDS}-0.20-t)/0.30))'" \
    -t "${SHOT_SECONDS}" -an -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow \
    -r "${FPS}" "${clip}"
  clips+=("${clip}")
done

[[ ${#clips[@]} -gt 1 ]] || {
  echo "no battlefield clips produced" >&2
  exit 1
}

# --- end card over live footage --------------------------------------------
END_SOURCE="${FRAME_DIR}/battle_of_zama"
[[ -d "${END_SOURCE}" ]] || END_SOURCE="${HOOK_SOURCE}"
end_clip="${SHOT_DIR}/99_end.mp4"
ffmpeg -y -v error -framerate "${FPS}" \
  -pattern_type glob -i "${END_SOURCE}/frame_*.png" \
  -vf "${GRADE},eq=brightness=-0.05:saturation=1.05,\
drawtext=fontfile=${FONT}:text='STANDARD OF IRON':fontcolor=white:fontsize=96:\
x=(w-tw)/2:y=h*0.40:shadowcolor=black@0.85:shadowx=3:shadowy=3:\
alpha='min(1,max(0,t-0.25)/0.5)',\
drawtext=fontfile=${FONT_BODY}:text='Eight battlefields. One road to Zama.':fontcolor=white@0.92:fontsize=40:\
x=(w-tw)/2:y=h*0.53:shadowcolor=black@0.85:shadowx=2:shadowy=2:\
alpha='min(1,max(0,t-0.75)/0.5)'" \
  -t "${END_SECONDS}" -an -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow \
  -r "${FPS}" "${end_clip}"
clips+=("${end_clip}")

# --- assemble ---------------------------------------------------------------
# Cross-dissolve every cut. xfade needs an absolute offset per join, so the
# running length is tracked as each clip is folded in.
inputs=()
for clip in "${clips[@]}"; do inputs+=(-i "${clip}"); done

filter=""
prev="0:v"
offset=0
clip_index=0
for clip in "${clips[@]}"; do
  duration="$(ffprobe -v error -show_entries format=duration -of csv=p=0 "${clip}")"
  if [[ ${clip_index} -eq 0 ]]; then
    offset="$(awk -v d="${duration}" -v t="${TRANSITION_SECONDS}" 'BEGIN{print d-t}')"
    clip_index=1
    continue
  fi
  filter+="[${prev}][${clip_index}:v]xfade=transition=fade:duration=${TRANSITION_SECONDS}:offset=${offset}[x${clip_index}];"
  prev="x${clip_index}"
  offset="$(awk -v o="${offset}" -v d="${duration}" -v t="${TRANSITION_SECONDS}" 'BEGIN{print o+d-t}')"
  clip_index=$((clip_index + 1))
done
filter="${filter%;}"

video_only="${WORK}/video.mp4"
ffmpeg -y -v error "${inputs[@]}" \
  -filter_complex "${filter}" -map "[${prev}]" \
  -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow -r "${FPS}" "${video_only}"

total="$(ffprobe -v error -show_entries format=duration -of csv=p=0 "${video_only}")"

# Music runs under the whole cut, normalised, with a short fade at the tail
# only - fading in at the head would soften the hook.
if [[ -f "${MUSIC}" ]]; then
  ffmpeg -y -v error -i "${video_only}" -i "${MUSIC}" \
    -filter_complex "[1:a]atrim=0:${total},loudnorm=I=-15:TP=-1.5:LRA=11,\
afade=t=out:st=$(awk -v t="${total}" 'BEGIN{print t-2.0}'):d=2.0[a]" \
    -map 0:v -map "[a]" -c:v copy -c:a aac -b:a 192k -shortest "${PROMO}"
else
  echo "music not found at ${MUSIC}; writing silent cut" >&2
  cp "${video_only}" "${PROMO}"
fi

echo
echo "promo reel: ${PROMO}"
ffprobe -v error -show_entries format=duration -of csv=p=0 "${PROMO}" |
  awk '{printf "duration: %.1fs\n", $1}'
