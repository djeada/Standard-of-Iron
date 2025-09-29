#!/usr/bin/env bash

# Standard-of-Iron — dependency checker and auto-installer (Debian/Ubuntu + Arch/Manjaro)
#
# Verifies required toolchain and Qt/QML runtime modules and installs any missing ones.
# Safe to run multiple times. Requires sudo privileges for installation.
#
# Usage:
#   ./scripts/setup-deps.sh               # interactive install as needed
#   ./scripts/setup-deps.sh --yes         # non-interactive (assume yes)
#   ./scripts/setup-deps.sh --dry-run     # show actions without installing
#   ./scripts/setup-deps.sh --no-install  # only verify, do not install
#   ./scripts/setup-deps.sh --allow-similar  # allow proceeding on similar distros
#
set -euo pipefail

MIN_CMAKE="3.21.0"
MIN_GXX="10.0.0"

ASSUME_YES=false
DRY_RUN=false
NO_INSTALL=false
ALLOW_SIMILAR=false

for arg in "$@"; do
  case "$arg" in
    -y|--yes) ASSUME_YES=true ;;
    --dry-run) DRY_RUN=true ;;
    --no-install) NO_INSTALL=true ;;
    --allow-similar) ALLOW_SIMILAR=true ;;
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
  if ! command -v "$1" >/dev/null 2>&1; then
    err "Required command '$1' not found"
    return 1
  fi
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

read_os_release() {
  # shellcheck disable=SC1091
  if [ -f /etc/os-release ]; then
    . /etc/os-release
    echo "ID=${ID:-unknown}"
    echo "ID_LIKE=${ID_LIKE:-}"
    echo "PRETTY_NAME=${PRETTY_NAME:-}"
  else
    echo "ID=unknown"
    echo "ID_LIKE="
    echo "PRETTY_NAME="
  fi
}

has_apt()    { command -v apt-get >/dev/null 2>&1; }
has_pacman() { command -v pacman  >/dev/null 2>&1; }

is_deb_family_exact() {
  case "$1" in
    ubuntu|debian|linuxmint|pop) return 0 ;;
    *) return 1 ;;
  esac
}
is_deb_family_like() {
  case " $1 " in
    *" debian "*|*" ubuntu "*) return 0 ;;
    *) return 1 ;;
  esac
}

is_arch_family_exact() {
  case "$1" in
    arch|manjaro|endeavouros|garuda) return 0 ;;
    *) return 1 ;;
  esac
}
is_arch_family_like() {
  case " $1 " in
    *" arch "*) return 0 ;;
    *) return 1 ;;
  esac
}

detect_distro() {
  local id like pretty
  while IFS='=' read -r k v; do
    case "$k" in
      ID) id=${v} ;;
      ID_LIKE) like=${v} ;;
      PRETTY_NAME) pretty=${v} ;;
    esac
  done < <(read_os_release)

  id=${id%\"}; id=${id#\"}
  like=${like%\"}; like=${like#\"}
  pretty=${pretty%\"}; pretty=${pretty#\"}

  echo "$id" "$like" "$pretty"
}

# ===================== Package maps =====================

# APT (Debian/Ubuntu) toolchain + GL/Vulkan
APT_PKGS=(
  build-essential
  cmake
  git
  pkg-config
  libgl1-mesa-dev
  mesa-utils
  libglx-mesa0
  mesa-vulkan-drivers
  libegl1
)

# APT Qt6 dev headers/tools (filtered later)
QT6_DEV_PKGS=(
  qt6-base-dev
  qt6-base-dev-tools
  qt6-declarative-dev
  qt6-tools-dev
  qt6-tools-dev-tools
)

# APT Qt6 QML runtime modules (filtered)
QT6_QML_RUN_PKGS=(
  qml6-module-qtqml
  qml6-module-qtquick
  qml6-module-qtquick-window
  qml6-module-qtquick-layouts
  qml6-module-qtquick-controls
  qml6-module-qtqml-workerscript
  qml6-module-qtquick-templates
)

# APT Qt5 fallback (filtered)
QT5_QML_RUN_PKGS=(
  qml-module-qtqml
  qml-module-qtquick2
  qml-module-qtquick-window2
  qml-module-qtquick-layouts
  qml-module-qtquick-controls2
  qtbase5-dev
  qtdeclarative5-dev
)

# PACMAN (Arch/Manjaro) toolchain + GL/Vulkan
PAC_PKGS=(
  base-devel      # build tools incl. gcc/g++
  cmake
  git
  pkgconf         # provides pkg-config
  mesa            # GL/EGL/GLX (via libglvnd)
  mesa-demos      # glxinfo/glxgears (like mesa-utils)
  libglvnd
  vulkan-icd-loader
  vulkan-tools
)

# PACMAN Qt6 dev / runtime
QT6_DEV_PAC=(
  qt6-base
  qt6-declarative
  qt6-tools
  qt6-shadertools
)

QT6_QML_RUN_PAC=(
  qt6-declarative
  qt6-quickcontrols2
)

# PACMAN Qt5 fallback
QT5_QML_RUN_PAC=(
  qt5-base
  qt5-declarative
  qt5-quickcontrols2
)

# ===================== APT helpers =====================

apt_pkg_available() { apt-cache show "$1" >/dev/null 2>&1; }

filter_available_pkgs_apt() {
  local out=() p
  for p in "$@"; do
    if apt_pkg_available "$p"; then
      out+=("$p")
    fi
  done
  printf '%s\n' "${out[@]}"
}

dpkg_installed() { dpkg -s "$1" >/dev/null 2>&1; }

apt_update_once() {
  if [ "${_APT_UPDATED:-0}" != 1 ]; then
    info "Updating apt package lists"
    if $DRY_RUN; then
      echo "sudo apt-get update -y"
    else
      sudo apt-get update -y
    fi
    _APT_UPDATED=1
  fi
}

apt_install() {
  local to_install=()
  local pkg
  for pkg in "$@"; do
    [ -n "${pkg:-}" ] || continue
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
      case "${ans:-Y}" in y|Y) ;; *) warn "User declined install"; return 0 ;; esac
    fi
    apt_update_once
    info "Installing: ${to_install[*]}"
    if $DRY_RUN; then
      echo "DEBIAN_FRONTEND=noninteractive sudo apt-get install -y ${to_install[*]}"
    else
      DEBIAN_FRONTEND=noninteractive sudo apt-get install -y "${to_install[@]}"
    fi
  fi
}

