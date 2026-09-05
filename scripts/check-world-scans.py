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

A scan is `collect_entities_with<T>` or `for_each_entity`, which walks every
entity in the registry and is strictly the more expensive of the two. Counting
call sites alone misses the shape that actually hurts, so the check separately
records which scans sit inside a `for` or `while` body -- one scan per iteration
is quadratic in a way one scan per tick is not -- and fails when a new one
appears. That list lives in scripts/world_scan_nested_allow.json.

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

SCAN = re.compile(r"\bcollect_entities_with\s*<|\bfor_each_entity\s*\(")

LOOP_HEADER = re.compile(r"^\s*(for|while)\s*\(|^\s*\}?\s*do\s*\{")

DEFINITION_FILES = {
    "game/core/world.h",
    "game/core/world.cpp",
    "game/core/world_spatial_index.cpp",
}

NESTED_ALLOWED_FILE = "scripts/world_scan_nested_allow.json"

LAYERS = ("game", "app", "ui", "render", "tools")
SOURCE_SUFFIXES = (".h", ".cpp")


def bucket(relative: str) -> str:
    parts = relative.split("/")

    return "/".join(parts[:2]) if len(parts) > 2 else parts[0]


def indent_of(line: str) -> int:
    return len(line) - len(line.lstrip())


def scan_lines(text: str) -> list[tuple[int, str, int]]:
    """Every scan call site as (line number, line, count of hits)."""
    found = []
    for number, line in enumerate(text.splitlines(), start=1):
        stripped = line.lstrip()
        if stripped.startswith("//") or stripped.startswith("*"):
            continue
        hits = len(SCAN.findall(line))
        if hits:
            found.append((number, line, hits))
    return found


def nested_scans(text: str) -> list[int]:
    """Scan call sites that sit inside a loop body, by indentation.

    A full scan under a `for` or `while` is the quadratic shape the module
    docstring warns about: one scan per asker, once per iteration. Indentation
    is a heuristic -- it cannot see a scan called through a helper -- so this is
    reported rather than budgeted, and a false positive is cheap to read past.
    """
    lines = text.splitlines()
    open_loops: list[int] = []
    nested: list[int] = []
    for number, line in enumerate(lines, start=1):
        stripped = line.lstrip()
        if stripped.startswith("//") or stripped.startswith("*"):
            continue
        indent = indent_of(line)
        while open_loops and indent <= open_loops[-1]:
            open_loops.pop()
        if SCAN.search(line) and open_loops:
            nested.append(number)
        if LOOP_HEADER.match(line):
            open_loops.append(indent)
    return nested


def count(root: Path) -> tuple[Counter[str], list[str]]:
    counts: Counter[str] = Counter()
    nested: list[str] = []
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
            text = source.read_text(errors="ignore")
            for _, _, hits in scan_lines(text):
                counts[bucket(relative)] += hits
            nested.extend(f"{relative}:{n}" for n in nested_scans(text))
    return counts, nested


def main(argv: list[str]) -> int:
    args = [a for a in argv[1:] if not a.startswith("--")]
    write = "--write" in argv
    root = Path(args[0]).resolve() if args else Path(__file__).resolve().parents[1]
    counts, nested = count(root)
    budget_path = root / BUDGET_FILE
    nested_path = root / NESTED_ALLOWED_FILE

    if write or not budget_path.exists():
        budget_path.write_text(
            json.dumps(dict(sorted(counts.items())), indent=2) + "\n"
        )
        nested_path.write_text(json.dumps(sorted(nested), indent=2) + "\n")
        print(
            f"check-world-scans: wrote {BUDGET_FILE} ({sum(counts.values())} scans)"
            f" and {NESTED_ALLOWED_FILE} ({len(nested)} inside a loop)"
        )
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
    allowed_nested = (
        set(json.loads(nested_path.read_text())) if nested_path.exists() else set()
    )
    new_nested = sorted(set(nested) - allowed_nested)
    if new_nested:
        print("check-world-scans: a full-world scan now runs inside a loop:")
        for site in new_nested:
            print(f"  {site}")
        print(
            "\nOne scan per iteration is O(entities * iterations) per tick. Hoist the\n"
            "scan out of the loop, or answer it from world.spatial_index(). If the\n"
            f"loop is bounded and the scan is genuinely needed, add it to\n{NESTED_ALLOWED_FILE}\n"
            "in the same change and say why."
        )
        return 1
    stale_nested = sorted(allowed_nested - set(nested))
    if stale_nested:
        print(
            "check-world-scans: these loop-nested scans are gone -- run with --write "
            "to keep the gain:"
        )
        for site in stale_nested:
            print(f"  {site}")
    print(
        f"check-world-scans: ok ({sum(counts.values())} scans within budget,"
        f" {len(nested)} inside a loop)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
