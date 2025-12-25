#!/usr/bin/env bash
# scripts/remove-comments.sh
# Remove comments from C/C++, Python, and shader source files in-place.

set -Eeuo pipefail
trap 'echo "error: line $LINENO: $BASH_COMMAND" >&2' ERR

EXTS_DEFAULT="c,cc,cpp,cxx,h,hh,hpp,hxx,ipp,inl,tpp,qml,vert,frag,glsl,py"
ROOTS=(".")
DRY_RUN=0
BACKUP=0          # OFF by default
QUIET=0
EXTS="$EXTS_DEFAULT"

usage() {
  cat <<'USAGE'
remove-comments.sh - strip comments from C/C++, Python, and shader files.

Usage:
  scripts/remove-comments.sh [options] [PATH ...]

Options:
  -x, --ext       Comma-separated extensions to scan (default: c,cc,cpp,cxx,h,hh,hpp,hxx,ipp,inl,tpp,qml,vert,frag,glsl,py)
  -n, --dry-run   Show files that would be modified; don't write changes
  --backup        Create FILE.bak before writing (default: OFF)
  -q, --quiet     Less output
  -h, --help      Show this help
Examples:
  scripts/remove-comments.sh
  scripts/remove-comments.sh --backup src/ include/
  scripts/remove-comments.sh -x c,cpp,hpp
  scripts/remove-comments.sh assets/shaders/
USAGE
}

log()  { (( QUIET == 0 )) && printf '%s\n' "$*"; }
die()  { printf 'error: %s\n' "$*" >&2; exit 1; }

# --- arg parsing ---
args=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -x|--ext) EXTS="${2:?missing extensions}"; shift 2 ;;
    -n|--dry-run) DRY_RUN=1; shift ;;
    --backup) BACKUP=1; shift ;;
    -q|--quiet) QUIET=1; shift ;;
    -h|--help) usage; exit 0 ;;
    --) shift; break ;;
    -*) die "Unknown option: $1" ;;
    *)  args+=("$1"); shift ;;
  esac
