#!/usr/bin/env bash
# Record a mission for a few seconds, then replay it with digest verification.
#
# The recording carries the world digest every 30 ticks; the replay compares
# the live digest against it and exits 0 if every one matched, 12 at the first
# tick that did not. A 12 means the simulation has a source of nondeterminism
# -- the bug two players in a lockstep match would see as different games.
#
# Needs a display (the game renders); run under Xephyr or a real X server:
#   DISPLAY=:77 scripts/check-replay-determinism.sh build assets/missions/battle_of_trebia.json 20
set -euo pipefail

build_dir=${1:-build}
mission=${2:-assets/missions/battle_of_trebia.json}
seconds=${3:-20}
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT

mkdir -p "$scratch/cfg/djeada"
printf '[audio]\nmaster_volume=0\n' >"$scratch/cfg/djeada/StandardOfIron.ini"
replay="$scratch/match.soireplay"

cd "$build_dir/bin"
XDG_CONFIG_HOME="$scratch/cfg" ./standard_of_iron --mission-file "$mission" \
  --record-replay "$replay" --skip-briefing --benchmark-seconds "$seconds" >"$scratch/record.log" 2>&1 ||
  {
    echo "recording run failed"
    tail -20 "$scratch/record.log"
    exit 1
  }
echo "recorded $(grep -c '"type"' "$replay") commands, $(grep -c '"digest"' "$replay") digests"

set +e
XDG_CONFIG_HOME="$scratch/cfg" ./standard_of_iron --replay "$replay" --replay-verify >"$scratch/replay.log" 2>&1
status=$?
set -e
grep -E "SOI_REPLAY_VERIFY|replay: digest diverged" "$scratch/replay.log" || true
exit $status
