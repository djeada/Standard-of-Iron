#!/usr/bin/env bash
# Run the RPG playability gate: build, unit tests, then every required rpg_*
# Arena scenario, one at a time, into a fresh artifact directory.
#
# Sequential is not a style choice. Two arena_app processes rendering at once
# contaminate each other's frame times and can starve one into its watchdog,
# which then reports as an empty-issue failure. The gate runs one scenario at a
# time and refuses to accept an incomplete run as a behavioural result.
#
# The manifest at tools/arena/rpg_gate_manifest.json decides which scenarios are
# required and which are pinned expected-red while their gate is still open.
# arena_rpg_gate_manifest_test keeps that manifest in step with the registry, so
# a new rpg_* scenario cannot be added without appearing here.
#
#   scripts/run-rpg-gates.sh                       # full gate
#   scripts/run-rpg-gates.sh --skip-build          # reuse the current build
#   scripts/run-rpg-gates.sh --scenario rpg_locomotion
#   scripts/run-rpg-gates.sh --baseline            # write the comparison baseline
#
# Rendering needs a display; this script defaults DISPLAY to :0 when unset.
set -uo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo_root" || exit 2

build_dir=build
fps=60
capture_interval=0
jobs=4
artifact_dir=""
skip_build=0
skip_tests=0
enforce_performance=0
baseline=0
scenarios=()
duration_override=""
watchdog_multiplier=""

usage() {
  awk 'NR > 1 { if ($0 !~ /^#/) exit; sub(/^# ?/, ""); print }' "${BASH_SOURCE[0]}"
  cat <<'EOF'

Options:
  --build-dir DIR         Build directory (default: build)
  --artifact-dir DIR      Artifact root (default: artifacts/rpg-gates/run-<stamp>)
  --baseline              Write to artifacts/rpg-gates/baseline instead
  --fps N                 Fixed sampling rate for batch runs (default: 60)
  --capture-interval S    Seconds between frame captures; 0 disables (default: 0)
  --duration S            Override every scenario duration (debugging only)
  --watchdog-multiplier N Wall-clock watchdog as a multiple of scenario duration
  --scenario ID           Run only this scenario; repeatable
  --jobs N                Build parallelism (default: 4)
  --skip-build            Do not build; use the binaries already present
  --skip-tests            Do not run the unit-test filters
  --enforce-performance   Fail on frame-budget issues (reference hardware only)
  -h, --help              This text
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      build_dir=$2
      shift 2
      ;;
    --artifact-dir)
      artifact_dir=$2
      shift 2
      ;;
    --baseline)
      baseline=1
      shift
      ;;
    --fps)
      fps=$2
      shift 2
      ;;
    --capture-interval)
      capture_interval=$2
      shift 2
      ;;
    --duration)
      duration_override=$2
      shift 2
      ;;
    --watchdog-multiplier)
      watchdog_multiplier=$2
      shift 2
      ;;
    --scenario)
      scenarios+=("$2")
      shift 2
      ;;
    --jobs)
      jobs=$2
      shift 2
      ;;
    --skip-build)
      skip_build=1
      shift
      ;;
    --skip-tests)
      skip_tests=1
      shift
      ;;
    --enforce-performance)
      enforce_performance=1
      shift
      ;;
    -h | --help)
      usage
      exit 0
      ;;
    *)
      echo "unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

manifest=tools/arena/rpg_gate_manifest.json
if [[ ! -f $manifest ]]; then
  echo "gate manifest not found: $manifest" >&2
  exit 2
fi

if [[ -z $artifact_dir ]]; then
  if [[ $baseline -eq 1 ]]; then
    artifact_dir=artifacts/rpg-gates/baseline
  else
    artifact_dir=artifacts/rpg-gates/run-$(date +%Y%m%d-%H%M%S)
  fi
fi

bold=$'\033[1m'
red=$'\033[31m'
green=$'\033[32m'
yellow=$'\033[33m'
reset=$'\033[0m'

step() { echo "${bold}==> $*${reset}"; }
fail() { echo "${red}$*${reset}" >&2; }

