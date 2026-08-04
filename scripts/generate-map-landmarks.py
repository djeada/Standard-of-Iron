#!/usr/bin/env python3
"""Stamp landmark compositions into Standard of Iron map JSON.

A campaign map's interest does not all come from its settlements. The ground
between them needs places worth walking to: a sanctuary on a ridge with a
statue-lined way up to it, a burnt-out hamlet at a crossroads, a picket camped
beside a monument at the edge of a wood. This tool turns a map's ``landmarks``
array - authorial intent, a handful of lines each - into the temples, props,
troops and groves that make those places exist.

Kinds
  sanctuary  a temple outside any settlement, a statue-lined approach, ruins
  shrine     a wayside altar: statues and ruins, no temple
  hamlet     a dead village of abandoned homes, ruins and dead trees
  watch      a picket beside a monument: tents, cart, rack, one statue

A map may also carry a ``groves`` array of standalone woods. A forest does not
block movement - it marks its ground as forest, which thickens the tree scatter
and gives the ground cover - so a grove is placed for what it screens: the
approach to a wall with no gate on it, the flank of a road a column has to march
down, the timber a gather objective is measured against. Groves are validated for
open ground and settlement clearance like everything else here, and carry the
same ``landmark`` tag.

Everything this tool writes carries a ``landmark`` key naming the landmark it
belongs to, and a run removes its own previous output before writing the new
one. Entries without that key are left alone, so a map's hand-placed scatter and
its generated landmarks can share the same arrays.

Guards are authored with the ``guard`` behaviour. Without it the AI folds them
into its strategic pool and marches them away, and the landmark they were put
there to hold is unheld within a minute of the mission starting.

Placement is checked against the same passability field the road and settlement
generators use. A statue or an abandoned home blocks the cell it stands on, so
those two are additionally kept clear of roads - a monument in the roadway makes
formations file around it - and of each other. Anything with nowhere legal to
stand is dropped rather than pushed somewhere it does not belong.

The command is dry-run by default. Pass --write to update the map.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import types
from dataclasses import dataclass, field
from pathlib import Path
from typing import Sequence

LANDMARK_KEY = "landmark"

BLOCKING_PROPS = ("statue", "abandoned_home")

ROAD_KEEP_OUT = 3.0

SETTLEMENT_KEEP_OUT = 12.0

PROP_SPACING = 3.5

LANDMARK_SPACING = 60.0

GROVE_KEEP_OUT = 22.0


@dataclass
class Piece:
    """One authored item in a landmark composition, in local frame units.

    ``along`` runs away from the approach, ``across`` runs to its left, so a
    composition reads the same whichever way its landmark faces.
    """

    kind: str
    along: float
    across: float
    scale: float = 1.0


@dataclass
class KindSpec:
    temple: bool
    pieces: tuple[Piece, ...]
    guard_along: float = -12.0


def statue_way(count: int, start: float, step: float) -> tuple[Piece, ...]:
    """Statues in facing pairs along an approach, the way up to a sanctuary."""
    pieces: list[Piece] = []
    for index in range(count):
        along = start - step * (index // 2)
        across = 5.5 if index % 2 == 0 else -5.5
        pieces.append(Piece("statue", along, across))
    return tuple(pieces)


KIND_SPECS: dict[str, KindSpec] = {
    "sanctuary": KindSpec(
        temple=True,
        pieces=(
            *statue_way(4, -9.0, 9.0),
            Piece("ruins", 9.0, 7.0, 1.1),
            Piece("ruins", 11.0, -6.0, 0.9),
            Piece("dead_tree", 14.0, 1.0),
            Piece("firecamp", -6.0, 0.0),
        ),
    ),
    "shrine": KindSpec(
        temple=False,
        pieces=(
            Piece("statue", 0.0, 0.0, 1.1),
            Piece("statue", -8.0, 5.0),
            Piece("statue", -8.0, -5.0),
            Piece("ruins", 7.0, 5.0, 1.0),
            Piece("ruins", 8.0, -6.0, 0.85),
            Piece("ruins", 13.0, 1.0, 1.15),
            Piece("firecamp", -14.0, 0.0),
        ),
        guard_along=-20.0,
    ),
    "hamlet": KindSpec(
        temple=False,
        pieces=(
            Piece("abandoned_home", 0.0, 6.0),
            Piece("abandoned_home", -7.0, -5.0),
            Piece("abandoned_home", 8.0, -7.0),
            Piece("abandoned_home", 12.0, 5.0, 0.9),
            Piece("ruins", -12.0, 3.0, 1.1),
            Piece("ruins", 4.0, -13.0, 0.9),
            Piece("dead_tree", -4.0, 12.0),
            Piece("dead_tree", 16.0, -2.0, 0.9),
        ),
        guard_along=-18.0,
    ),
    "watch": KindSpec(
        temple=False,
        pieces=(
            Piece("statue", 4.0, 0.0),
            Piece("tent", -6.0, 5.0),
            Piece("tent", -7.0, -4.0),
            Piece("tent", -12.0, 2.0, 0.95),
            Piece("supply_cart", -3.0, -8.0),
            Piece("weapon_rack", -2.0, 8.0),
            Piece("firecamp", -8.0, 0.0),
            Piece("ruins", 12.0, -6.0, 0.9),
        ),
        guard_along=-14.0,
    ),
}


FACING_FRAMES = {
    "north": ((0.0, 1.0), (1.0, 0.0)),
    "south": ((0.0, -1.0), (-1.0, 0.0)),
    "east": ((-1.0, 0.0), (0.0, 1.0)),
    "west": ((1.0, 0.0), (0.0, -1.0)),
}


class LandmarkError(RuntimeError):
    """Raised when a landmark cannot be placed legally."""


@dataclass
class Landmark:
    id: str
    kind: str
    x: float
    z: float
    facing: str = "south"
    nation: str | None = None
    player_id: int = -1
    on_hill: bool = False
    guards: tuple[tuple[str, int], ...] = ()
    forest: dict | None = None
    scale: float = 1.0

    props: list[dict] = field(default_factory=list)
    structures: list[dict] = field(default_factory=list)
    spawns: list[dict] = field(default_factory=list)
    groves: list[dict] = field(default_factory=list)

    @staticmethod
    def from_json(entry: dict) -> "Landmark":
        missing = [key for key in ("id", "kind", "x", "z") if key not in entry]
        if missing:
            raise LandmarkError(f"landmark is missing {', '.join(missing)}")
        kind = str(entry["kind"]).lower()
        if kind not in KIND_SPECS:
            raise LandmarkError(f"unknown landmark kind: {kind}")
        facing = str(entry.get("facing", "south")).lower()
        if facing not in FACING_FRAMES:
            raise LandmarkError(f"unknown facing: {facing}")
        guards = tuple(
            (str(item["type"]), int(item.get("count", 1)))
            for item in entry.get("guards") or []
        )
        return Landmark(
            id=str(entry["id"]),
            kind=kind,
            x=float(entry["x"]),
            z=float(entry["z"]),
            facing=facing,
            nation=entry.get("nation"),
            player_id=int(entry.get("player_id", -1)),
            on_hill=bool(entry.get("on_hill", False)),
            guards=guards,
            forest=entry.get("forest"),
            scale=float(entry.get("scale", 1.0)),
        )


def load_settlement_generator():
    """Reuse the settlement tool's terrain mask and hill geometry."""
    path = Path(__file__).resolve().parent / "generate-map-settlements.py"
    module = types.ModuleType("soi_settlement_generator")
    module.__dict__["__name__"] = "soi_settlement_generator"
    module.__dict__["__file__"] = str(path)
    sys.modules["soi_settlement_generator"] = module
    exec(compile(path.read_text(), str(path), "exec"), module.__dict__)
    return module


