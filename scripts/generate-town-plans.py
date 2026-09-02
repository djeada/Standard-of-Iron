#!/usr/bin/env python3
"""Generate the AI commanders' castle blueprints, assets/data/ai/town_plans.json.

Each commander raises its own settlement at runtime from a town plan: an
ordered list of buildings at offsets in the settlement's frame (x right, -z
toward the enemy, origin on the primary barracks, which nothing may stand within
9 m of). The builder walks the steps in order, skipping any that already stand
or cannot be afforded, so the *order* is the shape of a half-built town - the
front wall and its gate first, then the towers that cover it, then the flanks,
then the rear, then the keep. A player who sees two towers and a gate knows
which commander is behind them before the rest goes up.

The circuits come from settlement_geometry, the same rasteriser that stamps the
campaign maps' pre-built towns, so a commander's runtime castle and its authored
one on a campaign map share a shape. Walls are single 2 m links on the lattice
(rotation 0 along x, 90 along z); gates are one link with the wall standing
off three links either side, exactly as the map generator lays them.

Dry run by default; --write replaces the file.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from settlement_geometry import (
    Cell,
    Region,
    bastion_apexes,
    bastioned_polygon,
    cut_gate,
    ellipse_polygon,
    partition_runs,
    path_cells,
    rectangle_polygon,
    region_cells,
    run_is_horizontal,
)

GATE_CELLS = 3
GATE_HALF_SPAN = 4.5
FOOTPRINT = {
    "barracks": 4.4,
    "home": 1.5,
    "marketplace": 1.5,
    "defense_tower": 1.5,
    "catapult": 1.5,
    "ballista": 1.5,
}

STANDOFF = {
    "defense_tower": 4.0,
    "home": 4.0,
    "marketplace": 4.0,
    "barracks": 6.0,
    "catapult": 4.0,
    "ballista": 4.0,
}
"""How far a building keeps from a wall link in the plan frame.

