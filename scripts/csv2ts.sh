#!/usr/bin/env bash
set -euo pipefail

# csv2ts.sh — Apply CSV translations back into Qt .ts files
#
# Typical usage:
#   1) Update an existing locale using its CSV:
#      ./scripts/csv2ts.sh -i translations/csv/app_de.csv -t translations/app_de.ts -o translations/updated
#
#   2) Create a new locale from English template + CSV:
#      ./scripts/csv2ts.sh -i translations/csv/app_fr.csv -t translations/app_en.ts -l fr_FR -o translations/updated
#
#   3) In-place update (overwrites the template .ts):
#      ./scripts/csv2ts.sh -i translations/csv/app_de.csv -t translations/app_de.ts --inplace --backup
#
# CSV expectations (auto-detected):
#   - 2-column with header: "source,<anything>"  (e.g., "translation_de" or "translation")
#   - 2-column without header: column 1=source, column 2=translation
#   - Plurals in CSV may be:
#       - Joined in a single cell, forms separated by " | "
#       - OR "exploded": rows with source suffixed by " [plural N]" (0-based)
#
# Notes:
# - Matches ts2csv defaults: embedded newlines were flattened to "\n" during export;
#   this script unflattens those back to real newlines by default.
# - Messages not present in CSV remain unchanged.
# - If the template message is plural (numerus="yes"), CSV can be joined or exploded; both are supported.
# - If CSV provides fewer plural forms than exist in template, only provided indices are updated; others kept.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTDIR="$REPO_ROOT/translations/updated"

INPUT_CSV=""
TEMPLATE_TS=""
LANG_OVERRIDE=""
PLURAL_SEP=" | "
INPLACE=0
BACKUP=0
CLEAR_WHEN_EMPTY=1
KEEP_TEXT_WHEN_EMPTY=0

# Parse args
ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -i|--input) INPUT_CSV="$2"; shift 2;;
    -t|--template) TEMPLATE_TS="$2"; shift 2;;
    -o|--outdir) OUTDIR="$2"; shift 2;;
    -l|--lang) LANG_OVERRIDE="$2"; shift 2;;
    --plural-sep) PLURAL_SEP="$2"; shift 2;;
    --inplace) INPLACE=1; shift;;
    --backup) BACKUP=1; shift;;
    --clear-when-empty) CLEAR_WHEN_EMPTY=1; KEEP_TEXT_WHEN_EMPTY=0; shift;;
    --keep-text-when-empty) KEEP_TEXT_WHEN_EMPTY=1; CLEAR_WHEN_EMPTY=0; shift;;
    -h|--help)
      grep '^# ' "$0" | sed 's/^# //'
      exit 0
      ;;
    *) ARGS+=("$1"); shift;;
  esac
done
set -- "${ARGS[@]}"

if [[ -z "$INPUT_CSV" ]]; then
  echo "Error: --input CSV is required" >&2
  exit 1
fi
if [[ -z "$TEMPLATE_TS" ]]; then
  echo "Error: --template TS is required" >&2
  exit 1
fi

if [[ $INPLACE -eq 0 ]]; then
  mkdir -p "$OUTDIR"
fi

if [[ $INPLACE -eq 1 && $BACKUP -eq 1 ]]; then
  cp -f "$TEMPLATE_TS" "$TEMPLATE_TS.bak"
fi

python3 - <<'PY' "$INPUT_CSV" "$TEMPLATE_TS" "$OUTDIR" "$LANG_OVERRIDE" "$PLURAL_SEP" "$INPLACE" "$CLEAR_WHEN_EMPTY" "$KEEP_TEXT_WHEN_EMPTY"
import sys, re, csv, xml.etree.ElementTree as ET
from pathlib import Path

INPUT_CSV = Path(sys.argv[1])
TEMPLATE_TS = Path(sys.argv[2])
OUTDIR = Path(sys.argv[3])
LANG_OVERRIDE = sys.argv[4]
PLURAL_SEP = sys.argv[5]
INPLACE = sys.argv[6] == "1"
CLEAR_WHEN_EMPTY = sys.argv[7] == "1"
KEEP_TEXT_WHEN_EMPTY = sys.argv[8] == "1"

def decode_flattened_newlines(s: str) -> str:
    if s is None:
        return ""
    # normalize AND unflatten literal "\n" back to real newlines
    s = s.replace("\r\n", "\n").replace("\r", "\n")
    return s.replace("\\n", "\n")

