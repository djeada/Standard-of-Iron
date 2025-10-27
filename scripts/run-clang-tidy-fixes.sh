#!/usr/bin/env bash
# clang-tidy fixer: git-only by default; --all to scan whole project
# Runs only on files covered by compile_commands.json (safer).
# Parallelized via xargs -P using a temp helper script (bash).

set -euo pipefail

# ---------- Defaults ----------
BUILD_DIR_RELATIVE="build"
DEFAULT_LANG_VALUE="en"
SEARCH_PATHS_DEFAULT="app game render tools ui utils"
GIT_ONLY=1
GIT_BASE="${CLANG_TIDY_GIT_BASE:-origin/main}"

DEFAULT_CHECKS="-*,readability-braces-around-statements"
CHECKS_OVERRIDE="${CLANG_TIDY_AUTO_FIX_CHECKS:-$DEFAULT_CHECKS}"

HEADER_FILTER="${CLANG_TIDY_HEADER_FILTER:-^(?!.*third_party).*$}"
CLANG_TIDY_NICE="${CLANG_TIDY_NICE:-0}"

if command -v nproc >/dev/null 2>&1; then
  DEFAULT_JOBS=$(( ( $(nproc) + 1 ) / 2 ))
else
  DEFAULT_JOBS=2
fi
JOBS="${CLANG_TIDY_JOBS:-$DEFAULT_JOBS}"

QUIET=1
FIX_ERRORS=0
INCLUDE_HEADERS=0            # <- do NOT feed headers directly
ALLOW_UNCOVERED=0            # <- only run files present in compile DB by default
PASSES=1
RAW_PATHS=""
USER_EXPORT_FIXES_DIR=""
AGGRESSIVE=0

print_help() {
  cat <<'EOF'
Usage: scripts/run-clang-tidy-fixes.sh [options]

Options:
  --all                    Run on the whole project (disables git-only; enables verbose)
  --base=<ref>             Git base to diff against (default: origin/main)
  --paths="<p1 p2 ...>"    Space- or comma-separated root paths to search
  --jobs=<N>               Parallel jobs (xargs -P). Default: ~half cores
  --checks="<pattern>"     Override -checks (e.g., "-*,bugprone-*")
  --nice | --no-nice       Lower (or not) CPU/IO priority for clang-tidy
  --build-dir=<dir>        Build dir with compile_commands.json (default: build)
  --default-lang=<val>     CMake DEFAULT_LANG cache var (default: en)

  --verbose | --no-quiet   Show clang-tidy output
  --quiet                  Force quiet
  --fix-errors             Use -fix-errors (stronger auto-fix)
  --passes=<N>             Run up to N passes (default: 1)
  --aggressive             Shortcut: --fix-errors + --passes=3
  --no-headers             (ignored; headers not run directly by default)
  --allow-uncovered        Attempt files not in compile DB (adds -std=c++20)
  --export-fixes=<dir>     Save per-file fixes YAMLs into this directory

  -h|--help                Show this help

Env (also supported):
  CLANG_TIDY_GIT_BASE, CLANG_TIDY_JOBS, CLANG_TIDY_AUTO_FIX_CHECKS,
  CLANG_TIDY_HEADER_FILTER, CLANG_TIDY_NICE, CLANG_TIDY_FIX_PATHS,
  CLANG_TIDY_EXTRA_ARG (single), CLANG_TIDY_EXTRA_ARGS (space-separated)
EOF
}

