#!/usr/bin/env python3
"""
Validate that mounted troop configs do not carry per-unit ground offsets.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path


MOUNTED_IDS = {"horse_swordsman", "horse_archer", "horse_spearman"}
DATA_ROOT = Path(__file__).resolve().parent.parent / "assets" / "data"


def load_troops(path: Path) -> list[dict]:
    with path.open("r", encoding="utf-8") as f:
        data = json.load(f)
    return data.get("troops", [])


def main() -> int:
    files = [DATA_ROOT / "troops" / "base.json"]
    files.extend(sorted((DATA_ROOT / "nations").glob("*.json")))

    errors = []

    for path in files:
        for troop in load_troops(path):
            troop_id = troop.get("id")
            if troop_id not in MOUNTED_IDS:
                continue
            visuals = troop.get("visuals", {})
            val = visuals.get("selection_ring_ground_offset")
            if val not in (None, 0, 0.0):
                errors.append(
                    f"{path}: {troop_id} has non-zero selection_ring_ground_offset {val}"
                )

    if errors:
        for err in errors:
            print(err, file=sys.stderr)
        return 1

    print("Mounted ground offsets are empty/zero across configs.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
