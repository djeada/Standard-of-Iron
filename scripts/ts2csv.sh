#!/usr/bin/env bash
set -euo pipefail

# ts2csv.sh — Convert Qt .ts translation files to CSV
# Usage examples:
#   ./scripts/ts2csv.sh                 # convert all in ./translations -> ./translations/csv (2-column)
#   ./scripts/ts2csv.sh -m 1col         # single-column CSV of source strings
#   ./scripts/ts2csv.sh -o ./out csv/translations/app_de.ts
#   ./scripts/ts2csv.sh --bom --no-header
#
# Flags:
#   -o, --outdir DIR          Output directory (default: <repo>/translations/csv)
#   -m, --mode MODE           "2col" (default) or "1col"
#       --include-unfinished  Include messages with translation type="unfinished" (default: on)
#       --exclude-unfinished  Exclude unfinished (default is include)
#       --keep-obsolete       Keep messages marked obsolete/vanished (default: skip)
#       --bom                 Write UTF-8 with BOM (Excel-friendly)
#       --no-header           Do not write header row
#       --explode-plurals     Instead of joining plural forms, create separate rows
#
# Notes:
# - Language is taken from TS/@language if present; otherwise guessed from filename (e.g. *_de.ts -> de).
# - Plural forms (numerus) are joined with " | " unless --explode-plurals is used.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEFAULT_INDIR="$REPO_ROOT/translations"
OUTDIR="$REPO_ROOT/translations/csv"

MODE="2col"
INCLUDE_UNFINISHED=1
KEEP_OBSOLETE=0
WRITE_BOM=0
WRITE_HEADER=1
EXPLODE_PLURALS=0

# Parse args
ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -o|--outdir) OUTDIR="$2"; shift 2;;
    -m|--mode) MODE="$2"; shift 2;;
    --include-unfinished) INCLUDE_UNFINISHED=1; shift;;
    --exclude-unfinished) INCLUDE_UNFINISHED=0; shift;;
    --keep-obsolete) KEEP_OBSOLETE=1; shift;;
    --bom) WRITE_BOM=1; shift;;
    --no-header) WRITE_HEADER=0; shift;;
    --explode-plurals) EXPLODE_PLURALS=1; shift;;
    -h|--help)
      grep '^# ' "$0" | sed 's/^# //'
      exit 0
      ;;
    *) ARGS+=("$1"); shift;;
  esac
done
set -- "${ARGS[@]}"

mkdir -p "$OUTDIR"

