#!/usr/bin/env bash
# Robust, gentle clang-tidy runner.
# - Resolves absolute/relative paths from compile_commands.json
# - Avoids running directly on headers; analyzes through TUs
# - Throttled (nice/ionice), per-file timeout, soft memory cap (Linux)
# - Skips generated build/autogen files (Qt moc/uic, *_autogen)
# - Verbose discovery + clean summary

set -euo pipefail

BUILD_DIR="build"
CHECKS="modernize-*,readability-*,performance-*,misc-*,-misc-unused-parameters,-readability-identifier-naming"

# ---- options ----
DRY_RUN=false
MEM_CAP_MB=4096          # soft cap per process (Linux only)
FILE_TIMEOUT="300s"      # per-TU timeout; "" to disable
SHOW_DISCOVERY=1         # print TU count + a few samples

if [[ "${1:-}" == "--dry-run" ]]; then
  DRY_RUN=true
  echo "🧪 Dry-run mode: no fixes will be applied."
fi

# ---- sanity ----
if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
  echo "❌ Error: $BUILD_DIR/compile_commands.json not found."
  echo "Run CMake with: -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
  exit 1
fi

REPO_ABS="$(pwd)"

escape_regex() { sed -e 's/[.[\*^$+?(){}|\\]/\\&/g' <<<"$1"; }
HEADER_FILTER="^$(escape_regex "$REPO_ABS")/.*"

# Accept a wide set of C/C++ TU extensions (case-insensitive)
is_source_ext() {
  shopt -s nocasematch
  [[ "$1" =~ \.(c|cc|cpp|cxx|c\+\+|C|ixx|ix|mm|m)$ ]]
}