def read_csv_maps(csv_path: Path):
    """
    Returns two maps:
      raw_map[src] = raw string value from CSV (unflattened newlines)
      exploded[src] = {index: form_text} for any " [plural N]" rows
    Joined plurals (value contains PLURAL_SEP) are kept as a single string in raw_map.
    We only split by PLURAL_SEP later if the target message is actually plural.
    """
    raw_map: dict[str, str] = {}
    exploded: dict[str, dict[int, str]] = {}

    with csv_path.open("r", newline="", encoding="utf-8-sig") as f:
        rdr = csv.reader(f)  # handles commas/quotes/multiline properly
        rows = list(rdr)

    if not rows:
        return raw_map, exploded

    # Detect header
    header = None
    if any(isinstance(h, str) and h.lower() == "source" for h in rows[0]):
        header = [h.strip() for h in rows[0]]
        rows = rows[1:]

    # Column indices
    if header:
        try:
            source_idx = [i for i, h in enumerate(header) if h.lower() == "source"][0]
        except IndexError:
            raise SystemExit("CSV header must contain a 'source' column")
        trans_idx_candidates = [i for i, h in enumerate(header) if i != source_idx]
        if not trans_idx_candidates:
            raise SystemExit("CSV must have a translation column")
        trans_idx = trans_idx_candidates[0]
    else:
        if len(rows[0]) < 2:
            raise SystemExit("CSV without header must have at least 2 columns: source, translation")
        source_idx, trans_idx = 0, 1

    plural_tag_re = re.compile(r"\s*\[plural\s+(\d+)\]\s*$", re.IGNORECASE)

    for r in rows:
        if not r:
            continue
        if len(r) <= max(source_idx, trans_idx):
            r = (r + [""] * (max(source_idx, trans_idx) + 1 - len(r)))
        src = (r[source_idx] or "").strip()
        if not src:
            continue
        val = decode_flattened_newlines(r[trans_idx] or "")

        m = plural_tag_re.search(src)
        if m:
            base_src = plural_tag_re.sub("", src).rstrip()
            d = exploded.setdefault(base_src, {})
            d[int(m.group(1))] = val
            continue

        # store raw (possibly joined) value; don't split here
        raw_map[src] = val

    return raw_map, exploded

def text_of(elem: ET.Element) -> str:
    parts = []
    if elem.text:
        parts.append(elem.text)
    for child in elem:
        parts.append(text_of(child))
        if child.tail:
            parts.append(child.tail)
    return "".join(parts)

def set_text(elem: ET.Element, value: str):
    # Replace all content with a single text node
    for child in list(elem):
        elem.remove(child)
    elem.text = value

def update_ts(template_ts: Path, raw_map: dict, exploded: dict, lang_override: str | None):
    tree = ET.parse(template_ts)
    root = tree.getroot()

    if lang_override:
        root.set("language", lang_override)

    for msg in root.findall(".//context/message"):
        source_el = msg.find("source")
        if source_el is None:
            continue
        src = (text_of(source_el) or "").strip()
        if not src:
            continue

        numerus = (msg.get("numerus") or "").lower() == "yes"
        trans_el = msg.find("translation")
        if trans_el is None:
            trans_el = ET.SubElement(msg, "translation")

        if src not in raw_map and src not in exploded:
            # Not provided in CSV — leave untouched
            continue

        if numerus:
            # prefer exploded rows if present
            if src in exploded:
                idx_map = exploded[src]
                max_index = max(idx_map.keys()) if idx_map else -1
                forms = ["" for _ in range(max_index + 1)]
                for i, v in idx_map.items():
                    if i >= 0:
                        if i >= len(forms):
                            forms.extend([""] * (i + 1 - len(forms)))
                        forms[i] = v
            else:
                raw_val = raw_map.get(src, "")
                # only split joined forms if the message is numerus
                forms = [s.strip() for s in raw_val.split(PLURAL_SEP)] if PLURAL_SEP in raw_val else [raw_val]

            existing = trans_el.findall("numerusform")
            if existing:
                for i, child in enumerate(existing):
                    if i < len(forms):
                        set_text(child, forms[i])
                for i in range(len(existing), len(forms)):
                    n = ET.SubElement(trans_el, "numerusform")
                    set_text(n, forms[i])
            else:
                for v in forms:
                    n = ET.SubElement(trans_el, "numerusform")
                    set_text(n, v)

            empty_all = all((v.strip() == "") for v in forms)
            if empty_all:
                if CLEAR_WHEN_EMPTY:
                    for child in trans_el.findall("numerusform"):
                        set_text(child, "")
                trans_el.set("type", "unfinished")
            else:
                trans_el.attrib.pop("type", None)

        else:
            # non-plural: take the raw value as-is, even if it contains the plural separator
            val = raw_map.get(src, "")
            if val.strip() == "":
                if CLEAR_WHEN_EMPTY:
                    set_text(trans_el, "")
                trans_el.set("type", "unfinished")
            else:
                set_text(trans_el, val)
                trans_el.attrib.pop("type", None)

    return tree

def write_tree(tree: ET.ElementTree, out_path: Path):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    tree.write(out_path, encoding="utf-8", xml_declaration=True)

raw_map, exploded = read_csv_maps(INPUT_CSV)
tree = update_ts(TEMPLATE_TS, raw_map, exploded, LANG_OVERRIDE or None)

out_path = TEMPLATE_TS if INPLACE else (OUTDIR / TEMPLATE_TS.name)
write_tree(tree, out_path)
print(f"[OK] Applied {INPUT_CSV.name} -> {out_path}")

PY

echo "Done."