if [[ $skip_build -eq 0 ]]; then
  step "Building arena_app, app_tests and arena_tests (-j$jobs)"
  if ! cmake --build "$build_dir" -j"$jobs" --target arena_app app_tests arena_tests; then
    fail "build failed"
    exit 2
  fi
fi

for binary in arena_app app_tests arena_tests; do
  if [[ ! -x $build_dir/bin/$binary ]]; then
    fail "missing $build_dir/bin/$binary; run without --skip-build"
    exit 2
  fi
done

test_status=0
if [[ $skip_tests -eq 0 ]]; then
  step "Unit and integration tests"
  while IFS=$'\t' read -r binary filter; do
    [[ -z $binary ]] && continue
    echo "  ${bold}$binary${reset} --gtest_filter='$filter'"
    if ! "$build_dir/bin/$binary" --gtest_filter="$filter" --gtest_brief=1; then
      fail "  $binary failed"
      test_status=2
    fi
  done < <(python3 -c '
import json, sys
with open(sys.argv[1], encoding="utf-8") as handle:
    manifest = json.load(handle)
for entry in manifest.get("unit_test_filters", []):
    print(entry["binary"] + "\t" + entry["filter"])
' "$manifest")
fi

mapfile -t manifest_scenarios < <(python3 -c '
import json, sys
with open(sys.argv[1], encoding="utf-8") as handle:
    manifest = json.load(handle)
default = int(manifest.get("scenario_defaults", {}).get("repeats", 1))
for entry in manifest.get("scenarios", []):
    print(entry["id"] + "\t" + str(int(entry.get("repeats", default))))
' "$manifest" | cut -f1)

repeats_for() {
  python3 -c '
import json, sys
with open(sys.argv[1], encoding="utf-8") as handle:
    manifest = json.load(handle)
default = int(manifest.get("scenario_defaults", {}).get("repeats", 1))
for entry in manifest.get("scenarios", []):
    if entry["id"] == sys.argv[2]:
        print(int(entry.get("repeats", default)))
        break
else:
    print(default)
' "$manifest" "$1"
}

if [[ ${#scenarios[@]} -eq 0 ]]; then
  scenarios=("${manifest_scenarios[@]}")
else
  for requested in "${scenarios[@]}"; do
    found=0
    for known in "${manifest_scenarios[@]}"; do
      [[ $requested == "$known" ]] && found=1 && break
    done
    if [[ $found -eq 0 ]]; then
      fail "scenario '$requested' is not in $manifest"
      exit 2
    fi
  done
fi

rm -rf "$artifact_dir"
mkdir -p "$artifact_dir"

commit=$(git rev-parse HEAD 2>/dev/null || echo unknown)
dirty=false
if ! git diff --quiet 2>/dev/null || ! git diff --cached --quiet 2>/dev/null; then
  dirty=true
fi
build_type=$(sed -n 's/^CMAKE_BUILD_TYPE:[^=]*=\(.*\)$/\1/p' "$build_dir/CMakeCache.txt" 2>/dev/null)
[[ -z $build_type ]] && build_type=unknown

export DISPLAY=${DISPLAY:-:0}

# A full sweep runs longer than the desktop's 600 s blanking timeout, and a
# blanked display stalls the presented frame loop rather than slowing it. The
# watchdog then fires on whichever scenario happens to be running at the ten
# minute mark -- a different one every time, which reads as a flaky scenario and
# is nothing of the kind. Blanking is suspended for the sweep and restored on
# exit, including on interrupt.
blanking_saved=0
saver_timeout=0
saver_cycle=0
dpms_standby=0
dpms_suspend=0
dpms_off=0
restore_screen_blanking() {
  if [[ $blanking_saved -eq 1 ]]; then
    xset s "$saver_timeout" "$saver_cycle" 2>/dev/null || true
    xset dpms "$dpms_standby" "$dpms_suspend" "$dpms_off" 2>/dev/null || true
  fi
}
if command -v xset >/dev/null 2>&1 && xset q >/dev/null 2>&1; then
  read -r saver_timeout saver_cycle < <(xset q | awk '/timeout:/ { print $2, $4; exit }')
  read -r dpms_standby dpms_suspend dpms_off < <(
    xset q | awk '/Standby:/ { print $2, $4, $6; exit }'
  )
  blanking_saved=1
  trap restore_screen_blanking EXIT INT TERM
  xset s off -dpms 2>/dev/null || true
fi

python3 - "$artifact_dir/gate_run.json" "$commit" "$dirty" "$build_type" "$fps" "$DISPLAY" <<'PY'
import json, platform, sys

path, commit, dirty, build_type, fps, display = sys.argv[1:7]
with open(path, "w", encoding="utf-8") as handle:
    json.dump(
        {
            "commit": commit,
            "dirty_worktree": dirty == "true",
            "build_type": build_type,
            "fixed_fps": int(fps),
            "display": display,
            "host": platform.node(),
            "platform": platform.platform(),
            "sequential": True,
        },
        handle,
        indent=2,
    )
    handle.write("\n")
PY

step "Running ${#scenarios[@]} RPG scenarios sequentially at ${fps} Hz into $artifact_dir"
echo "    commit $commit (dirty=$dirty), build type $build_type, DISPLAY=$DISPLAY"

run_status=0
for scenario in "${scenarios[@]}"; do
  repeats=$(repeats_for "$scenario")
  if [[ $repeats -gt 1 ]]; then
    echo "  ${bold}$scenario${reset} (x$repeats)"
  else
    echo "  ${bold}$scenario${reset}"
  fi
  for ((run = 1; run <= repeats; run++)); do
    if [[ $run -eq 1 ]]; then
      run_root=$artifact_dir
      log="$artifact_dir/$scenario.log"
    else
      run_root=$artifact_dir/repeats/run-$run
      mkdir -p "$run_root"
      log="$run_root/$scenario.log"
    fi
    args=(--batch --scenario "$scenario" --fps "$fps"
      --capture-interval "$capture_interval" --artifact-dir "$run_root")
    if [[ -n $duration_override ]]; then
      args+=(--duration "$duration_override")
    fi
    if [[ -n $watchdog_multiplier ]]; then
      args+=(--watchdog-multiplier "$watchdog_multiplier")
    fi
    if ! "$build_dir/bin/arena_app" "${args[@]}" >"$log" 2>&1; then
      echo "    ${yellow}run $run/$repeats exited nonzero; see $log${reset}"
    fi
    if [[ ! -f $run_root/$scenario/report.json ]]; then
      echo "    ${red}run $run/$repeats wrote no report.json${reset}"
      run_status=3
    fi
  done
done

step "Merging run_config with the commit and build type"
python3 - "$artifact_dir" "$commit" "$dirty" "$build_type" <<'PY'
import json
import sys
from pathlib import Path

root, commit, dirty, build_type = Path(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4]
for config_path in sorted(root.glob("**/run_config.json")):
    try:
        with config_path.open(encoding="utf-8") as handle:
            config = json.load(handle)
    except (OSError, json.JSONDecodeError):
        continue
    config["commit"] = commit
    config["dirty_worktree"] = dirty == "true"
    config["build_type"] = build_type
    with config_path.open("w", encoding="utf-8") as handle:
        json.dump(config, handle, indent=2)
        handle.write("\n")
PY

step "Gate summary"
report_args=("$artifact_dir" --manifest "$manifest" --json "$artifact_dir/gate_summary.json")
if [[ $enforce_performance -eq 1 ]]; then
  report_args+=(--enforce-performance)
fi
for scenario in "${scenarios[@]}"; do
  report_args+=(--scenario "$scenario")
done
python3 scripts/rpg_gate_report.py "${report_args[@]}"
summary_status=$?

status=0
if [[ $test_status -ne 0 ]]; then
  status=$test_status
elif [[ $summary_status -ne 0 ]]; then
  status=$summary_status
elif [[ $run_status -ne 0 ]]; then
  status=$run_status
fi

echo
if [[ $status -eq 0 ]]; then
  echo "${green}${bold}RPG gate: every scenario matched its manifest expectation.${reset}"
else
  echo "${red}${bold}RPG gate failed (exit $status). Artifacts: $artifact_dir${reset}"
fi
exit $status
