#!/usr/bin/env python3
"""Ratchet entity-object component access down, never up.

The ECS stores component data in the registry, one dense pool per type, and the
fast way to reach it is a query that starts from the component:

    for (auto [id, transform, movement] :
         world.view<TransformComponent, MovementComponent>()) { ... }

`entity->get_component<T>()` is the old shape. It starts from an entity object,
resolves a handle, and asks one component at a time, so a system written that
way pays a lookup per component per entity and keeps `Entity*` alive in code
that only needed an id. Both forms still compile -- the migration is
incremental -- so this check keeps the old one from spreading: it counts the
entity-centric call sites per directory and fails if any directory has *more*
than the budget recorded in scripts/entity_access_budget.json.

When you convert a system, lower the budget (`--write` rewrites it to the
current counts) so the gain is kept.

`game/core/` is exempt: entity.h defines these members, and world.cpp is the
one place that implements the old API in terms of the new storage.

  usage: check-entity-access.py [repo-root] [--write]
"""

from __future__ import annotations

import json
import re
import sys
from collections import Counter
from pathlib import Path

BUDGET_FILE = "scripts/entity_access_budget.json"

ACCESS = re.compile(
    r"(?:->|\.)(?:get_component|has_component|add_component|remove_component)\s*<"
)

EXEMPT_PREFIXES = ("game/core/",)

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
            if relative.startswith(EXEMPT_PREFIXES):
                continue
            for line in source.read_text(errors="ignore").splitlines():
                stripped = line.lstrip()
                if stripped.startswith("//") or stripped.startswith("*"):
                    continue
                hits = len(ACCESS.findall(line))
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
            f"check-entity-access: wrote {BUDGET_FILE} "
            f"({sum(counts.values())} entity-centric accesses)"
        )
        return 0

    budget = json.loads(budget_path.read_text())
    over = {k: (v, budget.get(k, 0)) for k, v in counts.items() if v > budget.get(k, 0)}
    under = {k: (v, budget[k]) for k, v in counts.items() if v < budget.get(k, 0)}
    gone = {k: v for k, v in budget.items() if k not in counts and v > 0}

    if over:
        print("check-entity-access: more entity-object component access than allowed:")
        for k, (now, allowed) in sorted(over.items()):
            print(f"  {k}: {now} (budget {allowed})")
        print(
            "\nAsk the registry for the components instead:\n"
            "  for (auto [id, a, b] : world.view<A, B>()) { ... }\n"
            "  world.try_get<T>(id) / world.has<T>(id) / world.emplace<T>(id)\n"
            f"If an entity object is genuinely the right shape, raise the budget in\n"
            f"{BUDGET_FILE} in the same change and say why."
        )
        return 1
    if under or gone:
        print(
            "check-entity-access: fewer accesses than budgeted -- "
            "run with --write to keep the gain:"
        )
        for k, (now, allowed) in sorted(under.items()):
            print(f"  {k}: {now} (budget {allowed})")
        for k, allowed in sorted(gone.items()):
            print(f"  {k}: 0 (budget {allowed})")
    print(f"check-entity-access: ok ({sum(counts.values())} accesses within budget)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
