#!/usr/bin/env bash
# Runs every C++ test binary the project builds.
#
# The Makefile and all four CI workflows call this script, so there is one
# answer to "what does running the tests mean" instead of five that drift. A
# binary listed here that is missing from the build directory is an error, not a
# skip -- silently not running a suite is how a suite stops working.
#
# usage: scripts/run-tests.sh [build-dir] [extra gtest args...]

set -uo pipefail

build_dir=${1:-build}
if [ $# -gt 0 ]; then
  shift
fi
bin_dir="${build_dir}/bin"

# Keep in step with soi_test_binaries in tests/CMakeLists.txt.
suites=(simulation_tests persistence_tests render_tests app_tests tools_tests)

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
    echo "error: ${bin_dir}/${suite} not built. Run 'make test-build' first." >&2
    status=1
    failed+=("${suite} (not built)")
    continue
  fi

  echo "--- ${suite} ---"
  if ! "${binary}" "$@"; then
    status=1
    failed+=("${suite}")
  fi
done

# The QML design-system suite needs Qt QuickTest, which a minimal Qt install may
# not ship. That one is genuinely optional, and CMake says so by not defining
# the target at all.
# The battlefield verifier is the acceptance gate for gameplay behaviour, not a
# unit test: it drives the production system registry and asserts on movement,
# combat, responsiveness and AI. docs/GAMEPLAY_VERIFICATION.md has always called
# it the gate to run before a release candidate, but it lived only in CTest, so
# no pull request and no release tag ever ran it. It runs here now, at the same
# 15 seconds per scenario CTest uses; the 60-second soak stays a manual step.
verifier=$(resolve battlefield_gameplay_verifier)
if [ -n "${verifier}" ]; then
  echo "--- battlefield_gameplay_verifier ---"
  if ! "${verifier}" --all --seconds "${SOI_VERIFIER_SECONDS:-15}"; then
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

if [ ${status} -ne 0 ]; then
  echo ""
  echo "failing suites: ${failed[*]}" >&2
fi

exit ${status}