# ===================== PACMAN helpers =====================

pacman_pkg_available() { sudo pacman -Si "$1" >/dev/null 2>&1 || pacman -Si "$1" >/dev/null 2>&1; }

filter_available_pkgs_pacman() {
  local out=() p
  for p in "$@"; do
    if pacman_pkg_available "$p"; then
      out+=("$p")
    fi
  done
  printf '%s\n' "${out[@]}"
}

pacman_installed() { pacman -Qi "$1" >/dev/null 2>&1; }

pacman_update_once() {
  if [ "${_PAC_UPDATED:-0}" != 1 ]; then
    info "Refreshing pacman package databases"
    if $DRY_RUN; then
      echo "sudo pacman -Sy"
    else
      sudo pacman -Sy --noconfirm
    fi
    _PAC_UPDATED=1
  fi
}

pacman_install() {
  local to_install=()
  local pkg
  for pkg in "$@"; do
    [ -n "${pkg:-}" ] || continue
    if pacman_installed "$pkg"; then
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
      read -r -p "Install missing packages (pacman): ${to_install[*]} ? [Y/n] " ans
      case "${ans:-Y}" in y|Y) ;; *) warn "User declined install"; return 0 ;; esac
    fi
    pacman_update_once
    info "Installing: ${to_install[*]}"
    local args=(-S --needed)
    $ASSUME_YES && args+=(--noconfirm)
    if $DRY_RUN; then
      echo "sudo pacman ${args[*]} ${to_install[*]}"
    else
      sudo pacman "${args[@]}" "${to_install[@]}"
    fi
  fi
}

# ===================== Checks =====================

check_tool_versions() {
  info "Checking toolchain versions"

  need_cmd cmake
  local cmv
  cmv=$(cmake --version | head -n1 | awk '{print $3}')
  if semver_ge "$cmv" "$MIN_CMAKE"; then
    ok "cmake $cmv (>= $MIN_CMAKE)"
  else
    warn "cmake $cmv (< $MIN_CMAKE) — will attempt to install newer via package manager"
  fi

  need_cmd g++
  local gxv
  gxv=$(g++ --version | head -n1 | awk '{print $NF}')
  if semver_ge "$gxv" "$MIN_GXX"; then
    ok "g++ $gxv (>= $MIN_GXX)"
  else
    warn "g++ $gxv (< $MIN_GXX) — will attempt to install compiler toolchain"
  fi

  need_cmd git; ok "git $(git --version | awk '{print $3}')"
  need_cmd pkg-config || need_cmd pkgconf
  if command -v pkg-config >/dev/null 2>&1; then
    ok "pkg-config $(pkg-config --version)"
  else
    ok "pkgconf $(pkgconf --version)"
  fi
}

install_runtime_apt() {
  info "Installing base toolchain (APT)"
  apt_install "${APT_PKGS[@]}"

  info "Installing Qt6 SDK/dev packages (APT)"
  mapfile -t _qt6dev < <(filter_available_pkgs_apt "${QT6_DEV_PKGS[@]}")
  apt_install "${_qt6dev[@]}"

  info "Installing Qt6 QML runtime modules (APT)"
  mapfile -t _qt6qml < <(filter_available_pkgs_apt "${QT6_QML_RUN_PKGS[@]}")
  apt_install "${_qt6qml[@]}"

  info "Installing Qt5 QML runtime modules (fallback, if available; APT)"
  mapfile -t _qt5qml < <(filter_available_pkgs_apt "${QT5_QML_RUN_PKGS[@]}")
  apt_install "${_qt5qml[@]}"
}

