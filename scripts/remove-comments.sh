#!/usr/bin/env bash
# scripts/remove-comments.sh
# Remove comments from C/C++, Python, and shader source files in-place.

set -Eeuo pipefail
trap 'echo "error: line $LINENO: $BASH_COMMAND" >&2' ERR

EXTS_DEFAULT="c,cc,cpp,cxx,h,hh,hpp,hxx,ipp,inl,tpp,qml,vert,frag,glsl,py"
ROOTS=(".")
DRY_RUN=0
BACKUP=0
QUIET=0
JOBS=0
EXTS="$EXTS_DEFAULT"
EXCLUDE_DIRS=(.git .svn build build-tidy build-debug third_party venv .venv)

usage() {
  cat <<'USAGE'
remove-comments.sh - strip comments from C/C++, Python, and shader files.

Usage:
  scripts/remove-comments.sh [options] [PATH ...]

Options:
  -x, --ext       Comma-separated extensions to scan (default: c,cc,cpp,cxx,h,hh,hpp,hxx,ipp,inl,tpp,qml,vert,frag,glsl,py)
  -j, --jobs      Number of parallel workers (default: nproc)
  -n, --dry-run   Show files that would be modified; don't write changes
  --exclude-dir   Directory basename to skip (can be repeated)
  --backup        Create FILE.bak before writing
  -q, --quiet     Less output
  -h, --help      Show this help
Examples:
  scripts/remove-comments.sh
  scripts/remove-comments.sh -j 4
  scripts/remove-comments.sh --backup src/ include/
  scripts/remove-comments.sh -x c,cpp,hpp
  scripts/remove-comments.sh assets/shaders/
USAGE
}

log() { if ((QUIET == 0)); then printf '%s\n' "$*"; fi; }
die() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

# --- arg parsing ---
args=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -x | --ext)
      EXTS="${2:?missing extensions}"
      shift 2
      ;;
    -j | --jobs)
      JOBS="${2:?missing job count}"
      shift 2
      ;;
    -n | --dry-run)
      DRY_RUN=1
      shift
      ;;
    --exclude-dir)
      EXCLUDE_DIRS+=("${2:?missing directory name}")
      shift 2
      ;;
    --backup)
      BACKUP=1
      shift
      ;;
    -q | --quiet)
      QUIET=1
      shift
      ;;
    -h | --help)
      usage
      exit 0
      ;;
    --)
      shift
      break
      ;;
    -*) die "Unknown option: $1" ;;
    *)
      args+=("$1")
      shift
      ;;
  esac