SETTLEMENTS = load_settlement_generator()


def road_segments(
    definition: dict,
) -> list[tuple[tuple[float, float], tuple[float, float], float]]:
    segments = []
    for road in definition.get("roads") or []:
        raw = road.get("waypoints") or [road.get("start"), road.get("end")]
        points = [(float(p[0]), float(p[1])) for p in raw if p]
        half = float(road.get("width", 3.0)) * 0.5
        for start, end in zip(points, points[1:], strict=False):
            segments.append((start, end, half))
    return segments


def distance_to_segment(
    point: tuple[float, float],
    start: tuple[float, float],
    end: tuple[float, float],
) -> float:
    delta_x = end[0] - start[0]
    delta_z = end[1] - start[1]
    length_sq = delta_x * delta_x + delta_z * delta_z
    if length_sq <= 0.0:
        return math.hypot(point[0] - start[0], point[1] - start[1])
    travel = max(
        0.0,
        min(
            1.0,
            ((point[0] - start[0]) * delta_x + (point[1] - start[1]) * delta_z)
            / length_sq,
        ),
    )
    near_x = start[0] + delta_x * travel
    near_z = start[1] + delta_z * travel
    return math.hypot(point[0] - near_x, point[1] - near_z)


def road_clearance(segments: Sequence, x: float, z: float) -> float:
    return min(
        (
            distance_to_segment((x, z), start, end) - half
            for start, end, half in segments
        ),
        default=float("inf"),
    )


