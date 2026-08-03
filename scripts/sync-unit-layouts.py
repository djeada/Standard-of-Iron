#!/usr/bin/env python3
"""Regenerate assets/data/formations/unit_layouts/*.json from the C++ built-ins.

The JSON overlays the built-in styles in game/formation/unit_layout.cpp at
runtime, so the two copies drift silently unless the JSON is generated from the
source of truth. Run after touching any make_style(...) block:

    python3 scripts/sync-unit-layouts.py [--check]

--check exits non-zero instead of writing, for CI.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "game" / "formation" / "unit_layout.cpp"
OUT_DIR = ROOT / "assets" / "data" / "formations" / "unit_layouts"


FIELDS = [
    "lateral_spacing_scale",
    "depth_spacing_scale",
    "rank_stagger",
    "rank_echelon",
    "rank_arc",
    "front_rank_tightening",
    "rear_rank_loosening",
    "rear_depth_bias",
    "file_grouping",
    "group_gap",
    "group_depth_stagger",
    "weapon_clearance",
    "wedge_slope",
    "wedge_growth",
    "cluster_pull",
    "cluster_size",
    "radius_scale",
    "column_files",
    "lateral_jitter",
    "depth_jitter",
    "rear_jitter_gain",
    "facing_jitter_degrees",
    "min_separation_scale",
]

BLOCK = re.compile(
    r'make_style\("(?P<id>[a-z_.]+)",\s*UnitLayoutShape::(?P<shape>\w+)\);'
    r"(?P<body>.*?)out\.push_back\(style\);",
    re.DOTALL,
)
ASSIGN = re.compile(r"style\.(\w+) = (-?[0-9.]+)F;")


def parse_styles(text: str) -> dict[str, dict]:
    styles = {}
    for match in BLOCK.finditer(text):
        style = {"shape": shape_to_json(match.group("shape"))}
        for field, value in ASSIGN.findall(match.group("body")):
            if field in FIELDS:
                style[field] = round(float(value), 4)
        styles[match.group("id")] = style
    return styles


def shape_to_json(name: str) -> str:
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


def render(style_id: str, style: dict) -> str:
    doc = {"id": style_id, "shape": style["shape"]}
    for field in FIELDS:
        if field in style:
            doc[field] = style[field]
    return json.dumps(doc, indent=2) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    styles = parse_styles(SOURCE.read_text())
    faction_styles = {k: v for k, v in styles.items() if "." in k}

    stale = []
    written = 0
    for style_id, style in sorted(faction_styles.items()):
        path = OUT_DIR / (style_id.replace(".", "_") + ".json")
        payload = render(style_id, style)
        if path.exists() and path.read_text() == payload:
            continue
        if args.check:
            stale.append(path.relative_to(ROOT))
            continue
        path.write_text(payload)
        written += 1

    known = {sid.replace(".", "_") + ".json" for sid in faction_styles}
    orphans = [p for p in sorted(OUT_DIR.glob("*.json")) if p.name not in known]
    for path in orphans:
        if args.check:
            stale.append(path.relative_to(ROOT))
        else:
            path.unlink()

    if stale:
        print("unit layout JSON is out of date:", file=sys.stderr)
        for path in stale:
            print(f"  {path}", file=sys.stderr)
        return 1

    print(f"sync-unit-layouts: {written} written, {len(orphans)} removed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