The runtime snaps every link to its own 2 m lattice, which can move it a metre
and a half from where the plan put it; a building that stood 3 m from the
link on paper then finds its slot occupied and is never raised."""
ANCHOR_CLEARANCE = 9.0
WALL_CAP = 128
TOWER_CAP = 12
HOME_CAP = 12


@dataclass
class Step:
    building: str
    x: float
    z: float
    rotation: float | None = None

    def to_json(self) -> dict:
        entry: dict = {
            "building": self.building,
            "x": round(self.x, 1),
            "z": round(self.z, 1),
        }
        if self.rotation is not None:
            entry["rotation"] = round(self.rotation, 1)
        return entry


@dataclass
class Blueprint:
    id: str
    display_name: str
    steps: list[Step] = field(default_factory=list)
    links: list[Step] = field(default_factory=list)

    def add(
        self, building: str, x: float, z: float, rotation: float | None = None
    ) -> None:
        """Place a building at the nearest spot to (x, z) that the runtime will accept.

        The wanted spot is first choice; failing that, spots are tried inward
        toward the anchor and then round about, so a house authored against a
        wall ends up a lattice snap away from it rather than never raised.
        """
        if building in ("wall_segment", "wall_gate"):
            self.steps.append(Step(building, x, z, rotation))
            return
        candidates = [(x, z)]
        inward = math.hypot(x, z)
        for step in range(1, 9):
            fraction = max(0.0, (inward - step) / inward) if inward > 0 else 0.0
            candidates.append((x * fraction, z * fraction))
        for ring in range(1, 12):
            for index in range(8 * ring):
                angle = 2.0 * math.pi * index / (8 * ring)
                candidates.append(
                    (x + math.cos(angle) * ring, z + math.sin(angle) * ring)
                )
        for cx, cz in candidates:
            if self.fits(building, cx, cz):
                self.steps.append(Step(building, round(cx, 1), round(cz, 1), rotation))
                return
        raise SystemExit(f"{self.id}: no room for a {building} near {x},{z}")

    def fits(self, building: str, x: float, z: float) -> bool:
        if math.hypot(x, z) < ANCHOR_CLEARANCE + FOOTPRINT.get(building, 1.5):
            return False
        reach = STANDOFF.get(building, 4.0)
        for link in self.links:
            gate_reach = (
                reach + GATE_HALF_SPAN if link.building == "wall_gate" else reach
            )
            if math.hypot(x - link.x, z - link.z) < gate_reach:
                return False
        for step in self.steps:
            if step.building in ("wall_segment", "wall_gate"):
                continue
            gap = FOOTPRINT.get(building, 1.5) + FOOTPRINT.get(step.building, 1.5) + 0.6
            if math.hypot(x - step.x, z - step.z) < gap:
                return False
        return True

    def count(self, building: str) -> int:
        return sum(1 for step in self.steps if step.building == building)


def wall_steps(
    cells: set[Cell], gates: list[tuple[float, float]]
) -> tuple[list[Step], list[Step]]:
    """Cut the gates, then order the wall so the enemy-facing side goes up first."""
    runs = partition_runs(cells)
    gate_steps: list[Step] = []
    for target in gates:
        runs, gate_cell, horizontal = cut_gate(runs, target, GATE_CELLS)
        if gate_cell is None:
            continue
        gate_steps.append(
            Step(
                "wall_gate",
                float(gate_cell[0]),
                float(gate_cell[1]),
                0.0 if horizontal else 90.0,
            )
        )
    links: list[Step] = []
    for run in runs:
        rotation = 0.0 if run_is_horizontal(run) else 90.0
        for cell in run:
            links.append(Step("wall_segment", float(cell[0]), float(cell[1]), rotation))

    links.sort(key=lambda step: (step.z, abs(step.x)))
    return links, gate_steps


def ring_cells(region: Region) -> set[Cell]:
    return region_cells(region, None)


def fabian_bulwark() -> Blueprint:
    """A double castrum: outer curtain, a keep inside it, towers on every corner."""
    plan = Blueprint("roman_bulwark", "Fabian Bulwark")
    outer = ring_cells(Region([rectangle_polygon(0, 0, 22, 18)]))
    keep = ring_cells(Region([rectangle_polygon(0, 0, 12, 10)]))
    """Outer 40x32, keep 24x20: the keep's links stand 10 m off the anchor, which
    is the least the builder will accept, and the ward between is one house deep."""
    outer_links, outer_gates = wall_steps(outer, [(0, -18), (0, 18)])
    keep_links, keep_gates = wall_steps(keep, [(0, -10)])
    plan.links = outer_links + outer_gates + keep_links + keep_gates
    for x, z in ((-12, 14), (12, 14)):
        plan.add("home", x, z)
    plan.add("marketplace", -17, 0)
    front = [s for s in outer_links if s.z <= -16]
    rest = [s for s in outer_links if s.z > -16]
    for x, z in ((-17, -14), (17, -14), (-8, -14)):
        plan.add("defense_tower", x, z)
    for step in front:
        plan.steps.append(step)
    plan.steps.extend(outer_gates[:1])
    for step in rest:
        plan.steps.append(step)
    plan.steps.extend(outer_gates[1:])
    for x, z in ((-17, 14), (17, 14)):
        plan.add("defense_tower", x, z)
    for step in keep_links:
        plan.steps.append(step)
    plan.steps.extend(keep_gates)
    for x, z in ((-17, 4), (17, 4), (-17, -4), (17, -4), (-17, 9), (17, 9)):
        plan.add("home", x, z)
    plan.add("ballista", -17, -9)
    plan.add("ballista", 17, -9)
    return plan


def consular_star() -> Blueprint:
    """A bastioned trace with a tower behind every point and engines inside."""
    plan = Blueprint("roman_assault_camp", "Consular Star Camp")
    stretch = 1.0 / math.cos(math.pi / 4)
    half_x, half_z = 15.0 * stretch, 13.0 * stretch
    cells = ring_cells(Region([bastioned_polygon(0, 0, half_x, half_z, 4, 9.0, 5.0)]))
    links, gates = wall_steps(cells, [(0, -13), (0, 13)])
    plan.links = links + gates
    for x, z in ((-9, 0), (9, 0)):
        plan.add("home", x, z)
    plan.add("marketplace", 0, 8)
    front = [s for s in links if s.z <= -9]
    rest = [s for s in links if s.z > -9]
    towers = sorted(bastion_apexes(0, 0, half_x, half_z, 4, -5.0), key=lambda t: t[1])
    for x, z in towers[:2]:
        plan.add("defense_tower", x, z)
    plan.steps.extend(front)
    plan.steps.extend(gates[:1])
    for x, z in towers[2:]:
        plan.add("defense_tower", x, z)
    plan.steps.extend(rest)
    plan.steps.extend(gates[1:])
    for x, z in ((-8, -7), (8, -7), (0, 11), (0, -11)):
        plan.add("home", x, z)
    plan.add("catapult", -8, 5)
    plan.add("catapult", 8, 5)
    return plan


def vanguard_chevron() -> Blueprint:
    """An open V thrown forward of three barracks; nothing closes the rear."""
    plan = Blueprint("roman_vanguard_camp", "Vanguard Chevron")
    left = path_cells([(-24, -2), (0, -20)], None)
    right = path_cells([(0, -20), (24, -2)], None)
    cells = left | right
    links, _gates = wall_steps(cells, [])
    plan.links = links
    plan.add("barracks", -12, 6)
    plan.add("barracks", 12, 6)
    for x, z in ((-6, 12), (6, 12)):
        plan.add("home", x, z)
    plan.steps.extend(links)
    plan.add("defense_tower", 0, -13)
    plan.add("defense_tower", -22, 3)
    plan.add("defense_tower", 22, 3)
    plan.add("barracks", 0, 16)
    plan.add("marketplace", 0, 22)
    for x, z in ((-16, 12), (16, 12), (-10, 18), (10, 18)):
        plan.add("home", x, z)
    return plan


def punic_ring_town() -> Blueprint:
    """A round town: market at the heart, homes against the wall, towers all round."""
    plan = Blueprint("punic_trade_town", "Punic Ring Town")
    cells = ring_cells(Region([ellipse_polygon(0, 0, 19, 17)]))
    links, gates = wall_steps(cells, [(0, -17), (0, 17)])
    plan.links = links + gates
    plan.add("marketplace", 0, -11)
    for x, z in ((-10, -7), (10, -7)):
        plan.add("home", x, z)
    ring_towers = sorted(
        (
            (
                math.cos(math.pi * 0.5 + 2.0 * math.pi * index / 6) * 14.5,
                -math.sin(math.pi * 0.5 + 2.0 * math.pi * index / 6) * 12.5,
            )
            for index in range(6)
        ),
        key=lambda t: t[1],
    )
    for x, z in ring_towers[:3]:
        plan.add("defense_tower", x, z)
    plan.steps.extend([s for s in links if s.z <= -12])
    plan.steps.extend(gates[:1])
    for x, z in ring_towers[3:]:
        plan.add("defense_tower", x, z)
    plan.steps.extend([s for s in links if s.z > -12])
    plan.steps.extend(gates[1:])
    for x, z in (
        (-13, 1),
        (13, 1),
        (-9, 8),
        (9, 8),
        (-13, -3),
        (13, -3),
        (-4, 9),
        (4, 9),
    ):
        plan.add("home", x, z)
    plan.add("ballista", -7, -7)
    return plan


def barcid_raider_camp() -> Blueprint:
    """No wall at all: three barracks on a triangle, towers on its points, horse lines."""
    plan = Blueprint("punic_raider_camp", "Barcid Triangle Camp")
    plan.add("barracks", -14, 6)
    plan.add("barracks", 14, 6)
    plan.add("marketplace", 0, 12)
    for x, z in ((-7, 14), (7, 14)):
        plan.add("home", x, z)
    plan.add("barracks", 0, -16)
    plan.add("defense_tower", 0, -24)
    plan.add("defense_tower", -22, 12)
    plan.add("defense_tower", 22, 12)
    for x, z in ((-14, 16), (14, 16), (-20, 2), (20, 2), (-8, -8), (8, -8)):
        plan.add("home", x, z)
    return plan


def hannibalic_hexagon() -> Blueprint:
    """Six sides, six towers, engines inside; the gate faces the enemy."""
    plan = Blueprint("punic_grand_camp", "Hannibalic Hexagon")
    hexagon = [
        (math.cos(math.pi * index / 3) * 21.0, math.sin(math.pi * index / 3) * 19.0)
        for index in range(6)
    ]
    """Vertices on the x axis, so a flat side faces the enemy and carries the gate."""
    cells = ring_cells(Region([hexagon]))
    links, gates = wall_steps(cells, [(0, -19), (0, 19)])
    plan.links = links + gates
    plan.add("home", 10, 8)
    plan.add("home", -11, 1)
    corners = [
        (math.cos(math.pi * index / 3) * 16.5, math.sin(math.pi * index / 3) * 14.5)
        for index in range(6)
    ]
    corners.sort(key=lambda corner: corner[1])
    for x, z in corners[:3]:
        plan.add("defense_tower", x, z)
    plan.steps.extend([s for s in links if s.z <= -10])
    plan.steps.extend(gates[:1])
    for x, z in corners[3:]:
        plan.add("defense_tower", x, z)
    plan.add("marketplace", 11, 1)
    plan.steps.extend([s for s in links if s.z > -10])
    plan.steps.extend(gates[1:])
    plan.add("barracks", -28, 4)
    plan.add("barracks", 28, 4)
    for x, z in ((-10, -5), (-4, 13), (4, 13)):
        plan.add("home", x, z)
    plan.add("catapult", -7, 7)
    plan.add("catapult", 7, 7)
    return plan


BLUEPRINTS = (
    fabian_bulwark,
    consular_star,
    vanguard_chevron,
    punic_ring_town,
    barcid_raider_camp,
    hannibalic_hexagon,
)


def check(plan: Blueprint) -> list[str]:
    problems: list[str] = []
    if plan.count("wall_segment") > WALL_CAP:
        problems.append(f"{plan.count('wall_segment')} wall links, cap {WALL_CAP}")
    if plan.count("defense_tower") > TOWER_CAP:
        problems.append(f"{plan.count('defense_tower')} towers, cap {TOWER_CAP}")
    if plan.count("home") > HOME_CAP:
        problems.append(f"{plan.count('home')} homes, cap {HOME_CAP}")
    links = [
        (s.x, s.z) for s in plan.steps if s.building in ("wall_segment", "wall_gate")
    ]
    for step in plan.steps:
        if step.building in ("wall_segment", "wall_gate"):
            continue
        reach = STANDOFF.get(step.building, 4.0)
        for lx, lz in links:
            if math.hypot(step.x - lx, step.z - lz) < reach:
                problems.append(
                    f"{step.building} at {step.x},{step.z} stands {math.hypot(step.x - lx, step.z - lz):.1f} "
                    f"from the wall link at {lx},{lz}; the lattice snap needs {reach}"
                )
                break
    seen: dict[tuple[float, float], str] = {}
    for step in plan.steps:
        key = (round(step.x, 1), round(step.z, 1))
        if key in seen:
            problems.append(f"{step.building} and {seen[key]} share {key}")
        seen[key] = step.building
    return problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args(argv)
    repo = Path(__file__).resolve().parents[1]
    path = repo / "assets" / "data" / "ai" / "town_plans.json"
    existing = json.loads(path.read_text())

    plans: dict[str, dict] = {}
    failures = 0
    for build in BLUEPRINTS:
        plan = build()
        problems = check(plan)
        summary = ", ".join(
            f"{plan.count(name)} {name}"
            for name in (
                "barracks",
                "home",
                "defense_tower",
                "wall_segment",
                "wall_gate",
                "marketplace",
                "catapult",
                "ballista",
            )
            if plan.count(name)
        )
        print(
            f"{plan.id:24s} {plan.display_name:22s} {len(plan.steps):3d} steps: {summary}"
        )
        for problem in problems:
            print(f"  ERROR: {problem}", file=sys.stderr)
            failures += 1
        plans[plan.id] = {
            "display_name": plan.display_name,
            "steps": [step.to_json() for step in plan.steps],
        }
    if failures:
        return 1
    if args.write:
        existing["plans"] = plans
        path.write_text(json.dumps(existing, indent=2) + "\n")
        print(f"wrote {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