# ---------- Parse CLI ----------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --all) GIT_ONLY=0; QUIET=0; shift ;;
    --base=*) GIT_BASE="${1#*=}"; shift ;;
    --base) GIT_BASE="${2:-$GIT_BASE}"; shift 2 ;;
    --paths=*) RAW_PATHS="${1#*=}"; shift ;;
    --paths) RAW_PATHS="${2:-}"; shift 2 ;;
    --jobs=*) JOBS="${1#*=}"; shift ;;
    --jobs) JOBS="${2:-$JOBS}"; shift 2 ;;
    --checks=*) CHECKS_OVERRIDE="${1#*=}"; shift ;;
    --checks) CHECKS_OVERRIDE="${2:-$CHECKS_OVERRIDE}"; shift 2 ;;
    --nice) CLANG_TIDY_NICE=1; shift ;;
    --no-nice) CLANG_TIDY_NICE=0; shift ;;
    --build-dir=*) BUILD_DIR_RELATIVE="${1#*=}"; shift ;;
    --build-dir) BUILD_DIR_RELATIVE="${2:-$BUILD_DIR_RELATIVE}"; shift 2 ;;
    --default-lang=*) DEFAULT_LANG_VALUE="${1#*=}"; shift ;;
    --default-lang) DEFAULT_LANG_VALUE="${2:-$DEFAULT_LANG_VALUE}"; shift 2 ;;
    --verbose|--no-quiet) QUIET=0; VERBOSE_CMD=1; shift ;;
    --quiet) QUIET=1; shift ;;
    --fix-errors) FIX_ERRORS=1; shift ;;
    --passes=*) PASSES="${1#*=}"; shift ;;
    --passes) PASSES="${2:-$PASSES}"; shift 2 ;;
    --aggressive) AGGRESSIVE=1; FIX_ERRORS=1; PASSES=$(( PASSES < 3 ? 3 : PASSES )); shift ;;
    --no-headers) INCLUDE_HEADERS=0; shift ;;
    --allow-uncovered) ALLOW_UNCOVERED=1; shift ;;
    --export-fixes=*) USER_EXPORT_FIXES_DIR="${1#*=}"; shift ;;
    -h|--help) print_help; exit 0 ;;
    *) echo "Unknown option: $1"; print_help; exit 2 ;;
  esac
done

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# Paths to search
if [[ -n "${RAW_PATHS:-}" ]]; then
  RAW_PATHS="${RAW_PATHS//,/ }"
  IFS=' ' read -r -a SEARCH_PATHS <<< "$RAW_PATHS"
else
  IFS=' ' read -r -a SEARCH_PATHS <<< "${CLANG_TIDY_FIX_PATHS:-$SEARCH_PATHS_DEFAULT}"
fi

# ---------- Tools / Setup ----------
if ! command -v clang-tidy >/dev/null 2>&1; then
  echo "⚠ clang-tidy not found; skipping automatic lint fixes."
  exit 0
fi

[[ "$QUIET" -eq 0 ]] && clang-tidy --version || true

BUILD_DIR="${REPO_ROOT}/${BUILD_DIR_RELATIVE}"
mkdir -p "$BUILD_DIR"

if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
  echo "Generating compile_commands.json via CMake (DEFAULT_LANG=${DEFAULT_LANG_VALUE})..."
  cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DDEFAULT_LANG="${DEFAULT_LANG_VALUE}" >/dev/null
fi

if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
  echo "⚠ Unable to locate compile_commands.json in ${BUILD_DIR_RELATIVE}; skipping clang-tidy fixes."
  exit 0
fi

# ---------- Collect sources (filesystem) ----------
declare -a FS_SOURCES=()
SRC_GLOBS=( '*.c' '*.cc' '*.cpp' '*.cxx' )
# headers intentionally NOT included as standalone inputs

add_sources_from_find() {
  local root="$1"
  local -a find_expr=( \( -name "${SRC_GLOBS[0]}" )
  for g in "${SRC_GLOBS[@]:1}"; do find_expr+=( -o -name "$g" ); done
  find_expr+=( \) )
  while IFS= read -r -d '' FILE; do
    REL_PATH="${FILE#$REPO_ROOT/}"
    FS_SOURCES+=("$REL_PATH")
  done < <(find "$root" \
            -path "$root/third_party" -prune -o \
            -type f "${find_expr[@]}" \
            -print0)
}

if [[ "$GIT_ONLY" -eq 1 ]]; then
  mapfile -t GIT_FILES < <(git diff --name-only --diff-filter=ACMR "$GIT_BASE"... -- "${SRC_GLOBS[@]}" 2>/dev/null || true)
  for f in "${GIT_FILES[@]:-}"; do [[ -f "$f" ]] && FS_SOURCES+=("$f"); done
else
  for SEARCH_PATH in "${SEARCH_PATHS[@]}"; do
    ABS_PATH="$REPO_ROOT/$SEARCH_PATH"
    [[ -d "$ABS_PATH" ]] || continue
    add_sources_from_find "$ABS_PATH"
  done
fi

