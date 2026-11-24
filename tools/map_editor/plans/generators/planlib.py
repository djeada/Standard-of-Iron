"""Helpers for writing map_cli operation files (tools/map_editor/plans/*.ops.json)."""

import json


def spline(points, step=14.0):
    """Catmull-Rom through the control points, sampled about every `step` units."""
    import math

    pts = [points[0]] + list(points) + [points[-1]]
    out = []
    for i in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[i + 1], pts[i + 2]
        n = max(1, int(math.hypot(p2[0] - p1[0], p2[1] - p1[1]) / step))
        for k in range(n):
            t = k / n
            t2, t3 = t * t, t * t * t
            out.append(
                [
                    round(
                        0.5
                        * (
                            (2 * p1[a])
                            + (-p0[a] + p2[a]) * t
                            + (2 * p0[a] - 5 * p1[a] + 4 * p2[a] - p3[a]) * t2
                            + (-p0[a] + 3 * p1[a] - 3 * p2[a] + p3[a]) * t3
                        ),
                        2,
                    )
                    for a in (0, 1)
                ]
            )
    out.append([points[-1][0], points[-1][1]])
    return out


SEA_LEVEL = -0.6  # lake "height" that gives quays a bank to stand on
DOCK_SETBACK = 2.25  # dock centre this far inland of the water's edge
# River width that draws one metre of open water at a quay: the drawn edge reaches
# 0.81 x the width (bridge_geometry.h), less the sand margin along it. Measured on
# the cothon: at 1.15 the water meets the dock quays and the hulls float.
RIVER_DRAWN_WIDTH = 1.15


class Water:
    """Python port of Game::Map::Lake's outline (terrain_features.h)."""

    def __init__(self, x, z, width, depth, rotation=0.0, shape="natural"):
        self.x, self.z, self.w, self.d, self.rot, self.shape = (
            x,
            z,
            width,
            depth,
            rotation,
            shape,
        )

    def scale(self, ang):
        import math

        if self.shape == "round":
            return 1.0
        if self.shape == "basin":
            c, s = abs(math.cos(ang)), abs(math.sin(ang))
            return (c**12 + s**12) ** (-1.0 / 12)
        phase = self.x * 0.071 + self.z * 0.113
        return min(
            1.10,
            max(
                0.90,
                1.0
                + math.sin(ang * 3 + phase) * 0.055
                + math.sin(ang * 7 - phase * 1.7) * 0.025,
            ),
        )

    def wet(self, x, z, pad=0.0):
        import math

        a = -math.radians(self.rot)
        dx, dz = x - self.x, z - self.z
        lx = dx * math.cos(a) - dz * math.sin(a)
        lz = dx * math.sin(a) + dz * math.cos(a)
        nx, nz = lx / max(self.w * 0.5 + pad, 1e-4), lz / max(self.d * 0.5 + pad, 1e-4)
        return math.hypot(nx, nz) <= self.scale(math.atan2(nz, nx))


