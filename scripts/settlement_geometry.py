#!/usr/bin/env python3
"""Wall circuits for settlements that are not rectangles.

The runtime spawns a wall run as an axis-aligned chain on a 2-unit lattice
(``WallNetworkService::build_axis_aligned_chain``), so a diagonal run is not a
diagonal wall - it is a straight wall in the wrong place. Every shape here is
therefore built as lattice cells and only then grouped back into runs, which is
also what lets a circle or a bastion carry a gate: a gate needs a straight span
of wall to sit in, and the cells know where those are.

A circuit is defined by the ground it encloses rather than by its outline. The
cells that get a wall are the lattice cells inside the region that touch a cell
outside it, so a union of overlapping shapes - two lobes and the neck between
them - produces one closed outline without anyone computing that outline. It is
sealed by construction: any 4-connected path from inside to outside has to leave
the region at some step, and the cell it leaves from is a wall.

Diagonal steps are then closed with an L cell. The pathfinder already refuses to
cut a corner between two blocked cells, so a bare diagonal step is not a hole,
but the wall renderer picks its piece from a 4-neighbour mask and a diagonal
chain reads as a dotted line of isolated posts rather than a wall.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Callable, Sequence

WALL_SEGMENT_SPACING = 2

Cell = tuple[int, int]
Point = tuple[float, float]

NEIGHBOURS = (
    (WALL_SEGMENT_SPACING, 0),
    (-WALL_SEGMENT_SPACING, 0),
    (0, WALL_SEGMENT_SPACING),
    (0, -WALL_SEGMENT_SPACING),
)

DIAGONALS = (
    (WALL_SEGMENT_SPACING, WALL_SEGMENT_SPACING),
    (WALL_SEGMENT_SPACING, -WALL_SEGMENT_SPACING),
    (-WALL_SEGMENT_SPACING, WALL_SEGMENT_SPACING),
    (-WALL_SEGMENT_SPACING, -WALL_SEGMENT_SPACING),
)


def snap_wall_coordinate(value: float) -> int:
    """The lattice cell a wall authored at ``value`` actually spawns on."""
    return int(math.floor(value / WALL_SEGMENT_SPACING + 0.5)) * WALL_SEGMENT_SPACING


def lattice_range(low: int, high: int) -> list[int]:
    return list(range(low, high + 1, WALL_SEGMENT_SPACING))


def polygon_contains(polygon: Sequence[Point], x: float, z: float) -> bool:
    """Ray cast, with the boundary counted as inside on the low side."""
    inside = False
    count = len(polygon)
    for index in range(count):
        x0, z0 = polygon[index]
        x1, z1 = polygon[(index + 1) % count]
        if (z0 > z) == (z1 > z):
            continue
        crossing = x0 + (z - z0) * (x1 - x0) / (z1 - z0)
        if crossing > x:
            inside = not inside
    return inside


def segment_distance(
    px: float, pz: float, ax: float, az: float, bx: float, bz: float
) -> float:
    delta_x = bx - ax
    delta_z = bz - az
    length = delta_x * delta_x + delta_z * delta_z
    if length <= 1e-12:
        return math.hypot(px - ax, pz - az)
    along = ((px - ax) * delta_x + (pz - az) * delta_z) / length
    along = max(0.0, min(1.0, along))
    return math.hypot(px - (ax + along * delta_x), pz - (az + along * delta_z))


def polygon_edge_distance(polygon: Sequence[Point], x: float, z: float) -> float:
    count = len(polygon)
    return min(
        segment_distance(
            x,
            z,
            polygon[index][0],
            polygon[index][1],
            polygon[(index + 1) % count][0],
            polygon[(index + 1) % count][1],
        )
        for index in range(count)
    )


def polyline_distance(path: Sequence[Point], x: float, z: float) -> float:
    return min(
        segment_distance(x, z, a[0], a[1], b[0], b[1])
        for a, b in zip(path, path[1:], strict=False)
    )


def ellipse_polygon(
    cx: float, cz: float, half_x: float, half_z: float, sides: int = 64
) -> list[Point]:
    return [
        (
            cx + math.cos(2.0 * math.pi * index / sides) * half_x,
            cz + math.sin(2.0 * math.pi * index / sides) * half_z,
        )
        for index in range(sides)
    ]


EDGE_INCLUSION = 0.01
"""A lattice cell sitting exactly on a polygon edge belongs to the polygon.