# If no files passed, use all .ts in default translations dir
if [[ $# -eq 0 ]]; then
  mapfile -t FILES < <(find "$DEFAULT_INDIR" -maxdepth 1 -type f -name '*.ts' | sort)
else
  FILES=("$@")
fi

if [[ ${#FILES[@]} -eq 0 ]]; then
  echo "No .ts files found. Looked in: $DEFAULT_INDIR" >&2
  exit 1
fi

python3 - <<'PY' "$OUTDIR" "$MODE" "$INCLUDE_UNFINISHED" "$KEEP_OBSOLETE" "$WRITE_BOM" "$WRITE_HEADER" "$EXPLODE_PLURALS" "${FILES[@]}"
import sys, os, re, csv, argparse, xml.etree.ElementTree as ET
from pathlib import Path

# Pull configuration from argv (provided by the bash wrapper for simplicity)
OUTDIR          = Path(sys.argv[1])
MODE            = sys.argv[2]           # "2col" | "1col"
INCL_UNFINISHED = sys.argv[3] == "1"
KEEP_OBSOLETE   = sys.argv[4] == "1"
WRITE_BOM       = sys.argv[5] == "1"
WRITE_HEADER    = sys.argv[6] == "1"
EXPLODE_PLURALS = sys.argv[7] == "1"
FILES           = [Path(p) for p in sys.argv[8:]]

OUTDIR.mkdir(parents=True, exist_ok=True)

def guess_lang(ts_path: Path, root: ET.Element) -> str:
    lang = (root.get("language") or "").strip()
    if lang:
        return lang
    # fallback: extract last _xx before .ts
    m = re.search(r'_([A-Za-z]{2,})(?:_[A-Za-z0-9]+)?\.ts$', ts_path.name)
    return (m.group(1) if m else "unknown")

def should_skip_translation(trans_el: ET.Element) -> bool:
    t = (trans_el.get("type") or "").lower()
    if t in ("obsolete", "vanished"):
        return not KEEP_OBSOLETE
    return False

def is_unfinished(trans_el: ET.Element) -> bool:
    return (trans_el.get("type") or "").lower() == "unfinished"

def text_of(elem: ET.Element) -> str:
    # Concatenate text including nested nodes, preserving newlines
    parts = []
    if elem.text:
        parts.append(elem.text)
    for child in elem:
        parts.append(text_of(child))
        if child.tail:
            parts.append(child.tail)
    return "".join(parts)

def plural_forms(trans_el: ET.Element):
    # Return list of plural form strings if numerus; else single string list
    nufs = list(trans_el.findall("./numerusform"))
    if nufs:
        return [text_of(n).strip() for n in nufs]
    return [ (text_of(trans_el) or "").strip() ]

def src_text(msg_el: ET.Element) -> str:
    s = msg_el.find("source")
    return (text_of(s) if s is not None else "").strip()

def is_obsolete_or_vanished(msg_el: ET.Element) -> bool:
    t = msg_el.find("translation")
    if t is None: 
        return False
    typ = (t.get("type") or "").lower()
    return typ in ("obsolete","vanished")

def each_message(root: ET.Element):
    for msg in root.findall(".//context/message"):
        yield msg

def explode_plural_rows(source: str, forms: list[str]):
    rows = []
    for idx, val in enumerate(forms):
        rows.append((f"{source} [plural {idx}]", val))
    return rows

def write_csv(rows, header, out_path: Path, bom=False):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    encoding = "utf-8-sig" if bom else "utf-8"
    with out_path.open("w", newline="", encoding=encoding) as f:
        w = csv.writer(f)  # default excels at quoting; keeps embedded newlines
        if header:
            w.writerow(header)
        for r in rows:
            w.writerow(r)

for ts_path in FILES:
    try:
        tree = ET.parse(ts_path)
        root = tree.getroot()
    except Exception as e:
        print(f"[ERROR] Failed to parse {ts_path}: {e}", file=sys.stderr)
        continue

    lang = guess_lang(ts_path, root)
    rows = []

    if MODE == "1col":
        seen = set()
        for msg in each_message(root):
            if not KEEP_OBSOLETE and is_obsolete_or_vanished(msg):
                continue
            s = src_text(msg)
            if not s:
                continue
            if s not in seen:
                seen.add(s)
                rows.append([s])
        header = ["source"] if WRITE_HEADER else None
        out_name = ts_path.stem + "_single.csv"
        out_path = OUTDIR / out_name
        write_csv(rows, header, out_path, bom=WRITE_BOM)
        print(f"[OK] {ts_path.name} -> {out_path}")
        continue

    # MODE == "2col"
    for msg in each_message(root):
        if not KEEP_OBSOLETE and is_obsolete_or_vanished(msg):
            continue
        s = src_text(msg)
        if not s:
            continue
        t_el = msg.find("translation")
        if t_el is None:
            t_forms = [""]
            unfinished = True
        else:
            t_forms = plural_forms(t_el)
            unfinished = is_unfinished(t_el)

        if not INCL_UNFINISHED and unfinished:
            continue

        if EXPLODE_PLURALS and len(t_forms) > 1:
            rows.extend(explode_plural_rows(s, t_forms))
        else:
            t_joined = " | ".join(t_forms)
            rows.append([s, t_joined])

    header = (["source", f"translation_{lang}"] if WRITE_HEADER else None)
    out_name = ts_path.stem + ".csv"
    out_path = OUTDIR / out_name
    write_csv(rows, header, out_path, bom=WRITE_BOM)
    print(f"[OK] {ts_path.name} -> {out_path}")

PY

echo "Done. CSV files are in: $OUTDIR"