def settlement_clearance(definition: dict, x: float, z: float) -> float:
    """Distance from a point to the nearest settlement's outer ring."""
    best = float("inf")
    for raw in definition.get("settlements") or []:
        spec = SETTLEMENTS.TIER_SPECS[str(raw["tier"]).lower()]
        scale = float(raw.get("scale", 1.0))
        offset_x = abs(x - float(raw["x"])) - spec.outer_half_x * scale
        offset_z = abs(z - float(raw["z"])) - spec.outer_half_z * scale
        best = min(best, max(offset_x, offset_z))
    return best


class Site:
    """Where a landmark's pieces are allowed to stand."""

    def __init__(self, definition: dict):
        self.definition = definition
        self.base_walkable = SETTLEMENTS.TerrainMask(definition, 0.0, influence=1.0)
        self.base_buildable = SETTLEMENTS.TerrainMask(definition, 3.0)
        self.walkable = self.base_walkable
        self.buildable = self.base_buildable
        self.roads = road_segments(definition)
        self.taken: list[tuple[float, float]] = []
        for entry in definition.get("structures") or []:
            if entry.get("type") == "wall_segment":
                continue
            if entry.get(LANDMARK_KEY) is not None:

                continue
            self.taken.append((float(entry["x"]), float(entry["z"])))
        for prop in definition.get("world_props") or []:
            if prop.get(LANDMARK_KEY) is None:
                self.taken.append((float(prop["x"]), float(prop["z"])))

    def use_hill(self, hill: dict | None) -> None:
        """Treat one hill as standable, the way a settlement on its crown is.

        The shared passability field calls a whole hill blocked, crown included,
        because roads have to route around it. A landmark placed on a crown is
        standing on the part of it that is walkable, so that hill has to come out
        of the field while its pieces are laid.
        """
        if hill is None:
            self.walkable = self.base_walkable
            self.buildable = self.base_buildable
            return
        self.walkable = SETTLEMENTS.TerrainMask(
            self.definition, 0.0, exclude_hill=hill, influence=1.0
        )
        self.buildable = SETTLEMENTS.TerrainMask(
            self.definition, 3.0, exclude_hill=hill
        )

    def free(self, x: float, z: float, blocking: bool, spacing: float) -> bool:
        if not self.walkable.walkable(x, z):
            return False
        if settlement_clearance(self.definition, x, z) < SETTLEMENT_KEEP_OUT:
            return False
        if blocking and road_clearance(self.roads, x, z) < ROAD_KEEP_OUT:
            return False
        return all(
            math.hypot(x - other_x, z - other_z) >= spacing
            for other_x, other_z in self.taken
        )

    def claim(self, x: float, z: float) -> None:
        self.taken.append((x, z))

    def settle(
        self,
        x: float,
        z: float,
        blocking: bool,
        spacing: float = PROP_SPACING,
        reach: int = 4,
    ) -> tuple[float, float] | None:
        """Nudge a piece onto legal ground, or give up on it."""
        if self.free(x, z, blocking, spacing):
            return x, z
        for ring in range(1, reach + 1):
            for index in range(8 * ring):
                angle = (2.0 * math.pi * index) / (8 * ring)
                candidate_x = x + math.cos(angle) * 2.0 * ring
                candidate_z = z + math.sin(angle) * 2.0 * ring
                if self.free(candidate_x, candidate_z, blocking, spacing):
                    return candidate_x, candidate_z
        return None


