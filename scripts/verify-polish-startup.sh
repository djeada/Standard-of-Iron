#!/usr/bin/env bash
# Exercise the main menu, campaign overlay and gameplay renderer with a saved
# Polish profile. This catches localized QML layout loops before they ship.

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

binary="${1:?usage: verify-polish-startup.sh APP_BINARY}"
if [[ "$binary" != /* ]]; then
  binary="$repo_root/$binary"
fi
if [[ ! -x "$binary" ]]; then
  echo "Application binary not found or not executable: $binary" >&2
  exit 2
fi

runner=()
if [[ -z "${DISPLAY:-}" ]]; then
  if ! command -v xvfb-run >/dev/null 2>&1; then
    echo "xvfb-run is required when DISPLAY is not set" >&2
    exit 2
  fi
  runner=(xvfb-run --auto-servernum --)
fi

profile_dir="$(mktemp -d "${TMPDIR:-/tmp}/soi-polish-startup.XXXXXX")"
trap 'rm -rf -- "$profile_dir"' EXIT
mkdir -p "$profile_dir/djeada"
printf '[ui]\nlanguage=pl\n' >"$profile_dir/djeada/StandardOfIron.ini"

run_check() {
  local label="$1"
  local marker="$2"
  local log_file="$3"
  shift 3

  printf '>> %s\n' "$label"
  if ! timeout 45s "${runner[@]}" env \
    "XDG_CONFIG_HOME=$profile_dir" \
    "LIBGL_ALWAYS_SOFTWARE=1" \
    "QT_OPENGL=desktop" \
    "QT_LOGGING_RULES=qt.rhi.*=false" \
    "$binary" "$@" >"$log_file" 2>&1; then
    tail -n 60 "$log_file"
    return 1
  fi
  if ! grep -Fq -- "$marker" "$log_file"; then
    echo "Expected marker was missing: $marker" >&2
    tail -n 60 "$log_file"
    return 1
  fi
}

run_check "Polish main menu frame" "SOI_SCREENSHOT: PASS" \
  "$profile_dir/menu.log" \
  --screenshot "$profile_dir/menu.png" --screenshot-view menu --screenshot-delay 1000
run_check "Polish campaign screen frame" "SOI_SCREENSHOT: PASS" \
  "$profile_dir/campaign.log" \
  --screenshot "$profile_dir/campaign.png" --screenshot-view campaign --screenshot-delay 1200
run_check "Polish gameplay frame" "SOI_RENDERER_SELF_TEST: PASS" \
  "$profile_dir/renderer.log" --renderer-self-test

echo "Polish startup checks passed"
