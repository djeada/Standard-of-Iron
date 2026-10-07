"""Checks and metrics for a candidate map.

The pipeline's own validation gates run first - they are the same commands an
author runs before review - and the structural checks here cover what they do
not: the road graph as a graph, settlements and commanders, bounds, open
ground. A ``fail`` rejects the candidate; a ``warn`` is reported and kept.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .context import COMMANDERS
from .geometry import (
    FOREST,
    HILL,
    MOUNTAIN,
    SETTLEMENT,
    WATER,
    build_field,
    dist,
    feature_points,
    polyline_crossings,
    polyline_distance,
    settlement_radius,
)
from .materialize import run_gate

COMMANDER_TYPES = {kind for kinds in COMMANDERS.values() for kind in kinds}


@dataclass
class Report:
    checks: list[dict[str, Any]] = field(default_factory=list)
    metrics: dict[str, Any] = field(default_factory=dict)

    def add(
        self,
        check_id: str,
        status: str,
        message: str,
        objects: list[dict] | None = None,
    ) -> None:
        entry: dict[str, Any] = {"id": check_id, "status": status, "message": message}
        if objects:
            entry["objects"] = objects[:20]
        self.checks.append(entry)

    @property
    def ok(self) -> bool:
        return all(check["status"] != "fail" for check in self.checks)

    def failures(self) -> list[str]:
        return [check["id"] for check in self.checks if check["status"] == "fail"]


def _obj(kind: str, index: int, p) -> dict[str, Any]:
    return {
        "kind": kind,
        "index": index,
        "x": round(float(p[0]), 2),
        "z": round(float(p[1]), 2),
    }


class RoadGraph:
    """Roads as a graph: endpoints, junctions and crossings are nodes."""

    TOLERANCE = 2.5

    def __init__(self, roads: list[dict[str, Any]]) -> None:
        self.polylines = [feature_points(r) for r in roads]
        self.indices = [i for i, p in enumerate(self.polylines) if len(p) >= 2]
        self.nodes: list[tuple[float, float]] = []
        self.edges: list[tuple[int, int, int]] = []
        stops: dict[int, list[tuple[float, int]]] = {i: [] for i in self.indices}

        def node_for(p) -> int:
            for index, q in enumerate(self.nodes):
                if dist(p, q) <= self.TOLERANCE:
                    return index
            self.nodes.append((float(p[0]), float(p[1])))
            return len(self.nodes) - 1

        def location(points, p) -> float:
            best, travelled, best_at = math.inf, 0.0, 0.0
            for a, b in zip(points, points[1:], strict=False):
                seg = dist(a, b)
                for t in (i / 8 for i in range(9)):
                    q = (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
                    d = dist(p, q)
                    if d < best:
                        best, best_at = d, travelled + seg * t
                travelled += seg
            return best_at

        for i in self.indices:
            points = self.polylines[i]
            for end in (points[0], points[-1]):
                n = node_for(end)
                stops[i].append((location(points, end), n))
                for j in self.indices:
                    if (
                        j != i
                        and polyline_distance(end, self.polylines[j]) <= self.TOLERANCE
                    ):
                        stops[j].append((location(self.polylines[j], end), n))
        for a_pos, i in enumerate(self.indices):
            for j in self.indices[a_pos + 1 :]:
                for hit in polyline_crossings(self.polylines[i], self.polylines[j]):
                    n = node_for(hit)
                    stops[i].append((location(self.polylines[i], hit), n))
                    stops[j].append((location(self.polylines[j], hit), n))
        for i in self.indices:
            ordered = sorted(set(stops[i]))
            nodes = [n for _, n in ordered]
            compact = [n for k, n in enumerate(nodes) if k == 0 or n != nodes[k - 1]]
            for a, b in zip(compact, compact[1:], strict=False):
                self.edges.append((a, b, i))

    def components(self) -> list[set[int]]:
        parent = list(range(len(self.nodes)))

        def find(x: int) -> int:
            while parent[x] != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x

        for a, b, _ in self.edges:
            parent[find(a)] = find(b)
        groups: dict[int, set[int]] = {}
        for n in range(len(self.nodes)):
            groups.setdefault(find(n), set()).add(n)
        return sorted(groups.values(), key=len, reverse=True)

    def degree(self) -> dict[int, int]:
        degree = {n: 0 for n in range(len(self.nodes))}
        for a, b, _ in self.edges:
            degree[a] += 1
            degree[b] += 1
        return degree


def entrance_clusters(hill: dict[str, Any]) -> int:
    """Distinct approaches, counted as ``game/map/terrain_topology_audit.cpp`` does.

    The loader expands an entrance into every grid cell within its radius, so
    two ramps are one approach unless their centres are further apart than
    both radii plus the cluster distance.
    """
    entrances = [
        (float(e["x"]), float(e["z"]), float(e.get("radius", e.get("width", 0.0) / 2)))
        for e in hill.get("entrances") or []
        if "x" in e
    ]
    if not entrances:
        return 0
    radius = float(hill.get("radius", 0.0) or 0.0)
    width = float(hill.get("width", 0.0) or 0.0) or radius * 2.0
    depth = float(hill.get("depth", 0.0) or 0.0) or radius * 2.0
    reach = max(2.5, max(1.0, min(width, depth)) * 0.12)
    parent = list(range(len(entrances)))

    def find(x: int) -> int:
        while parent[x] != x:
            x = parent[x]
        return x

    for i, (xi, zi, ri) in enumerate(entrances):
        for j in range(i + 1, len(entrances)):
            xj, zj, rj = entrances[j]
            if dist((xi, zi), (xj, zj)) <= ri + rj + reach:
                parent[find(i)] = find(j)
    return len({find(i) for i in range(len(entrances))})


def _hills(definition: dict[str, Any], report: Report) -> None:
    generated, authored = [], []
    for index, hill in enumerate(definition.get("terrain") or []):
        if str(hill.get("type", "hill")).lower() != "hill":
            continue
        if entrance_clusters(hill) >= 2:
            continue
        target = generated if hill.get("generated") else authored
        target.append(_obj("terrain", index, (hill["x"], hill["z"])))
    if generated:
        report.add(
            "terrain.hill_approaches",
            "fail",
            f"{len(generated)} generated hill(s) with fewer than two approaches",
            generated,
        )
    elif authored:
        report.add(
            "terrain.hill_approaches",
            "warn",
            f"{len(authored)} authored hill(s) with fewer than two approaches",
            authored,
        )
    else:
        report.add("terrain.hill_approaches", "pass", "every hill has two approaches")


def _gates(definition: dict[str, Any], scratch: Path, report: Report) -> None:
    gates = []
    if definition.get("rivers"):
        gates.append(
            ("water", "water.gate", "water gate: rivers end on an edge, bank or shore")
        )
    if definition.get("roads"):
        gates.append(
            ("roads", "roads.gate", "road gate: no obstacle crossings, bridges valid")
        )
    if any(not s.get("authored") for s in definition.get("settlements") or []):
        gates.append(
            (
                "settlements",
                "settlements.gate",
                "settlement gate: rings closed, buildings fit",
            )
        )
    for gate, check_id, label in gates:
        subject = definition
        if gate == "water":
            subject = {
                **definition,
                "rivers": [r for r in definition["rivers"] if r.get("shape") != "ring"],
            }
            if not subject["rivers"]:
                report.add(check_id, "pass", label)
                continue
        run = run_gate(gate, subject, scratch)
        if run.ok:
            report.add(check_id, "pass", label)
        else:
            errors = [
                line for line in run.lines(40) if "ERROR" in line or "FAIL" in line
            ]
            report.add(
                check_id, "fail", f"{label} - " + ("; ".join(errors[-3:]) or "failed")
            )
    placement = run_gate("placement", definition, scratch)
    if placement.ok:
        report.add(
            "placement.clean", "pass", "no overlapping, drowned or road-blocking bodies"
        )
    else:
        lines = [
            line for line in placement.lines(30) if "defect" in line or ":" in line
        ]
        report.add(
            "placement.clean",
            "warn",
            "placement defects left after correction - " + "; ".join(lines[-2:]),
        )


def _bounds(definition: dict[str, Any], width: int, height: int, report: Report) -> int:
    bad: list[dict] = []
    limit_x, limit_z = width - 1 + 0.6, height - 1 + 0.6

    def inside(p) -> bool:
        return -0.6 <= float(p[0]) <= limit_x and -0.6 <= float(p[1]) <= limit_z

    for key in (
        "terrain",
        "lakes",
        "forests",
        "settlements",
        "landmarks",
        "structures",
        "spawns",
        "world_props",
    ):
        for index, entry in enumerate(definition.get(key) or []):
            kind = key.rstrip("s") if key != "terrain" else "terrain"
            points = [
                entry[k] for k in ("start", "end") if isinstance(entry.get(k), list)
            ]
            if "x" in entry:
                points.append((entry["x"], entry["z"]))
            outside = next((p for p in points if not inside(p)), None)
            if outside is not None:
                bad.append(_obj(kind, index, outside))
    for key, kind in (("rivers", "river"), ("roads", "road"), ("bridges", "bridge")):
        for index, entry in enumerate(definition.get(key) or []):
            for p in feature_points(entry):
                if not inside(p):
                    bad.append(_obj(kind, index, p))
                    break
    if bad:
        report.add("map.bounds", "fail", f"{len(bad)} object(s) outside the map", bad)
    else:
        report.add("map.bounds", "pass", "every object inside the map")
    return len(bad)


def _roads(definition, width, height, report: Report, metrics: dict) -> None:
    roads = definition.get("roads") or []
    if not roads:
        report.add("roads.present", "fail", "no roads")
        return
    graph = RoadGraph(roads)
    components = graph.components()
    metrics["road_components"] = len(components)
    if len(components) == 1:
        report.add("roads.connected", "pass", "connected road graph")
    else:
        stray = [
            _obj("road", 0, graph.nodes[next(iter(group))]) for group in components[1:]
        ]
        report.add(
            "roads.connected",
            "fail",
            f"road graph has {len(components)} components",
            stray,
        )

    def on_edge(p) -> bool:
        return p[0] <= 3.0 or p[1] <= 3.0 or p[0] >= width - 4.0 or p[1] >= height - 4.0

    def side(p) -> str:
        return min(
            (
                ("west", p[0]),
                ("north", p[1]),
                ("east", width - 1 - p[0]),
                ("south", height - 1 - p[1]),
            ),
            key=lambda item: item[1],
        )[0]

    degree = graph.degree()
    edge_nodes = [n for n, d in degree.items() if d == 1 and on_edge(graph.nodes[n])]
    metrics["edge_connections"] = len(edge_nodes)
    sides = {side(graph.nodes[n]) for n in edge_nodes}
    if len(edge_nodes) >= 2 and len(sides) >= 2:
        report.add(
            "roads.edge_connections",
            "pass",
            f"{len(edge_nodes)} edge connections on {len(sides)} sides",
        )
    else:
        report.add(
            "roads.edge_connections",
            "fail",
            f"only {len(edge_nodes)} road(s) reach the map edge",
            [_obj("road", 0, graph.nodes[n]) for n in edge_nodes],
        )

    loops = len(graph.edges) - len(graph.nodes) + len(components)
    metrics["route_loops"] = loops
    if loops >= 1:
        report.add("roads.loop", "pass", f"{loops} route loop(s)")
    else:

        standard = min(width, height) >= 500
        report.add(
            "roads.loop",
            "fail" if standard else "warn",
            "no route loop: every approach is a single road",
        )

    anchors = [
        (float(s["x"]), float(s["z"]), settlement_radius(s) + 30.0)
        for s in definition.get("settlements") or []
    ]
    anchors += [
        (float(e["x"]), float(e["z"]), 14.0)
        for t in definition.get("terrain") or []
        for e in t.get("entrances") or []
    ]
    anchors += [
        (float(lm["x"]), float(lm["z"]), 60.0)
        for lm in definition.get("landmarks") or []
    ]
    dead = [
        n
        for n, d in degree.items()
        if d == 1
        and not on_edge(graph.nodes[n])
        and not any(dist(graph.nodes[n], (x, z)) <= r for x, z, r in anchors)
    ]
    metrics["dead_ends"] = len(dead)
    if dead:
        report.add(
            "roads.dead_ends",
            "warn",
            f"{len(dead)} road(s) end in empty ground",
            [_obj("road", 0, graph.nodes[n]) for n in dead],
        )
    else:
        report.add(
            "roads.dead_ends",
            "pass",
            "every road ends at an edge, settlement, ramp or landmark",
        )

    bridges = definition.get("bridges") or []
    metrics["bridges"] = len(bridges)
    rivers = [r for r in definition.get("rivers") or [] if feature_points(r)]
    if rivers and not bridges:
        report.add(
            "roads.bridges", "fail", "rivers but no bridges: the water is never crossed"
        )
    else:
        report.add("roads.bridges", "pass", f"{len(bridges)} bridge(s)")

    main = components[0] if components else set()
    unreached = []
    for index, s in enumerate(definition.get("settlements") or []):
        centre = (float(s["x"]), float(s["z"]))
        reach = settlement_radius(s) + 25.0
        touching = [
            i
            for i, points in enumerate(graph.polylines)
            if len(points) >= 2 and polyline_distance(centre, points) <= reach
        ]
        connected = any(
            any(
                edge_road == i and (a in main or b in main)
                for a, b, edge_road in graph.edges
            )
            for i in touching
        )
        if not connected:
            unreached.append(_obj("settlement", index, centre))
    if unreached:
        report.add(
            "settlements.reachable",
            "fail",
            f"{len(unreached)} settlement(s) off the road network",
            unreached,
        )
    else:
        report.add(
            "settlements.reachable", "pass", "every settlement is on the road network"
        )


def _settlements_and_armies(definition, params, report: Report, metrics: dict) -> None:
    settlements = definition.get("settlements") or []
    metrics["settlements"] = len(settlements)
    owners = {s.get("player_id") for s in settlements}
    if 1 in owners and 2 in owners:
        report.add("settlements.sides", "pass", "both sides hold a settlement")
    else:
        report.add(
            "settlements.sides", "fail", "a side has no settlement to start from"
        )

    spacing = math.inf
    for i, a in enumerate(settlements):
        for b in settlements[i + 1 :]:
            spacing = min(spacing, dist((a["x"], a["z"]), (b["x"], b["z"])))
    metrics["min_settlement_spacing"] = (
        round(spacing, 1) if spacing < math.inf else None
    )

    spawns = definition.get("spawns") or []
    by_owner: dict[int, int] = {}
    for s in spawns:
        if s.get("type") in COMMANDER_TYPES:
            by_owner[s.get("player_id")] = by_owner.get(s.get("player_id"), 0) + 1
    armed = {s.get("player_id") for s in spawns} | owners
    wrong = [
        p
        for p in sorted(x for x in armed if isinstance(x, int) and x > 0)
        if by_owner.get(p, 0) != 1
    ]
    if wrong:
        report.add(
            "spawns.commanders",
            "fail",
            "players without exactly one commander: "
            + ", ".join(str(p) for p in wrong),
        )
    else:
        report.add("spawns.commanders", "pass", "one commander per side")

    commander = next(
        (
            s
            for s in spawns
            if s.get("player_id") == 1 and s.get("type") in COMMANDER_TYPES
        ),
        None,
    )
    enemy = [s for s in settlements if s.get("player_id") == 2]
    if commander and enemy:
        reach = min(
            dist((commander["x"], commander["z"]), (s["x"], s["z"])) for s in enemy
        )
        metrics["spawn_to_objective"] = round(reach, 1)

    landmarks = definition.get("landmarks") or []
    gap = math.inf
    for i, a in enumerate(landmarks):
        for b in landmarks[i + 1 :]:
            gap = min(gap, dist((a["x"], a["z"]), (b["x"], b["z"])))
    metrics["min_landmark_spacing"] = round(gap, 1) if gap < math.inf else None


def _ground(definition, width, height, params, report: Report, metrics: dict) -> None:
    field = build_field(definition, width, height)
    metrics["terrain_blockage"] = round(field.fraction(HILL | MOUNTAIN), 3)
    metrics["forest_blockage"] = round(field.fraction(FOREST), 3)
    metrics["water_coverage"] = round(field.fraction(WATER), 3)
    clear = total = 0
    for z in range(20, height - 20, 12):
        for x in range(20, width - 20, 12):
            total += 1
            if not field.blocked(
                x, z, 12.0, WATER | HILL | MOUNTAIN | FOREST | SETTLEMENT
            ):
                clear += 1
    ratio = clear / total if total else 0.0
    metrics["formation_area_ratio"] = round(ratio, 3)
    target = 0.25 + 0.35 * float(params.get("tactical.formation_space", 0.5))
    if ratio >= target:
        report.add(
            "tactical.formation_space",
            "pass",
            f"{ratio:.0%} open ground for formations",
        )
    else:
        report.add(
            "tactical.formation_space",
            "warn",
            f"{ratio:.0%} open ground for formations, below the {target:.0%} target",
        )


def validate(
    definition: dict[str, Any],
    width: int,
    height: int,
    params: dict,
    sectors: int,
    scratch: Path,
) -> Report:
    report = Report()
    metrics = report.metrics
    metrics["sectors"] = sectors
    _gates(definition, scratch, report)
    metrics["out_of_bounds"] = _bounds(definition, width, height, report)
    _hills(definition, report)
    _roads(definition, width, height, report, metrics)
    _settlements_and_armies(definition, params, report, metrics)
    _ground(definition, width, height, params, report, metrics)
    return report