def hill_crown_at(
    definition: dict, x: float, z: float
) -> tuple[dict, float, float] | None:
    """The hill a landmark should sit on, with the half-extents of its crown."""
    hill = SETTLEMENTS.find_hill_at(definition.get("terrain") or [], x, z, radius=70.0)
    if hill is None:
        return None
    half_x, half_z = SETTLEMENTS.hill_extents(hill)
    crown_x, crown_z = SETTLEMENTS.usable_crown_extent(
        half_x * 2.0, half_z * 2.0, float(hill.get("height", 2.0)), 0.8
    )
    return hill, crown_x, crown_z


def place(landmark: Landmark, site: Site) -> None:
    """Lay out one landmark's temple, props, groves and guards."""
    spec = KIND_SPECS[landmark.kind]
    forward, left = FACING_FRAMES[landmark.facing]
    crown: tuple[float, float] | None = None

    site.use_hill(None)
    if landmark.on_hill:
        found = hill_crown_at(site.definition, landmark.x, landmark.z)
        if found is None:
            raise LandmarkError(
                f"{landmark.id} is marked on_hill but no hill is near it"
            )
        hill, crown_x, crown_z = found
        landmark.x = float(hill["x"])
        landmark.z = float(hill["z"])
        crown = (crown_x, crown_z)
        site.use_hill(hill)

    def to_world(piece_along: float, piece_across: float) -> tuple[float, float]:
        along = piece_along * landmark.scale
        across = piece_across * landmark.scale
        return (
            landmark.x + forward[0] * along + left[0] * across,
            landmark.z + forward[1] * along + left[1] * across,
        )

    def on_crown(x: float, z: float) -> bool:
        if crown is None:
            return True
        reach_x = abs(x - landmark.x) / max(crown[0], 0.001)
        reach_z = abs(z - landmark.z) / max(crown[1], 0.001)
        return math.hypot(reach_x, reach_z) <= 1.0

    if spec.temple:
        seat = site.settle(landmark.x, landmark.z, True, 8.0, reach=6)
        if seat is None or not site.buildable.clear_for(
            *seat, SETTLEMENTS.BUILDING_SIZES["temple"]
        ):
            raise LandmarkError(
                f"{landmark.id}: no clear ground for a temple near "
                f"{landmark.x:.0f},{landmark.z:.0f}"
            )

        landmark.x, landmark.z = seat
        x, z = seat
        site.claim(x, z)
        entry = {
            "type": "temple",
            "x": round(x, 2),
            "z": round(z, 2),
            "player_id": landmark.player_id,
            LANDMARK_KEY: landmark.id,
        }
        if landmark.nation:
            entry["nation"] = landmark.nation
        landmark.structures.append(entry)

    for piece in spec.pieces:
        x, z = to_world(piece.along, piece.across)
        blocking = piece.kind in BLOCKING_PROPS
        spacing = 5.0 if blocking else PROP_SPACING
        settled = site.settle(x, z, blocking, spacing)
        if settled is None or not on_crown(*settled):
            continue
        site.claim(*settled)
        prop = {
            "type": piece.kind,
            "x": round(settled[0], 2),
            "z": round(settled[1], 2),
            "scale": round(piece.scale, 2),
            "rotation": round(bearing_degrees(landmark.facing, piece.across), 2),
            LANDMARK_KEY: landmark.id,
        }
        if piece.kind == "firecamp":
            prop["radius"] = 3.2
            prop["intensity"] = 1.2
            prop["persistent"] = True
        landmark.props.append(prop)

    guard_row = 0
    for unit_type, count in landmark.guards:
        for index in range(count):
            along = spec.guard_along - guard_row * 4.0
            across = (index - (count - 1) * 0.5) * 4.0
            x, z = to_world(along, across)
            settled = site.settle(x, z, False, 2.5)
            if settled is None:
                continue
            site.claim(*settled)
            landmark.spawns.append(
                {
                    "type": unit_type,
                    "x": round(settled[0], 2),
                    "z": round(settled[1], 2),
                    "player_id": landmark.player_id,
                    "behavior": "guard",
                    LANDMARK_KEY: landmark.id,
                }
            )
        guard_row += 1

    if landmark.forest:
        bearing = str(landmark.forest.get("bearing", "east")).lower()
        if bearing not in FACING_FRAMES:
            raise LandmarkError(f"{landmark.id}: unknown forest bearing {bearing}")
        direction = FACING_FRAMES[bearing][0]
        radius = float(landmark.forest.get("radius", 26.0))
        distance = float(landmark.forest.get("distance", radius + 16.0))
        landmark.groves.append(
            {
                "type": "forest",
                "x": round(landmark.x - direction[0] * distance, 2),
                "z": round(landmark.z - direction[1] * distance, 2),
                "radius": round(radius, 2),
                LANDMARK_KEY: landmark.id,
            }
        )


