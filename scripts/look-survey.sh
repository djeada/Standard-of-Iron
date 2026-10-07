#!/usr/bin/env bash
# Capture the whole-game look survey: 60 arena shots from the curated promo
# cameras (tools/arena/promos/look_survey.json) plus mission-start frames
# from the real game, with the player's camera and the HUD up.
#
#   scripts/look-survey.sh <out-dir> [build-dir]
#
# Needs an unlocked desktop session on DISPLAY (default :0); check
# `loginctl list-sessions` first. Takes about 30 minutes on an RTX 5060.
# Compare two runs with scripts/look-survey-compare.py.
set -euo pipefail

out="${1:?usage: look-survey.sh <out-dir> [build-dir]}"
build="${2:-build}"
root="$(cd "$(dirname "$0")/.." && pwd)"
display="${DISPLAY:-:0}"
mkdir -p "$out/arena" "$out/game"
out="$(cd "$out" && pwd)"

missions=(battle_of_trebia battle_of_cannae crossing_the_alps
  siege_of_aurelia_magna iron_sepulcher_watch the_timber_levy
  crossing_the_rhone battle_of_zama hold_the_sallow_ford)
rpg_missions=(battle_of_trebia the_timber_levy)

cd "$root/$build"
DISPLAY="$display" PULSE_SERVER=unix:/nonexistent SOI_AUDIO_OFFLINE=1 \
  timeout 3600 ./bin/arena_app \
  --promo-spec "$root/tools/arena/promos/look_survey.json" \
  --promo-out "$out/arena" >"$out/arena.log" 2>&1

capture() {
  local mission="$1" view="$2"
  DISPLAY="$display" QT_QPA_PLATFORM=xcb PULSE_SERVER=unix:/nonexistent \
    SOI_AUDIO_OFFLINE=1 timeout 300 ./bin/standard_of_iron \
    --mission-file "../assets/missions/$mission.json" --skip-briefing \
    --screenshot-view "$view" --screenshot "$out/game/${mission}_$view.png" \
    --screenshot-delay 400000 --screenshot-size 1920x1080 \
    >"$out/game/${mission}_$view.log" 2>&1 || echo "capture failed: $mission $view"
}

for mission in "${missions[@]}"; do capture "$mission" hud; done
for mission in "${rpg_missions[@]}"; do capture "$mission" rpg; done

echo "Look survey written to $out"
