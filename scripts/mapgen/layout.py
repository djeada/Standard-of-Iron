"""The tactical skeleton every stage builds on.

A battlefield is laid out before anything is placed: which way the armies face,
where the sectors divide, what each boundary is made of, where the strategic
routes run and where they cross. The terrain, water, settlement and road
stages then materialise that plan. Keeping it in one place is what makes the
result read as a battlefield - the river sits where the routes need a
crossing, the fort sits behind it - instead of independent random layers.

All coordinates here are layout coordinates: ``u`` from the player's edge to
the enemy's, ``v`` across.
"""

from __future__ import annotations

import random
from dataclasses import dataclass, field
from typing import Any

from .geometry import Frame

LANE_MARGIN = 0.1


@dataclass
class Boundary:
    index: int
    u: float
    kind: str


@dataclass
class Link:
    u: float
    lane_a: int
    lane_b: int


@dataclass
class Site:
    u: float
    v: float
    role: str
    lane: int


@dataclass
class Layout:
    frame: Frame
    sectors: int
    boundaries: list[Boundary]
    lanes: list[list[tuple[float, float]]]
    links: list[Link]
    sites: list[Site]
    start: tuple[float, float]
    enemy_main: tuple[float, float]
    coast_side: int | None
    v_range: tuple[float, float]
    symmetric: bool
    extra: dict[str, Any] = field(default_factory=dict)

    def sector_bounds(self) -> list[tuple[float, float]]:
        edges = [0.0] + [b.u for b in self.boundaries] + [1.0]
        return list(zip(edges, edges[1:], strict=False))

    def sector_centres(self) -> list[float]:
        return [(a + b) * 0.5 for a, b in self.sector_bounds()]

    def lane_v(self, lane: int, u: float) -> float:
        points = self.lanes[lane]
        for (u0, v0), (u1, v1) in zip(points, points[1:], strict=False):
            if u0 <= u <= u1:
                t = 0.0 if u1 == u0 else (u - u0) / (u1 - u0)
                return v0 + (v1 - v0) * t
        return points[-1][1] if u > points[-1][0] else points[0][1]

    def lane_world(self, lane: int) -> list[tuple[float, float]]:
        return [self.frame.to_world(u, v) for u, v in self.lanes[lane]]

    def to_json(self) -> dict[str, Any]:
        return {
            "orientation": self.frame.orientation,
            "sectors": self.sectors,
            "boundaries": [
                {"u": round(b.u, 4), "kind": b.kind} for b in self.boundaries
            ],
            "coast_side": self.coast_side,
        }


def _boundary_kinds(rng: random.Random, count: int, params: dict) -> list[str]:
    kinds = ["open"] * count
    order = sorted(
        range(count), key=lambda i: (abs(i - (count - 1) / 2.0), rng.random())
    )
    rivers = min(int(params["water.rivers"]), count)
    if params["water.coast"]:

        rivers = 0
    for index in order[:rivers]:
        kinds[index] = "river"
    choke = float(params["tactical.choke_intensity"])
    ridge_bias = float(params["terrain.ridge_bias"])
    forest_bias = float(params["forests.density"])
    for index in order[rivers:]:
        roll = rng.random()
        ridge_chance = (0.25 + 0.6 * choke) * ridge_bias + 0.5 * float(
            params["terrain.mountains"]
        )
        forest_chance = (0.2 + 0.5 * choke) * forest_bias
        if roll < ridge_chance:
            kinds[index] = "ridge"
        elif roll < ridge_chance + forest_chance:
            kinds[index] = "forest"
    return kinds


