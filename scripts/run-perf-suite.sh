#!/usr/bin/env bash
# Reproducible Ultra / Full-LOD performance suite.
#
# Builds RelWithDebInfo, refuses to measure a contaminated machine, records the
# environment that the numbers depend on, and runs every fixture the Ultra
# performance plan gates on. Raw JSON lands under artifacts/perf/<run-id>/ and
# is not committed; only the manifest and the verdict summary are meant to be
# quoted in a change description.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="${BUILD_DIR:-build-perf}"
SECONDS_PER_MISSION="${SECONDS_PER_MISSION:-30}"
REPEATS="${REPEATS:-5}"
JOBS="${JOBS:-4}"
PRESET="${PRESET:-ultra}"
SKIP_BUILD=0
SKIP_ARENA=0
SKIP_PERF=0
USE_REPLAY=1
ALLOW_CONTENDED=0

usage() {
  cat <<'EOF'
usage: run-perf-suite.sh [options]

  --seconds N        measured seconds per mission run (default 30)
  --repeats N        interleaved A/B repeats per fixture (default 5)
  --jobs N           build parallelism (default 4)
  --preset NAME      graphics preset (default ultra)
  --skip-build       measure the tree as already built
  --skip-arena       run only the campaign missions
  --skip-perf        do not collect perf record / perf stat profiles
  --no-replay        play each repeat live instead of replaying one recording
  --allow-contended  measure anyway when the machine is busy
  -h, --help         this message
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --seconds)
      SECONDS_PER_MISSION="$2"
      shift 2
      ;;
    --repeats)
      REPEATS="$2"
      shift 2
      ;;
    --jobs)
      JOBS="$2"
      shift 2
      ;;
    --preset)
      PRESET="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --skip-arena)
      SKIP_ARENA=1
      shift
      ;;
    --skip-perf)
      SKIP_PERF=1
      shift
      ;;
    --no-replay)
      USE_REPLAY=0
      shift
      ;;
    --allow-contended)
      ALLOW_CONTENDED=1
      shift
      ;;
    -h | --help)
      usage
      exit 0
      ;;
    *)
      echo "run-perf-suite: unknown argument '$1'" >&2
      usage >&2
      exit 2
      ;;
  esac
done

RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)"
OUT_DIR="artifacts/perf/$RUN_ID"
mkdir -p "$OUT_DIR"

fail() {
  echo "run-perf-suite: $*" >&2
  exit 1
}

# ---- refuse to measure a machine that cannot produce a comparable number ----

