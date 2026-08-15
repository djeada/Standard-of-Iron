#!/usr/bin/env python3
"""Ratchet the number of ambient-session lookups down, never up.

`OwnerRegistry::instance()`, `TerrainService::instance()` and friends resolve
"the current match" through the ambient session binding
(game/core/ambient_session.h). One match per process, that is fine. Two
sessions in one process -- a host with a local client, a replay watched from
the menu, a spectator -- need the code that touches per-match state to be
handed a `SessionContext&` (or the specific service) instead.

That migration is incremental: pass the session when you touch a file. This
check keeps it from going backwards. It counts the call sites per directory
and fails if any directory has *more* than the budget recorded in
scripts/ambient_instance_budget.json. When you remove some, lower the budget
(`--write` rewrites it to the current counts) so the gain is kept.

  usage: check-ambient-instances.py [repo-root] [--write]
"""

from __future__ import annotations

import json
import re
import sys
from collections import Counter
from pathlib import Path

BUDGET_FILE = "scripts/ambient_instance_budget.json"

AMBIENT = re.compile(
    r"\b(?:OwnerRegistry|TerrainService|VisibilityService|PlayerResourceRegistry"
    r"|NationRegistry|GlobalStatsRegistry|TroopCountRegistry"
    r"|BuildingCollisionRegistry|MarketplaceSystem)::instance\s*\("
    r"|\bSessionContext::active(?:_or_null)?\s*\("
)

LAYERS = ("game", "app", "ui", "render", "tools")
SOURCE_SUFFIXES = (".h", ".cpp")


DEFINITION_FILES = {
    "game/systems/owner_registry.cpp",
    "game/map/terrain_service.cpp",
    "game/map/visibility_service.cpp",
    "game/systems/player_resource_registry.cpp",
    "game/systems/nation_registry.cpp",
    "game/systems/global_stats_registry.cpp",
    "game/systems/troop_count_registry.cpp",
    "game/systems/building_collision_registry.cpp",
    "game/systems/marketplace_system.cpp",
    "game/session/session_context.cpp",
}


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
                hits = len(AMBIENT.findall(line))
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
        print(
            f"check-ambient-instances: wrote {BUDGET_FILE} ({sum(counts.values())} call sites)"
        )
        return 0

    budget = json.loads(budget_path.read_text())
    over = {k: (v, budget.get(k, 0)) for k, v in counts.items() if v > budget.get(k, 0)}
    under = {k: (v, budget[k]) for k, v in counts.items() if v < budget.get(k, 0)}
    gone = {k: v for k, v in budget.items() if k not in counts and v > 0}

    if over:
        print("check-ambient-instances: more ambient lookups than the budget allows:")
        for k, (now, allowed) in sorted(over.items()):
            print(f"  {k}: {now} (budget {allowed})")
        print(
            "\nPass the SessionContext (or the service) into the code you touched instead of\n"
            "calling X::instance(); if a new lookup is unavoidable, raise the budget in\n"
            f"{BUDGET_FILE} in the same change and say why."
        )
        return 1
    if under or gone:
        print(
            "check-ambient-instances: fewer lookups than budgeted -- run with --write to keep the gain:"
        )
        for k, (now, allowed) in sorted(under.items()):
            print(f"  {k}: {now} (budget {allowed})")
        for k, allowed in sorted(gone.items()):
            print(f"  {k}: 0 (budget {allowed})")
    print(
        f"check-ambient-instances: ok ({sum(counts.values())} call sites within budget)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
