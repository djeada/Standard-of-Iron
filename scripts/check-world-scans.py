#!/usr/bin/env python3
"""Ratchet the number of full-world entity scans down, never up.

`world->collect_entities_with<T>()` walks every entity carrying `T` and
materialises a `std::vector<Entity*>` of them. That is the right shape for a
question that genuinely concerns every entity -- counting an owner's army,
sweeping components whose timer expired. It is the wrong shape for "what is near
me?", and the difference does not show up until a battle is large: one radius
question answered by a full scan costs O(units) per asker, so a system that asks
it once per unit costs O(units^2) per tick.

The engine rule is therefore:

    A local, radius-bounded gameplay query goes through the world's spatial
    index (`Engine::Core::WorldSpatialIndex`, reached as `world.spatial_index()`),
    not through a full entity scan.

The index is rebuilt once per tick and answers a query in proportion to what is
actually nearby. `Game::Systems::Combat::collect_unit_ids_near` is the
convenience wrapper the combat paths use.

This check keeps the rule from eroding. It counts the scan call sites per
directory and fails if any directory has *more* than the budget recorded in
scripts/world_scan_budget.json. When you remove some, lower the budget
(`--write` rewrites it to the current counts) so the gain is kept.

DEFINITION_FILES below lists where the query itself is defined, plus the one
place a full scan is the whole point: the index's own rebuild has to look at
every entity in order to index it.

  usage: check-world-scans.py [repo-root] [--write]
"""

from __future__ import annotations

import json
import re
import sys
from collections import Counter
from pathlib import Path

BUDGET_FILE = "scripts/world_scan_budget.json"

SCAN = re.compile(r"\bcollect_entities_with\s*<")

DEFINITION_FILES = {
    "game/core/world.h",
    "game/core/world.cpp",
    "game/core/world_spatial_index.cpp",
}

LAYERS = ("game", "app", "ui", "render", "tools")
SOURCE_SUFFIXES = (".h", ".cpp")


def bucket(relative: str) -> str:
    parts = relative.split("/")

    return "/".join(parts[:2]) if len(parts) > 2 else parts[0]


def count(root: Path) -> Counter[str]:
    counts: Counter[str] = Counter()
    for layer in LAYERS:
        directory = root / layer
        if not directory.is_dir():
            continue
        for source in sorted(directory.rglob("*")):
            if source.suffix not in SOURCE_SUFFIXES:
                continue
            relative = source.relative_to(root).as_posix()
            if relative in DEFINITION_FILES:
                continue
            for line in source.read_text(errors="ignore").splitlines():
                stripped = line.lstrip()
                if stripped.startswith("//") or stripped.startswith("*"):
                    continue
                hits = len(SCAN.findall(line))
                if hits:
                    counts[bucket(relative)] += hits
    return counts


def main(argv: list[str]) -> int:
    args = [a for a in argv[1:] if not a.startswith("--")]
    write = "--write" in argv
    root = Path(args[0]).resolve() if args else Path(__file__).resolve().parents[1]
    counts = count(root)
    budget_path = root / BUDGET_FILE

    if write or not budget_path.exists():
        budget_path.write_text(
            json.dumps(dict(sorted(counts.items())), indent=2) + "\n"
        )
        print(f"check-world-scans: wrote {BUDGET_FILE} ({sum(counts.values())} scans)")
        return 0

    budget = json.loads(budget_path.read_text())
    over = {k: (v, budget.get(k, 0)) for k, v in counts.items() if v > budget.get(k, 0)}
    under = {k: (v, budget[k]) for k, v in counts.items() if v < budget.get(k, 0)}
    gone = {k: v for k, v in budget.items() if k not in counts and v > 0}

    if over:
        print("check-world-scans: more full-world scans than the budget allows:")
        for k, (now, allowed) in sorted(over.items()):
            print(f"  {k}: {now} (budget {allowed})")
        print(
            "\nIf the new query is radius-bounded, ask the spatial index instead:\n"
            "  world.spatial_index().refresh(world);\n"
            "  world.spatial_index().for_each_in_radius(x, z, radius, ...);\n"
            "If it genuinely concerns every entity, raise the budget in\n"
            f"{BUDGET_FILE} in the same change and say why."
        )
        return 1
    if under or gone:
        print(
            "check-world-scans: fewer scans than budgeted -- run with --write to keep the gain:"
        )
        for k, (now, allowed) in sorted(under.items()):
            print(f"  {k}: {now} (budget {allowed})")
        for k, allowed in sorted(gone.items()):
            print(f"  {k}: 0 (budget {allowed})")
    print(f"check-world-scans: ok ({sum(counts.values())} scans within budget)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
