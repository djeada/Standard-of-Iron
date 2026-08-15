#!/usr/bin/env bash
# Record a headless bot skirmish, replay it, and require every recorded digest
# to match. Exit 12 from the replay means the simulation is not deterministic.
set -euo pipefail
bin=${SOI_HEADLESS:-build/bin/soi_headless}
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
"$bin" --scenario "${1:-bot_skirmish}" --seconds "${2:-20}" --seed "${3:-7}" \
  --record "$scratch/match.soireplay" --digest-every 30
"$bin" --replay "$scratch/match.soireplay" --verify