check_environment() {
  local problems=()

  if [[ -z "${DISPLAY:-}" ]]; then
    problems+=("no DISPLAY; the GPU gates need a real desktop")
  fi

  local load
  load="$(awk '{print $1}' /proc/loadavg)"
  if (($(echo "$load > 4.0" | bc -l))); then
    problems+=("load average is $load; another job is competing for the CPU")
  fi

  local rivals
  rivals="$(pgrep -c -f 'standard_of_iron|arena_app' || true)"
  if [[ "$rivals" != "0" ]]; then
    problems+=("$rivals game processes are already running")
  fi

  if command -v xset >/dev/null 2>&1; then
    if xset -q 2>/dev/null | grep -q 'Monitor is Off'; then
      problems+=("the display is blanked; GPU timings will be wrong")
    fi
  fi

  if ((${#problems[@]} == 0)); then
    return 0
  fi

  printf 'run-perf-suite: the machine is not measurement-clean:\n' >&2
  printf '  - %s\n' "${problems[@]}" >&2
  if ((ALLOW_CONTENDED == 0)); then
    fail "refusing to measure; pass --allow-contended to record diagnostic numbers anyway"
  fi
  echo "run-perf-suite: --allow-contended given; the numbers are diagnostic, not a gate" >&2
}

check_environment

# ---- record what the numbers depend on --------------------------------------

write_manifest() {
  local commit dirty
  commit="$(git rev-parse HEAD)"
  dirty="$(git status --porcelain | wc -l)"
  {
    echo "{"
    echo "  \"run_id\": \"$RUN_ID\","
    echo "  \"commit\": \"$commit\","
    echo "  \"working_tree_dirty_files\": $dirty,"
    echo "  \"graphics_preset\": \"$PRESET\","
    echo "  \"deterministic_replay\": $((USE_REPLAY == 1 ? 1 : 0)),"
    echo "  \"measured_seconds\": $SECONDS_PER_MISSION,"
    echo "  \"repeats\": $REPEATS,"
    echo "  \"build_dir\": \"$BUILD_DIR\","
    echo "  \"kernel\": \"$(uname -sr)\","
    echo "  \"cpu\": \"$(awk -F': ' '/model name/{print $2; exit}' /proc/cpuinfo)\","
    echo "  \"load_average\": \"$(cut -d' ' -f1-3 /proc/loadavg)\","
    echo "  \"gpu\": \"$(command -v nvidia-smi >/dev/null 2>&1 && nvidia-smi --query-gpu=name,driver_version --format=csv,noheader | head -1 || echo unknown)\","
    echo "  \"assets_hash\": \"$(find assets -type f -newer /dev/null -print0 2>/dev/null | sort -z | xargs -0 sha1sum 2>/dev/null | sha1sum | cut -d' ' -f1)\""
    echo "}"
  } >"$OUT_DIR/manifest.json"
  echo "run-perf-suite: manifest -> $OUT_DIR/manifest.json"
}

write_manifest

# ---- build ------------------------------------------------------------------

if ((SKIP_BUILD == 0)); then
  echo "run-perf-suite: configuring $BUILD_DIR (RelWithDebInfo)"
  cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo >"$OUT_DIR/configure.log" 2>&1 ||
    fail "cmake configure failed; see $OUT_DIR/configure.log"
  echo "run-perf-suite: building standard_of_iron arena_app sim_benchmark"
  cmake --build "$BUILD_DIR" --target standard_of_iron arena_app sim_benchmark \
    -j "$JOBS" >"$OUT_DIR/build.log" 2>&1 ||
    fail "build failed; see $OUT_DIR/build.log"
fi

GAME="$BUILD_DIR/bin/standard_of_iron"
ARENA="$BUILD_DIR/bin/arena_app"
SIM="$BUILD_DIR/bin/sim_benchmark"
[[ -x "$GAME" ]] || fail "missing $GAME"

# ---- campaign missions ------------------------------------------------------

MISSIONS=(
  "second_punic_war/crossing_the_rhone:rhone"
  "second_punic_war/crossing_the_alps:alps"
  "second_punic_war/battle_of_ticino:ticino"
)

# A/B comparison is only meaningful when both sides simulate the same match, so
# each mission is recorded once and every measured repeat replays that recording.
# --replay shuts out local input and the computer opponent, which is what makes
# repeat 5 comparable to repeat 1 and to a candidate build's repeat 1.
REPLAY_DIR="$OUT_DIR/replays"

record_mission_replay() {
  local mission="$1" label="$2"
  local replay="$REPLAY_DIR/${label}.soireplay"
  mkdir -p "$REPLAY_DIR"
  echo "run-perf-suite: recording $label"
  "$GAME" \
    --campaign-mission "$mission" \
    --skip-briefing \
    --graphics-preset "$PRESET" \
    --record-replay "$replay" \
    --benchmark-seconds "$SECONDS_PER_MISSION" \
    >"$OUT_DIR/${label}.record.log" 2>&1 ||
    echo "run-perf-suite: recording $label exited non-zero (see log)" >&2
  [[ -s "$replay" ]]
}

run_mission() {
  local mission="$1" label="$2" repeat="$3"
  local output="$OUT_DIR/${label}.run${repeat}.json"
  local replay="$REPLAY_DIR/${label}.soireplay"
  echo "run-perf-suite: $label repeat $repeat"
  if ((USE_REPLAY == 1)) && [[ -s "$replay" ]]; then
    "$GAME" \
      --replay "$replay" \
      --graphics-preset "$PRESET" \
      --benchmark-seconds "$SECONDS_PER_MISSION" \
      --benchmark-output "$output" \
      >"$OUT_DIR/${label}.run${repeat}.log" 2>&1 ||
      echo "run-perf-suite: $label repeat $repeat exited non-zero (see log)" >&2
    return 0
  fi
  "$GAME" \
    --campaign-mission "$mission" \
    --skip-briefing \
    --graphics-preset "$PRESET" \
    --benchmark-seconds "$SECONDS_PER_MISSION" \
    --benchmark-output "$output" \
    >"$OUT_DIR/${label}.run${repeat}.log" 2>&1 ||
    echo "run-perf-suite: $label repeat $repeat exited non-zero (see log)" >&2
}

# Sampled separately from the timed runs, and only once per mission: attaching a
# profiler perturbs the very frame times the gates are measured from. The delay
# skips loading and the warm-up so the profile is steady-state play rather than
# startup, which is what dominated the earlier hand-collected captures.
PERF_DELAY="${PERF_DELAY:-8}"

profile_mission() {
  local mission="$1" label="$2"
  if ! command -v perf >/dev/null 2>&1; then
    return 0
  fi
  echo "run-perf-suite: perf record $label (after ${PERF_DELAY}s)"
  perf record --delay "$((PERF_DELAY * 1000))" -g --call-graph dwarf -F 499 \
    -o "$OUT_DIR/${label}.perf.data" -- \
    "$GAME" \
    --campaign-mission "$mission" \
    --skip-briefing \
    --graphics-preset "$PRESET" \
    --benchmark-seconds "$SECONDS_PER_MISSION" \
    --benchmark-output "$OUT_DIR/${label}.perf-run.json" \
    >"$OUT_DIR/${label}.perf.log" 2>&1 ||
    echo "run-perf-suite: perf record $label failed (see log)" >&2

  if [[ -f "$OUT_DIR/${label}.perf.data" ]]; then
    perf report --stdio --no-children -i "$OUT_DIR/${label}.perf.data" \
      >"$OUT_DIR/${label}.perf-report.txt" 2>/dev/null || true
  fi

  perf stat -d -x, -o "$OUT_DIR/${label}.perf-stat.csv" -- \
    "$GAME" \
    --campaign-mission "$mission" \
    --skip-briefing \
    --graphics-preset "$PRESET" \
    --benchmark-seconds "$SECONDS_PER_MISSION" \
    --benchmark-output "$OUT_DIR/${label}.stat-run.json" \
    >"$OUT_DIR/${label}.perf-stat.log" 2>&1 ||
    echo "run-perf-suite: perf stat $label failed (see log)" >&2
}

# Interleaved A/B/A/B: every fixture is visited once per round, so a machine
# that warms or throttles during the suite biases every fixture equally.
if ((USE_REPLAY == 1)); then
  for entry in "${MISSIONS[@]}"; do
    record_mission_replay "${entry%%:*}" "${entry##*:}" ||
      echo "run-perf-suite: ${entry##*:} produced no replay; falling back to live play" >&2
  done
fi

for ((repeat = 1; repeat <= REPEATS; repeat++)); do
  for entry in "${MISSIONS[@]}"; do
    run_mission "${entry%%:*}" "${entry##*:}" "$repeat"
  done
done

if ((SKIP_PERF == 0)); then
  for entry in "${MISSIONS[@]}"; do
    profile_mission "${entry%%:*}" "${entry##*:}"
  done
fi

# ---- arena scale fixtures ---------------------------------------------------

if ((SKIP_ARENA == 0)) && [[ -x "$ARENA" ]]; then
  SCENARIOS=(
    campaign_scale_battle
    massed_battle_250
    massed_battle_500
    massed_battle_1000
    seven_ai_scale
  )
  for scenario in "${SCENARIOS[@]}"; do
    echo "run-perf-suite: arena $scenario"
    "$ARENA" \
      --batch \
      --scenario "$scenario" \
      --graphics-quality "$PRESET" \
      --prewarm \
      --profile \
      --capture-interval 0 \
      --artifact-dir "$OUT_DIR/arena/$scenario" \
      >"$OUT_DIR/arena-$scenario.log" 2>&1 ||
      echo "run-perf-suite: arena $scenario exited non-zero (see log)" >&2
  done
fi

# ---- CPU-only simulation fixture -------------------------------------------

if [[ -x "$SIM" ]]; then
  echo "run-perf-suite: sim_benchmark"
  "$SIM" --units 2000 --ticks 600 --json "$OUT_DIR/sim_benchmark.json" \
    >"$OUT_DIR/sim_benchmark.txt" 2>&1 || true
fi

# ---- verdict ----------------------------------------------------------------

python3 scripts/summarize-perf-suite.py "$OUT_DIR" | tee "$OUT_DIR/summary.txt"
echo "run-perf-suite: raw reports in $OUT_DIR"