def bearing_degrees(facing: str, across: float) -> float:
    """Yaw for a prop, so paired statues turn to face each other across a way."""
    base = {"north": 0.0, "east": 90.0, "south": 180.0, "west": 270.0}[facing]
    if across > 0.5:
        return (base + 90.0) % 360.0
    if across < -0.5:
        return (base + 270.0) % 360.0
    return base


def strip_generated(entries: Sequence[dict] | None) -> list[dict]:
    """Drop the previous run's output, leaving everything else in place.

    The ``landmark`` key is the ownership record, and it survives the map editor:
    the editor keeps unknown keys on every element it round trips, and unknown
    root fields with them. Anything without the key was put there by hand and is
    not this tool's to remove.
    """
    return [entry for entry in entries or [] if entry.get(LANDMARK_KEY) is None]


def plan_groves(definition: dict, site: Site) -> list[dict]:
    """Turn the map's standalone ``groves`` intent into forest features."""
    planned: list[dict] = []
    for entry in definition.get("groves") or []:
        missing = [key for key in ("id", "x", "z", "radius") if key not in entry]
        if missing:
            raise LandmarkError(f"grove is missing {', '.join(missing)}")
        x = float(entry["x"])
        z = float(entry["z"])
        if not site.base_walkable.walkable(x, z):
            raise LandmarkError(
                f"{entry['id']}: a grove centre must be open ground, "
                f"{x:.0f},{z:.0f} is not"
            )
        clearance = settlement_clearance(definition, x, z)
        if clearance < GROVE_KEEP_OUT:
            raise LandmarkError(
                f"{entry['id']}: {clearance:.0f} from a settlement wall; a wood "
                f"inside {GROVE_KEEP_OUT:.0f} grows through the streets"
            )
        planned.append(
            {
                "type": "forest",
                "x": round(x, 2),
                "z": round(z, 2),
                "radius": round(float(entry["radius"]), 2),
                LANDMARK_KEY: str(entry["id"]),
            }
        )
    return planned


