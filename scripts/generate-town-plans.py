#!/usr/bin/env python3
"""Generate the AI commanders' castle blueprints, assets/data/ai/town_plans.json.

Each commander raises its own settlement at runtime from a town plan: an
ordered list of buildings at offsets in the settlement's frame (x right, -z
toward the enemy, origin on the primary barracks, which nothing may stand within
9 m of). The builder walks the steps in order, skipping any that already stand
or cannot be afforded, so the *order* is the shape of a half-built town.

A circuit is laid the way a wall is actually built: as one growing run. The
walk starts at the front gate and adds links outward along both arms, round
the front corners, down the flanks, and closes at the rear, where the rear
gate is cut when the arms reach it. Whatever the economy has paid for at any
moment is therefore a solid wall with two ends, never a ring of posts with a
gap between every pair - a half-built castle is half a castle, not a fence
with holes. The front towers and the front gate go first, so the enemy-facing
side is the part that exists earliest.

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
    WALL_SEGMENT_SPACING,
    Cell,
    Point,
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
    "home": 5.0,
    "marketplace": 4.5,
    "barracks": 6.5,
    "catapult": 4.0,
    "ballista": 4.0,
}
"""How far a building keeps from a wall link in the plan frame.

A crew raises a link standing within a metre of its centre, and reaches that
spot along the wall's own line only while the run is still open; once the
neighbours stand it must come at the slot from inside or outside the ring. A
house (4.4 m across, plus the nav grid's padding) 4 m off the line left no
walkable cell on the inside, and Scipio's crews abandoned the same front slots
thirty times in a row while standing six metres from them."""
ANCHOR_CLEARANCE = 9.0
FRONT_SHARE = 1.0 / 3.0
"""How much of a circuit, walked outward from the front gate, is its silhouette:
the front curtain, its corners and the start of the flanks."""
WALL_CAP = 128
TOWER_CAP = 12
HOME_CAP = 12


@dataclass
class Step:
    building: str
    x: float
    z: float
    rotation: float | None = None
    dist: int = 0
    """How many links along the circuit this stands from the front gate."""

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
    silhouette_steps: int = 0
    """How many opening steps carry the town's identity: the front towers, the
    front gate and the front third of the circuit walked outward from it. The
    builder lets these go up as soon as the town can feed itself, and holds the
    rest of the ring behind them until the roofs are on."""

    def close_silhouette(self) -> None:
        self.silhouette_steps = len(self.steps)

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


def circuit_walk(
    cells: set[Cell], gates: list[tuple[float, float]], root: Point | None = None
) -> list[Step]:
    """Cut the gates, then order the circuit as a walk outward from the front gate.

    Every link and gate comes back in build order with ``dist`` set to its
    distance along the trace from the walk's root: the first gate if there is
    one, else the cell nearest ``root``, else the front-most cell. Links at the
    same distance on the two arms alternate, left arm first, so the wall grows
    symmetrically and stays one connected run at every prefix. A gate other
    than the root is emitted the moment the walk reaches it, so the arms close
    through it rather than leaving a hole for it.
    """
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
        for cell in sorted(run):
            links.append(Step("wall_segment", float(cell[0]), float(cell[1]), rotation))

    if gate_steps:
        origin: Cell = (int(gate_steps[0].x), int(gate_steps[0].z))
    elif root is not None:
        origin = min(
            cells, key=lambda cell: math.hypot(cell[0] - root[0], cell[1] - root[1])
        )
    else:
        origin = min(cells, key=lambda cell: (cell[1], abs(cell[0])))

    distance: dict[Cell, int] = {origin: 0}
    frontier = [origin]
    while frontier:
        following: list[Cell] = []
        for x, z in frontier:
            for dx in (-WALL_SEGMENT_SPACING, 0, WALL_SEGMENT_SPACING):
                for dz in (-WALL_SEGMENT_SPACING, 0, WALL_SEGMENT_SPACING):
                    neighbour = (x + dx, z + dz)
                    if neighbour == (x, z) or neighbour not in cells:
                        continue
                    if neighbour in distance:
                        continue
                    distance[neighbour] = distance[(x, z)] + 1
                    following.append(neighbour)
        frontier = following

    far = len(cells) + 1
    for step in links + gate_steps:
        step.dist = distance.get((int(step.x), int(step.z)), far)
    walk = links + gate_steps

    walk.sort(key=lambda step: (step.dist, step.building != "wall_gate", step.x))
    return walk


def split_front(
    walk: list[Step], share: float = FRONT_SHARE
) -> tuple[list[Step], list[Step]]:
    """The front of a circuit walk - the gate and the nearest ``share`` of the
    links either side of it - and everything behind that."""
    links = [step for step in walk if step.building == "wall_segment"]
    wanted = max(1, math.ceil(len(links) * share))
    cutoff = links[wanted - 1].dist if links else 0
    front = [step for step in walk if step.dist <= cutoff]
    rest = [step for step in walk if step.dist > cutoff]
    return front, rest


def ring_cells(region: Region) -> set[Cell]:
    return region_cells(region, None)


def inside_corner(region: Region, x: float, z: float, clearance: float) -> Point:
    """The spot on the line from a polygon corner to the anchor that stands
    ``clearance`` inside the trace: where a corner tower goes, whatever the
    corner's angle. The wall's links sit within a lattice step of the polygon
    edge, so callers add that step to the standoff they want from the links."""
    px, pz = x, z
    while region.edge_distance(px, pz) < clearance and math.hypot(px, pz) > 1.0:
        px -= x * 0.02
        pz -= z * 0.02
    return px, pz


def fabian_bulwark() -> Blueprint:
    """A double castrum: outer curtain, a keep inside it, towers on every corner."""
    plan = Blueprint("roman_bulwark", "Fabian Bulwark")
    outer = ring_cells(Region([rectangle_polygon(0, 0, 22, 18)]))
    keep = ring_cells(Region([rectangle_polygon(0, 0, 12, 10)]))
    """Outer 40x32, keep 24x20: the keep's links stand 10 m off the anchor, which
    is the least the builder will accept, and the ward between is one house deep."""
    outer_walk = circuit_walk(outer, [(0, -18), (0, 18)])
    keep_walk = circuit_walk(keep, [(0, -10)])
    plan.links = outer_walk + keep_walk
    for x, z in ((-12, 14), (12, 14)):
        plan.add("home", x, z)
    plan.add("marketplace", -17, 0)
    front, rest = split_front(outer_walk)
    for x, z in ((-17, -14), (17, -14), (-8, -14)):
        plan.add("defense_tower", x, z)
    plan.steps.extend(front)
    plan.close_silhouette()
    plan.steps.extend(rest)
    for x, z in ((-17, 14), (17, 14)):
        plan.add("defense_tower", x, z)
    plan.steps.extend(keep_walk)
    for x, z in ((-17, 4), (17, 4), (-17, -4), (17, -4), (-17, 9), (17, 9)):
        plan.add("home", x, z)
    plan.add("ballista", -17, -9)
    plan.add("ballista", 17, -9)
    return plan


def consular_star() -> Blueprint:
    """A bastioned trace with a tower behind every point and engines inside."""
    plan = Blueprint("roman_assault_camp", "Consular Star Camp")
    stretch = 1.0 / math.cos(math.pi / 4)
    half_x, half_z = 17.0 * stretch, 15.0 * stretch
    """17x15: at 15x13 the ward could not hold two homes and a market five
    metres off the curtain, and a house any nearer leaves no walkable cell
    between it and the wall for the crew raising the last links."""
    cells = ring_cells(Region([bastioned_polygon(0, 0, half_x, half_z, 4, 9.0, 5.0)]))
    walk = circuit_walk(cells, [(0, -15), (0, 15)])
    plan.links = walk
    for x, z in ((-9, 0), (9, 0)):
        plan.add("home", x, z)
    plan.add("marketplace", 0, 8)
    front, rest = split_front(walk)
    towers = sorted(bastion_apexes(0, 0, half_x, half_z, 4, -5.0), key=lambda t: t[1])
    for x, z in towers[:2]:
        plan.add("defense_tower", x, z)
    plan.steps.extend(front)
    plan.close_silhouette()
    plan.steps.extend(rest)
    for x, z in towers[2:]:
        plan.add("defense_tower", x, z)

    for x, z in ((-7, -6), (7, -6), (0, 11)):
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
    walk = circuit_walk(cells, [], root=(0, -20))
    plan.links = walk
    plan.add("barracks", -12, 6)
    plan.add("barracks", 12, 6)
    for x, z in ((-6, 12), (6, 12)):
        plan.add("home", x, z)
    front, rest = split_front(walk)
    plan.add("defense_tower", 0, -13)
    plan.steps.extend(front)
    plan.close_silhouette()
    plan.steps.extend(rest)
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
    walk = circuit_walk(cells, [(0, -17), (0, 17)])
    plan.links = walk
    plan.add("marketplace", 0, -11)
    for x, z in ((-10, -7), (10, -7)):
        plan.add("home", x, z)
    ring_towers = sorted(
        (
            (
                math.cos(math.pi * (0.5 + 1.0 / 6.0) + 2.0 * math.pi * index / 6)
                * 14.5,
                -math.sin(math.pi * (0.5 + 1.0 / 6.0) + 2.0 * math.pi * index / 6)
                * 12.5,
            )
            for index in range(6)
        ),
        key=lambda t: t[1],
    )
    """Towers thirty degrees off the gate axis: one on the axis stood between
    the front gate and the market with room for neither."""
    front, rest = split_front(walk)
    for x, z in ring_towers[:3]:
        plan.add("defense_tower", x, z)
    plan.steps.extend(front)
    plan.close_silhouette()
    plan.steps.extend(rest)
    for x, z in ring_towers[3:]:
        plan.add("defense_tower", x, z)
    for x, z in (
        (-13, 1),
        (13, 1),
        (-9, 8),
        (9, 8),
        (-13, -3),
        (13, -3),
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
    plan.close_silhouette()
    for x, z in ((-14, 16), (14, 16), (-20, 2), (20, 2), (-8, -8), (8, -8)):
        plan.add("home", x, z)
    return plan


def hannibalic_hexagon() -> Blueprint:
    """Six sides, six towers, engines inside; the gate faces the enemy."""
    plan = Blueprint("punic_grand_camp", "Hannibalic Hexagon")
    hexagon = [
        (math.cos(math.pi * index / 3) * 23.0, math.sin(math.pi * index / 3) * 21.0)
        for index in range(6)
    ]
    """Vertices on the x axis, so a flat side faces the enemy and carries the gate.
    23x21: the flat sides of a 21x19 hexagon stand only 16 m out, and a ward
    that shallow could not hold the six towers, the homes and the engines at
    the standoff a crew needs to reach the last links of the ring."""
    region = Region([hexagon])
    cells = ring_cells(region)
    walk = circuit_walk(cells, [(0, -21), (0, 21)])
    plan.links = walk
    plan.add("home", 10, 8)
    plan.add("home", -11, 1)
    corners = [
        inside_corner(region, x, z, STANDOFF["defense_tower"] + 1.5) for x, z in hexagon
    ]
    corners.sort(key=lambda corner: corner[1])
    front, rest = split_front(walk)
    for x, z in corners[:3]:
        plan.add("defense_tower", x, z)
    plan.steps.extend(front)
    plan.close_silhouette()
    plan.steps.extend(rest)
    for x, z in corners[3:]:
        plan.add("defense_tower", x, z)
    plan.add("marketplace", 11, 1)
    plan.add("barracks", -30, 4)
    plan.add("barracks", 30, 4)
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
            f"{plan.id:24s} {plan.display_name:22s} {len(plan.steps):3d} steps "
            f"({plan.silhouette_steps} silhouette): {summary}"
        )
        for problem in problems:
            print(f"  ERROR: {problem}", file=sys.stderr)
            failures += 1
        plans[plan.id] = {
            "display_name": plan.display_name,
            "silhouette_steps": plan.silhouette_steps,
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