done
((${#args[@]})) && ROOTS=("${args[@]}")

# Build extension list
IFS=',' read -r -a EXT_ARR <<<"$EXTS"
((${#EXT_ARR[@]})) || die "No extensions provided"

FIND_NAME=()
for e in "${EXT_ARR[@]}"; do
  e="${e#.}"
  FIND_NAME+=(-o -iname "*.${e}")
done
FIND_NAME=("${FIND_NAME[@]:1}")

FIND_PRUNE=()
for d in "${EXCLUDE_DIRS[@]}"; do
  [[ -n "$d" ]] || continue
  FIND_PRUNE+=(-o -name "$d")
done
FIND_PRUNE=("${FIND_PRUNE[@]:1}")

# Pick Python
if command -v python3 >/dev/null 2>&1; then
  PYTHON_BIN=python3
elif command -v python >/dev/null 2>&1; then
  PYTHON_BIN=python
else
  die "Python is required but not found."
fi

# Default jobs
if ((JOBS == 0)); then
  JOBS=$(command -v nproc >/dev/null 2>&1 && nproc || echo 4)
fi

# Write the Python filter to a temp script
TMP_SCRIPT=$(mktemp /tmp/rmcomments.XXXXXX.py)
trap 'rm -f "$TMP_SCRIPT"' EXIT

cat > "$TMP_SCRIPT" << 'PYEOF'
import sys, os, re, shutil

RAW_PREFIX = re.compile(rb'(?:u8|u|U|L)?R"([^\s()\\]{0,16})\(')
NAMESPACE_END_COMMENT = re.compile(
    rb'//[ \t]*namespace'
    rb'(?:[ \t]+[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)*)?'
    rb'[ \t]*\r?$'
)

def isspace(b):
    return b in b' \t\r\n\v\f'

def trim_horizontal_space(out):
    while out and out[-1] in (0x20, 0x09):
        out.pop()

def strip_cpp_comments(b: bytes) -> bytes:
    out = bytearray()
    i = 0
    n = len(b)

    def prev_byte():
        return out[-1] if out else None

    while i < n:
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

        if c == 0x22 or c == 0x27:
            quote = c
            out.append(c); i += 1
            while i < n:
                ch = b[i]; out.append(ch); i += 1
                if ch == 0x5C and i < n:
                    out.append(b[i]); i += 1
                elif ch == quote:
                    break
            continue

        if c == 0x2F and i + 1 < n:
            nx = b[i+1]
            if nx == 0x2F:
                line_end = b.find(b'\n', i)
                if line_end == -1:
                    line_end = n
                line_start = out.rfind(b'\n') + 1
                line_prefix = bytes(out[line_start:]).rstrip(b' \t\r')
                comment = b[i:line_end]
                if (
                    line_prefix == b'}'
                    and NAMESPACE_END_COMMENT.fullmatch(comment) is not None
                ):
                    out += comment
                    i = line_end
                    continue

                trim_horizontal_space(out)
                i += 2
                while i < n and b[i] != 0x0A:
                    i += 1
                if i < n and b[i] == 0x0A:
                    if i > 0 and b[i-1] == 0x0D:
                        out += b'\r\n'
                    else:
                        out += b'\n'
                    i += 1
                continue
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
                nextc = b[i] if i < n else None
                p = prev_byte()
                if had_nl:
                    trim_horizontal_space(out)
                    p = prev_byte()
                    if p not in (None, 0x0A, 0x0D):
                        out.append(0x0A)
                else:
                    if nextc in (None, 0x0A, 0x0D):
                        trim_horizontal_space(out)
                    elif p is not None and not isspace(p) and not isspace(nextc):
                        out.append(0x20)
                continue

        out.append(c); i += 1

    return bytes(out)

def strip_python_comments(b: bytes) -> bytes:
    out = bytearray()
    i = 0
    n = len(b)

    if n > 2 and b[0] == 0x23 and b[1] == 0x21:
        while i < n and b[i] != 0x0A:
            out.append(b[i])
            i += 1
        if i < n and b[i] == 0x0A:
            out.append(b[i])
            i += 1

    while i < n:
        c = b[i]

        if c == 0x22 or c == 0x27:
            quote = c
            if i + 2 < n and b[i+1] == quote and b[i+2] == quote:
                out.append(c); out.append(c); out.append(c)
                i += 3
                while i < n:
                    ch = b[i]
                    out.append(ch)
                    i += 1
                    if ch == 0x5C and i < n:
                        out.append(b[i])
                        i += 1
                    elif ch == quote and i + 1 < n and b[i] == quote and i + 2 < n and b[i+1] == quote:
                        out.append(b[i])
                        out.append(b[i+1])
                        i += 2
                        break
            else:
                out.append(c)
                i += 1
                while i < n:
                    ch = b[i]
                    out.append(ch)
                    i += 1
                    if ch == 0x5C and i < n:
                        out.append(b[i])
                        i += 1
                    elif ch == quote:
                        break
                    elif ch == 0x0A:
                        break
            continue

        if c == 0x23:
            trim_horizontal_space(out)
            i += 1
            while i < n and b[i] != 0x0A:
                i += 1
            if i < n and b[i] == 0x0A:
                if i > 0 and b[i-1] == 0x0D:
                    out += b'\r\n'
                else:
                    out += b'\n'
                i += 1
            continue

        out.append(c)
        i += 1

    return bytes(out)


def process(path):
    dry_run = os.environ.get('DRY_RUN') == '1'
    backup = os.environ.get('BACKUP') == '1'

    with open(path, 'rb') as f:
        original = f.read()

    is_python = path.lower().endswith('.py')
    result = strip_python_comments(original) if is_python else strip_cpp_comments(original)

    if result == original:
        return 'unchanged'

    if dry_run:
        return 'would_modify'

    if backup:
        shutil.copy2(path, path + '.bak')

    mode = os.stat(path).st_mode
    with open(path, 'wb') as f:
        f.write(result)
    os.chmod(path, mode)
    return 'modified'


if __name__ == '__main__':
    path = sys.argv[1]
    try:
        status = process(path)
        print(f'{status}:{path}')
    except Exception as e:
        print(f'error:{path}:{e}')
        sys.exit(1)
PYEOF

# Export settings for parallel workers
export DRY_RUN BACKUP
RESULTS=$(mktemp /tmp/rmcomments.XXXXXX)
trap 'rm -f "$TMP_SCRIPT" "$RESULTS"' EXIT

log "Scanning: ${ROOTS[*]}"
log "Extensions: $EXTS"
log "Excluded dirs: ${EXCLUDE_DIRS[*]}"
log "Jobs: $JOBS"
((DRY_RUN)) && log "(dry run)"

# Find and process files in parallel
find "${ROOTS[@]}" \( -type d \( "${FIND_PRUNE[@]}" \) -prune \) -o \
    \( -type f \( "${FIND_NAME[@]}" \) -print0 \) | \
    xargs -0 -P "$JOBS" -n 1 "$PYTHON_BIN" "$TMP_SCRIPT" > "$RESULTS"

# Parse results
processed=0
modified=0
would_modify=0

while IFS= read -r line; do
  ((processed += 1))
  case "$line" in
    modified:*)
      ((modified += 1))
      ((QUIET)) || echo "${line#modified:}" >&2
      ;;
    would_modify:*)
      ((would_modify += 1))
      ((DRY_RUN)) && echo "would modify: ${line#would_modify:}"
      ;;
    unchanged:*)
      ;;
    error:*)
      path="${line#error:}"
      echo "error: Python filter failed on ${path%%:*}" >&2
      ;;
  esac
done < "$RESULTS"

rm -f "$RESULTS" "$TMP_SCRIPT"
trap '' EXIT

if ((DRY_RUN == 1)); then
  echo "dry run complete. processed: $processed file(s); would modify: $would_modify"
else
  echo "done. processed: $processed file(s); modified: $modified"
fi
