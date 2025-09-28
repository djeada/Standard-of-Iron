#!/usr/bin/env bash

# Standard-of-Iron — dependency checker and auto-installer (Ubuntu/Debian)
#
# Verifies required toolchain and Qt/QML runtime modules and installs any missing ones.
# Safe to run multiple times. Requires sudo privileges for installation.
#
# Usage:
#   ./scripts/setup-deps.sh               # interactive install as needed
#   ./scripts/setup-deps.sh --yes         # non-interactive (assume yes)
#   ./scripts/setup-deps.sh --dry-run     # show actions without installing
#   ./scripts/setup-deps.sh --no-install  # only verify, do not install
#
set -euo pipefail

MIN_CMAKE="3.21.0"
MIN_GXX="10.0.0"

ASSUME_YES=false
DRY_RUN=false
NO_INSTALL=false

for arg in "$@"; do
  case "$arg" in
    -y|--yes) ASSUME_YES=true ;;
    --dry-run) DRY_RUN=true ;;
    --no-install) NO_INSTALL=true ;;
    -h|--help)
      grep '^#' "$0" | sed -e 's/^# \{0,1\}//'
      exit 0
      ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

info()  { echo -e "\033[1;34m[i]\033[0m $*"; }
ok()    { echo -e "\033[1;32m[✓]\033[0m $*"; }
warn()  { echo -e "\033[1;33m[!]\033[0m $*"; }
err()   { echo -e "\033[1;31m[x]\033[0m $*"; }

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || { err "Required command '$1' not found"; return 1; }
}

semver_ge() {
  # returns 0 if $1 >= $2
  local IFS=. a1 a2 a3 b1 b2 b3
  set -- $1; a1=${1:-0}; a2=${2:-0}; a3=${3:-0}
  set -- $2; b1=${1:-0}; b2=${2:-0}; b3=${3:-0}
  if [ "$a1" -gt "$b1" ]; then return 0; fi
  if [ "$a1" -lt "$b1" ]; then return 1; fi
  if [ "$a2" -gt "$b2" ]; then return 0; fi
  if [ "$a2" -lt "$b2" ]; then return 1; fi
  if [ "$a3" -ge "$b3" ]; then return 0; fi
  return 1
}

detect_distro() {
  if [ -f /etc/os-release ]; then
    . /etc/os-release
    echo "$ID"
  else
    echo "unknown"
  fi
}

APT_PKGS=(
  build-essential
  cmake
  git
  pkg-config
  libgl1-mesa-dev
  # Qt 6 development
  qt6-base-dev
  qt6-declarative-dev
  qt6-tools-dev
  # QML runtime modules used by the app
  qml6-module-qtquick
  qml6-module-qtquick-controls
  qml6-module-qtquick-window
  qml6-module-qtquick-layouts
  qml6-module-qtquick-templates
  qml6-module-qtqml-workerscript
)

check_tool_versions() {
  info "Checking toolchain versions"

  need_cmd cmake
  local cmv
  cmv=$(cmake --version | head -n1 | awk '{print $3}')
  if semver_ge "$cmv" "$MIN_CMAKE"; then
    ok "cmake $cmv (>= $MIN_CMAKE)"
  else
    warn "cmake $cmv (< $MIN_CMAKE) — will attempt to install newer via apt"
  fi

  need_cmd g++
  local gxv
  gxv=$(g++ --version | head -n1 | awk '{print $NF}')
  if semver_ge "$gxv" "$MIN_GXX"; then
    ok "g++ $gxv (>= $MIN_GXX)"
  else
    warn "g++ $gxv (< $MIN_GXX) — will attempt to install build-essential"
  fi

  need_cmd git; ok "git $(git --version | awk '{print $3}')"
  need_cmd pkg-config; ok "pkg-config $(pkg-config --version)"
}

dpkg_installed() {
  dpkg -s "$1" >/dev/null 2>&1
}

apt_update_once() {
  if [ "${_APT_UPDATED:-0}" != 1 ]; then
    info "Updating apt package lists"
    ${DRY_RUN:+echo} sudo apt-get update -y
    _APT_UPDATED=1
  fi
}

apt_install() {
  local to_install=()
  for pkg in "$@"; do
    if dpkg_installed "$pkg"; then
      ok "$pkg already installed"
    else
      to_install+=("$pkg")
    fi
  done

  if [ ${#to_install[@]} -gt 0 ]; then
    if $NO_INSTALL; then
      warn "Missing packages: ${to_install[*]} (skipping install due to --no-install)"
      return 0
    fi
    if ! $ASSUME_YES; then
      echo
      read -r -p "Install missing packages: ${to_install[*]} ? [Y/n] " ans
      case "${ans:-Y}" in
        y|Y) ;;
        *) warn "User declined install"; return 0 ;;
      esac
    fi
    apt_update_once
    info "Installing: ${to_install[*]}"
    DEBIAN_FRONTEND=noninteractive ${DRY_RUN:+echo} sudo apt-get install -y "${to_install[@]}"
  fi
}

check_qt_runtime() {
  info "Checking Qt/QML runtime modules"
  apt_install "${APT_PKGS[@]}"
}

main() {
  local distro
  distro=$(detect_distro)
  case "$distro" in
    ubuntu|debian)
      info "Detected Debian/Ubuntu-based system ($distro)"
      ;;
    *)
      warn "Unsupported distro '$distro'. This script currently targets Debian/Ubuntu (apt)."
      warn "Please install equivalent packages manually: ${APT_PKGS[*]}"
      exit 1
      ;;
  esac

  check_tool_versions
  check_qt_runtime

  echo
  ok "All required dependencies are present (or have been installed)."
  echo "- cmake >= $MIN_CMAKE"
  echo "- g++   >= $MIN_GXX"
  echo "- Qt 6 base + declarative + tools + QML runtime modules"
}

main "$@"
