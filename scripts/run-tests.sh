#!/usr/bin/env bash
# Runs the project's C++ test binaries.
#
# The default profile is the complete suite used by weekly/release validation.
# Pull requests set SOI_TEST_PROFILE=pr to run only the fast headless/core suites;
# renderer/application/arena/tool suites and long acceptance checks stay in the
# scheduled/manual workflows.
#
# usage: scripts/run-tests.sh [build-dir] [extra gtest args...]

set -uo pipefail

build_dir=${1:-build}
if [ $# -gt 0 ]; then
  shift
fi
bin_dir="${build_dir}/bin"

# Keep in step with soi_test_binaries in tests/CMakeLists.txt.
# tests/architecture/module_boundary_test.cpp parses this first suites=(...) list
# and fails if the complete default suite drifts from CMake.
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
    suites=(
      simulation_tests
      combat_balance_tests
      ai_tests
      campaign_tests
      persistence_tests
    )
    run_extended=0
    ;;
  *)
    echo "error: unknown SOI_TEST_PROFILE '${profile}' (expected 'pr' or 'full')" >&2
    exit 2
    ;;
esac

export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-offscreen}

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
    echo "error: ${bin_dir}/${suite} not built for ${profile} profile." >&2
    status=1
    failed+=("${suite} (not built)")
    continue
  fi

  echo "--- ${suite} (${profile}) ---"
  if ! "${binary}" "$@"; then
    status=1
    failed+=("${suite}")
  fi
done

# The complete profile owns the expensive acceptance and presentation suites.
# They are intentionally absent from pull-request CI: weekly/release validation
# still runs them with the same script, so there is one full-suite definition.
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
