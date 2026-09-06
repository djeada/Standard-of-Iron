#!/usr/bin/env bash
# Runs the project's C++ test binaries.
#
# There is one suite list, below, and both profiles build and run all of it.
# The profiles differ only in which individual tests they execute:
#
#   full  everything. Weekly sanitizer and coverage lanes, extended validation
#         and release verification use this, plus the acceptance binaries that
#         are not GoogleTest suites (the gameplay verifier, the QML suite and
#         the headless replay round trip).
#   pr    everything except the tests named in tests/extended_tests.txt: the
#         headless battles and the sweeps over every shipped asset. Those spend
#         seconds each and had grown to over ninety minutes, which is longer
#         than a pull-request lane is allowed to take. Every test binary is
#         still built and still run.
#
# Set SOI_TEST_REPORT_DIR to collect GoogleTest JSON reports, one per suite;
# scripts/check-test-speed.py reads them to keep the fast profile fast.
#
# usage: scripts/run-tests.sh [build-dir] [extra gtest args...]

set -uo pipefail

build_dir=${1:-build}
if [ $# -gt 0 ]; then
  shift
fi
bin_dir="${build_dir}/bin"
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)

# Keep in step with soi_test_binaries in tests/CMakeLists.txt.
# tests/architecture/module_boundary_test.cpp parses the first suite-array
# declaration below and fails if it drifts from CMake.
suites=(
  simulation_tests
  combat_balance_tests
  ai_tests
  campaign_tests
  persistence_tests
  render_tests
  app_tests
  arena_tests
  tools_tests
)

profile=${SOI_TEST_PROFILE:-full}
case "${profile}" in
  full)
    run_extended=1
    ;;
  pr)
    run_extended=0
    ;;
  *)
    echo "error: unknown SOI_TEST_PROFILE '${profile}' (expected 'pr' or 'full')" >&2
    exit 2
    ;;
esac

manifest="${repo_root}/tests/extended_tests.txt"
gtest_filter=()
if [ "${run_extended}" -eq 0 ]; then
  if [ ! -f "${manifest}" ]; then
    echo "error: ${manifest} is missing; the pr profile cannot subtract from it." >&2
    exit 2
  fi
  # Comments and blank lines out, one ':'-joined negative filter in.
  excluded=$(sed -e 's/#.*//' -e 's/[[:space:]]//g' "${manifest}" |
    grep -v '^$' | paste -sd:)
  if [ -z "${excluded}" ]; then
    echo "error: ${manifest} lists no patterns." >&2
    exit 2
  fi
  gtest_filter=("--gtest_filter=-${excluded}")
fi

export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-offscreen}

report_dir=${SOI_TEST_REPORT_DIR:-}
if [ -n "${report_dir}" ]; then
  mkdir -p "${report_dir}"
fi

resolve() {
  if [ -x "${bin_dir}/$1" ]; then
    echo "${bin_dir}/$1"
  elif [ -x "${bin_dir}/$1.exe" ]; then
    echo "${bin_dir}/$1.exe"
  fi
}

status=0
failed=()

for suite in "${suites[@]}"; do
  binary=$(resolve "${suite}")
  if [ -z "${binary}" ]; then
    echo "error: ${bin_dir}/${suite} not built." >&2
    status=1
    failed+=("${suite} (not built)")
    continue
  fi

  report=()
  if [ -n "${report_dir}" ]; then
    report=("--gtest_output=json:${report_dir}/${suite}.json")
  fi

  echo "--- ${suite} (${profile}) ---"
  # The profile filter goes first so an explicit --gtest_filter in "$@" -- the
  # way a developer narrows a run by hand -- still wins.
  if ! "${binary}" "${gtest_filter[@]+"${gtest_filter[@]}"}" \
    "${report[@]+"${report[@]}"}" "$@"; then
    status=1
    failed+=("${suite}")
  fi
done

# The acceptance and presentation binaries below are not GoogleTest suites, so
# there is nothing in them for a test-level filter to subtract. They are whole
# scenarios measured in minutes and belong to the full profile only.
if [ "${run_extended}" -eq 1 ]; then
  verifier=$(resolve battlefield_gameplay_verifier)
  if [ -n "${verifier}" ]; then
    echo "--- battlefield_gameplay_verifier ---"
    if ! "${verifier}" --all --seconds "${SOI_VERIFIER_SECONDS:-15}" \
      --determinism-runs 2; then
      status=1
      failed+=("battlefield_gameplay_verifier")
    fi
  else
    echo "error: ${bin_dir}/battlefield_gameplay_verifier not built." >&2
    status=1
    failed+=("battlefield_gameplay_verifier (not built)")
  fi

  qml_binary=$(resolve design_system_qml_tests)
  if [ -n "${qml_binary}" ]; then
    echo "--- design_system_qml_tests ---"
    if ! "${qml_binary}" -input tests/ui/qml; then
      status=1
      failed+=("design_system_qml_tests")
    fi
  else
    echo "--- design_system_qml_tests skipped (Qt QuickTest not available) ---"
  fi

  headless=$(resolve soi_headless)
  if [ -n "${headless}" ]; then
    echo "--- headless_replay_round_trip ---"
    if ! SOI_HEADLESS="${headless}" bash scripts/check-headless-replay.sh; then
      status=1
      failed+=("headless_replay_round_trip")
    fi
  else
    echo "error: ${bin_dir}/soi_headless not built." >&2
    status=1
    failed+=("headless_replay_round_trip (not built)")
  fi
fi

if [ ${status} -ne 0 ]; then
  echo ""
  echo "failing suites: ${failed[*]}" >&2
fi

exit ${status}