def build_layout(rng: random.Random, width: int, height: int, params: dict) -> Layout:
    frame = Frame(width, height, rng.randrange(4))
    sectors = int(params["tactical.sectors"])
    symmetric = bool(params["tactical.symmetric"])

    coast_side: int | None = None
    v_min, v_max = 0.0, 1.0
    if params["water.coast"]:
        coast_side = rng.choice((0, 1))
        if coast_side == 0:
            v_min = 0.24
        else:
            v_max = 0.76

    boundaries: list[Boundary] = []
    kinds = _boundary_kinds(rng, sectors - 1, params)
    for k in range(1, sectors):
        jitter = 0.0 if symmetric else rng.uniform(-0.035, 0.035)
        boundaries.append(Boundary(k - 1, k / sectors + jitter, kinds[k - 1]))
    if symmetric:

        for i in range(len(boundaries) // 2):
            boundaries[-1 - i].kind = boundaries[i].kind

    routes = int(params["roads.routes"])
    usable = v_max - v_min
    spread = (0.22 + 0.4 * float(params["tactical.flank_bias"])) * usable
    centre = (
        v_min + usable * 0.5 + (0.0 if symmetric else rng.uniform(-0.06, 0.06) * usable)
    )
    if routes == 1:
        lane_vs = [centre]
    elif routes == 2:
        lane_vs = [centre - spread * 0.5, centre + spread * 0.5]
    else:
        lane_vs = [centre - spread * 0.5, centre, centre + spread * 0.5]
    lo, hi = v_min + LANE_MARGIN, v_max - LANE_MARGIN
    lane_vs = [min(max(v, lo), hi) for v in lane_vs]

    control_us = sorted(
        {0.0, 1.0, *[b.u for b in boundaries]}
        | {
            (a + b) * 0.5
            for a, b in zip(
                [0.0] + [b.u for b in boundaries],
                [b.u for b in boundaries] + [1.0],
                strict=False,
            )
        }
    )
    lanes: list[list[tuple[float, float]]] = []
    for lane_v in lane_vs:
        points = []
        for u in control_us:
            wander = 0.0
            if 0.0 < u < 1.0 and not symmetric:

                wander = rng.uniform(-0.085, 0.085) * usable
            points.append((u, min(max(lane_v + wander, lo), hi)))
        lanes.append(points)
    if symmetric and len(lanes) >= 2:

        lanes[-1] = [(1.0 - u, 1.0 - v) for u, v in reversed(lanes[0])]

    links: list[Link] = []
    centres = [
        (a + b) * 0.5
        for a, b in zip(
            [0.0] + [b.u for b in boundaries],
            [b.u for b in boundaries] + [1.0],
            strict=False,
        )
    ]
    wanted = int(params["roads.loops"])
    if len(lanes) >= 2 and wanted:
        middle = centres[1:-1] or centres
        ranked = sorted(middle, key=lambda u: (abs(u - 0.5), rng.random()))
        for i, u in enumerate(ranked[:wanted]):
            pair = i % (len(lanes) - 1)
            links.append(Link(u, pair, pair + 1))
        if wanted > len(ranked):

            for u in [centres[0], centres[-1]][: wanted - len(ranked)]:
                links.append(Link(u, 0, len(lanes) - 1))
    if len(lanes) == 1 and wanted:
        middle = centres[1:-1] or centres
        for u in sorted(middle, key=lambda u: (abs(u - 0.5), rng.random()))[:wanted]:
            links.append(Link(u, 0, 0))
    links.sort(key=lambda link: link.u)

    start_lane = 0
    enemy_lane = len(lanes) - 1
    start = (0.09, 0.0)
    start = (start[0], next_v(lanes, start_lane, start[0]))
    main_u = 0.88
    if params["water.ring_moat"]:

        main_u = min(main_u, 1.0 - 125.0 / frame.u_length)
    enemy_main = (main_u, next_v(lanes, enemy_lane, main_u))

    sites: list[Site] = []
    for link in links:
        for lane in (link.lane_a, link.lane_b):
            sites.append(Site(link.u, next_v(lanes, lane, link.u), "town", lane))
    for b in boundaries:
        for lane in range(len(lanes)):
            sites.append(
                Site(
                    min(b.u + 0.07, 0.95), next_v(lanes, lane, b.u + 0.07), "fort", lane
                )
            )
    for c in centres:
        for lane in range(len(lanes)):
            sites.append(Site(c, next_v(lanes, lane, c), "camp", lane))
    rng.shuffle(sites)

    return Layout(
        frame=frame,
        sectors=sectors,
        boundaries=boundaries,
        lanes=lanes,
        links=links,
        sites=sites,
        start=start,
        enemy_main=enemy_main,
        coast_side=coast_side,
        v_range=(v_min, v_max),
        symmetric=symmetric,
    )


def next_v(lanes: list[list[tuple[float, float]]], lane: int, u: float) -> float:
    points = lanes[lane]
    for (u0, v0), (u1, v1) in zip(points, points[1:], strict=False):
        if u0 <= u <= u1:
            t = 0.0 if u1 == u0 else (u - u0) / (u1 - u0)
            return v0 + (v1 - v0) * t
    return points[-1][1]