def order_structures(kept: list[dict], fresh: list[dict]) -> list[dict]:
    """Point buildings first, wall runs last.

    The map editor canonicalises a structures array that way, so appending a
    landmark temple after the walls would make an untouched editor round trip
    reorder the file (see MapEditorMapDataTest.RealMapRoundTrips...).
    """
    points = [entry for entry in kept if entry.get("type") != "wall_segment"]
    walls = [entry for entry in kept if entry.get("type") == "wall_segment"]
    return points + fresh + walls


def serialise_like(path: Path, definition: dict) -> str:
    original = path.read_text()
    sorted_keys = original.startswith('{\n    "')
    indent = 4 if sorted_keys else 2
    return json.dumps(definition, indent=indent, sort_keys=sorted_keys) + "\n"


def process_map(path: Path, *, write: bool) -> bool:
    definition = json.loads(path.read_text())
    raw_landmarks = definition.get("landmarks")
    if not raw_landmarks:
        print(f"{path}: SKIP: no landmarks block")
        return True

    landmarks = [Landmark.from_json(entry) for entry in raw_landmarks]
    site = Site(definition)
    standalone = plan_groves(definition, site)

    structures: list[dict] = []
    props: list[dict] = []
    spawns: list[dict] = []
    groves: list[dict] = list(standalone)
    for landmark in landmarks:
        place(landmark, site)
        structures.extend(landmark.structures)
        props.extend(landmark.props)
        spawns.extend(landmark.spawns)
        groves.extend(landmark.groves)

    temples = sum(1 for entry in structures if entry["type"] == "temple")
    statues = sum(1 for prop in props if prop["type"] == "statue")
    homes = sum(1 for prop in props if prop["type"] == "abandoned_home")
    print(
        f"{path} [{'GENERATE' if write else 'VALIDATE'}] PASS: "
        f"landmarks={len(landmarks)}, temples={temples}, statues={statues}, "
        f"abandoned_homes={homes}, props={len(props)}, guards={len(spawns)}, "
        f"groves={len(groves)}"
    )
    for landmark in landmarks:
        thin = len(landmark.props) < len(KIND_SPECS[landmark.kind].pieces) - 2
        if thin:
            print(
                f"  WARNING: {landmark.id} placed only {len(landmark.props)} of "
                f"{len(KIND_SPECS[landmark.kind].pieces)} props"
            )

    for index, first in enumerate(landmarks):
        for second in landmarks[index + 1 :]:
            gap = math.hypot(first.x - second.x, first.z - second.z)
            if gap < LANDMARK_SPACING:
                print(
                    f"  WARNING: {first.id} and {second.id} are {gap:.0f} apart; "
                    f"two landmarks inside {LANDMARK_SPACING:.0f} read as one place"
                )

    if not write:
        return True

    definition["structures"] = order_structures(
        strip_generated(definition.get("structures")), structures
    )
    definition["world_props"] = strip_generated(definition.get("world_props")) + props
    definition["spawns"] = strip_generated(definition.get("spawns")) + spawns
    definition["terrain"] = strip_generated(definition.get("terrain")) + groves
    path.write_text(serialise_like(path, definition))
    print(f"  wrote {len(structures) + len(props) + len(spawns) + len(groves)} entries")
    return True


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("maps", nargs="*", type=Path, help="map JSON files")
    parser.add_argument(
        "--campaign",
        action="store_true",
        help="process all eight campaign maps in campaign order",
    )
    parser.add_argument(
        "--write", action="store_true", help="update the map after a clean pass"
    )
    args = parser.parse_args(argv)
    if not args.campaign and not args.maps:
        parser.error("provide at least one map or use --campaign")
    return args


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    repo_root = Path(__file__).resolve().parents[1]
    paths = list(args.maps)
    if args.campaign:
        paths.extend(repo_root / relative for relative in SETTLEMENTS.CAMPAIGN_MAPS)

    failures = 0
    for path in list(dict.fromkeys(item.resolve() for item in paths)):
        try:
            if not process_map(path, write=args.write):
                failures += 1
        except (OSError, json.JSONDecodeError, LandmarkError) as error:
            print(f"{path}: ERROR: {error}", file=sys.stderr)
            failures += 1
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
