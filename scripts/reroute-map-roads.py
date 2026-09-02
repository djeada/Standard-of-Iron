#!/usr/bin/env python3
"""Reroute only the roads a new terrain feature broke.

``generate-map-roads.py`` regenerates a whole map, and on the campaign maps
that is not a neutral operation: any road it cannot route is dropped from the
network, and a bridge approach it cannot complete aborts the write. Raising a
hill under a settlement breaks two or three roads, not eighty, so this tool
re-routes exactly the roads that now cross blocking terrain and writes nothing
else back.

Each broken road is routed again with the same machinery - the map's routing
field, its authored junctions as anchors, and ``legal_terminal``, which moves an
endpoint that is now inside a hill out to that hill's nearest entrance. Run the
settlement generator first so the hill has entrances to move to, then this,
then the settlement generator again so its gates follow the rerouted roads.
"""

from __future__ import annotations

import argparse
import json
import sys
import types
from pathlib import Path


def _load_road_generator():
    path = Path(__file__).resolve().parent / "generate-map-roads.py"
    module = types.ModuleType("soi_road_generator")
    module.__dict__["__name__"] = "soi_road_generator"
    module.__dict__["__file__"] = str(path)
    sys.modules["soi_road_generator"] = module
    exec(compile(path.read_text(), str(path), "exec"), module.__dict__)
    return module


def broken_roads(roads_module, field, roads: list[dict]) -> list[int]:
    """Indices of roads that cross blocking terrain or end on it."""
    broken: list[int] = []
    for index, road in enumerate(roads):
        points = [
            field.coords.to_grid(point) for point in roads_module.road_points(road)
        ]
        crosses = any(
            not field.line_passable(start, end)
            for start, end in zip(points, points[1:], strict=False)
        )
        ends_blocked = any(
            not field.passable(int(round(point[0])), int(round(point[1])))
            for point in (points[0], points[-1])
        )
        if crosses or ends_blocked:
            broken.append(index)
    return broken


def hill_at(definition: dict, centre: tuple[float, float]) -> dict:
    for feature in definition.get("terrain") or []:
        if str(feature.get("type", "")).lower() != "hill":
            continue
        if (
            abs(float(feature.get("x", 0.0)) - centre[0]) < 0.5
            and abs(float(feature.get("z", 0.0)) - centre[1]) < 0.5
        ):
            return feature
    raise SystemExit(f"no hill at {centre[0]},{centre[1]}")


def without_hills(definition: dict, hills: list[dict]) -> dict:
    stripped = json.loads(json.dumps(definition))
    stripped["terrain"] = [
        feature
        for feature in stripped.get("terrain") or []
        if not any(feature is not hill and feature == hill for hill in hills)
        and feature not in hills
    ]
    return stripped


def entrance_outside(
    roads_module, field, hill: dict, point: tuple[float, float]
) -> tuple[float, float] | None:
    """The hill entrance nearest ``point``, stepped out to passable ground.

    An endpoint under the hill was an approach to whatever stood there before;
    the approach to a hill fort is its ramp, so that is where the road now ends.
    """
    centre = field.coords.to_grid((float(hill["x"]), float(hill["z"])))
    best: tuple[float, tuple[float, float]] | None = None
    for entrance in hill.get("entrances") or []:
        authored = (float(entrance.get("x", 0.0)), float(entrance.get("z", 0.0)))
        mouth = field.coords.to_grid(authored)
        outward = roads_module.normalized(roads_module.sub(mouth, centre))
        if outward == (0.0, 0.0):
            continue
        for step in range(0, 40):
            candidate = roads_module.add(mouth, roads_module.mul(outward, float(step)))
            cell = int(round(candidate[0])), int(round(candidate[1]))
            if field.passable(*cell):
                gap = roads_module.distance(point, mouth)
                if best is None or gap < best[0]:
                    best = (gap, candidate)
                break
    return None if best is None else best[1]


def settlement_gates(
    definition: dict, settlement_id: str
) -> tuple[tuple[float, float], list[tuple[float, float]]]:
    """A settlement's centre and its gate positions, in authored coordinates.

    A road that reaches a walled settlement has to reach it through a gate, and
    the generator puts gates where the roads cross the ring - but a curved ring
    only has straight spans in a few places, so the gate can sit a wall's length
    from the crossing. Bending the road's last leg to the gate closes that gap.
    """
    centre = None
    for entry in definition.get("settlements") or []:
        if entry.get("id") == settlement_id:
            centre = (float(entry["x"]), float(entry["z"]))
    if centre is None:
        raise SystemExit(f"no settlement {settlement_id}")
    gates = [
        (float(entry["x"]), float(entry["z"]))
        for entry in definition.get("structures") or []
        if entry.get("settlement") == settlement_id and entry.get("type") == "wall_gate"
    ]
    if not gates:
        raise SystemExit(f"{settlement_id} has no gates; generate it first")
    return centre, gates


