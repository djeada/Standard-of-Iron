#!/usr/bin/env python3
"""Fail the build when the client gives an order without going through the
command queue.

The rule: everything under app/ and ui/ is a *client*. It reads the match, it
asks services whether an order would be accepted so it can phrase a refusal,
and then it submits a `Game::Command` payload. It never applies the order
itself -- not by calling the movement, production, marketplace, formation or
placement services' mutating entry points, and not by writing the fields of
`BuilderProductionComponent` that those services own.

That is what lets the same client drive a local match, a remote one and a
replay: the payload is the only thing that crosses.

Two things are checked in app/ and ui/:

  * no call to a service entry point that applies an order (the list below is
    the set the dispatcher calls; keep it in step with command_dispatcher.cpp);
  * no assignment to the task fields of BuilderProductionComponent.

The commander's first-person control mode is a local input mode, not an
order; the files that implement it may reset the controlled commander's own
movement.

  usage: check-command-boundary.py [repo-root]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

CLIENT_LAYERS = ("app", "ui")


FPV_FILES = {
    "app/core/commander_mode_coordinator.cpp",
    "app/core/commander_control_controller.cpp",
}

ORDER_ENTRY_POINTS = re.compile(
    r"""
    \b(?:
        OrderService::(?!reset_movement\b)\w+
      | CommandService::(?:move_units|move_unit|issue_ground_move|attack_target)
      | ProductionService::(?:start_production|set_rally_point)
      | MarketplaceSystem::instance\(\)\.(?:buy|sell)_resource
      | (?:marketplace|market)\(\)\.(?:buy|sell)_resource
      | WallPlanService::commit
      | StructurePlacementService::place
      | ArmyFormationService::(?:commit|release)
      | PlayerResourceRegistry::instance\(\)\.spend
      | TerrainService::instance\(\)\.reserve_world_prop
      | (?:begin_flag_rally|request_aura_ability)\s*\(
    )
    """,
    re.VERBOSE,
)

FPV_ALLOWED = re.compile(r"\bOrderService::reset_movement\b")

BUILDER_TASK_WRITE = re.compile(
    r"""
    \b\w*builder\w*(?:->|\.)
    (?:
        has_construction_site | construction_site_\w+ | product_type
      | build_time | time_remaining | in_progress | at_construction_site
      | construction_complete | has_task_target | task_target_\w+
      | queued_construction_site_ids | structure_task_entity_id
      | bypass_movement_active
    )
    \s*=(?!=)
    """,
    re.VERBOSE,
)

SOURCE_SUFFIXES = (".h", ".cpp")


def sources(root: Path) -> list[Path]:
    found: list[Path] = []
    for layer in CLIENT_LAYERS:
        directory = root / layer
        if directory.is_dir():
            found.extend(p for p in directory.rglob("*") if p.suffix in SOURCE_SUFFIXES)
    return sorted(found)


def violations(root: Path) -> list[str]:
    found: list[str] = []
    for source in sources(root):
        relative = source.relative_to(root).as_posix()
        for number, line in enumerate(
            source.read_text(errors="ignore").splitlines(), 1
        ):
            stripped = line.lstrip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            match = ORDER_ENTRY_POINTS.search(line)
            if match:
                found.append(
                    f"{relative}:{number}: applies an order itself: {match.group(0).strip()}"
                )
            if relative not in FPV_FILES and FPV_ALLOWED.search(line):
                found.append(
                    f"{relative}:{number}: OrderService::reset_movement outside the FPV files"
                )
            match = BUILDER_TASK_WRITE.search(line)
            if match:
                found.append(
                    f"{relative}:{number}: writes a builder task field: {match.group(0).strip()}"
                )
    return found


def main(argv: list[str]) -> int:
    root = (
        Path(argv[1]).resolve()
        if len(argv) > 1
        else Path(__file__).resolve().parents[1]
    )
    found = violations(root)
    if found:
        print("check-command-boundary: the client applies orders it should submit:")
        for item in found:
            print(f"  {item}")
        print(
            "\nSubmit a Game::Command payload (see docs/ARCHITECTURE.md, 'The command "
            "pipeline'); if a payload does not exist yet, add one to game/command/command.h "
            "and a handler to command_dispatcher.cpp."
        )
        return 1
    print("check-command-boundary: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