install_runtime_pacman() {
  info "Installing base toolchain (pacman)"
  pacman_install "${PAC_PKGS[@]}"

  info "Installing Qt6 SDK/dev packages (pacman)"
  mapfile -t _qt6dev < <(filter_available_pkgs_pacman "${QT6_DEV_PAC[@]}")
  pacman_install "${_qt6dev[@]}"

  info "Ensuring Qt6 QML runtime (pacman)"
  mapfile -t _qt6qml < <(filter_available_pkgs_pacman "${QT6_QML_RUN_PAC[@]}")
  pacman_install "${_qt6qml[@]}"

  info "Installing Qt5 QML runtime (fallback, if available; pacman)"
  mapfile -t _qt5qml < <(filter_available_pkgs_pacman "${QT5_QML_RUN_PAC[@]}")
  pacman_install "${_qt5qml[@]}"
}

main() {
  local id like pretty
  read -r id like pretty < <(detect_distro)

  info "Detected system: ${pretty:-$id} (ID=$id; ID_LIKE='${like:-}')."

  local pm=""
  if is_deb_family_exact "$id"; then
    pm="apt"; info "Exact Debian/Ubuntu family detected ($id)."
  elif is_arch_family_exact "$id"; then
    pm="pacman"; info "Exact Arch/Manjaro family detected ($id)."
  elif is_deb_family_like "${like:-}" && has_apt; then
    pm="apt"
    warn "No exact match, but this system is *similar* to Debian/Ubuntu and has apt-get."
    if $ALLOW_SIMILAR || $ASSUME_YES; then
      info "Proceeding with Debian/Ubuntu-compatible steps due to --allow-similar/--yes."
    else
      echo
      read -r -p "Proceed using Debian/Ubuntu package set on this similar distro? [Y/n] " ans
      case "${ans:-Y}" in y|Y) info "Continuing with Debian/Ubuntu-compatible steps." ;;
        *) err "User declined proceeding on a similar distro."; exit 1 ;; esac
    fi
  elif is_arch_family_like "${like:-}" && has_pacman; then
    pm="pacman"
    warn "No exact match, but this system is *similar* to Arch and has pacman."
    if $ALLOW_SIMILAR || $ASSUME_YES; then
      info "Proceeding with Arch-compatible steps due to --allow-similar/--yes."
    else
      echo
      read -r -p "Proceed using Arch/Manjaro package set on this similar distro? [Y/n] " ans
      case "${ans:-Y}" in y|Y) info "Continuing with Arch-compatible steps." ;;
        *) err "User declined proceeding on a similar distro."; exit 1 ;; esac
    fi
  else
    # Fall back based on package manager presence
    if has_apt; then
      pm="apt"
      warn "Unknown distro '$id', but apt-get is present."
      $ALLOW_SIMILAR || $ASSUME_YES || {
        read -r -p "Proceed using apt-based steps? (may or may not work) [y/N] " ans
        case "${ans:-N}" in y|Y) ;; *) err "Exiting. Use --allow-similar to override."; exit 1 ;; esac
      }
    elif has_pacman; then
      pm="pacman"
      warn "Unknown distro '$id', but pacman is present."
      $ALLOW_SIMILAR || $ASSUME_YES || {
        read -r -p "Proceed using pacman-based steps? (may or may not work) [y/N] " ans
        case "${ans:-N}" in y|Y) ;; *) err "Exiting. Use --allow-similar to override."; exit 1 ;; esac
      }
    else
      err "No supported package manager found (apt-get or pacman). Please install equivalents manually."
      echo "Required (roughly):"
      echo " - build tools (gcc/g++), cmake >= $MIN_CMAKE, git, pkg-config"
      echo " - OpenGL/EGL/GLX/Vulkan runtime (Mesa + ICD loader + tools)"
      echo " - Qt6 base + declarative + tools + Quick Controls 2 (or Qt5 fallback)"
      exit 1
    fi
  fi

  check_tool_versions

  case "$pm" in
    apt)    install_runtime_apt ;;
    pacman) install_runtime_pacman ;;
    *)      err "Internal error: unknown package manager '$pm'"; exit 1 ;;
  esac

  echo
  ok "All required dependencies are present (or have been installed)."
  echo "- cmake >= $MIN_CMAKE"
  echo "- g++   >= $MIN_GXX"
  echo "- Qt 6 base + declarative + tools + QML runtime modules (Qt5 QML fallback if available)"
}

main "$@"
