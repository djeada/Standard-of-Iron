#!/usr/bin/env bash
# Fast formatting check for CI: only inspect files changed against a git base.
set -euo pipefail

BASE_REF="${FORMAT_CHECK_BASE:-${GITHUB_BASE_REF:+origin/${GITHUB_BASE_REF}}}"
BASE_REF="${BASE_REF:-origin/main}"
JOBS="${FORMAT_JOBS:-}"

if [[ -z "$JOBS" ]]; then
  if command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
  else
    JOBS="4"
  fi
fi

if ! git rev-parse --verify "$BASE_REF" >/dev/null 2>&1; then
  echo "format-check-ci: base '$BASE_REF' is unavailable; falling back to HEAD~1."
  BASE_REF="HEAD~1"
fi

MERGE_BASE="$(git merge-base "$BASE_REF" HEAD 2>/dev/null || true)"
if [[ -z "$MERGE_BASE" ]]; then
  MERGE_BASE="$BASE_REF"
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

CXX_LIST="$TMP_DIR/cxx.files"
SHADER_LIST="$TMP_DIR/shader.files"
QML_LIST="$TMP_DIR/qml.files"
PY_LIST="$TMP_DIR/python.files"
: > "$CXX_LIST"
: > "$SHADER_LIST"
: > "$QML_LIST"
: > "$PY_LIST"

while IFS= read -r -d '' file; do
  [[ -f "$file" ]] || continue
  case "$file" in
    build/*|build-*/*|third_party/*|*/venv/*|*/.venv/*) continue ;;
    *.c|*.cc|*.cpp|*.cxx|*.h|*.hh|*.hpp|*.hxx) printf '%s\0' "$file" >> "$CXX_LIST" ;;
    *.frag|*.vert) printf '%s\0' "$file" >> "$SHADER_LIST" ;;
    *.qml) printf '%s\0' "$file" >> "$QML_LIST" ;;
    *.py) printf '%s\0' "$file" >> "$PY_LIST" ;;
  esac
done < <(git diff --name-only -z --diff-filter=ACMR "$MERGE_BASE"...HEAD)

FAILED=0

has_files() {
  [[ -s "$1" ]]
}

if has_files "$CXX_LIST"; then
  if command -v clang-format >/dev/null 2>&1; then
    echo "Checking changed C/C++ files with clang-format..."
    xargs -0 -r -n 64 -P "$JOBS" clang-format --dry-run -Werror --style=file < "$CXX_LIST" || FAILED=1
  else
    echo "clang-format not found."
    FAILED=1
  fi
fi

if has_files "$SHADER_LIST"; then
  if command -v clang-format >/dev/null 2>&1; then
    echo "Checking changed shader files with clang-format..."
    xargs -0 -r -n 64 -P "$JOBS" clang-format --dry-run -Werror --style=file < "$SHADER_LIST" || FAILED=1
  else
    echo "clang-format not found."
    FAILED=1
  fi
fi

QMLFORMAT="${QMLFORMAT:-}"
if [[ -z "$QMLFORMAT" ]]; then
  if command -v qmlformat >/dev/null 2>&1; then
    QMLFORMAT="$(command -v qmlformat)"
  elif command -v qmlformat6 >/dev/null 2>&1; then
    QMLFORMAT="$(command -v qmlformat6)"
  elif command -v qmlformat-qt6 >/dev/null 2>&1; then
    QMLFORMAT="$(command -v qmlformat-qt6)"
  elif [[ -x /usr/lib/qt6/bin/qmlformat ]]; then
    QMLFORMAT="/usr/lib/qt6/bin/qmlformat"
  elif [[ -x /usr/lib/qt5/bin/qmlformat ]]; then
    QMLFORMAT="/usr/lib/qt5/bin/qmlformat"
  fi
fi

if has_files "$QML_LIST"; then
  if [[ -n "$QMLFORMAT" && ( -x "$QMLFORMAT" || "$(command -v "$QMLFORMAT" 2>/dev/null)" ) ]]; then
    echo "Checking changed QML files with qmlformat..."
    while IFS= read -r -d '' file; do
      formatted="$TMP_DIR/$(basename "$file").formatted"
      "$QMLFORMAT" -w 4 "$file" > "$formatted" 2>/dev/null || FAILED=1
      if ! diff -q "$file" "$formatted" >/dev/null 2>&1; then
        echo "QML file needs formatting: $file"
        FAILED=1
      fi
    done < "$QML_LIST"
  else
    echo "qmlformat not found; skipping changed QML formatting check."
  fi
fi

if has_files "$PY_LIST"; then
  if command -v black >/dev/null 2>&1; then
    echo "Checking changed Python files with black..."
    xargs -0 -r black --check --quiet --workers "$JOBS" < "$PY_LIST" || FAILED=1
  else
    echo "black not found; skipping changed Python formatting check."
  fi
fi

if [[ "$FAILED" -eq 0 ]]; then
  echo "Changed-file formatting checks passed."
else
  echo "Changed-file formatting checks failed. Run 'make format' locally."
  exit 1
fi
