#!/bin/bash
# `standard_of_iron --version` must print the version and exit 0 without
# opening a window. It runs with no display and the xcb platform forced, so
# any attempt to create the GUI application would fail the test.
#
# usage: tests/version_option_test.sh [APP_BINARY]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
APP="${1:-$BUILD_DIR/bin/standard_of_iron}"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

fail() {
  echo -e "${RED}✗ $1${NC}"
  if [ -n "${2:-}" ] && [ -f "$2" ]; then
    cat "$2"
  fi
  exit 1
}

echo "Version option tests"
echo "===================="
echo ""

if [ ! -x "$APP" ]; then
  fail "Game binary not found at: $APP"
fi

expected_version=$(sed -n 's/^project([^)]*VERSION \([0-9.]*\).*/\1/p' \
  "$REPO_ROOT/CMakeLists.txt" | head -n 1)
if [ -z "$expected_version" ]; then
  fail "Could not read the project version from CMakeLists.txt"
fi
expected_first_line="Standard of Iron $expected_version"

TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT

for flag in --version -v; do
  echo "Test: $flag prints the version without a display..."
  out="$TEMP_DIR/out.txt"
  err="$TEMP_DIR/err.txt"
  status=0
  timeout 20s env -u DISPLAY -u WAYLAND_DISPLAY QT_QPA_PLATFORM=xcb \
    "$APP" "$flag" >"$out" 2>"$err" || status=$?
  if [ "$status" -ne 0 ]; then
    cat "$out"
    fail "$flag exited with status $status" "$err"
  fi
  first_line=$(head -n 1 "$out")
  if [ "$first_line" != "$expected_first_line" ]; then
    fail "$flag printed '$first_line', expected '$expected_first_line'" "$out"
  fi
  if ! grep -Eq '^Qt: [0-9]+\.[0-9]+' "$out"; then
    fail "$flag did not report the Qt version" "$out"
  fi
  if grep -Eq 'QGuiApplication|\[Pre-Init\]|OpenGL' "$out" "$err"; then
    fail "$flag started window or OpenGL set-up" "$err"
  fi
  echo -e "${GREEN}✓ $flag passed${NC}"
done

echo ""
echo "Test: --help lists --version..."
help="$TEMP_DIR/help.txt"
status=0
timeout 60s env QT_QPA_PLATFORM=offscreen "$APP" --help >"$help" 2>&1 || status=$?
if [ "$status" -ne 0 ]; then
  fail "--help exited with status $status" "$help"
fi
if ! grep -q -- '--version' "$help"; then
  fail "--help does not list --version" "$help"
fi
echo -e "${GREEN}✓ --help passed${NC}"

echo ""
echo -e "${GREEN}All version option tests passed${NC}"