if [ ${#FS_SOURCES[@]} -eq 0 ]; then
  echo "No C/C++ sources found under: ${SEARCH_PATHS[*]}"
  exit 0
fi

# ---------- Build set of files from compile_commands.json ----------
DB_FILES_TXT="$(mktemp)"
if command -v jq >/dev/null 2>&1; then
  jq -r '.[].file' "$BUILD_DIR/compile_commands.json" | sed 's#^\./##' > "$DB_FILES_TXT"
else
  # best-effort without jq
  grep -oE '"file":\s*"[^"]+"' "$BUILD_DIR/compile_commands.json" | sed 's/^"file":\s*"\(.*\)"/\1/' > "$DB_FILES_TXT"
fi

# Normalize DB paths to repo-relative
DB_REL_TXT="$(mktemp)"
awk -v root="$REPO_ROOT/" '{ f=$0; sub("^"root, "", f); print f }' "$DB_FILES_TXT" > "$DB_REL_TXT"

# ---------- Intersect (default) or union (if --allow-uncovered) ----------
declare -a SOURCES=()
if [[ "$ALLOW_UNCOVERED" -eq 1 ]]; then
  # union: everything we found on FS; clang-tidy will guess for uncovered (may error)
  SOURCES=("${FS_SOURCES[@]}")
else
  # intersection: only files present in compile DB
  while IFS= read -r F; do
    if grep -Fxq "$f" "$DB_REL_TXT"; then
      SOURCES+=("$f")
    fi
  done < <(printf "%s\n" "${FS_SOURCES[@]}" | sort -u)
fi

covered=${#SOURCES[@]}
total=${#FS_SOURCES[@]}
echo "ℹ coverage: ${covered}/${total} source files are in compile_commands.json."

if [ ${#SOURCES[@]} -eq 0 ]; then
  echo "Nothing to run: none of the selected sources are in the compilation database."
  echo "Tip: build with CMake/Ninja or use 'bear -- cmake --build build' to capture compile commands."
  exit 0
fi

# ---------- Run config ----------
NICE_PREFIX=()
if [[ "$CLANG_TIDY_NICE" == "1" ]]; then
  command -v nice >/dev/null 2>&1 && NICE_PREFIX+=(nice -n 10)
  command -v ionice >/dev/null 2>&1 && NICE_PREFIX+=(ionice -c 2 -n 7)
fi

EXTRA_ARGS=()
[[ -n "${CLANG_TIDY_EXTRA_ARG:-}" ]] && EXTRA_ARGS+=("--extra-arg=${CLANG_TIDY_EXTRA_ARG}")
if [[ -n "${CLANG_TIDY_EXTRA_ARGS:-}" ]]; then
  # shellcheck disable=SC2206
  EXTRA_ARGS+=(${CLANG_TIDY_EXTRA_ARGS})
fi

# If attempting uncovered files, default to a reasonable standard to help parsing
if [[ "$ALLOW_UNCOVERED" -eq 1 && -z "${CLANG_TIDY_EXTRA_ARG:-}" && -z "${CLANG_TIDY_EXTRA_ARGS:-}" ]]; then
  EXTRA_ARGS+=("--extra-arg=-std=c++20")
fi

# Quote each extra arg so the helper can safely eval back to an array
EXTRA_ARGS_STRING=""
for a in "${EXTRA_ARGS[@]:-}"; do
  EXTRA_ARGS_STRING+=" $(printf '%q' "$a")"
done

COMMON_ARGS_BASE=( -p "$BUILD_DIR" "-header-filter=$HEADER_FILTER" )
[[ -n "$CHECKS_OVERRIDE" ]] && COMMON_ARGS_BASE+=("-checks=$CHECKS_OVERRIDE")
[[ "$FIX_ERRORS" -eq 1 ]] && COMMON_ARGS_BASE+=(-fix-errors)

# Prepare export dir (per-file) if requested
if [[ -n "$USER_EXPORT_FIXES_DIR" ]]; then
  mkdir -p "$USER_EXPORT_FIXES_DIR"
fi
TMP_EXPORT_DIR="$(mktemp -d "${BUILD_DIR}/clang-tidy-fixes-XXXX")"
trap 'rm -rf "$TMP_EXPORT_DIR"' EXIT

echo "Running clang-tidy fixes on ${#SOURCES[@]} file(s) ..."
echo "  checks: ${CHECKS_OVERRIDE}"
echo "  parallel jobs: ${JOBS}"
[[ "$FIX_ERRORS" -eq 1 ]] && echo "  using -fix-errors"
[[ "$PASSES" -gt 1 ]] && echo "  multi-pass: ${PASSES} passes (aggressive=${AGGRESSIVE})"

# ---------- Helper script (bash) ----------
TMP_HELPER="$(mktemp "${BUILD_DIR}/clang-tidy-one-XXXX.sh")"
cat > "$TMP_HELPER" <<'HLP'
#!/usr/bin/env bash
set -euo pipefail
f="$1"
export_dir="$2"

yaml="${export_dir}/$(echo "$f" | tr '/ ' '__').yaml"

if [[ "$f" = /* ]]; then
  tu="$f"
else
  tu="$REPO_ROOT/$f"
fi

cmd=(clang-tidy -fix -p "$BUILD_DIR" "-header-filter=$HEADER_FILTER")
[[ -n "${CHECKS:-}" ]] && cmd+=("-checks=$CHECKS")
[[ "${FIX_ERRORS:-0}" -eq 1 ]] && cmd+=(-fix-errors)

cmd+=("-export-fixes=$yaml" "$tu")

if [[ -n "${EXTRA_ARGS_STRING:-}" ]]; then
  eval "extra=( ${EXTRA_ARGS_STRING} )"
  filtered_extra=()
  for arg in "${extra[@]}"; do
    [[ -n "$arg" ]] && filtered_extra+=("$arg")
  done
  if [[ ${#filtered_extra[@]} -gt 0 ]]; then
    cmd+=(-- "${filtered_extra[@]}")
  fi
fi

run_cmd() {
  if [[ "${NICE:-0}" -eq 1 ]]; then
    if command -v ionice >/dev/null 2>&1; then
      ionice -c 2 -n 7 nice -n 10 "${cmd[@]}"
    else
      nice -n 10 "${cmd[@]}"
    fi
  else
    "${cmd[@]}"
  fi
}

if [[ "${QUIET:-1}" -eq 1 ]]; then
  run_cmd >/dev/null 2>&1 || true
else
  echo "[clang-tidy] $f"
  if [[ "${VERBOSE_CMD:-0}" -eq 1 ]]; then
    printf '        %s\n' "$(printf '%q ' "${cmd[@]}")"
  fi
  run_cmd || true
fi
HLP
chmod +x "$TMP_HELPER"

# Export env for helper
export BUILD_DIR HEADER_FILTER EXTRA_ARGS_STRING QUIET
export REPO_ROOT VERBOSE_CMD
export CHECKS="${CHECKS_OVERRIDE}"
export FIX_ERRORS
export NICE="${CLANG_TIDY_NICE}"

# ---------- Run (parallel) ----------
pass=1
while [[ $pass -le $PASSES ]]; do
  [[ $PASSES -gt 1 ]] && echo "==> Pass ${pass}/${PASSES}"

  if command -v xargs >/dev/null 2>&1; then
    printf '%s\0' "${SOURCES[@]}" | xargs -0 -P "$JOBS" -I{} bash "$TMP_HELPER" "{}" "$TMP_EXPORT_DIR"
  else
    for f in "${SOURCES[@]}"; do bash "$TMP_HELPER" "$f" "$TMP_EXPORT_DIR"; done
  fi

  # Early stop if using multiple passes and nothing else changed
  if [[ $PASSES -gt 1 ]] && command -v git >/dev/null 2>&1; then
    if git diff --quiet -- "${SOURCES[@]}"; then
      echo "No further changes detected; stopping early."
      break
    fi
  fi

  pass=$((pass + 1))
done

# ---------- Summaries ----------
repl_total=0
diag_total=0
if ls "$TMP_EXPORT_DIR"/*.yaml >/dev/null 2>&1; then
  while IFS= read -r y; do
    r=$(grep -c 'ReplacementText:' "$y" || true)
    d=$(grep -c 'DiagnosticMessage:' "$y" || true)
    repl_total=$((repl_total + r))
    diag_total=$((diag_total + d))
  done < <(ls "$TMP_EXPORT_DIR"/*.yaml 2>/dev/null || true)
  echo "Summary: diagnostics=${diag_total}, replacements=${repl_total} (from per-file YAMLs)."
  if [[ -n "$USER_EXPORT_FIXES_DIR" ]]; then
    mkdir -p "$USER_EXPORT_FIXES_DIR"
    cp -f "$TMP_EXPORT_DIR"/*.yaml "$USER_EXPORT_FIXES_DIR"/ 2>/dev/null || true
    echo "Per-file fixes exported to: $USER_EXPORT_FIXES_DIR"
  fi
else
  echo "Summary: no fixes exported (no applicable fix-its)."
fi

# Changed files summary (git)
if command -v git >/dev/null 2>&1; then
  mapfile -t changed < <(git diff --name-only -- "${SOURCES[@]}")
  if [[ ${#changed[@]} -gt 0 ]]; then
    echo "Changed files (${#changed[@]}):"
    printf '  %s\n' "${changed[@]:0:20}"
    [[ ${#changed[@]} -gt 20 ]] && echo "  ... and $(( ${#changed[@]} - 20 )) more"
  else
    echo "No tracked files changed (git diff is clean for the selected sources)."
  fi
fi

exit 0