class Plan:
    def __init__(self, nation, player=1):
        self.ops = []
        self.nation = nation
        self.player = player
        self.waters = []
        self.rivers = []
        self.reserved = []
        self.solids = []  # footprints a connecting street may not cross

    def wet(self, x, z, pad=0.0):
        if any(w.wet(x, z, pad) for w in self.waters):
            return True
        import math

        for pts, width in self.rivers:
            reach = width / 2 + pad
            for a, b in zip(pts, pts[1:], strict=False):
                dx, dz = b[0] - a[0], b[1] - a[1]
                L2 = dx * dx + dz * dz
                t = (
                    0.0
                    if L2 == 0
                    else max(0.0, min(1.0, ((x - a[0]) * dx + (z - a[1]) * dz) / L2))
                )
                if math.hypot(x - a[0] - dx * t, z - a[1] - dz * t) <= reach:
                    return True
        return False

    def shore(self, origin, direction, reach=400.0):
        """First water's edge met walking from `origin` (dry) along `direction`."""
        import math

        n = math.hypot(*direction)
        ux, uz = direction[0] / n, direction[1] / n
        lo, t = 0.0, 0.0
        while t < reach and not self.wet(origin[0] + ux * t, origin[1] + uz * t):
            lo, t = t, t + 1.0
        if t >= reach:
            return None
        hi = t
        for _ in range(24):
            mid = (lo + hi) / 2
            if self.wet(origin[0] + ux * mid, origin[1] + uz * mid):
                hi = mid
            else:
                lo = mid
        return [round(origin[0] + ux * hi, 2), round(origin[1] + uz * hi, 2)]

    # --- raw
    def add(self, kind, **fields):
        self.ops.append({"op": "add", "kind": kind, "json": fields})

    def map(self, **fields):
        self.ops.append({"op": "map", **fields})

    def root(self, key, value):
        self.ops.append({"op": "root", "key": key, "value": value})

    # --- terrain
    def lake(self, x, z, width, depth, rotation=0, height=None, shape=None, calm=False):
        j = dict(type="lake", x=x, z=z, width=width, depth=depth, rotation=rotation)
        if calm:
            j["calm"] = True
        if height is not None:
            j["height"] = height
        if shape:
            j["shape"] = shape
        self.add("terrain", **j)
        self.waters.append(Water(x, z, width, depth, rotation, shape or "natural"))

    def sea(self, x, z, width, depth, rotation=0):
        self.lake(x, z, width, depth, rotation, height=SEA_LEVEL)

    def basin(self, x, z, width, depth, rotation=0, shape="basin", lift=0.01):
        """A harbour: quays all round, water a hair above the sea so the two never fight."""
        self.lake(
            x,
            z,
            width,
            depth,
            rotation,
            height=SEA_LEVEL + lift,
            shape=shape,
            calm=True,
        )

    def rise(self, x, z, width, depth, height, taper=0.6, rotation=0):
        self.ops.append(
            {
                "op": "add",
                "kind": "terrain",
                "json": {
                    "type": "flat",
                    "x": x,
                    "z": z,
                    "width": width,
                    "depth": depth,
                    "height": height,
                    "taper": taper,
                    "raise": True,
                    "rotation": rotation,
                },
            }
        )

    def mountain(self, x, z, width, depth, height, rotation=0):
        self.add(
            "terrain",
            type="mountain",
            x=x,
            z=z,
            width=width,
            depth=depth,
            height=height,
            rotation=rotation,
        )

    def forest(self, ident, x, z, radius):
        self.add("forest", id=ident, x=x, z=z, radius=radius)

    # --- linear
    def river(self, points, width, height=None, calm=False):
        j = {"type": "river", "start": points[0], "end": points[-1], "width": width}
        if calm:
            j["calm"] = True
        if height is not None:
            j["height"] = height
        self.rivers.append((points, width))
        if len(points) > 2:
            j["waypoints"] = points
        self.add("linear", **j)

    def ring_river(self, x, z, radius, width, segments=20, height=None, calm=False):
        import math

        # Half a segment of phase: a bridge along x or z then crosses a straight
        # reach square on, as the shipped-bridge checks require.
        pts = [
            [
                round(x + radius * math.cos(2 * math.pi * (i + 0.5) / segments), 2),
                round(z + radius * math.sin(2 * math.pi * (i + 0.5) / segments), 2),
            ]
            for i in range(segments + 1)
        ]
        self.river(pts, width, height, calm)

    def road(self, a, b, width=3.6, style="stone", waypoints=None, keep_clear=False):
        if keep_clear and not waypoints:
            h = width / 2 + 0.6
            self.reserve(
                min(a[0], b[0]) - h,
                min(a[1], b[1]) - h,
                max(a[0], b[0]) + h,
                max(a[1], b[1]) + h,
            )
        j = {"type": "road", "start": a, "end": b, "width": width, "style": style}
        if waypoints:
            # The editor's waypoint list includes both endpoints.
            j["waypoints"] = [a, *waypoints, b]
        self.add("linear", **j)

    def bridge(self, a, b, width=10, height=0.6):
        self.add("linear", type="bridge", start=a, end=b, width=width, height=height)

    def paving(self, x0, z0, x1, z1, style="flagstone", strip=12):
        self.ops.append(
            {"op": "paving", "rect": [x0, z0, x1, z1], "style": style, "strip": strip}
        )

    # --- town
    def owner(self):
        return {"player_id": self.player, "nation": self.nation}

    def block(self, ident, x, z, wide, deep, rotation=0, alley=-1, style="tenement"):
        self.ops.append(
            {
                "op": "block",
                "id": ident,
                "origin": [x, z],
                "lots": [wide, deep],
                "rotation": rotation,
                "alley": alley,
                "style": style,
                **self.owner(),
            }
        )

    def terrace(self, ident, a, b, facing, skip=(), style="tenement", kind="home"):
        self.ops.append(
            {
                "op": "terrace",
                "id": ident,
                "from": a,
                "to": b,
                "facing": facing,
                "skip": list(skip),
                "style": style,
                "type": kind,
                **self.owner(),
            }
        )

    def stoa(self, ident, a, b, facing, skip=()):
        """A run of porticoes."""
        self.terrace(ident, a, b, facing, skip, style="portico", kind="marketplace")

    def quay(self, ident, a, b, facing, skip=()):
        """A run of docks, fronts to the water."""
        self.terrace(ident, a, b, facing, skip, style="dock", kind="marketplace")

    def structure(self, ident, kind, x, z, rotation=0, style=None, **extra):
        j = {
            "type": kind,
            "x": x,
            "z": z,
            "rotation": rotation,
            "id": ident,
            "group": ident,
            **self.owner(),
            **extra,
        }
        if style:
            j["style"] = style
        self.add("structure", **j)

    def wall(self, ident, points, closed=True, tower_spacing=44, style="masonry"):
        legs = list(
            zip(points, points[1:] + (points[:1] if closed else []), strict=False)
        )
        for a, b in legs[: len(points) if closed else len(points) - 1]:
            self.reserve(
                min(a[0], b[0]) - 2.5,
                min(a[1], b[1]) - 2.5,
                max(a[0], b[0]) + 2.5,
                max(a[1], b[1]) + 2.5,
            )
        op = {
            "op": "wall",
            "id": ident,
            "points": points,
            "closed": closed,
            "tower_spacing": tower_spacing,
            **self.owner(),
        }
        if style:
            op["style"] = style
        self.ops.append(op)

    def gate(self, ident, x, z):
        self.ops.append({"op": "gate", "id": ident, "at": [x, z]})

    def prop(self, kind, x, z, scale=1.0, rotation=0):
        self.add("prop", type=kind, x=x, z=z, scale=scale, rotation=rotation)

    def folk(self, ident, kind, points, radius=14, behavior="guard"):
        for i, (x, z) in enumerate(points):
            self.add(
                "spawn",
                type=kind,
                x=x,
                z=z,
                id=f"{ident}_{i}",
                group=ident,
                behavior=behavior,
                guard_radius=radius,
                **self.owner(),
            )

    # --- composites
    def reserve(self, x0, z0, x1, z1):
        """Keep district blocks off this rectangle."""
        self.reserved.append((min(x0, x1), min(z0, z1), max(x0, x1), max(z0, z1)))

    def free(self, x0, z0, x1, z1, pad=1.0):
        for a, b, c, d in self.reserved:
            if x0 - pad < c and x1 + pad > a and z0 - pad < d and z1 + pad > b:
                return False
        steps = 5
        for i in range(steps + 1):
            for j in range(steps + 1):
                if self.wet(
                    x0 + (x1 - x0) * i / steps, z0 + (z1 - z0) * j / steps, 3.0
                ):
                    return False
        return True

    def district(
        self,
        name,
        x0,
        z0,
        x1,
        z1,
        wide=6,
        deep=1,
        street=3.6,
        style="tenement",
        seed=1,
        rotation=0.0,
        trees=True,
    ):
        """Fill a rectangle with perimeter blocks and the streets between them.

        `rotation` turns the whole district about the rectangle's centre; blocks
        that would stand on water or a reserved rectangle are left out, and the
        streets only run where blocks were built.
        """
        import math
        import random

        rng = random.Random(seed)
        LOT_W, LOT_D, PAVE = 4.3, 4.4, 0.5
        bw, bd = wide * LOT_W, 2 * LOT_D + deep * LOT_W
        pitch_x, pitch_z = bw + 2 * PAVE + street, bd + 2 * PAVE + street
        nx = max(1, int((x1 - x0 + street) // pitch_x))
        nz = max(1, int((z1 - z0 + street) // pitch_z))
        ox = x0 + ((x1 - x0) - (nx * pitch_x - street - 2 * PAVE)) / 2
        oz = z0 + ((z1 - z0) - (nz * pitch_z - street - 2 * PAVE)) / 2
        cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
        c, sn = math.cos(math.radians(rotation)), math.sin(math.radians(rotation))

        def world(x, z):
            u, v = x - cx, z - cz
            return [round(cx + c * u + sn * v, 2), round(cz - sn * u + c * v, 2)]

        kinds = [
            "olive_tree",
            "cypress_tree",
            "plant",
            "plant",
            "supply_cart",
            "olive_tree",
        ]
        built = set()
        for j in range(nz):
            for i in range(nx):
                bx, bz = ox + i * pitch_x, oz + j * pitch_z
                corners = [
                    world(bx - 1, bz - 1),
                    world(bx + bw + 1, bz - 1),
                    world(bx + bw + 1, bz + bd + 1),
                    world(bx - 1, bz + bd + 1),
                ]
                xs, zs = [q[0] for q in corners], [q[1] for q in corners]
                if not self.free(min(xs), min(zs), max(xs), max(zs), pad=street / 2):
                    continue
                built.add((i, j))
                origin = world(bx, bz)
                self.block(
                    f"{name}_{i}_{j}",
                    origin[0],
                    origin[1],
                    wide,
                    deep,
                    rotation,
                    alley=rng.randint(1, wide - 2),
                    style=style,
                )
                if trees and deep >= 2:
                    reach_x, reach_z = max(bw / 2 - LOT_D - 3, 1), max(
                        bd / 2 - LOT_D - 2.2, 0.6
                    )
                    for _ in range(rng.randint(1, 3)):
                        at = world(
                            bx + bw / 2 + rng.uniform(-reach_x, reach_x),
                            bz + bd / 2 + rng.uniform(-reach_z, reach_z),
                        )
                        self.prop(
                            rng.choice(kinds),
                            at[0],
                            at[1],
                            round(rng.uniform(0.85, 1.05), 2),
                        )
        # Streets: each grid line runs past the blocks that stand on it.
        for i in range(nx + 1):
            rows = [j for j in range(nz) if (i, j) in built or (i - 1, j) in built]
            if not rows:
                continue
            x = ox - PAVE - street / 2 + i * pitch_x
            self.road(
                world(x, oz + min(rows) * pitch_z - PAVE - street),
                world(x, oz + (max(rows) + 1) * pitch_z - PAVE),
                street,
            )
        for j in range(nz + 1):
            cols = [i for i in range(nx) if (i, j) in built or (i, j - 1) in built]
            if not cols:
                continue
            z = oz - PAVE - street / 2 + j * pitch_z
            self.road(
                world(ox + min(cols) * pitch_x - PAVE - street, z),
                world(ox + (max(cols) + 1) * pitch_x - PAVE, z),
                street,
            )
        return len(built)

    def fill(
        self,
        name,
        x0,
        z0,
        x1,
        z1,
        kinds=((7, 2), (5, 2), (4, 1), (3, 1), (2, 0)),
        street=3.6,
        style="tenement",
        seed=1,
        rotation=0.0,
        trees=True,
        step=1.0,
        pad=1.6,
    ):
        """Pack a rectangle with perimeter blocks, largest first, round whatever is reserved.

        Blocks are tried on a fine lattice in the rectangle's own (turned) frame,
        so they close up against each other and against obstacles a street apart.
        Each block lays the street round itself.
        """
        import math
        import random

        rng = random.Random(seed)
        LOT_W, LOT_D, PAVE = 4.3, 4.4, 0.5
        gap = street + 2 * PAVE
        cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
        c, sn = math.cos(math.radians(rotation)), math.sin(math.radians(rotation))

        def world(x, z):
            u, v = x - cx, z - cz
            return [round(cx + c * u + sn * v, 2), round(cz - sn * u + c * v, 2)]

        placed = []  # local rects of this district
        props = [
            "olive_tree",
            "cypress_tree",
            "plant",
            "plant",
            "supply_cart",
            "olive_tree",
        ]
        count = 0
        for wide, deep in kinds:
            bw, bd = wide * LOT_W, 2 * LOT_D + deep * LOT_W
            z = z0
            while z + bd <= z1 + 1e-6:
                x = x0
                while x + bw <= x1 + 1e-6:
                    hit = next(
                        (
                            r
                            for r in placed
                            if x - gap < r[2]
                            and x + bw + gap > r[0]
                            and z - gap < r[3]
                            and z + bd + gap > r[1]
                        ),
                        None,
                    )
                    if hit is not None:
                        x = max(x + step, hit[2] + gap)
                        continue
                    corners = [
                        world(x, z),
                        world(x + bw, z),
                        world(x + bw, z + bd),
                        world(x, z + bd),
                    ]
                    xs, zs = [q[0] for q in corners], [q[1] for q in corners]
                    if not self.free(min(xs), min(zs), max(xs), max(zs), pad=pad):
                        x += step
                        continue
                    placed.append((x, z, x + bw, z + bd))
                    origin = world(x, z)
                    ident = f"{name}_{count}"
                    self.block(
                        ident,
                        origin[0],
                        origin[1],
                        wide,
                        deep,
                        rotation,
                        alley=rng.randint(1, wide - 2) if wide >= 3 else -1,
                        style=style,
                    )
                    h = PAVE + street / 2
                    ring = [
                        world(x - h, z - h),
                        world(x + bw + h, z - h),
                        world(x + bw + h, z + bd + h),
                        world(x - h, z + bd + h),
                    ]
                    for k in range(4):
                        self.road(ring[k], ring[(k + 1) % 4], street)
                    if trees and deep >= 2:
                        reach_x, reach_z = max(bw / 2 - LOT_D - 3, 1), max(
                            bd / 2 - LOT_D - 2.2, 0.6
                        )
                        for _ in range(rng.randint(1, 3)):
                            at = world(
                                x + bw / 2 + rng.uniform(-reach_x, reach_x),
                                z + bd / 2 + rng.uniform(-reach_z, reach_z),
                            )
                            self.prop(
                                rng.choice(props),
                                at[0],
                                at[1],
                                round(rng.uniform(0.85, 1.05), 2),
                            )
                    count += 1
                    x += bw + gap
                z += step
        for ax, az, bx, bz in placed:
            corners = [world(ax, az), world(bx, az), world(bx, bz), world(ax, bz)]
            xs, zs = [q[0] for q in corners], [q[1] for q in corners]
            self.solids.append(
                (min(xs) + 0.4, min(zs) + 0.4, max(xs) - 0.4, max(zs) - 0.4)
            )
            self.reserve(
                min(xs) - PAVE - street / 2,
                min(zs) - PAVE - street / 2,
                max(xs) + PAVE + street / 2,
                max(zs) + PAVE + street / 2,
            )
        return count

    def square(
        self, name, x0, z0, x1, z1, sides="ns", paving="flagstone", statues=0, grand=""
    ):
        """A paved square with stoas on the named sides (n, s, w, e), fronts inward."""
        LOT = 5.5
        self.paving(x0 - 1.5, z0 - 1.5, x1 + 1.5, z1 + 1.5, style=paving)

        def side(key, a, b, facing):
            if key in sides:
                self.terrace(
                    f"{name}_{key}",
                    a,
                    b,
                    facing,
                    style="basilica" if key in grand else "portico",
                    kind="marketplace",
                )

        side("n", [x0, z0 - LOT / 2], [x1, z0 - LOT / 2], 0)
        side("s", [x0, z1 + LOT / 2], [x1, z1 + LOT / 2], 180)
        side("w", [x0 - LOT / 2, z0], [x0 - LOT / 2, z1], 90)
        side("e", [x1 + LOT / 2, z0], [x1 + LOT / 2, z1], 270)
        self.reserve(
            x0 - (LOT + 1 if "w" in sides else 1),
            z0 - (LOT + 1 if "n" in sides else 1),
            x1 + (LOT + 1 if "e" in sides else 1),
            z1 + (LOT + 1 if "s" in sides else 1),
        )
        for k in range(statues):
            t = (k + 0.5) / statues
            self.prop(
                "statue", round(x0 + (x1 - x0) * t, 1), round((z0 + z1) / 2, 1), 1.1
            )

    def harbour(self, name, cx, cz, width, depth, sides="nwe", shape="basin"):
        """A basin with docks on the named sides, fronts to the water. Its open side is the sea's."""
        self.basin(cx, cz, width, depth, shape=shape)
        x0, x1, z0, z1 = cx - width / 2, cx + width / 2, cz - depth / 2, cz + depth / 2
        inset = 3.0
        if "n" in sides:
            self.quay(
                f"{name}_n",
                [x0 + inset, z0 - DOCK_SETBACK],
                [x1 - inset, z0 - DOCK_SETBACK],
                0,
            )
        if "s" in sides:
            self.quay(
                f"{name}_s",
                [x0 + inset, z1 + DOCK_SETBACK],
                [x1 - inset, z1 + DOCK_SETBACK],
                180,
            )
        if "w" in sides:
            self.quay(
                f"{name}_w",
                [x0 - DOCK_SETBACK, z0 + inset],
                [x0 - DOCK_SETBACK, z1 - inset],
                90,
            )
        if "e" in sides:
            self.quay(
                f"{name}_e",
                [x1 + DOCK_SETBACK, z0 + inset],
                [x1 + DOCK_SETBACK, z1 - inset],
                270,
            )
        self.reserve(x0 - 8, z0 - 8, x1 + 8, z1 + 8)

    def grove(
        self, x0, z0, x1, z1, count, kinds=("olive_tree",), seed=1, scale=(0.9, 1.15)
    ):
        import random

        rng = random.Random(seed)
        for _ in range(count):
            self.prop(
                rng.choice(kinds),
                round(rng.uniform(x0, x1), 1),
                round(rng.uniform(z0, z1), 1),
                round(rng.uniform(*scale), 2),
            )

    def avenue(self, a, b, spacing=9.0, offset=4.0, kind="cypress_tree"):
        """Trees down both sides of the line a -> b."""
        import math

        n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) // spacing))
        dx, dz = (b[0] - a[0]) / n, (b[1] - a[1]) / n
        L = math.hypot(dx, dz)
        px, pz = -dz / L * offset, dx / L * offset
        for k in range(n + 1):
            for sgn in (-1, 1):
                self.prop(
                    kind,
                    round(a[0] + dx * k + sgn * px, 1),
                    round(a[1] + dz * k + sgn * pz, 1),
                    1.05,
                )

    def cothon(
        self, name, cx, cz, radius, width, docks=14, gap_deg=0.0, gap_width=50.0
    ):
        """A round harbour: a ring of water about an island, docks all round its outer quay
        except across `gap_deg` (degrees, 0 = +x, 90 = +z) where its channel leaves."""
        import math

        # Authored at the width that draws `width` of open water (RIVER_DRAWN_WIDTH):
        # a ring authored at `width` floods its own quays and drowns the docks' hulls.
        self.ring_river(
            cx,
            cz,
            radius,
            width / RIVER_DRAWN_WIDTH,
            segments=28,
            height=SEA_LEVEL + 0.01,
            calm=True,
        )
        outer = radius + width / 2
        reach = outer + DOCK_SETBACK
        for k in range(docks):
            ang = 360.0 * k / docks
            off = abs((ang - gap_deg + 180) % 360 - 180)
            if off < gap_width / 2:
                continue
            a = math.radians(ang)
            x, z = cx + reach * math.cos(a), cz + reach * math.sin(a)
            facing = math.degrees(math.atan2(cx - x, cz - z)) % 360
            self.structure(
                f"{name}_dock_{k}",
                "marketplace",
                round(x, 2),
                round(z, 2),
                round(facing, 1),
                style="dock",
            )
        self.reserve(cx - outer - 12, cz - outer - 12, cx + outer + 12, cz + outer + 12)

    def temple(self, ident, x, z, rotation=0, cypress=True):
        self.structure(ident, "temple", x, z, rotation)
        w, d = (12.4, 9.2) if rotation % 180 == 0 else (9.2, 12.4)
        self.reserve(x - w / 2 - 2, z - d / 2 - 2, x + w / 2 + 2, z + d / 2 + 2)
        self.solids.append((x - w / 2, z - d / 2, x + w / 2, z + d / 2))

    def crowd(self, ident, x0, z0, x1, z1, count, seed=1, radius=14, kind="civilian"):
        import random

        rng = random.Random(seed)
        pts = [
            (round(rng.uniform(x0, x1), 1), round(rng.uniform(z0, z1), 1))
            for _ in range(count)
        ]
        self.folk(ident, kind, pts, radius)

    # --- one road network
    def _road_segments(self):
        """Every road segment the ops will lay, as the topology audit sees them."""
        import math

        segs = []
        for op in self.ops:
            if (
                op["op"] == "add"
                and op["kind"] == "linear"
                and op["json"]["type"] in ("road", "bridge")
            ):
                j = op["json"]
                pts = j.get("waypoints") or [j["start"], j["end"]]
                for a, b in zip(pts, pts[1:], strict=False):
                    if a != b:
                        segs.append((tuple(a), tuple(b)))
            elif op["op"] == "paving":
                x0, z0, x1, z1 = op["rect"]
                x0, x1, z0, z1 = min(x0, x1), max(x0, x1), min(z0, z1), max(z0, z1)
                along_x = (x1 - x0) >= (z1 - z0)
                across = (z1 - z0) if along_x else (x1 - x0)
                strip = min(max(op.get("strip", 7.0), 2.0), max(across, 2.0))
                n = max(1, math.ceil(across / max(strip - 1.2, 1.0)))
                width = min(strip, across)
                pitch = (across - width) / (n - 1) if n > 1 else 0.0
                for i in range(n):
                    o = width / 2 + pitch * i
                    segs.append(
                        ((x0, z0 + o), (x1, z0 + o))
                        if along_x
                        else ((x0 + o, z0), (x0 + o, z1))
                    )
                if n > 1:
                    r, mx, mz = (across - width) / 2, (x0 + x1) / 2, (z0 + z1) / 2
                    segs.append(
                        ((mx, mz - r), (mx, mz + r))
                        if along_x
                        else ((mx - r, mz), (mx + r, mz))
                    )
        return segs

    @staticmethod
    def _closest(p, a, b):
        dx, dz = b[0] - a[0], b[1] - a[1]
        L2 = dx * dx + dz * dz
        t = (
            0.0
            if L2 == 0
            else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dz) / L2))
        )
        return (a[0] + dx * t, a[1] + dz * t)

    @staticmethod
    def _cross(a, b, c, d):
        def o(p, q, r):
            return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])

        return (o(a, b, c) * o(a, b, d) < 0) and (o(c, d, a) * o(c, d, b) < 0)

    def _components(self, segs, join=1.75):
        import math

        parent = list(range(len(segs)))

        def find(i):
            while parent[i] != i:
                parent[i] = parent[parent[i]]
                i = parent[i]
            return i

        def near(p, s):
            q = self._closest(p, *s)
            return math.hypot(p[0] - q[0], p[1] - q[1]) <= join

        for i in range(len(segs)):
            ax0, ax1 = (
                min(segs[i][0][0], segs[i][1][0]) - join,
                max(segs[i][0][0], segs[i][1][0]) + join,
            )
            az0, az1 = (
                min(segs[i][0][1], segs[i][1][1]) - join,
                max(segs[i][0][1], segs[i][1][1]) + join,
            )
            for j in range(i + 1, len(segs)):
                if find(i) == find(j):
                    continue
                b = segs[j]
                if (
                    max(b[0][0], b[1][0]) < ax0
                    or min(b[0][0], b[1][0]) > ax1
                    or max(b[0][1], b[1][1]) < az0
                    or min(b[0][1], b[1][1]) > az1
                ):
                    continue
                a = segs[i]
                if (
                    self._cross(a[0], a[1], b[0], b[1])
                    or near(a[0], b)
                    or near(a[1], b)
                    or near(b[0], a)
                    or near(b[1], a)
                ):
                    parent[find(i)] = find(j)
        return [find(i) for i in range(len(segs))]

    def _blocked(self, a, b):
        steps = max(2, int(max(abs(b[0] - a[0]), abs(b[1] - a[1])) / 0.75))
        for k in range(steps + 1):
            x, z = a[0] + (b[0] - a[0]) * k / steps, a[1] + (b[1] - a[1]) * k / steps
            if self.wet(x, z, 1.0) or any(
                r[0] < x < r[2] and r[1] < z < r[3] for r in self.solids
            ):
                return True
        return False

    def connect_roads(self, max_len=18.0, width=3.6, style="stone"):
        """Join detached road networks to the largest one with short streets; returns what is left."""
        import collections
        import math

        for _ in range(200):
            segs = self._road_segments()
            comp = self._components(segs)
            sizes = collections.Counter(comp)
            if len(sizes) <= 1:
                return 1
            main = sizes.most_common(1)[0][0]
            best = None
            for i, s in enumerate(segs):
                if comp[i] == main:
                    continue
                for j, t in enumerate(segs):
                    if comp[j] != main:
                        continue
                    for p, other in ((s[0], t), (s[1], t)):
                        q = self._closest(p, *other)
                        d = math.hypot(p[0] - q[0], p[1] - q[1])
                        if (
                            d <= max_len
                            and (best is None or d < best[0])
                            and not self._blocked(p, q)
                        ):
                            best = (d, p, q)
                    for p, other in ((t[0], s), (t[1], s)):
                        q = self._closest(p, *other)
                        d = math.hypot(p[0] - q[0], p[1] - q[1])
                        if (
                            d <= max_len
                            and (best is None or d < best[0])
                            and not self._blocked(p, q)
                        ):
                            best = (d, p, q)
            if best is None:
                import collections as _c

                for root, size in _c.Counter(comp).items():
                    if root != main:
                        k = comp.index(root)
                        print("  detached:", size, "segment(s) at", segs[k])
                break
            _, p, q = best
            if (
                math.hypot(p[0] - q[0], p[1] - q[1]) < 3.0
            ):  # the editor lays no road shorter
                ux, uz = q[0] - p[0], q[1] - p[1]
                n = math.hypot(ux, uz) or 1.0
                p = (p[0] - ux / n * 1.6, p[1] - uz / n * 1.6)
                q = (q[0] + ux / n * 1.6, q[1] + uz / n * 1.6)
            self.road(
                [round(p[0], 2), round(p[1], 2)],
                [round(q[0], 2), round(q[1], 2)],
                width,
                style,
            )
        return len(set(self._components(self._road_segments())))

    def tally(self):
        import collections

        c = collections.Counter()
        for op in self.ops:
            if op["op"] == "block":
                w, d = op["lots"]
                c["home"] += 2 * w + 2 * d - (1 if op.get("alley", -1) >= 0 else 0)
            elif op["op"] == "terrace":
                import math

                a, b = op["from"], op["to"]
                lot = 4.3 if op.get("type", "home") == "home" else 5.5
                c[op.get("style", "home")] += int(
                    (math.hypot(b[0] - a[0], b[1] - a[1]) + 0.01) // lot
                )
            elif op["op"] == "add" and op["kind"] == "structure":
                c[op["json"]["type"]] += 1
            elif op["op"] == "add" and op["kind"] == "spawn":
                c["spawn"] += 1
            elif op["op"] == "add" and op["kind"] == "prop":
                c["prop"] += 1
        return dict(c)

    def write(self, path):
        with open(path, "w") as f:
            json.dump(self.ops, f, indent=1)
            f.write("\n")
        return len(self.ops)