# Fast absolute path helper using python3 -c with stdin/args (no here-doc on stdin!)
abspath_from_dir_file() {
  local dir="$1" file="$2"
  if [[ "$file" = /* ]]; then
    python3 -c 'import os,sys; print(os.path.abspath(sys.stdin.read().strip()))' <<<"$file"
  else
    python3 - "$dir" "$file" <<'PY'
import os, sys
dir_ = sys.argv[1]
file_ = sys.argv[2]
print(os.path.abspath(os.path.join(dir_, file_)))
PY
  fi
}

echo "🔍 Collecting translation units from $BUILD_DIR/compile_commands.json ..."

FILES=()

if command -v jq >/dev/null 2>&1; then
  # Primary: use .file (resolving relative to .directory)
  while IFS=$'\t' read -r DIR FILE; do
    [[ -n "${FILE:-}" ]] || continue
    ABS="$(abspath_from_dir_file "$DIR" "$FILE")"
    if is_source_ext "$ABS"; then
      FILES+=("$ABS")
    fi
  done < <(jq -r '.[] | [.directory, (.file // "")] | @tsv' "$BUILD_DIR/compile_commands.json")

  # Fallback: derive from arguments/command if needed
  if [[ ${#FILES[@]} -eq 0 ]]; then
    while IFS=$'\t' read -r DIR HAS_ARGS; do
      SRC=""
      if [[ "$HAS_ARGS" == "args" ]]; then
        # flatten tokens from .arguments
        mapfile -t TOKS < <(jq -r '.arguments[]' <<<"$(jq -c '.arguments' <<<"$(jq -c ' .' -r)")" 2>/dev/null)
      else
        # split .command string
        CMD="$(jq -r '.command' <<<"$(jq -c ' .' -r)")"
        read -r -a TOKS <<<"$CMD"
      fi
      for t in "${TOKS[@]:-}"; do
        if is_source_ext "$t"; then SRC="$t"; fi
      done
      [[ -n "$SRC" ]] || continue
      ABS="$(abspath_from_dir_file "$DIR" "$SRC")"
      if is_source_ext "$ABS"; then
        FILES+=("$ABS")
      fi
    done < <(jq -r '.[] | . as $e | if has("arguments") then
                     [$e.directory, "args"] | @tsv
                   else
                     [$e.directory, "cmd"] | @tsv
                   end' "$BUILD_DIR/compile_commands.json")
  fi
else
  # Fallback without jq: parse directory/file pairs
  mapfile -t FILES < <(
    awk -F'"' '
      $2=="directory"{dir=$4}
      $2=="file"{
        f=$4
        if (substr(f,1,1)=="/") print f; else print dir "/" f
      }' "$BUILD_DIR/compile_commands.json" \
    | while IFS= read -r p; do
        python3 -c 'import os,sys; print(os.path.abspath(sys.stdin.read().strip()))' <<<"$p"
      done \
    | awk 'BEGIN{IGNORECASE=1} $0 ~ /\.(c|cc|cpp|cxx|c\+\+|C|ixx|ix|mm|m)$/ {print}' \
    | sort -u
  )

  # Fallback-from-command if still empty
  if [[ ${#FILES[@]} -eq 0 ]]; then
    mapfile -t FILES < <(
      awk -F'"' '
        $2=="directory"{dir=$4}
        $2=="command"{
          cmd=$4
          n=split(cmd, a, /[[:space:]]+/)
          src=""
          for(i=1;i<=n;i++) {
            t=a[i]
            if (match(t, /\.(c|cc|cpp|cxx|c\+\+|C|ixx|ix|mm|m)$/i)) src=t
          }
          if (src!="") {
            if (substr(src,1,1)=="/") print src;
            else print dir "/" src;
          }
        }' "$BUILD_DIR/compile_commands.json" \
      | while IFS= read -r p; do
          python3 -c 'import os,sys; print(os.path.abspath(sys.stdin.read().strip()))' <<<"$p"
        done \
      | sort -u
    )
  fi
fi

# Deduplicate (preserve order)
declare -A _seen=()
UNIQ=()
for f in "${FILES[@]}"; do
  [[ -n "$f" ]] || continue
  if [[ -z "${_seen[$f]:-}" ]]; then
    _seen["$f"]=1
    UNIQ+=("$f")
  fi
done
FILES=("${UNIQ[@]}")

# Filter out generated stuff to save RAM/time
FILTERED=()
for f in "${FILES[@]}"; do
  case "$f" in
    "$REPO_ABS/$BUILD_DIR/"* | *"/_autogen/"* | *"/autogen/"* | */mocs_compilation.cpp | */qrc_*.cpp )
      continue
      ;;
    * )
      FILTERED+=("$f")
      ;;
  esac
done
FILES=("${FILTERED[@]}")

if [[ ${#FILES[@]} -eq 0 ]]; then
  echo "⚠️  No translation units found."
  echo "   Hints:"
  echo "     • Check $BUILD_DIR/compile_commands.json for \"file\" or usable \"command/arguments\" entries."
  echo "     • Ensure you built C/C++ targets so the DB contains real TUs."
  exit 0
fi

# Discovery report
if [[ ${SHOW_DISCOVERY} -eq 1 ]]; then
  echo "📦 Found ${#FILES[@]} translation unit(s) (after filtering autogen/build)."
  echo "   Sample(s):"
  for s in "${FILES[@]:0:5}"; do
    if [[ "$s" == "$REPO_ABS"* ]]; then
      echo "   - ${s#$REPO_ABS/}"
    else
      echo "   - $s"
    fi
  done
fi

# ---- gentle scheduling ----
NI_CMD=()
command -v nice   >/dev/null 2>&1 && NI_CMD+=(nice -n 10)
command -v ionice >/dev/null 2>&1 && NI_CMD+=(ionice -c2 -n7)

# Soft mem cap
if [[ "$(uname -s)" == "Linux" ]]; then
  # shellcheck disable=SC3045
  ulimit -S -v $((MEM_CAP_MB * 1024)) || true
fi

FIX_FLAG=()
$DRY_RUN || FIX_FLAG=(-fix)

TIMEOUT_CMD=()
if [[ -n "$FILE_TIMEOUT" ]] && command -v timeout >/dev/null 2>&1; then
  TIMEOUT_CMD=(timeout "$FILE_TIMEOUT")
fi

echo
echo "🚀 Running clang-tidy (single CPU, throttled)…"
echo "   Checks: $CHECKS"
echo "   Header filter: $HEADER_FILTER"
[[ -n "$FILE_TIMEOUT" && ${#TIMEOUT_CMD[@]} -gt 0 ]] && echo "   Per-file timeout: $FILE_TIMEOUT"
[[ "$(uname -s)" == "Linux" ]] && echo "   Per-process soft mem cap: ${MEM_CAP_MB} MB"
echo

FAILED=()
SKIPPED=()

for ABS in "${FILES[@]}"; do
  if [[ ! -f "$ABS" ]]; then
    SKIPPED+=("$ABS")
    continue
  fi

  # Pretty label
  if [[ "$ABS" == "$REPO_ABS"* ]]; then REL="${ABS#$REPO_ABS/}"; else REL="$ABS"; fi
  echo "🔧 Processing: $REL"

  if ! "${NI_CMD[@]}" "${TIMEOUT_CMD[@]}" \
      clang-tidy -p "$BUILD_DIR" "${FIX_FLAG[@]}" \
        -checks="$CHECKS" \
        -header-filter="$HEADER_FILTER" \
        -extra-arg=-fno-color-diagnostics \
        -extra-arg=-Wno-unknown-warning-option \
        "$ABS"
  then
    FAILED+=("$REL")
  fi
done

echo
if [[ ${#FAILED[@]} -gt 0 ]]; then
  echo "⚠️  clang-tidy failed on ${#FAILED[@]} file(s):"
  printf '   - %s\n' "${FAILED[@]}"
  echo "💡 Tip: re-run with --dry-run to inspect without applying fixes."
else
  echo "✅ All files processed successfully!"
fi

if [[ ${#SKIPPED[@]} -gt 0 ]]; then
  echo
  echo "ℹ️  Skipped (not found on disk):"
  printf '   - %s\n' "${SKIPPED[@]}"
fi