Ray casting is half-open, so a rectangle authored on lattice coordinates would
otherwise lose its outermost cells and come out one link smaller on every side.
"""


def rectangle_polygon(
    cx: float, cz: float, half_x: float, half_z: float
) -> list[Point]:
    half_x += EDGE_INCLUSION
    half_z += EDGE_INCLUSION
    return [
        (cx - half_x, cz - half_z),
        (cx + half_x, cz - half_z),
        (cx + half_x, cz + half_z),
        (cx - half_x, cz + half_z),
    ]


def chamfered_rectangle_polygon(
    cx: float, cz: float, half_x: float, half_z: float, chamfer: float
) -> list[Point]:
    """A rectangle with its corners cut back, for a stepped stockade."""
    half_x += EDGE_INCLUSION
    half_z += EDGE_INCLUSION
    cut = max(0.0, min(chamfer, min(half_x, half_z) * 0.8))
    return [
        (cx - half_x + cut, cz - half_z),
        (cx + half_x - cut, cz - half_z),
        (cx + half_x, cz - half_z + cut),
        (cx + half_x, cz + half_z - cut),
        (cx + half_x - cut, cz + half_z),
        (cx - half_x + cut, cz + half_z),
        (cx - half_x, cz + half_z - cut),
        (cx - half_x, cz - half_z + cut),
    ]


def bastioned_polygon(
    cx: float,
    cz: float,
    half_x: float,
    half_z: float,
    sides: int,
    bastion: float,
    flank: float,
) -> list[Point]:
    """A curtain polygon with a triangular bastion thrown out at every corner.

    The curtain between two bastions stays straight, which is what a gate needs,
    and the bastion apex is where a tower goes: it sees along both curtains it
    stands between, which is the whole point of the trace.
    """
    base: list[Point] = []
    phase = math.pi / sides
    for index in range(sides):
        angle = phase + 2.0 * math.pi * index / sides
        base.append((cx + math.cos(angle) * half_x, cz + math.sin(angle) * half_z))

    polygon: list[Point] = []
    for index, corner in enumerate(base):
        previous = base[index - 1]
        following = base[(index + 1) % sides]
        back = _unit(previous[0] - corner[0], previous[1] - corner[1])
        forward = _unit(following[0] - corner[0], following[1] - corner[1])
        outward = _unit(-(back[0] + forward[0]), -(back[1] + forward[1]))
        polygon.append((corner[0] + back[0] * flank, corner[1] + back[1] * flank))
        polygon.append(
            (corner[0] + outward[0] * bastion, corner[1] + outward[1] * bastion)
        )
        polygon.append((corner[0] + forward[0] * flank, corner[1] + forward[1] * flank))
    return polygon


def bastion_apexes(
    cx: float, cz: float, half_x: float, half_z: float, sides: int, reach: float
) -> list[Point]:
    """Where the towers of a bastioned trace stand, one per corner."""
    phase = math.pi / sides
    apexes: list[Point] = []
    for index in range(sides):
        angle = phase + 2.0 * math.pi * index / sides
        direction = _unit(math.cos(angle) * half_z, math.sin(angle) * half_x)
        apexes.append(
            (
                cx + math.cos(angle) * half_x + direction[0] * reach,
                cz + math.sin(angle) * half_z + direction[1] * reach,
            )
        )
    return apexes


def _unit(x: float, z: float) -> Point:
    length = math.hypot(x, z)
    if length <= 1e-9:
        return (0.0, 0.0)
    return (x / length, z / length)


@dataclass
class Region:
    """The ground one circuit encloses, as a union of polygons."""

    polygons: list[list[Point]] = field(default_factory=list)

    def contains(self, x: float, z: float) -> bool:
        return any(polygon_contains(polygon, x, z) for polygon in self.polygons)

    def edge_distance(self, x: float, z: float) -> float:
        return min(polygon_edge_distance(polygon, x, z) for polygon in self.polygons)

    def bounds(self) -> tuple[float, float, float, float]:
        xs = [point[0] for polygon in self.polygons for point in polygon]
        zs = [point[1] for polygon in self.polygons for point in polygon]
        return (min(xs), min(zs), max(xs), max(zs))


@dataclass
class Circuit:
    """One closed wall ring, or one open curtain, with the gates it carries."""

    region: Region | None = None
    path: list[Point] | None = None
    gate_targets: list[Point] = field(default_factory=list)
    towers: list[Point] = field(default_factory=list)


def region_cells(
    region: Region, walkable: Callable[[float, float], bool] | None
) -> set[Cell]:
    """The lattice cells that carry this region's wall.

    A cell no unit can walk anyway is left out: terrain closing a side of a ring
    is the ring's wall there, and a wall in a river channel is entities spent on
    a place nothing can attack through.
    """
    left, top, right, bottom = region.bounds()
    inside: set[Cell] = set()
    for z in lattice_range(
        snap_wall_coordinate(top) - WALL_SEGMENT_SPACING,
        snap_wall_coordinate(bottom) + WALL_SEGMENT_SPACING,
    ):
        for x in lattice_range(
            snap_wall_coordinate(left) - WALL_SEGMENT_SPACING,
            snap_wall_coordinate(right) + WALL_SEGMENT_SPACING,
        ):
            if region.contains(float(x), float(z)):
                inside.add((x, z))

    boundary = {
        cell
        for cell in inside
        if any((cell[0] + dx, cell[1] + dz) not in inside for dx, dz in NEIGHBOURS)
    }
    boundary = close_diagonal_steps(boundary, lambda x, z: (x, z) in inside)
    if walkable is None:
        return boundary
    return {cell for cell in boundary if walkable(float(cell[0]), float(cell[1]))}


def path_cells(
    path: Sequence[Point], walkable: Callable[[float, float], bool] | None
) -> set[Cell]:
    """A curtain wall: the lattice cells under an open polyline."""
    cells: set[Cell] = set()
    for start, end in zip(path, path[1:], strict=False):
        x = snap_wall_coordinate(start[0])
        z = snap_wall_coordinate(start[1])
        target_x = snap_wall_coordinate(end[0])
        target_z = snap_wall_coordinate(end[1])
        cells.add((x, z))
        while (x, z) != (target_x, target_z):
            if abs(target_x - x) >= abs(target_z - z):
                x += WALL_SEGMENT_SPACING if target_x > x else -WALL_SEGMENT_SPACING
            else:
                z += WALL_SEGMENT_SPACING if target_z > z else -WALL_SEGMENT_SPACING
            cells.add((x, z))
    if walkable is None:
        return cells
    return {cell for cell in cells if walkable(float(cell[0]), float(cell[1]))}


def close_diagonal_steps(
    cells: set[Cell], inside: Callable[[int, int], bool]
) -> set[Cell]:
    """Turn every diagonal step in a wall into an L, so the run stays a wall."""
    result = set(cells)
    for _ in range(8):
        added: set[Cell] = set()
        for x, z in result:
            for dx, dz in DIAGONALS:
                if (x + dx, z + dz) not in result:
                    continue
                elbow_a = (x + dx, z)
                elbow_b = (x, z + dz)
                if elbow_a in result or elbow_b in result:
                    continue
                if elbow_a in added or elbow_b in added:
                    continue
                added.add(elbow_a if inside(*elbow_a) else elbow_b)
        if not added:
            break
        result |= added
    return result


def partition_runs(cells: set[Cell]) -> list[list[Cell]]:
    """Group cells into straight runs, each cell owned by exactly one run.

    A cell emitted twice is two wall entities standing in the same place, so the
    partition is what keeps a circuit's cost equal to its length.
    """
    remaining = set(cells)
    runs: list[list[Cell]] = []
    for cell in sorted(remaining, key=lambda item: (item[1], item[0])):
        if cell not in remaining:
            continue
        best: list[Cell] = [cell]
        for dx, dz in ((WALL_SEGMENT_SPACING, 0), (0, WALL_SEGMENT_SPACING)):
            run = [cell]
            probe = cell
            while True:
                probe = (probe[0] + dx, probe[1] + dz)
                if probe not in remaining:
                    break
                run.append(probe)
            if len(run) > len(best):
                best = run
        for member in best:
            remaining.discard(member)
        runs.append(best)
    return runs


def run_is_horizontal(run: Sequence[Cell]) -> bool:
    return len(run) < 2 or run[0][1] == run[-1][1]


def cut_gate(
    runs: list[list[Cell]], target: Point, gate_cells: int
) -> tuple[list[list[Cell]], Cell | None, bool]:
    """Open a gateway in the run nearest ``target``, and say where it landed.

    A gate needs a wall cell left standing either side of it, or the opening is
    just the end of a wall and a unit walks round it - so a run has to be longer
    than the gate to carry one.
    """
    best_index = -1
    best_distance = float("inf")
    for index, run in enumerate(runs):
        if len(run) < gate_cells + 2:
            continue
        for cell in run:
            distance = math.hypot(cell[0] - target[0], cell[1] - target[1])
            if distance < best_distance:
                best_distance = distance
                best_index = index
    if best_index < 0:
        return runs, None, True

    run = runs[best_index]
    horizontal = run_is_horizontal(run)
    order = sorted(run, key=lambda cell: cell[0] if horizontal else cell[1])
    low = gate_cells // 2 + 1
    high = len(order) - gate_cells // 2 - 2
    nearest = min(
        range(len(order)),
        key=lambda index: math.hypot(
            order[index][0] - target[0], order[index][1] - target[1]
        ),
    )
    centre = max(low, min(high, nearest))
    span = range(centre - gate_cells // 2, centre - gate_cells // 2 + gate_cells)
    opened = {order[index] for index in span}

    kept = [cell for cell in order if cell not in opened]
    rebuilt = [run for index, run in enumerate(runs) if index != best_index]
    rebuilt.extend(partition_runs(set(kept)))
    return rebuilt, order[centre], horizontal
