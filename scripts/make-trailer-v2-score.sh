#!/usr/bin/env bash
# Build the score for the revised "Command it. Fight in it." trailer (trailer_v2.json).
#
# The shipped tracks are 60 s long, so the piece is assembled from windows of
# six of them, crossfaded so each join is centred on a sequence boundary of the
# cut: the commanders at 12 s, the battle at 15 s, the dead at 28.4 s, the
# editor at 32.4 s and the title card at 40.3 s. The main theme builds under the
# editor insert and the elephant climax; the fanfare lands on the card.
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
# before it ends, so the joins sit at 12.0, 15.1, 28.4, 32.4 and 40.3 s.
ffmpeg -hide_banner -loglevel error -y \
  -i "${music}/combat/combat_shield_wall_at_dusk.ogg" \
  -i "${music}/campaign/campaign_hannibals_ascent.ogg" \
  -i "${music}/combat/combat_dust_of_cannae.ogg" \
  -i "${music}/events/skeletons_awaken.ogg" \
  -i "${music}/menu/main_theme_standard_of_iron.ogg" \
  -i "${music}/stingers/victory_fanfare.ogg" \
  -filter_complex "\
[0:a]atrim=0:12.75,asetpts=PTS-STARTPTS,volume=0.95[s1];\
[1:a]atrim=6:10.6,asetpts=PTS-STARTPTS,volume=0.9[s2];\
[2:a]atrim=12:26.8,asetpts=PTS-STARTPTS,volume=1.05[s3];\
[3:a]atrim=4:9.5,asetpts=PTS-STARTPTS,volume=1.0[s4];\
[4:a]atrim=0:9.4,asetpts=PTS-STARTPTS,volume=0.9[s5];\
[5:a]atrim=0:7,asetpts=PTS-STARTPTS,volume=1.0,apad=pad_dur=2[s6];\
[s1][s2]acrossfade=d=1.5:c1=tri:c2=tri[j1];\
[j1][s3]acrossfade=d=1.5:c1=tri:c2=tri[j2];\
[j2][s4]acrossfade=d=1.5:c1=tri:c2=tri[j3];\
[j3][s5]acrossfade=d=1.5:c1=tri:c2=tri[j4];\
[j4][s6]acrossfade=d=1.5:c1=tri:c2=tri[j5];\
[j5]atrim=0:44.8,asetpts=PTS-STARTPTS,afade=t=out:st=42.3:d=2.5[mix]" \
  -map "[mix]" -ar 44100 -c:a libvorbis -q:a 6 "${out}"

printf 'wrote %s (%s s)\n' "${out}" \
  "$(ffprobe -v error -show_entries format=duration -of csv=p=0 "${out}")"