done
((${#args[@]})) && ROOTS=("${args[@]}")

# Build extension list (portable; no mapfile)
IFS=',' read -r -a EXT_ARR <<< "$EXTS"
((${#EXT_ARR[@]})) || die "No extensions provided"

# Build find predicates
FIND_NAME=()
for e in "${EXT_ARR[@]}"; do
  e="${e#.}"
  FIND_NAME+=(-o -iname "*.${e}")
done
FIND_NAME=("${FIND_NAME[@]:1}")    # drop leading -o

# Pick Python
if command -v python3 >/dev/null 2>&1; then
  PYTHON_BIN=python3
elif command -v python >/dev/null 2>&1; then
  PYTHON_BIN=python
else
  die "Python is required but not found."
fi

# Python filter as a literal (processes BYTES; preserves UTF-8/Unicode)
PY_FILTER=$(cat <<'PYCODE'
import sys, re, os

# Read raw bytes and operate purely on bytes so UTF-8 (and any other) is preserved.
path = sys.argv[1]
with open(path, 'rb') as f:
    data = f.read()

# Detect file type based on extension
is_python = path.lower().endswith('.py')

RAW_PREFIX = re.compile(rb'(?:u8|u|U|L)?R"([^\s()\\]{0,16})\(')

def isspace(b):  # b is an int 0..255
    return b in b' \t\r\n\v\f'

def strip_cpp_comments(b: bytes) -> bytes:
    out = bytearray()
    i = 0
    n = len(b)

    def prev_byte():
        return out[-1] if out else None

    while i < n:
        # C++ raw string?
        m = RAW_PREFIX.match(b, i)
        if m:
            delim = m.group(1)
            start = m.end()
            end_token = b')' + delim + b'"'
            j = b.find(end_token, start)
            if j != -1:
                out += b[i:j+len(end_token)]
                i = j + len(end_token)
                continue

        c = b[i]

        # Regular string / char literals
        if c == 0x22 or c == 0x27:  # " or '
            quote = c
            out.append(c); i += 1
            while i < n:
                ch = b[i]; out.append(ch); i += 1
                if ch == 0x5C and i < n:  # backslash -> escape next byte verbatim
                    out.append(b[i]); i += 1
                elif ch == quote:
                    break
            continue

        # Comments
        if c == 0x2F and i + 1 < n:  # '/'
            nx = b[i+1]
            # // line comment
            if nx == 0x2F:
                i += 2
                while i < n and b[i] != 0x0A:
                    i += 1
                if i < n and b[i] == 0x0A:
                    # Preserve CRLF if present
                    if i > 0 and b[i-1] == 0x0D:
                        out += b'\r\n'
                    else:
                        out += b'\n'
                    i += 1
                continue
            # /* block comment */
            if nx == 0x2A:
                i += 2
                had_nl = False
                while i < n - 1:
                    if b[i] == 0x0A:
                        had_nl = True
                    if b[i] == 0x2A and b[i+1] == 0x2F:
                        i += 2
                        break
                    i += 1
                # Insert minimal whitespace so tokens don't glue
                nextc = b[i] if i < n else None
                p = prev_byte()
                if had_nl:
                    if p not in (None, 0x0A, 0x0D):
                        out.append(0x0A)  # '\n'
                else:
                    if p is not None and not isspace(p) and (nextc is not None) and not isspace(nextc):
                        out.append(0x20)  # ' '
                continue

        # Default: copy byte verbatim (preserves any UTF-8 / binary)
        out.append(c); i += 1

    return bytes(out)

def strip_python_comments(b: bytes) -> bytes:
    out = bytearray()
    i = 0
    n = len(b)

    # Preserve shebang line if present at start of file
    if n > 2 and b[0] == 0x23 and b[1] == 0x21:  # '#!'
        while i < n and b[i] != 0x0A:
            out.append(b[i])
            i += 1
        if i < n and b[i] == 0x0A:
            out.append(b[i])
            i += 1

    while i < n:
        c = b[i]

        # String literals (single, double, and triple-quoted)
        if c == 0x22 or c == 0x27:  # " or '
            quote = c
            # Check for triple-quote
            if i + 2 < n and b[i+1] == quote and b[i+2] == quote:
                # Triple-quoted string
                out.append(c); out.append(c); out.append(c)
                i += 3
                while i < n:
                    ch = b[i]
                    out.append(ch)
                    i += 1
                    if ch == 0x5C and i < n:  # backslash escape
                        out.append(b[i])
                        i += 1
                    elif ch == quote and i + 1 < n and b[i] == quote and i + 2 < n and b[i+1] == quote:
                        # Found closing triple-quote
                        out.append(b[i])
                        out.append(b[i+1])
                        i += 2
                        break
            else:
                # Single or double quoted string
                out.append(c)
                i += 1
                while i < n:
                    ch = b[i]
                    out.append(ch)
                    i += 1
                    if ch == 0x5C and i < n:  # backslash escape
                        out.append(b[i])
                        i += 1
                    elif ch == quote:
                        break
                    elif ch == 0x0A:  # newline ends unclosed string
                        break
            continue

        # # comment
        if c == 0x23:  # '#'
            i += 1
            while i < n and b[i] != 0x0A:
                i += 1
            if i < n and b[i] == 0x0A:
                # Preserve line ending
                if i > 0 and b[i-1] == 0x0D:
                    out += b'\r\n'
                else:
                    out += b'\n'
                i += 1
            continue

        # Default: copy byte verbatim
        out.append(c)
        i += 1

    return bytes(out)

if is_python:
    sys.stdout.buffer.write(strip_python_comments(data))
else:
    sys.stdout.buffer.write(strip_cpp_comments(data))
PYCODE
)

changed=0
processed=0

process_file() {
  local f="$1"
  log "processing: $f"

  # Capture current file mode (GNU and BSD)
  local mode
  mode="$(stat -c '%a' "$f" 2>/dev/null || stat -f '%Lp' "$f" 2>/dev/null || echo '')"

  # mktemp: handle BSD/GNU differences
  local tmp
  tmp="$(mktemp 2>/dev/null || mktemp -t rmcomments)" || die "mktemp failed"

  # Run the Python filter; keep argv[1] = file path
  if ! printf '%s\n' "$PY_FILTER" | "$PYTHON_BIN" - "$f" >"$tmp"; then
    rm -f "$tmp"
    die "Python filter failed on $f"
  fi

  if ! cmp -s "$f" "$tmp"; then
    if (( DRY_RUN == 1 )); then
      echo "would modify: $f"
      rm -f "$tmp"
      ((processed+=1))
      return
    fi
    if (( BACKUP == 1 )); then
      cp -p -- "$f" "$f.bak" 2>/dev/null || cp -p "$f" "$f.bak" || true
    fi
    # Replace file
    mv -- "$tmp" "$f" 2>/dev/null || mv "$tmp" "$f"
    # Restore original mode if we captured it
    [[ -n "$mode" ]] && chmod "$mode" "$f" 2>/dev/null || true
    ((changed+=1))
  else
    rm -f "$tmp"
  fi
  ((processed+=1))
}

log "Scanning: ${ROOTS[*]}"
log "Extensions: $EXTS"
(( DRY_RUN )) && log "(dry run)"

# Find files and process
while IFS= read -r -d '' f; do
  process_file "$f"
done < <(
  find "${ROOTS[@]}" -type f \( "${FIND_NAME[@]}" \) \
    -not -path '*/.git/*' -not -path '*/.svn/*' -not -path '*/build/*' -print0
)

if (( DRY_RUN == 1 )); then
  echo "dry run complete. processed: $processed file(s); would modify: $changed"
else
  echo "done. processed: $processed file(s); modified: $changed"
fi
