#!/usr/bin/env bash
# scripts/remove_comments.sh
# Remove comments from C/C++ source files in-place.

set -euo pipefail

EXTS_DEFAULT="c,cc,cpp,cxx,h,hh,hpp,hxx,ipp,inl,tpp,qml"
ROOT="."
DRY_RUN=0
BACKUP=0
QUIET=0
EXTS="$EXTS_DEFAULT"

usage() {
  cat <<'USAGE'
remove_comments.sh - strip comments from C/C++ files.

Usage:
  scripts/remove_comments.sh [options] [PATH ...]

Options:
  -x, --ext       Comma-separated extensions to scan (default: c,cc,cpp,cxx,h,hh,hpp,hxx,ipp,inl,tpp,qml)
  -n, --dry-run   Show files that would be modified; don't write changes
  --no-backup     Do not create .bak backups (by default, a FILE.bak is kept)
  -q, --quiet     Less output
  -h, --help      Show this help
Examples:
  scripts/remove_comments.sh
  scripts/remove_comments.sh --no-backup src/ include/
  scripts/remove_comments.sh -x c,cpp,hpp
USAGE
}

# --- arg parsing ---
while [[ $# -gt 0 ]]; do
  case "$1" in
    -x|--ext) EXTS="${2:?missing extensions}"; shift 2 ;;
    -n|--dry-run) DRY_RUN=1; shift ;;
    --no-backup) BACKUP=0; shift ;;
    -q|--quiet) QUIET=1; shift ;;
    -h|--help) usage; exit 0 ;;
    --) shift; break ;;
    -*) echo "Unknown option: $1" >&2; usage; exit 2 ;;
    *) break ;;
  esac
done

if [[ $# -gt 0 ]]; then
  ROOT=("$@")
else
  ROOT=(".")
fi

# Build find predicates for the extensions
mapfile -t EXT_ARR < <(echo "$EXTS" | tr ',' '\n')
FIND_NAME=()
for e in "${EXT_ARR[@]}"; do
  FIND_NAME+=(-o -name "*.${e}")
done
# drop leading -o
if ((${#FIND_NAME[@]})); then FIND_NAME=("${FIND_NAME[@]:1}"); fi

# Choose a Python interpreter
PYTHON_BIN=""
if command -v python3 >/dev/null 2>&1; then
  PYTHON_BIN="python3"
elif command -v python >/dev/null 2>&1; then
  PYTHON_BIN="python"
else
  echo "Error: Python is required but not found." >&2
  exit 1
fi

# Python filter (reads a file path as argv[1], writes stripped content to stdout)
read -r -d '' PY_FILTER <<'PYCODE'
import sys, os, re

# Read as latin-1 to preserve bytes 1:1 (no decode errors); write same back.
path = sys.argv[1]
with open(path, 'rb') as f:
    data = f.read().decode('latin1')

# Detect C++ raw-string literal starting at position i
RAW_PREFIX = re.compile(r'(?:u8|u|U|L)?R"([^\s()\\]{0,16})\(')

def strip_comments(s: str) -> str:
    out = []
    i = 0
    n = len(s)

    def prev_char():
        return out[-1] if out else ''

    while i < n:
        # Try raw string literal
        m = RAW_PREFIX.match(s, i)
        if m:
            delim = m.group(1)
            start = m.end()
            end_token = ')' + delim + '"'
            j = s.find(end_token, start)
            if j != -1:
                out.append(s[i:j+len(end_token)])
                i = j + len(end_token)
                continue
            # Fallback: not actually a raw string (unterminated) -> treat normally

        c = s[i]

        # Normal strings / char literals
        if c == '"' or c == "'":
            quote = c
            out.append(c); i += 1
            while i < n:
                ch = s[i]; out.append(ch); i += 1
                if ch == '\\':
                    if i < n:
                        out.append(s[i]); i += 1
                elif ch == quote:
                    break
            continue

        # Comments
        if c == '/' and i + 1 < n:
            nx = s[i+1]
            # Single-line // ... (preserve EOL)
            if nx == '/':
                i += 2
                while i < n and s[i] != '\n':
                    i += 1
                if i < n and s[i] == '\n':
                    # Preserve CRLF if present
                    if i-1 >= 0 and s[i-1] == '\r':
                        out.append('\r\n')
                    else:
                        out.append('\n')
                    i += 1
                continue
            # Block /* ... */
            if nx == '*':
                i += 2
                had_nl = False
                while i < n-1:
                    if s[i] == '\n':
                        had_nl = True
                    if s[i] == '*' and s[i+1] == '/':
                        i += 2
                        break
                    i += 1
                # Insert minimal whitespace so tokens don't glue together
                nextc = s[i] if i < n else ''
                if had_nl:
                    if prev_char() not in ('', '\n', '\r'):
                        out.append('\n')
                else:
                    if prev_char() and not prev_char().isspace() and nextc and not nextc.isspace():
                        out.append(' ')
                continue

        # Default: copy byte
        out.append(c); i += 1

    return ''.join(out)

sys.stdout.write(strip_comments(data))
PYCODE

changed=0
processed=0

process_file() {
  local f="$1"
  (( QUIET == 0 )) && echo "processing: $f"
  local tmp
  tmp="$(mktemp)"
  # Run the Python filter
  "$PYTHON_BIN" - <<PYEOF "$f" >"$tmp"
$PY_FILTER
PYEOF

  if ! cmp -s "$f" "$tmp"; then
    (( DRY_RUN == 1 )) && { echo "would modify: $f"; rm -f "$tmp"; return; }
    if (( BACKUP == 1 )); then
      cp -p -- "$f" "$f.bak"
    fi
    mv -- "$tmp" "$f"
    chmod --reference="$f" "$f" 2>/dev/null || true
    ((changed++))
  else
    rm -f "$tmp"
  fi
  ((processed++))
}

# Find files and process
# shellcheck disable=SC2046
while IFS= read -r -d '' f; do
  process_file "$f"
done < <(
  find "${ROOT[@]}" -type f \( "${FIND_NAME[@]}" \) \
    -not -path '*/.git/*' -not -path '*/.svn/*' -not -path '*/build/*' -print0
)

if (( DRY_RUN == 1 )); then
  echo "dry run complete."
else
  echo "done. processed: $processed file(s); modified: $changed"
fi
