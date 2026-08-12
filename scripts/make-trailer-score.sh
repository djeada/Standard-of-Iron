#!/usr/bin/env bash
# Build the two-minute score the master trailer is cut to.
#
# Every full track is exactly 90 s, so a 120 s trailer cannot be scored from
# one of them. This assembles six windows into one continuous piece whose
# section boundaries land on the trailer's act cards -- the legion card at
# ~34 s, the battle card at ~61 s, the night card at ~98 s -- and bookends the
# whole thing by returning to the main theme under the end card.
#
# Re-derive the section lengths whenever the picture is re-cut: run
# place-trailer-cues.py's timeline() over the spec, read the card starts, and
# solve L1 = b1, Lk = bk - b(k-1) + 2.5 for each later join. The two stingers
# are only 10 s long, which caps L4 and L5 and therefore how late the last two
# joins can fall.
#
# Output is derived, so it goes to artifacts/ (gitignored) rather than assets/.
# scripts/promo-edit.py still runs it through the game's own audio mastering.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
music="${root}/assets/audio/music"
out="${1:-${root}/artifacts/promo/trailer_score.ogg}"
mkdir -p "$(dirname "${out}")"

# Section lengths chosen so the cumulative joins (crossfade 2.5 s) fall on the
# act cards and the two hard cuts that end the film: 35.3 -> 62.8 -> 99.3 ->
# 106.03 -> 113.53 -> 123.03, trimmed to 122.3.
#
# Windows are picked for continuous energy, not just for length. The main
# theme has a four-second rest at 0:24 and the first cut of this score put it
# straight under the wolf attack, which read as the audio having dropped out.
ffmpeg -hide_banner -loglevel error -y \
  -i "${music}/menu/main_theme_ancient_mediterranean_war.ogg" \
  -i "${music}/campaign/campaign_hannibal_march.ogg" \
  -i "${music}/combat/combat_large_scale_rome_carthage.ogg" \
  -i "${music}/stingers/mission_failed_distant_horn.ogg" \
  -i "${music}/stingers/defeat_tragic_low_brass.ogg" \
  -i "${music}/menu/main_theme_rome_vs_carthage.ogg" \
  -filter_complex "\
[0:a]atrim=32:67.3,asetpts=PTS-STARTPTS,volume=0.85[s1];\
[1:a]atrim=4:34,asetpts=PTS-STARTPTS[s2];\
[2:a]atrim=8:47,asetpts=PTS-STARTPTS,volume=1.05[s3];\
[3:a]atrim=0:9.23,asetpts=PTS-STARTPTS,volume=0.95[s4];\
[4:a]atrim=0:10,asetpts=PTS-STARTPTS,volume=1.0[s5];\
[5:a]atrim=0:12,asetpts=PTS-STARTPTS,volume=0.9[s6];\
[s1][s2]acrossfade=d=2.5:c1=tri:c2=tri[j1];\
[j1][s3]acrossfade=d=2.5:c1=tri:c2=tri[j2];\
[j2][s4]acrossfade=d=2.5:c1=tri:c2=tri[j3];\
[j3][s5]acrossfade=d=2.5:c1=tri:c2=tri[j4];\
[j4][s6]acrossfade=d=2.5:c1=tri:c2=tri[j5];\
[j5]atrim=0:122.3,asetpts=PTS-STARTPTS,afade=t=out:st=118.3:d=4[mix]" \
  -map "[mix]" -ar 44100 -c:a libvorbis -q:a 6 "${out}"

printf 'wrote %s (%s s)\n' "${out}" \
  "$(ffprobe -v error -show_entries format=duration -of csv=p=0 "${out}")"
