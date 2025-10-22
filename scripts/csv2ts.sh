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
#   - Plurals:
#       - Joined in a single cell: plural forms separated by " | "   (default)
#       - OR "exploded": create rows with source suffixed by " [plural N]" (0-based)
#
# Flags:
#   -i, --input CSV            Input CSV file
#   -t, --template TS          Template .ts file to update (existing locale or English)
#   -o, --outdir DIR           Where to write updated .ts (default: <repo>/translations/updated)
#   -l, --lang LANG            Override TS @language (e.g. de_DE, fr_FR)
#       --plural-sep SEP       Separator for joined plural forms (default: " | ")
#       --inplace              Write changes back to the template .ts path
#       --backup               When --inplace, create a .bak next to the template
#       --clear-when-empty     If translation cell empty, set type="unfinished" and clear text (default: on)
#       --keep-text-when-empty Keep previous text if CSV cell empty (still marks unfinished)
#
# Notes:
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
import sys, os, re, csv, xml.etree.ElementTree as ET
from pathlib import Path

INPUT_CSV = Path(sys.argv[1])
TEMPLATE_TS = Path(sys.argv[2])
OUTDIR = Path(sys.argv[3])
LANG_OVERRIDE = sys.argv[4]
PLURAL_SEP = sys.argv[5]
INPLACE = sys.argv[6] == "1"
CLEAR_WHEN_EMPTY = sys.argv[7] == "1"
KEEP_TEXT_WHEN_EMPTY = sys.argv[8] == "1"

def read_csv_map(csv_path: Path):
    """
    Returns:
      mapping: dict[source] -> list[str] (plural forms) OR single-element list for non-plural
      exploded_indices: dict[source] -> {idx: text} (if exploded plural rows were found)
    """
    mapping = {}
    exploded_indices = {}
    def add_mapping(src, val):
        if src not in mapping:
            mapping[src] = []
        mapping[src] = [val]  # non-plural default; may be replaced if joined contains multiple
    def set_plural(src, idx, val):
        d = exploded_indices.setdefault(src, {})
        d[int(idx)] = val

    with csv_path.open("r", newline="", encoding="utf-8-sig") as f:
        rdr = csv.reader(f)
        rows = list(rdr)

    if not rows:
        return mapping, exploded_indices

    # Detect header
    header = None
    if any(h.lower() == "source" for h in rows[0]):
        header = [h.strip() for h in rows[0]]
        rows = rows[1:]

    # Column indices
    if header:
        try:
            source_idx = [i for i,h in enumerate(header) if h.lower()=="source"][0]
        except IndexError:
            raise SystemExit("CSV header must contain a 'source' column")
        # translation column = first non-source column
        trans_idx_candidates = [i for i,h in enumerate(header) if i != source_idx]
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
        # pad short rows
        if len(r) <= max(source_idx, trans_idx):
            r = (r + [""]*(max(source_idx, trans_idx)+1 - len(r)))
        src = (r[source_idx] or "").strip()
        if not src:
            continue
        val = (r[trans_idx] or "")

        m = plural_tag_re.search(src)
        if m:
            base_src = plural_tag_re.sub("", src).rstrip()
            set_plural(base_src, m.group(1), val)
            continue

        # joined plural forms? split by PLURAL_SEP; keep raw if no sep
        if PLURAL_SEP in val:
            forms = [s for s in (p.strip() for p in val.split(PLURAL_SEP))]
            mapping[src] = forms
        else:
            add_mapping(src, val)

    # Merge exploded forms into mapping
    for base_src, idx_map in exploded_indices.items():
        max_index = max(idx_map.keys()) if idx_map else -1
        forms = ["" for _ in range(max_index+1)]
        for i, v in idx_map.items():
            if i < 0:
                continue
            if i >= len(forms):
                forms.extend([""] * (i+1-len(forms)))
            forms[i] = v
        mapping[base_src] = forms

    return mapping, exploded_indices

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

def update_ts(template_ts: Path, mapping: dict, lang_override: str | None):
    tree = ET.parse(template_ts)
    root = tree.getroot()

    if lang_override:
        root.set("language", lang_override)

    # namespace cleanup if present
    for ctx in root.findall(".//context"):
        for msg in ctx.findall("message"):
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

            if src not in mapping:
                # Not provided in CSV — leave untouched
                continue

            values = mapping[src]  # list[str] even for non-plural
            if numerus:
                # ensure <numerusform> children
                existing = trans_el.findall("numerusform")
                if existing:
                    # Update existing indices; if CSV gives fewer, update only provided
                    for i, child in enumerate(existing):
                        if i < len(values):
                            set_text(child, values[i])
                    # If CSV provides more, append more nodes
                    for i in range(len(existing), len(values)):
                        n = ET.SubElement(trans_el, "numerusform")
                        set_text(n, values[i])
                else:
                    # No children yet; create from CSV
                    for v in values:
                        n = ET.SubElement(trans_el, "numerusform")
                        set_text(n, v)

                # Mark finished/unfinished
                empty_all = all((v.strip() == "") for v in values)
                if empty_all:
                    if CLEAR_WHEN_EMPTY:
                        # clear all forms
                        for child in trans_el.findall("numerusform"):
                            set_text(child, "")
                    trans_el.set("type", "unfinished")
                else:
                    # remove 'type' attribute if present
                    if "type" in trans_el.attrib:
                        del trans_el.attrib["type"]

            else:
                val = values[0] if values else ""
                if val.strip() == "":
                    if CLEAR_WHEN_EMPTY:
                        set_text(trans_el, "")
                    # mark unfinished (or keep text if KEEP_TEXT_WHEN_EMPTY)
                    trans_el.set("type", "unfinished")
                else:
                    set_text(trans_el, val)
                    if "type" in trans_el.attrib:
                        del trans_el.attrib["type"]

    return tree, root

def write_tree(tree: ET.ElementTree, out_path: Path):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    tree.write(out_path, encoding="utf-8", xml_declaration=True)

mapping, _ = read_csv_map(INPUT_CSV)
tree, root = update_ts(TEMPLATE_TS, mapping, LANG_OVERRIDE or None)

if INPLACE:
    out_path = TEMPLATE_TS
else:
    out_path = OUTDIR / TEMPLATE_TS.name

write_tree(tree, out_path)
print(f"[OK] Applied {INPUT_CSV.name} -> {out_path}")

PY

echo "Done."
