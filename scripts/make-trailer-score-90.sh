#!/usr/bin/env bash
# Build the ninety-second score for the "Command it. Fight in it." trailer.
#
# The shipped tracks are 60 s long, so the piece is assembled from windows of
# five of them, crossfaded so the joins land on the trailer's sequence
# boundaries: the settlement at 8 s, the commander at 20 s, the battle at 31 s
# and 46 s, the dead at 59 s, the editor at 70 s, the payoff at 83 s.
#
# Output is derived and goes to artifacts/ (gitignored); promo-edit.py masters it.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
music="${root}/assets/audio/music"
out="${1:-${root}/artifacts/promo/trailer_score_90.ogg}"
mkdir -p "$(dirname "${out}")"

# Section lengths (crossfade 2.0 s): 22 -> 46 -> 61 -> 72 -> 85 -> 92, trimmed to 90.
ffmpeg -hide_banner -loglevel error -y \
  -i "${music}/combat/combat_shield_wall_at_dusk.ogg" \
  -i "${music}/campaign/campaign_hannibals_ascent.ogg" \
  -i "${music}/combat/combat_dust_of_cannae.ogg" \
  -i "${music}/events/skeletons_awaken.ogg" \
  -i "${music}/menu/main_theme_standard_of_iron.ogg" \
  -i "${music}/stingers/victory_fanfare.ogg" \
  -filter_complex "\
[0:a]atrim=0:22,asetpts=PTS-STARTPTS,volume=0.95[s1];\
[1:a]atrim=6:32,asetpts=PTS-STARTPTS,volume=0.9[s2];\
[2:a]atrim=12:29,asetpts=PTS-STARTPTS,volume=1.05[s3];\
[3:a]atrim=4:17,asetpts=PTS-STARTPTS,volume=1.0[s4];\
[4:a]atrim=0:15,asetpts=PTS-STARTPTS,volume=0.9[s5];\
[5:a]atrim=0:7,asetpts=PTS-STARTPTS,volume=1.0[s6];\
[s1][s2]acrossfade=d=2:c1=tri:c2=tri[j1];\
[j1][s3]acrossfade=d=2:c1=tri:c2=tri[j2];\
[j2][s4]acrossfade=d=2:c1=tri:c2=tri[j3];\
[j3][s5]acrossfade=d=2:c1=tri:c2=tri[j4];\
[j4][s6]acrossfade=d=2:c1=tri:c2=tri[j5];\
[j5]atrim=0:90,asetpts=PTS-STARTPTS,afade=t=out:st=87.5:d=2.5[mix]" \
  -map "[mix]" -ar 44100 -c:a libvorbis -q:a 6 "${out}"

printf 'wrote %s (%s s)\n' "${out}" \
  "$(ffprobe -v error -show_entries format=duration -of csv=p=0 "${out}")"