def process_map(
    path: Path,
    *,
    hills: list[tuple[float, float]],
    only_roads: list[int],
    to_gates: list[str],
    clearance: float,
    max_segment_length: float,
    write: bool,
) -> bool:
    roads_module = _load_road_generator()
    source = path.read_text(encoding="utf-8")
    definition = json.loads(source)
    roads = list(definition.get("roads") or [])
    if not roads:
        print(f"{path}: no roads")
        return True
    max_half_width = max(float(road.get("width", 3.0)) * 0.5 for road in roads)
    field = roads_module.RoutingField(definition, clearance + max_half_width)
    new_hills = [hill_at(definition, centre) for centre in hills]

    gate_moves: dict[int, dict[int, tuple[float, float]]] = {}
    for settlement_id in to_gates:
        centre, gates = settlement_gates(definition, settlement_id)
        centre = field.coords.to_grid(centre)
        gates = [field.coords.to_grid(gate) for gate in gates]
        for index, road in enumerate(roads):
            points = [
                field.coords.to_grid(point) for point in roads_module.road_points(road)
            ]
            for end in (0, -1):
                if roads_module.distance(points[end], centre) > 30.0:
                    continue
                arriving_from = points[1] if end == 0 else points[-2]
                gate = min(gates, key=lambda g: roads_module.distance(arriving_from, g))
                gate_moves.setdefault(index, {})[end] = gate
                print(
                    f"  road {index + 1:02d}: ends at the {settlement_id} gate "
                    f"{field.coords.from_grid(gate)}"
                )

    broken = set(broken_roads(roads_module, field, roads))
    if only_roads:
        broken = {index - 1 for index in only_roads}
    elif to_gates:
        broken = set(gate_moves)
    elif new_hills:
        baseline = roads_module.RoutingField(
            without_hills(definition, new_hills), clearance + max_half_width
        )
        already = set(broken_roads(roads_module, baseline, roads))
        if already:
            print(
                f"  leaving {len(already)} road(s) that were already crossing "
                f"terrain before the hill: {sorted(index + 1 for index in already)}"
            )
        broken -= already
    if not broken:
        print(f"{path}: no road was broken by the hill, nothing to do")
        return True

    anchors = roads_module.network_anchors(field, roads)
    failures = 0
    dropped: list[int] = []
    rerouted = 0
    for index in sorted(broken):
        road = roads[index]
        road_anchors = list(anchors[index])
        for end, gate in gate_moves.get(index, {}).items():
            road_anchors[end] = gate
        for end in (0, -1):
            cell = int(round(road_anchors[end][0])), int(round(road_anchors[end][1]))
            if field.passable(*cell):
                continue
            moved = None
            for hill in new_hills:
                moved = entrance_outside(roads_module, field, hill, road_anchors[end])
                if moved is not None:
                    break
            if moved is not None:
                road_anchors[end] = moved
        road_anchors = [
            anchor
            for anchor in road_anchors
            if field.passable(int(round(anchor[0])), int(round(anchor[1])))
        ]
        road_anchors = roads_module.deduplicate(road_anchors, epsilon=1.0)
        if len(road_anchors) < 2:
            dropped.append(index)
            print(f"  road {index + 1:02d}: dropped, it lay entirely under the hill")
            continue
        try:
            result = roads_module.generate_road(
                field, road, max_segment_length, road_anchors
            )
        except roads_module.RoadGenerationError as error:
            print(f"  road {index + 1:02d}: FAILED ({error})", file=sys.stderr)
            failures += 1
            continue
        if result.new_length > max(result.old_length * 2.25, result.old_length + 80.0):
            dropped.append(index)
            print(
                f"  road {index + 1:02d}: dropped, the way round the hill is "
                f"{result.new_length:.0f} for a road that was {result.old_length:.0f}"
            )
            continue
        check = roads_module.validate_roads(field, [result.road])
        if check.obstacle_violations or check.invalid_bridge_crossings:
            print(
                f"  road {index + 1:02d}: FAILED (unsafe generated geometry)",
                file=sys.stderr,
            )
            failures += 1
            continue
        roads[index] = result.road
        rerouted += 1
        print(
            f"  road {index + 1:02d}: {result.old_length:6.1f} -> "
            f"{result.new_length:6.1f}, {len(result.road['waypoints']):2d} points"
        )

    roads = [road for index, road in enumerate(roads) if index not in dropped]
    label = "REROUTE" if write else "DRY-RUN"
    print(
        f"{path} [{label}]: {len(broken)} broken by the hill, {rerouted} rerouted, "
        f"{len(dropped)} dropped, {failures} failed"
    )
    if failures:
        return False
    if write:
        path.write_text(
            roads_module.replace_top_level_array(source, "roads", roads),
            encoding="utf-8",
        )
        print("  wrote roads")
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("maps", nargs="+", type=Path)
    parser.add_argument(
        "--hill",
        action="append",
        default=[],
        metavar="X,Z",
        help="centre of a hill that was just added; only roads it broke are touched",
    )
    parser.add_argument(
        "--road",
        action="append",
        type=int,
        default=[],
        metavar="N",
        help="reroute only road N (1-based, as the road generator numbers them)",
    )
    parser.add_argument(
        "--to-gates",
        action="append",
        default=[],
        metavar="SETTLEMENT",
        help="bend roads that end inside this settlement so they end at its nearest gate",
    )
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--clearance", type=float, default=1.25)
    parser.add_argument("--max-segment-length", type=float, default=18.0)
    args = parser.parse_args(argv)
    failures = 0
    for path in args.maps:
        try:
            hills = [
                (float(text.split(",")[0]), float(text.split(",")[1]))
                for text in args.hill
            ]
            if not process_map(
                path,
                hills=hills,
                only_roads=args.road,
                to_gates=args.to_gates,
                clearance=args.clearance,
                max_segment_length=args.max_segment_length,
                write=args.write,
            ):
                failures += 1
        except (OSError, json.JSONDecodeError) as error:
            print(f"{path}: ERROR: {error}", file=sys.stderr)
            failures += 1
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
