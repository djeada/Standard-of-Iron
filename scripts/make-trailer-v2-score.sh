#!/usr/bin/env bash
# Build the score for the trailer (trailer_v2.json).
#
# The shipped tracks are 60 s long, so the piece is assembled from windows of
# five of them, crossfaded so each join is centred on a scene boundary of the
# cut: the town at 34.5 s, the Iron Sepulcher at 57.9 s, the editor at 72.2 s
# and the fire end card at 74.7 s. The fanfare lands on the card.
#
# Boundaries come from scripts/place-trailer-v2-cues.py's timeline; re-derive
# the window lengths if a shot changes length.
#
# Output is derived and goes to artifacts/ (gitignored); promo-edit.py masters it.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
music="${root}/assets/audio/music"
out="${1:-${root}/artifacts/promo/trailer_v2_score.ogg}"
mkdir -p "$(dirname "${out}")"

# Window lengths (crossfade 1.5 s): each section starts 1.5 s before the one
# before it ends, so the joins sit at 15.7, 34.5, 57.9, 72.2 and 74.7 s -- the
# city, the town, the army forming up, the barrow, the editor and the end card.
# The third window grew by 8.4 s when the formation beats stopped cutting over
# their own deployments and ramped through them instead.
# `atrim` takes start:end, so a window's length is the difference, and the join
# after it lands at (running total - 0.75).
ffmpeg -hide_banner -loglevel error -y \
  -i "${music}/combat/combat_shield_wall_at_dusk.ogg" \
  -i "${music}/base/base_hearth_and_harbor.ogg" \
  -i "${music}/base/base_march_of_the_old_gods.ogg" \
  -i "${music}/events/skeletons_awaken.ogg" \
  -i "${music}/menu/main_theme_standard_of_iron.ogg" \
  -i "${music}/stingers/victory_fanfare.ogg" \
  -filter_complex "\
[0:a]atrim=0:16.45,asetpts=PTS-STARTPTS,volume=0.95[s1];\
[1:a]atrim=4:24.3,asetpts=PTS-STARTPTS,volume=0.95[s2];\
[2:a]atrim=6:30.9,asetpts=PTS-STARTPTS,volume=0.95[s3];\
[3:a]atrim=2:17.8,asetpts=PTS-STARTPTS,volume=1.0[s4];\
[4:a]atrim=0:4.0,asetpts=PTS-STARTPTS,volume=0.9[s5];\
[5:a]atrim=0:7,asetpts=PTS-STARTPTS,volume=1.0,apad=pad_dur=2[s6];\
[s1][s2]acrossfade=d=1.5:c1=tri:c2=tri[j1];\
[j1][s3]acrossfade=d=1.5:c1=tri:c2=tri[j2];\
[j2][s4]acrossfade=d=1.5:c1=tri:c2=tri[j3];\
[j3][s5]acrossfade=d=1.5:c1=tri:c2=tri[j4];\
[j4][s6]acrossfade=d=1.5:c1=tri:c2=tri[j5];\
[j5]atrim=0:80.2,asetpts=PTS-STARTPTS,afade=t=out:st=77.7:d=2.5[mix]" \
  -map "[mix]" -ar 44100 -c:a libvorbis -q:a 6 "${out}"

printf 'wrote %s (%s s)\n' "${out}" \
  "$(ffprobe -v error -show_entries format=duration -of csv=p=0 "${out}")"
