#!/usr/bin/env bash
# Record a headless bot skirmish, replay it, and require every recorded digest
# to match. Exit 12 from the replay means the simulation is not deterministic.
set -euo pipefail
bin=${SOI_HEADLESS:-build/bin/soi_headless}
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
scenario=${1:-bot_skirmish}
seconds=${2:-120}
orders=()
if [[ "$scenario" == bot_skirmish ]]; then
  orders+=(--scripted-orders)
  if [[ -z ${2:-} || ${4:-} == --until-victory ]]; then
    orders+=(--until-victory)
  fi
fi
replay="$scratch/match.soireplay"
"$bin" --scenario "$scenario" --seconds "$seconds" --seed "${3:-7}" "${orders[@]}" \
  --record "$replay" --digest-every 30 --checkpoint-every 600 | tee "$scratch/live.log"
digest() { sed -n 's/.*; digest \([0-9]*\);.*/\1/p' "$1"; }
live_digest=$(digest "$scratch/live.log")
for run in 1 2; do
  "$bin" --replay "$replay" --verify | tee "$scratch/replay-$run.log"
  if [[ -z "$live_digest" || "$(digest "$scratch/replay-$run.log")" != "$live_digest" ]]; then
    echo "Replay $run ended with a different digest than the recorded match" >&2
    exit 12
  fi
done
