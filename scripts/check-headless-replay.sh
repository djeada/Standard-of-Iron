#!/usr/bin/env bash
# Record a skirmish, replay it from the start and from checkpoints, and require
# every recorded digest to match. Exit 12 from the replay means the simulation is not deterministic.
set -euo pipefail
bin=${SOI_HEADLESS:-build/bin/soi_headless}
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
scenario=${1:-bot_skirmish}
seconds=${2:-120}
launch=(--scenario "$scenario")
if [[ "$scenario" == *.json ]]; then
  launch=(--map "$scenario")
fi
orders=()
if [[ "$scenario" == bot_skirmish ]]; then
  orders+=(--scripted-orders)
  if [[ -z ${2:-} || ${4:-} == --until-victory ]]; then
    orders+=(--until-victory)
  fi
fi
replay="$scratch/match.soireplay"
checkpoint_interval=240
"$bin" "${launch[@]}" --seconds "$seconds" --seed "${3:-7}" "${orders[@]}" \
  --record "$replay" --digest-every 30 --checkpoint-every "$checkpoint_interval" | tee "$scratch/live.log"
digest() { sed -n 's/.*; digest \([0-9]*\);.*/\1/p' "$1"; }
live_digest=$(digest "$scratch/live.log")
for run in 1 2; do
  "$bin" --replay "$replay" --verify | tee "$scratch/replay-$run.log"
  if [[ -z "$live_digest" || "$(digest "$scratch/replay-$run.log")" != "$live_digest" ]]; then
    echo "Replay $run ended with a different digest than the recorded match" >&2
    exit 12
  fi
done

ticks=$(sed -n 's/.* ran \([0-9]*\) ticks .*/\1/p' "$scratch/live.log")
last_checkpoint=$(((ticks - 1) / checkpoint_interval * checkpoint_interval))
checkpoints=("$checkpoint_interval")
if ((last_checkpoint > checkpoint_interval)); then
  checkpoints+=("$last_checkpoint")
fi
for checkpoint in "${checkpoints[@]}"; do
  if ((checkpoint == 0 || checkpoint >= ticks)); then
    continue
  fi
  seek_tick=$((checkpoint + 1))
  "$bin" --replay "$replay" --seek "$seek_tick" --verify | tee "$scratch/seek.log"
  if ! grep -Fq "sought to tick $seek_tick from checkpoint $checkpoint" "$scratch/seek.log" ||
    [[ "$(digest "$scratch/seek.log")" != "$live_digest" ]]; then
    echo "Replay seek from checkpoint $checkpoint did not reproduce the match" >&2
    exit 12
  fi
done
