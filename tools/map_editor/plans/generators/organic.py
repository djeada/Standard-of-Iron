"""Organic town layout on top of planlib.Plan: curving streets, houses turned to
follow them, gardens in the courts behind, parks, rotated squares, arcs of
porticoes. Needs shapely (run with ~/.cache/soi-tools/venv/bin/python).

North is -z, east is +x. A structure's `rotation` is the way its front looks:
0 is +z, 90 is +x.
"""

import math
import random

from shapely.geometry import LineString, Point, Polygon, box
from shapely.ops import unary_union
from shapely import affinity

from planlib import spline

HOUSE_W, HOUSE_D = 4.3, 4.4  # home lot: along the street, street to court
SHOP_W, SHOP_D = 5.5, 5.5  # marketplace lot (portico, basilica, dock)
TEMPLE_W, TEMPLE_D = 12.4, 9.2


def facing_of(fx, fz):
    """Rotation whose front looks along (fx, fz)."""
    return round(math.degrees(math.atan2(fx, fz)) % 360.0, 1)


def rect(cx, cz, along, across, tx, tz):
    """Rectangle centred at (cx, cz), `along` units on the unit tangent (tx, tz)."""
    nx, nz = -tz, tx
    a, b = along / 2, across / 2
    return Polygon(
        [
            (cx - tx * a - nx * b, cz - tz * a - nz * b),
            (cx + tx * a - nx * b, cz + tz * a - nz * b),
            (cx + tx * a + nx * b, cz + tz * a + nz * b),
            (cx - tx * a + nx * b, cz - tz * a + nz * b),
        ]
    )


def smooth(points, step=6.0):
    return [tuple(q) for q in spline([list(p) for p in points], step)]


def blob(cx, cz, rx, rz, seed=1, wobble=0.18, n=28, rotation=0.0):
    """An irregular rounded outline, for parks, quarters and hills."""
    rng = random.Random(seed)
    ph = [rng.uniform(0, 6.3) for _ in range(3)]
    c, s = math.cos(math.radians(rotation)), math.sin(math.radians(rotation))
    pts = []
    for k in range(n):
        a = 2 * math.pi * k / n
        r = 1 + wobble * (
            0.6 * math.sin(2 * a + ph[0])
            + 0.3 * math.sin(3 * a + ph[1])
            + 0.2 * math.sin(5 * a + ph[2])
        )
        x, z = rx * r * math.cos(a), rz * r * math.sin(a)
        pts.append((cx + c * x - s * z, cz + s * x + c * z))
    return Polygon(pts)


class Index:
    """A coarse spatial hash of shapely geometries."""

    def __init__(self, cell=16.0):
        self.cell, self.cells = cell, {}

    def _keys(self, g):
        x0, z0, x1, z1 = g.bounds
        c = self.cell
        for i in range(int(math.floor(x0 / c)), int(math.floor(x1 / c)) + 1):
            for j in range(int(math.floor(z0 / c)), int(math.floor(z1 / c)) + 1):
                yield (i, j)

    def add(self, g):
        for k in self._keys(g):
            self.cells.setdefault(k, []).append(g)

    def hits(self, g, tol=0.0):
        seen = set()
        for k in self._keys(g):
            for h in self.cells.get(k, ()):
                if id(h) in seen:
                    continue
                seen.add(id(h))
                if tol > 0:
                    if h.intersection(g).area > tol:
                        return True
                elif h.intersects(g):
                    return True
        return False

    def near(self, g, dist):
        probe = g.buffer(dist)
        return self.hits(probe)


class Town:
    def __init__(self, plan, seed=1):
        self.p = plan
        self.rng = random.Random(seed)
        self.solid = Index()  # houses, buildings, squares, monuments
        self.roads = Index()  # road surfaces
        self.green = Index()  # trees already set
        self.kept = (
            Index()
        )  # squares and courts kept clear of houses (streets may join them)
        self.n_house = 0
        self.n_prop = 0
        self.road_lines = []

    # ------------------------------------------------------------ bookkeeping
    def wet(self, g, pad=1.0):
        x0, z0, x1, z1 = g.bounds
        pts = (
            [((x0 + x1) / 2, (z0 + z1) / 2)] + list(g.exterior.coords)[:-1]
            if hasattr(g, "exterior")
            else [(x0, z0)]
        )
        return any(self.p.wet(x, z, pad) for (x, z) in pts)

    def claim(self, g):
        self.solid.add(g)
        return g

    def free(self, g, region=None, roads=True, tol=0.25, pad=1.2):
        if region is not None and not region.contains(g):
            return False
        if self.solid.hits(g, tol) or self.kept.hits(g, tol):
            return False
        if roads and self.roads.hits(g, tol):
            return False
        if self.wet(g, pad):
            return False
        return True

    def keep_out(self, g):
        """Reserve ground (a square, a monument's court) against houses and planlib fills."""
        self.kept.add(g)
        x0, z0, x1, z1 = g.bounds
        self.p.reserve(x0, z0, x1, z1)

    # ------------------------------------------------------------ streets
    def street(self, points, width=4.0, style="stone", curve=True, step=7.0, mark=True):
        pts = (
            smooth(points, step)
            if curve and len(points) > 2
            else [tuple(q) for q in points]
        )
        pts = [(round(x, 2), round(z, 2)) for (x, z) in pts]
        dedup = [pts[0]]
        for q in pts[1:]:
            if math.hypot(q[0] - dedup[-1][0], q[1] - dedup[-1][1]) > 0.5:
                dedup.append(q)
        pts = dedup
        if len(pts) < 2:
            return None
        line = LineString(pts)
        if line.length < 3.0:
            return None
        mid = [list(q) for q in pts[1:-1]]
        self.p.road(
            list(pts[0]),
            list(pts[-1]),
            round(width, 2),
            style,
            waypoints=mid if mid else None,
        )
        if not hasattr(self, "road_ops"):
            self.road_ops = {}
        self.road_ops[id(line)] = (self.p.ops[-1], mark)
        if mark:
            self.roads.add(line.buffer(width / 2 + 0.3, cap_style=2))
        self.road_lines.append((line, width))
        return line

    def bridge(self, a, b, width=10):
        self.p.bridge(list(a), list(b), width)
        self.roads.add(LineString([a, b]).buffer(width / 2, cap_style=2))

    # ------------------------------------------------------------ buildings
    def house(self, ident, x, z, facing, style="tenement", kind="home", **extra):
        j = {
            "type": kind,
            "x": round(x, 2),
            "z": round(z, 2),
            "rotation": round(facing % 360, 1),
            "id": ident,
            **self.p.owner(),
            **extra,
        }
        if style:
            j["style"] = style
        self.p.add("structure", **j)
        self.n_house += 1

    def building(
        self,
        ident,
        kind,
        x,
        z,
        facing=0.0,
        w=None,
        d=None,
        style=None,
        pad=1.5,
        **extra,
    ):
        """Any structure, its footprint claimed (w along its front, d front to back)."""
        w = w or {
            "temple": TEMPLE_W,
            "marketplace": SHOP_W,
            "barracks": 7.2,
            "farm": 13.6,
            "defense_tower": 3.0,
            "home": HOUSE_W,
        }.get(kind, 6.0)
        d = d or {
            "temple": TEMPLE_D,
            "marketplace": SHOP_D,
            "barracks": 4.8,
            "farm": 13.6,
            "defense_tower": 3.0,
            "home": HOUSE_D,
        }.get(kind, 6.0)
        a = math.radians(facing)
        fx, fz = math.sin(a), math.cos(a)
        g = rect(x, z, w + 2 * pad, d + 2 * pad, fz, -fx)
        self.claim(g)
        j = {
            "type": kind,
            "x": round(x, 2),
            "z": round(z, 2),
            "rotation": round(facing % 360, 1),
            "id": ident,
            "group": ident,
            **self.p.owner(),
            **extra,
        }
        if style:
            j["style"] = style
        self.p.add("structure", **j)
        return g

    def frontage(
        self,
        ident,
        line,
        width,
        sides=(1, -1),
        region=None,
        styles=(("tenement", 1.0),),
        kind="home",
        alley=(6, 11),
        gap=0.0,
        setback=1.45,
        start=2.0,
        end=2.0,
        lot=None,
        depth=None,
        rows=1,
        stagger=0.0,
    ):
        """Houses along both sides of a street line, each turned to face it."""
        lot = lot or (SHOP_W if kind == "marketplace" else HOUSE_W)
        depth = depth or (SHOP_D if kind == "marketplace" else HOUSE_D)
        L = line.length
        made = 0
        weights = [w for (_, w) in styles]
        names = [s for (s, _) in styles]
        for side in sides:
            for row in range(rows):
                s = start + lot / 2
                run = 0
                next_alley = self.rng.randint(*alley) if alley else 10**9
                jog = self.rng.uniform(0, stagger)
                while s < L - end - lot / 2:
                    a, b = line.interpolate(max(0.0, s - 0.6)), line.interpolate(
                        min(L, s + 0.6)
                    )
                    tx, tz = b.x - a.x, b.y - a.y
                    n = math.hypot(tx, tz) or 1.0
                    tx, tz = tx / n, tz / n
                    nx, nz = -tz * side, tx * side
                    c = line.interpolate(s)
                    off = width / 2 + setback + jog + depth / 2 + row * (depth + 0.05)
                    cx, cz = c.x + nx * off, c.y + nz * off
                    g = rect(cx, cz, lot - 0.12, depth - 0.12, tx, tz)
                    if not self.free(g, region):
                        s += 0.7
                        run = 0
                        continue
                    self.claim(g)
                    style = self.rng.choices(names, weights)[0]
                    # The back row turns its fronts to the court behind.
                    front = facing_of(-nx, -nz) if row == 0 else facing_of(nx, nz)
                    self.house(
                        f"{ident}_{side > 0 and 'l' or 'r'}{row}_{made}",
                        cx,
                        cz,
                        front,
                        style=style,
                        kind=kind,
                    )
                    made += 1
                    run += 1
                    s += lot + gap
                    if run >= next_alley:
                        s += self.rng.uniform(2.6, 4.2)
                        run = 0
                        next_alley = self.rng.randint(*alley)
                        jog = self.rng.uniform(0, stagger)
        return made

    def arc(
        self,
        ident,
        cx,
        cz,
        rx,
        rz,
        a0,
        a1,
        kind="marketplace",
        style="portico",
        outward=False,
        rotation=0.0,
        lot=None,
        gaps=(),
        gap_width=12.0,
    ):
        """Buildings round an ellipse from angle a0 to a1 (degrees), fronts in (or out);
        none within `gap_width` degrees of an angle in `gaps` (the entrances)."""
        lot = lot or (SHOP_W if kind == "marketplace" else HOUSE_W)
        c, s = math.cos(math.radians(rotation)), math.sin(math.radians(rotation))

        def at(t):
            x, z = rx * math.cos(t), rz * math.sin(t)
            return cx + c * x - s * z, cz + s * x + c * z

        # Walk the arc in lot-long steps.
        t, t1 = math.radians(a0), math.radians(a1)
        made = 0
        while t < t1:
            x, z = at(t)
            lo, hi = t, t + 0.5
            for _ in range(30):
                mid = (lo + hi) / 2
                x2, z2 = at(mid)
                if math.hypot(x2 - x, z2 - z) < lot:
                    lo = mid
                else:
                    hi = mid
            tm = (t + lo) / 2
            if lo > t1 + 1e-6:
                break
            mx, mz = at(tm)
            x2, z2 = at(lo)
            tx, tz = (x2 - x) / lot, (z2 - z) / lot
            inx, inz = cx - mx, cz - mz
            n = math.hypot(inx, inz) or 1
            inx, inz = inx / n, inz / n
            fx, fz = (-inx, -inz) if outward else (inx, inz)
            deg = math.degrees(tm) % 360
            if any(abs((deg - gp + 180) % 360 - 180) < gap_width / 2 for gp in gaps):
                t = lo
                continue
            g = rect(
                mx, mz, lot - 0.1, SHOP_D if kind == "marketplace" else HOUSE_D, tx, tz
            )
            self.claim(g)
            self.house(
                f"{ident}_{made}", mx, mz, facing_of(fx, fz), style=style, kind=kind
            )
            made += 1
            t = lo
        return made

    def run(
        self,
        ident,
        a,
        b,
        facing,
        kind="marketplace",
        style="portico",
        lot=None,
        entrance_every=0,
    ):
        """A straight row (stoa, quay) from a to b, fronts looking along `facing` degrees; claims its ground.

        `entrance_every` leaves a passage through the row every that many lots (a
        basilica's doors), flanked by a pair of statues on the front."""
        lot = lot or (SHOP_W if kind == "marketplace" else HOUSE_W)
        L = math.hypot(b[0] - a[0], b[1] - a[1])
        n = int((L + 0.01) // lot)
        if n <= 0:
            return 0
        tx, tz = (b[0] - a[0]) / L, (b[1] - a[1]) / L
        first = (L - n * lot) / 2 + lot / 2
        doors = set()
        if entrance_every and n >= entrance_every + 2:
            gaps = max(1, n // entrance_every)
            doors = {round((g + 1) * n / (gaps + 1)) for g in range(gaps)}
            doors = {d for d in doors if 0 < d < n - 1}
        fa = math.radians(facing)
        fx, fz = math.sin(fa), math.cos(fa)
        made = 0
        for k in range(n):
            x, z = a[0] + tx * (first + k * lot), a[1] + tz * (first + k * lot)
            if k in doors:
                for sgn in (-1, 1):
                    px = x + tx * sgn * (lot / 2 - 0.4) + fx * (SHOP_D / 2 + 0.9)
                    pz = z + tz * sgn * (lot / 2 - 0.4) + fz * (SHOP_D / 2 + 0.9)
                    self.p.prop("statue", round(px, 1), round(pz, 1), 1.15)
                continue
            self.claim(
                rect(
                    x,
                    z,
                    lot - 0.1,
                    SHOP_D if kind == "marketplace" else HOUSE_D,
                    tx,
                    tz,
                )
            )
            self.house(f"{ident}_{k}", x, z, facing, style=style, kind=kind)
            made += 1
        return made

    # ------------------------------------------------------------ squares
    def plaza(
        self,
        ident,
        cx,
        cz,
        w,
        d,
        rotation=0.0,
        style="flagstone",
        sides="ns",
        grand="",
        strip=10.0,
        statues=0,
    ):
        """A paved square turned by `rotation`, stoas on the named sides fronting inward.

        Local frame: +u along the square's width (east at rotation 0), +v its depth (south).
        """
        c, s = math.cos(math.radians(rotation)), math.sin(math.radians(rotation))

        def W(u, v):
            return (round(cx + c * u - s * v, 2), round(cz + s * u + c * v, 2))

        n = max(1, math.ceil(d / (strip - 1.2)))
        sw = min(strip, d)
        pitch = (d - sw) / (n - 1) if n > 1 else 0
        for i in range(n):
            v = -d / 2 + sw / 2 + pitch * i
            self.street([W(-w / 2, v), W(w / 2, v)], sw, style, curve=False, mark=False)
        if n > 1:
            self.street(
                [W(0, -d / 2 + sw / 2), W(0, d / 2 - sw / 2)],
                min(strip, w),
                style,
                curve=False,
                mark=False,
            )
        L = SHOP_D / 2 + 0.3
        rot = rotation
        if "n" in sides:
            self.run(
                f"{ident}_n",
                W(-w / 2, -d / 2 - L),
                W(w / 2, -d / 2 - L),
                facing_of(-s, c),
                style="basilica" if "n" in grand else "portico",
                entrance_every=5 if "n" in grand else 0,
            )
        if "s" in sides:
            self.run(
                f"{ident}_s",
                W(-w / 2, d / 2 + L),
                W(w / 2, d / 2 + L),
                facing_of(s, -c),
                style="basilica" if "s" in grand else "portico",
                entrance_every=5 if "s" in grand else 0,
            )
        if "w" in sides:
            self.run(
                f"{ident}_w",
                W(-w / 2 - L, -d / 2),
                W(-w / 2 - L, d / 2),
                facing_of(c, s),
                style="basilica" if "w" in grand else "portico",
                entrance_every=5 if "w" in grand else 0,
            )
        if "e" in sides:
            self.run(
                f"{ident}_e",
                W(w / 2 + L, -d / 2),
                W(w / 2 + L, d / 2),
                facing_of(-c, -s),
                style="basilica" if "e" in grand else "portico",
                entrance_every=5 if "e" in grand else 0,
            )
        hw, hd = w / 2 + (SHOP_D + 1.5), d / 2 + (SHOP_D + 1.5)
        g = Polygon([W(-hw, -hd), W(hw, -hd), W(hw, hd), W(-hw, hd)])
        self.keep_out(g)
        for k in range(statues):
            u = -w / 2 + w * (k + 0.5) / statues
            x, z = W(u, 0)
            self.p.prop("statue", x, z, 1.1)
        return W

    # ------------------------------------------------------------ greenery
    def tree(self, kind, x, z, scale=1.0, spacing=0.0):
        if spacing > 0:
            g = Point(x, z).buffer(spacing / 2)
            if self.green.hits(g):
                return False
            self.green.add(g)
        self.p.prop(kind, round(x, 1), round(z, 1), round(scale, 2))
        self.n_prop += 1
        return True

    def scatter(
        self,
        region,
        count,
        kinds,
        spacing=4.5,
        clear=1.6,
        scale=(0.85, 1.15),
        seed=None,
        avoid_roads=True,
        tries=12,
        avoid_kept=False,
    ):
        """Poisson-ish scatter of trees/plants in the free parts of a region."""
        rng = random.Random(seed if seed is not None else self.rng.random())
        x0, z0, x1, z1 = region.bounds
        names = [k for (k, _) in kinds] if isinstance(kinds[0], tuple) else list(kinds)
        weights = (
            [w for (_, w) in kinds] if isinstance(kinds[0], tuple) else [1] * len(names)
        )
        made = 0
        for _ in range(count * tries):
            if made >= count:
                break
            x, z = rng.uniform(x0, x1), rng.uniform(z0, z1)
            pt = Point(x, z)
            if not region.contains(pt):
                continue
            g = pt.buffer(clear)
            if (
                self.solid.hits(g)
                or (avoid_roads and self.roads.hits(g))
                or self.p.wet(x, z, 1.5)
                or (avoid_kept and self.kept.hits(g))
            ):
                continue
            if self.tree(
                rng.choices(names, weights)[0], x, z, rng.uniform(*scale), spacing
            ):
                made += 1
        return made

    def courts(
        self,
        region,
        density=0.012,
        kinds=(("olive_tree", 3), ("cypress_tree", 2), ("plant", 3), ("pine_tree", 1)),
        seed=None,
        depth=5.0,
    ):
        """Gardens in the courts behind street fronts: trees well away from every street."""
        rng = random.Random(seed if seed is not None else self.rng.random())
        x0, z0, x1, z1 = region.bounds
        count = int(region.area * density)
        made = 0
        for _ in range(count * 8):
            if made >= count:
                break
            x, z = rng.uniform(x0, x1), rng.uniform(z0, z1)
            pt = Point(x, z)
            if not region.contains(pt):
                continue
            if (
                self.solid.hits(pt.buffer(1.4))
                or self.kept.hits(pt.buffer(1.4))
                or self.roads.hits(pt.buffer(depth))
                or self.p.wet(x, z, 1.5)
            ):
                continue
            names = [k for (k, _) in kinds]
            if self.tree(
                rng.choices(names, [w for (_, w) in kinds])[0],
                x,
                z,
                rng.uniform(0.8, 1.1),
                3.2,
            ):
                made += 1
        return made

    def avenue(
        self,
        line,
        spacing=8.0,
        offset=4.0,
        kind="cypress_tree",
        sides=(1, -1),
        scale=1.0,
        region=None,
    ):
        L = line.length
        made = 0
        s = spacing / 2
        while s < L:
            a, b = line.interpolate(max(0, s - 0.5)), line.interpolate(min(L, s + 0.5))
            tx, tz = b.x - a.x, b.y - a.y
            n = math.hypot(tx, tz) or 1
            c = line.interpolate(s)
            for side in sides:
                x, z = c.x - tz / n * offset * side, c.y + tx / n * offset * side
                pt = Point(x, z)
                if region is not None and not region.contains(pt):
                    continue
                if self.solid.hits(pt.buffer(1.2)) or self.p.wet(x, z, 1.0):
                    continue
                if self.tree(kind, x, z, scale, 2.0):
                    made += 1
            s += spacing
        return made

    def park(
        self,
        ident,
        region,
        seed=1,
        path_style="flagstone",
        path_width=2.4,
        trees=None,
        density=0.010,
        loop=True,
        statues=4,
        pines=True,
        centre=None,
        cross=2,
    ):
        """A garden: a winding walk round it, paths across, groves, cypress-lined walks, statues."""
        rng = random.Random(seed)
        inner = region.buffer(-7)
        lines = []
        if (
            loop
            and not inner.is_empty
            and inner.geom_type == "Polygon"
            and inner.area > 400
        ):
            ring = list(inner.simplify(2.0).exterior.coords)
            pts = ring[:: max(1, len(ring) // 14)]
            pts = [tuple(q) for q in spline([list(q) for q in pts + [pts[0]]], 5.0)][
                :-1
            ]
            h = len(pts) // 2
            lines.append(self.street(pts[: h + 1], path_width, path_style, curve=False))
            lines.append(
                self.street(pts[h:] + [pts[0]], path_width, path_style, curve=False)
            )
        cx, cz = centre if centre else (region.centroid.x, region.centroid.y)
        x0, z0, x1, z1 = region.bounds
        for k in range(cross):
            ang = rng.uniform(0, math.pi) + k * math.pi / max(cross, 1)
            r = max(x1 - x0, z1 - z0)
            raw = LineString(
                [
                    (cx - math.cos(ang) * r, cz - math.sin(ang) * r),
                    (cx + math.cos(ang) * r, cz + math.sin(ang) * r),
                ]
            )
            seg = raw.intersection(inner if not inner.is_empty else region)
            if seg.is_empty or seg.geom_type != "LineString" or seg.length < 12:
                continue
            a, b = seg.coords[0], seg.coords[-1]
            bend = rng.uniform(-0.12, 0.12) * seg.length
            mx, mz = (a[0] + b[0]) / 2 - math.sin(ang) * bend, (
                a[1] + b[1]
            ) / 2 + math.cos(ang) * bend
            line = self.street([a, (mx, mz), b], path_width, path_style, step=5.0)
            if line is not None:
                lines.append(line)
                self.avenue(
                    line,
                    spacing=7.5,
                    offset=path_width / 2 + 1.6,
                    kind="cypress_tree",
                    region=region,
                )
        for k, line in enumerate(l for l in lines if l is not None):
            if k == 0:
                self.avenue(
                    line,
                    spacing=11,
                    offset=path_width / 2 + 1.8,
                    kind="plant",
                    sides=(1,),
                    region=region,
                )
        # Statues where the walks meet the centre and round the loop.
        placed = 0
        for k in range(statues):
            ang = 2 * math.pi * k / max(statues, 1) + rng.uniform(-0.3, 0.3)
            for rad in (6.0, 9.0, 12.0):
                x, z = cx + math.cos(ang) * rad, cz + math.sin(ang) * rad
                pt = Point(x, z)
                if (
                    region.contains(pt)
                    and not self.solid.hits(pt.buffer(1.0))
                    and not self.roads.hits(pt.buffer(1.0))
                ):
                    self.p.prop("statue", round(x, 1), round(z, 1), 1.0)
                    self.solid.add(pt.buffer(1.2))
                    placed += 1
                    break
        kinds = trees or (
            (("pine_tree", 3), ("cypress_tree", 2), ("olive_tree", 3), ("plant", 2))
            if pines
            else (("olive_tree", 3), ("cypress_tree", 2), ("plant", 2))
        )
        # Groves: clusters rather than an even sprinkle.
        count = int(region.area * density)
        groves = max(2, count // 14)
        made = 0
        for g in range(groves):
            for _ in range(30):
                gx, gz = rng.uniform(x0, x1), rng.uniform(z0, z1)
                if region.contains(Point(gx, gz)):
                    break
            r = rng.uniform(8, 18)
            made += self.scatter(
                (
                    region.intersection(Point(gx, gz).buffer(r))
                    if region.intersects(Point(gx, gz).buffer(r))
                    else region
                ),
                count // groves,
                kinds,
                spacing=4.2,
                clear=1.6,
                seed=rng.random(),
            )
        # A light sprinkle on the lawns.
        made += self.scatter(
            region, count // 5, kinds, spacing=7.0, clear=1.8, seed=rng.random()
        )
        return made

    # ------------------------------------------------------------ street networks
    def lanes(
        self,
        region,
        angle=0.0,
        major=26.0,
        minor=(34.0, 62.0),
        warp=(9.0, 140.0),
        wiggle=2.5,
        major_width=3.8,
        minor_width=3.2,
        style="stone",
        seed=1,
        skip=0.12,
        origin=None,
        through=0.25,
        clear_of_roads=0.0,
    ):
        """A warped, staggered street web over a region: long lanes roughly along `angle`
        (degrees, 0 = east), short cross lanes between neighbours at staggered places.
        Returns the lane lines that were laid."""
        rng = random.Random(seed)
        self._clear_roads = clear_of_roads
        ox, oz = origin if origin else (region.centroid.x, region.centroid.y)
        c, s = math.cos(math.radians(angle)), math.sin(math.radians(angle))

        def W(u, v):
            return (ox + c * u - s * v, oz + s * u + c * v)

        # Region extent in the local frame.
        us, vs = [], []
        for x, z in region.exterior.coords:
            dx, dz = x - ox, z - oz
            us.append(c * dx + s * dz)
            vs.append(-s * dx + c * dz)
        u0, u1, v0, v1 = min(us) - 10, max(us) + 10, min(vs), max(vs)
        amp, wl = warp
        ph = rng.uniform(0, 6.28)
        allowed = region.buffer(-0.5)
        made = []

        def shared(u):
            return amp * math.sin(u / wl * 2 * math.pi + ph) + amp * 0.35 * math.sin(
                u / (wl * 0.43) * 2 * math.pi + ph * 1.7
            )

        rows = []
        v = v0 + major * rng.uniform(0.3, 0.7)
        while v < v1:
            p1 = rng.uniform(0, 6.28)
            l2 = rng.uniform(40, 90)
            pts = []
            u = u0
            while u <= u1:
                pts.append(
                    W(u, v + shared(u) + wiggle * math.sin(u / l2 * 2 * math.pi + p1))
                )
                u += 6.0
            rows.append((v, pts))
            v += major * rng.uniform(0.85, 1.2)
        for _, pts in rows:
            for piece in self._clip(LineString(pts), allowed):
                if piece.length > 14:
                    ln = self.street(
                        list(piece.coords), major_width, style, curve=False
                    )
                    if ln is not None:
                        made.append(ln)
        # Cross lanes between neighbouring rows, staggered.
        for k in range(len(rows) - 1):
            la, lb = LineString(rows[k][1]), LineString(rows[k + 1][1])
            u = u0 + rng.uniform(0, minor[1])
            while u < u1:
                if rng.random() > skip:
                    t = (u - u0) / (u1 - u0)
                    pa = la.interpolate(t, normalized=True)
                    pb = lb.interpolate(
                        min(1.0, max(0.0, t + rng.uniform(-6, 6) / (u1 - u0))),
                        normalized=True,
                    )
                    mx, mz = (pa.x + pb.x) / 2 + rng.uniform(-2, 2), (
                        pa.y + pb.y
                    ) / 2 + rng.uniform(-2, 2)
                    # Run a little past both lanes so the lane joins them.
                    ext = self._extend([(pa.x, pa.y), (mx, mz), (pb.x, pb.y)], 1.5)
                    for piece in self._clip(LineString(ext), allowed):
                        if piece.length > 8:
                            ln = self.street(
                                list(piece.coords), minor_width, style, curve=False
                            )
                            if ln is not None:
                                made.append(ln)
                u += rng.uniform(*minor)
        self._clear_roads = 0.0
        return made

    def radial(
        self, region, cx, cz, angles, r0, r1, width=4.0, style="stone", bend=0.0, seed=1
    ):
        """Streets running out from a centre, gently bent."""
        rng = random.Random(seed)
        made = []
        for a in angles:
            ar = math.radians(a)
            pts = []
            for k in range(0, 41):
                r = r0 + (r1 - r0) * k / 40
                da = math.radians(bend) * math.sin(k / 40 * math.pi) + rng.uniform(
                    -0.004, 0.004
                )
                pts.append((cx + r * math.cos(ar + da), cz + r * math.sin(ar + da)))
            for piece in self._clip(LineString(pts), region.buffer(-0.5)):
                if piece.length > 14:
                    ln = self.street(list(piece.coords), width, style, curve=False)
                    if ln is not None:
                        made.append(ln)
        return made

    def ring(
        self,
        region,
        cx,
        cz,
        rx,
        rz,
        width=3.6,
        style="stone",
        rotation=0.0,
        a0=0,
        a1=360,
        seed=1,
        wobble=0.04,
    ):
        rng = random.Random(seed)
        c, s = math.cos(math.radians(rotation)), math.sin(math.radians(rotation))
        ph = rng.uniform(0, 6.28)
        pts = []
        steps = max(8, int(abs(a1 - a0) / 4))
        for k in range(steps + 1):
            t = math.radians(a0 + (a1 - a0) * k / steps)
            r = 1 + wobble * math.sin(3 * t + ph)
            x, z = rx * r * math.cos(t), rz * r * math.sin(t)
            pts.append((cx + c * x - s * z, cz + s * x + c * z))
        made = []
        for piece in self._clip(LineString(pts), region.buffer(-0.5)):
            if piece.length > 10:
                ln = self.street(list(piece.coords), width, style, curve=False)
                if ln is not None:
                    made.append(ln)
        return made

    @staticmethod
    def _extend(pts, d):
        (ax, az), (bx, bz) = pts[0], pts[1]
        n = math.hypot(bx - ax, bz - az) or 1
        first = (ax - (bx - ax) / n * d, az - (bz - az) / n * d)
        (ax, az), (bx, bz) = pts[-2], pts[-1]
        n = math.hypot(bx - ax, bz - az) or 1
        last = (bx + (bx - ax) / n * d, bz + (bz - az) / n * d)
        return [first] + list(pts) + [last]

    def _clip(self, line, region):
        """Pieces of a line inside the region, off water and off ground kept clear."""
        inside = line.intersection(region)
        parts = []
        geoms = (
            [inside]
            if inside.geom_type == "LineString"
            else list(getattr(inside, "geoms", []))
        )
        for g in geoms:
            if g.geom_type != "LineString" or g.length < 1:
                continue
            # Break where the line runs through kept ground or water.
            cur = []
            L = g.length
            k = 0.0
            while k <= L:
                q = g.interpolate(k)
                bad = (
                    self.p.wet(q.x, q.y, 1.5)
                    or self.solid.hits(q.buffer(2.2))
                    or self.kept.hits(q.buffer(2.2))
                    or (
                        getattr(self, "_clear_roads", 0) > 0
                        and self.roads.hits(q.buffer(self._clear_roads))
                    )
                )
                if bad:
                    if len(cur) > 1:
                        parts.append(LineString(cur))
                    cur = []
                else:
                    cur.append((q.x, q.y))
                k += 2.0
            if len(cur) > 1:
                parts.append(LineString(cur))
        return parts

    def build_along(
        self,
        ident,
        lines,
        region=None,
        styles=(("tenement", 1.0),),
        alley=(5, 10),
        rows=1,
        stagger=0.0,
    ):
        made = 0
        for k, (line) in enumerate(lines):
            width = next((w for (l, w) in self.road_lines if l is line), 3.6)
            made += self.frontage(
                f"{ident}{k}",
                line,
                width,
                region=region,
                styles=styles,
                alley=alley,
                rows=rows,
                stagger=stagger,
            )
        return made

    def fill_houses(
        self, ident, region, lines=None, styles=(("tenement", 1.0),), alley=(5, 10)
    ):
        """Frontage on every street that touches the region (default: every street laid so far)."""
        made = 0
        cand = lines if lines is not None else [l for (l, _) in self.road_lines]
        for k, line in enumerate(cand):
            if not line.intersects(region):
                continue
            width = next((w for (l, w) in self.road_lines if l is line), 3.6)
            made += self.frontage(
                f"{ident}{k}", line, width, region=region, styles=styles, alley=alley
            )
        return made


def _wall_extras(Town):
    def wall(
        self, ident, points, closed=True, tower_spacing=44, style="masonry", clear=4.0
    ):
        """A wall op (angled legs allowed); its ground is claimed against houses and lanes."""
        pts = [[round(x / 2) * 2, round(z / 2) * 2] for (x, z) in points]
        op = {
            "op": "wall",
            "id": ident,
            "points": pts,
            "closed": closed,
            "tower_spacing": tower_spacing,
            **self.p.owner(),
        }
        if style:
            op["style"] = style
        self.p.ops.append(op)
        ring = pts + ([pts[0]] if closed else [])
        line = LineString(ring)
        self.claim(line.buffer(clear))
        self.wall_line = line
        return Polygon(pts) if closed else line

    def gate(self, ident, x, z):
        self.p.gate(ident, x, z)

    Town.wall = wall
    Town.gate = gate


_wall_extras(Town)


def _network_extras(Town):
    from shapely.ops import nearest_points

    def bridge(self, a, b, width=10):
        self.p.bridge(list(a), list(b), width)
        line = LineString([a, b])
        self.roads.add(line.buffer(width / 2, cap_style=2))
        self.road_lines.append((line, width))

    def connect(self, reach=34.0, width=3.4, style="stone", passes=60):
        """Join every detached piece of the road network to the rest with short streets."""
        for _ in range(passes):
            pieces = unary_union([l.buffer(w / 2 + 0.4) for (l, w) in self.road_lines])
            parts = sorted(getattr(pieces, "geoms", [pieces]), key=lambda g: -g.area)
            if len(parts) <= 1:
                return 1
            main = parts[0]
            joined = False
            for part in sorted(parts[1:], key=lambda g: g.distance(main)):
                if part.distance(main) > reach:
                    continue
                # Try the closest few point pairs along the piece's edge, nearest first.
                edge = part.exterior
                cands = []
                k = 0.0
                while k < edge.length:
                    q = edge.interpolate(k)
                    a, b = nearest_points(q, main)
                    d = a.distance(b)
                    if d <= reach:
                        cands.append((d, a, b))
                    k += 4.0
                cands.sort(key=lambda c: c[0])
                for d, a, b in cands[:40]:
                    ux, uz = b.x - a.x, b.y - a.y
                    n = math.hypot(ux, uz) or 1.0
                    ux, uz = ux / n, uz / n
                    pa = (a.x - ux * 1.5, a.y - uz * 1.5)
                    pb = (b.x + ux * 1.5, b.y + uz * 1.5)
                    seg = LineString([pa, pb])
                    if self.solid.hits(seg.buffer(0.8)) or any(
                        self.p.wet(
                            seg.interpolate(j / 8, normalized=True).x,
                            seg.interpolate(j / 8, normalized=True).y,
                            0.8,
                        )
                        for j in range(9)
                    ):
                        continue
                    self.street([pa, pb], width, style, curve=False)
                    joined = True
                    break
                if joined:
                    break
            if not joined:
                left = len(parts)
                for part in parts[1:]:
                    c = part.centroid
                    print(
                        f"  detached road piece near ({c.x:.0f}, {c.y:.0f}), {part.distance(main):.0f} from the network"
                    )
                return left
        return -1

    Town.bridge = bridge
    Town.connect = connect


_network_extras(Town)


def _defer_extras(Town):
    def later(self, fn, *args, **kw):
        if not hasattr(self, "jobs"):
            self.jobs = []
        self.jobs.append((fn, args, kw))

    def run_jobs(self):
        made = 0
        for fn, args, kw in getattr(self, "jobs", []):
            r = fn(*args, **kw)
            if fn.__name__ in ("frontage", "build_along"):
                made += r or 0
        self.jobs = []
        return made

    Town.later = later
    Town.run_jobs = run_jobs


_defer_extras(Town)


def _audit_connect(Town):
    from shapely.strtree import STRtree
    from shapely.ops import nearest_points

    JOIN = 1.75

    def components(lines):
        tree = STRtree(lines)
        parent = list(range(len(lines)))

        def find(i):
            while parent[i] != i:
                parent[i] = parent[parent[i]]
                i = parent[i]
            return i

        for i, l in enumerate(lines):
            probe = l.union(Point(l.coords[0]).buffer(JOIN)).union(
                Point(l.coords[-1]).buffer(JOIN)
            )
            for j in tree.query(probe):
                j = int(j)
                if j == i or find(i) == find(j):
                    continue
                o = lines[j]
                ends_o = (
                    Point(o.coords[0])
                    .buffer(JOIN)
                    .union(Point(o.coords[-1]).buffer(JOIN))
                )
                if probe.intersects(o) or ends_o.intersects(l):
                    parent[find(i)] = find(j)
        return [find(i) for i in range(len(lines))]

    def connect(self, reach=34.0, width=3.4, style="stone", passes=80):
        """Join detached road networks (the topology audit's rule: centre lines that cross or
        end within 1.75 of one another) to the largest one with short straight streets.
        """
        import collections

        for _ in range(passes):
            lines = [l for (l, _) in self.road_lines]
            comp = components(lines)
            sizes = collections.Counter()
            for i, c in enumerate(comp):
                sizes[c] += lines[i].length
            if len(sizes) <= 1:
                return 1
            main = sizes.most_common(1)[0][0]
            main_lines = [lines[i] for i in range(len(lines)) if comp[i] == main]
            main_tree = STRtree(main_lines)
            joined = False
            # Smallest gap first, over every detached piece.
            cands = {}
            for i, l in enumerate(lines):
                if comp[i] == main:
                    continue
                k = 0.0
                while k <= l.length:
                    q = l.interpolate(k)
                    jn = main_tree.nearest(q)
                    b = nearest_points(q, main_lines[int(jn)])[1]
                    d = q.distance(b)
                    if d <= reach:
                        cands.setdefault(comp[i], []).append((d, q, b))
                    # Rays, for pieces whose nearest neighbour is behind a building.
                    for r in range(16):
                        ang = 2 * math.pi * r / 16
                        ray = LineString(
                            [
                                (q.x, q.y),
                                (
                                    q.x + math.cos(ang) * reach,
                                    q.y + math.sin(ang) * reach,
                                ),
                            ]
                        )
                        best = None
                        for jn in main_tree.query(ray):
                            hit = ray.intersection(main_lines[int(jn)])
                            if hit.is_empty:
                                continue
                            hp = nearest_points(q, hit)[1]
                            dd = q.distance(hp)
                            if best is None or dd < best[0]:
                                best = (dd, hp)
                        if best is not None and best[0] > 0.05:
                            cands.setdefault(comp[i], []).append(
                                (best[0] + 0.01, q, best[1])
                            )
                    k += 3.0
            for c in sorted(cands, key=lambda c: min(x[0] for x in cands[c])):
                for d, a, b in sorted(cands[c], key=lambda x: x[0])[:400]:
                    ux, uz = b.x - a.x, b.y - a.y
                    n = math.hypot(ux, uz)
                    if n < 0.05:
                        continue
                    ux, uz = ux / n, uz / n
                    ext = 1.2 if n > 1.0 else 1.6
                    pa = (a.x - ux * ext, a.y - uz * ext)
                    pb = (b.x + ux * ext, b.y + uz * ext)
                    seg = LineString([pa, pb])
                    if self.solid.hits(seg.buffer(0.8)) or any(
                        self.p.wet(
                            seg.interpolate(jj / 8, normalized=True).x,
                            seg.interpolate(jj / 8, normalized=True).y,
                            0.8,
                        )
                        for jj in range(9)
                    ):
                        continue
                    self.street([pa, pb], width, style, curve=False)
                    joined = True
                    break
                if joined:
                    break
            if not joined:
                for c, size in sizes.items():
                    if c != main:
                        i = comp.index(c)
                        q = lines[i].interpolate(0.5, normalized=True)
                        print(
                            f"  detached road network ({size:.0f} long) near ({q.x:.0f}, {q.y:.0f})"
                        )
                return len(sizes)
        return -1

    Town.connect = connect
    Town.components = staticmethod(components)


_audit_connect(Town)


def _river_bridge(Town):
    def river_bridge(
        self,
        river_pts,
        river_width,
        near,
        deck=10.0,
        land=4.0,
        approach=None,
        style="stone",
        road_width=4.6,
    ):
        """A bridge square to the river at the reach nearest `near`, as the game refits it, with
        an approach road run onto the deck from each bank (to `approach` = (a, b) points if given).
        Returns (end_a, end_b): the deck ends on the -normal and +normal banks."""
        line = LineString(river_pts)
        s = line.project(Point(near))
        c = line.interpolate(s)
        a, b = line.interpolate(max(0, s - 2)), line.interpolate(
            min(line.length, s + 2)
        )
        tx, tz = b.x - a.x, b.y - a.y
        n = math.hypot(tx, tz) or 1
        nx, nz = -tz / n, tx / n
        half = river_width / 2 + land
        ea = (round(c.x - nx * half, 2), round(c.y - nz * half, 2))
        eb = (round(c.x + nx * half, 2), round(c.y + nz * half, 2))
        self.bridge(ea, eb, deck)
        on_a = (c.x - nx * (river_width / 2 - 1.5), c.y - nz * (river_width / 2 - 1.5))
        on_b = (c.x + nx * (river_width / 2 - 1.5), c.y + nz * (river_width / 2 - 1.5))
        if approach:
            pa, pb = approach
            if pa is not None:
                self.street([pa, ea, on_a], road_width, style, curve=False, mark=False)
            if pb is not None:
                self.street([on_b, eb, pb], road_width, style, curve=False, mark=False)
        return ea, eb

    Town.river_bridge = river_bridge


_river_bridge(Town)


def _clear_streets(Town):
    def clear_streets(self, kinds=("marketplace",), keep_styles=("dock",), tol=0.6):
        """Take out monument pieces a street runs through: where a street meets a
        stoa or a basilica front, it walks in through an opening."""
        sizes = {
            "marketplace": (SHOP_W - 0.3, SHOP_D - 0.3),
            "home": (HOUSE_W - 0.3, HOUSE_D - 0.3),
        }
        kept, removed = [], []
        for op in self.p.ops:
            j = op.get("json", {})
            if (
                op.get("op") == "add"
                and op.get("kind") == "structure"
                and j.get("type") in kinds
                and j.get("style") not in keep_styles
            ):
                w, d = sizes[j["type"]]
                a = math.radians(j.get("rotation", 0))
                fx, fz = math.sin(a), math.cos(a)
                g = rect(j["x"], j["z"], w, d, fz, -fx)
                if self.roads.hits(g, tol):
                    removed.append(j.get("id"))
                    continue
            kept.append(op)
        self.p.ops = kept
        return removed

    Town.clear_streets = clear_streets


_clear_streets(Town)


def _building_check(Town):
    orig = Town.building

    def building(
        self,
        ident,
        kind,
        x,
        z,
        facing=0.0,
        w=None,
        d=None,
        style=None,
        pad=1.5,
        **extra,
    ):
        g = orig(self, ident, kind, x, z, facing, w, d, style, pad, **extra)
        if not hasattr(self, "buildings"):
            self.buildings = []
        if kind in ("temple", "barracks", "farm"):
            a = math.radians(facing)
            fx, fz = math.sin(a), math.cos(a)
            ww = w or {"temple": 12.4, "barracks": 7.2, "farm": 13.6}[kind]
            dd = d or {"temple": 9.2, "barracks": 4.8, "farm": 13.6}[kind]
            self.buildings.append((ident, rect(x, z, ww, dd, fz, -fx)))
        return g

    def check_streets(self):
        """Buildings a street runs into (a walk along it would stop at their wall)."""
        hits = []
        for ident, g in getattr(self, "buildings", []):
            for line, width in self.road_lines:
                if (
                    width >= 3.0
                    and line.buffer(width / 2 - 0.4, cap_style=2).intersection(g).area
                    > 1.0
                ):
                    hits.append(
                        (
                            ident,
                            [round(v) for v in g.centroid.coords[0]],
                            width,
                            [
                                tuple(round(v) for v in line.coords[0]),
                                tuple(round(v) for v in line.coords[-1]),
                            ],
                        )
                    )
                    break
        return hits

    Town.building = building
    Town.check_streets = check_streets


_building_check(Town)


def _city_life(Town):
    from shapely.ops import unary_union, nearest_points
    import heapq

    def _set_line(self, line, pts):
        op, mark = self.road_ops[id(line)]
        j = op["json"]
        pts = [[round(x, 2), round(z, 2)] for (x, z) in pts]
        j["start"], j["end"] = pts[0], pts[-1]
        if len(pts) > 2:
            j["waypoints"] = pts[1:-1]
        else:
            j.pop("waypoints", None)
        new = LineString(pts)
        self.road_ops[id(new)] = (op, mark)
        for k, (l, w) in enumerate(self.road_lines):
            if l is line:
                self.road_lines[k] = (new, w)
                if mark:
                    self.roads.add(new.buffer(w / 2 + 0.3, cap_style=2))
                return new
        return new

    def tidy_dead_ends(self, reach=30.0, keep_near=(), edge=392.0):
        """Every street end meets another street: carry a loose end straight on to the
        nearest street within `reach`, else cut it back to its last junction."""
        extended = trimmed = 0
        for _ in range(10):
            changed = False
            for line, width in list(self.road_lines):
                if id(line) not in self.road_ops or not self.road_ops[id(line)][1]:
                    continue  # paving strips, bridges, links
                if line.coords[0] == line.coords[-1]:
                    continue  # a closed walk has no ends
                others = [l for (l, _) in self.road_lines if l is not line]
                if not others:
                    continue
                rest = unary_union(others)
                pts = list(line.coords)
                for end in (0, -1):
                    p = Point(pts[end])
                    if rest.distance(p) <= 1.75 or abs(p.x) > edge or abs(p.y) > edge:
                        continue
                    if any(p.distance(Point(k)) < 8 for k in keep_near):
                        continue
                    if self.p.wet(p.x, p.y, 3.0):
                        continue  # a quay end at the water
                    q = Point(pts[end - 1 if end == -1 else 1])
                    dx, dz = p.x - q.x, p.y - q.y
                    n = math.hypot(dx, dz) or 1
                    ray = LineString(
                        [(p.x, p.y), (p.x + dx / n * reach, p.y + dz / n * reach)]
                    )
                    hit = ray.intersection(rest)
                    if not hit.is_empty:
                        b = nearest_points(p, hit)[1]
                        seg = LineString(
                            [(p.x, p.y), (b.x + dx / n * 1.0, b.y + dz / n * 1.0)]
                        )
                        if (
                            not self.solid.hits(seg.buffer(1.0))
                            and not self.kept.hits(seg.buffer(0.5))
                            and not any(
                                self.p.wet(
                                    seg.interpolate(t / 6, normalized=True).x,
                                    seg.interpolate(t / 6, normalized=True).y,
                                    1.0,
                                )
                                for t in range(7)
                            )
                        ):
                            new_pts = (
                                pts + [(b.x + dx / n * 1.0, b.y + dz / n * 1.0)]
                                if end == -1
                                else [(b.x + dx / n * 1.0, b.y + dz / n * 1.0)] + pts
                            )
                            line = self._set_line(line, new_pts)
                            pts = list(line.coords)
                            extended += 1
                            changed = True
                            continue
                    # Cut back to the last place it meets another street.
                    cut = None
                    L = line.length
                    k = 0.0
                    while k <= L:
                        d = L - k if end == -1 else k
                        if rest.distance(line.interpolate(d)) <= 1.0:
                            cut = d
                            break
                        k += 1.0
                    if (
                        cut is None
                        or (end == -1 and cut < 3)
                        or (end == 0 and L - cut < 3)
                    ):
                        # A lane that meets the rest only at its other end is a blind alley: drop it.
                        if width <= 4.0 and not any(
                            line.distance(Point(b.coords[0])) < 2
                            or line.distance(Point(b.coords[-1])) < 2
                            for (b, w) in self.road_lines
                            if id(b) not in self.road_ops
                        ):
                            op, _ = self.road_ops.pop(id(line))
                            self.p.ops.remove(op)
                            self.road_lines = [
                                (l, w) for (l, w) in self.road_lines if l is not line
                            ]
                            trimmed += 1
                            changed = True
                            break
                        continue
                    if end == -1:
                        new_pts = [c for c in pts if line.project(Point(c)) < cut] + [
                            tuple(line.interpolate(cut + 0.3).coords[0])
                        ]
                    else:
                        new_pts = [
                            tuple(line.interpolate(max(0.0, cut - 0.3)).coords[0])
                        ] + [c for c in pts if line.project(Point(c)) > cut]
                    if len(new_pts) >= 2 and LineString(new_pts).length >= 3:
                        line = self._set_line(line, new_pts)
                        pts = list(line.coords)
                        trimmed += 1
                        changed = True
            if not changed:
                break
        print(f"  dead ends: {extended} carried on, {trimmed} cut back")
        return extended, trimmed

    def route(self, a, b, step=4.5):
        """Shortest walk along the streets from near a to near b, resampled."""
        lines = [
            LineString(self._extend(list(l.coords), 1.8))
            for (l, w) in self.road_lines
            if w >= 2.4
        ]
        net = unary_union(lines)
        segs = list(getattr(net, "geoms", [net]))
        graph = {}

        def key(c):
            return (round(c[0], 2), round(c[1], 2))

        for g in segs:
            cs = list(g.coords)
            for u, v in zip(cs, cs[1:]):
                d = math.hypot(v[0] - u[0], v[1] - u[1])
                graph.setdefault(key(u), []).append((key(v), d))
                graph.setdefault(key(v), []).append((key(u), d))
        nodes = list(graph)
        # Streets the game joins by near-touch (ends within 1.75) are joined here too.
        for u in [n for n in nodes if len(graph[n]) == 1]:
            best = None
            for v in nodes:
                if v == u or any(v == w for (w, _) in graph[u]):
                    continue
                d = math.hypot(v[0] - u[0], v[1] - u[1])
                if d <= 3.0 and (best is None or d < best[0]):
                    best = (d, v)
            if best is not None:
                graph[u].append((best[1], best[0]))
                graph[best[1]].append((u, best[0]))
        sa = min(nodes, key=lambda c: math.hypot(c[0] - a[0], c[1] - a[1]))
        sb = min(nodes, key=lambda c: math.hypot(c[0] - b[0], c[1] - b[1]))
        dist, prev, heap = {sa: 0.0}, {}, [(0.0, sa)]
        while heap:
            d, u = heapq.heappop(heap)
            if u == sb:
                break
            if d > dist.get(u, 1e18):
                continue
            for v, w in graph[u]:
                nd = d + w
                if nd < dist.get(v, 1e18):
                    dist[v], prev[v] = nd, u
                    heapq.heappush(heap, (nd, v))
        path = [sb]
        while path[-1] != sa and path[-1] in prev:
            path.append(prev[path[-1]])
        path.reverse()
        if len(path) < 2:
            raise RuntimeError(f"no street route from {a} to {b}")
        line = LineString(path)
        n = max(2, int(line.length // step) + 1)
        return [
            [
                round(line.interpolate(i * line.length / (n - 1)).x, 2),
                round(line.interpolate(i * line.length / (n - 1)).y, 2),
            ]
            for i in range(n)
        ], line.length

    def pool_garden(
        self,
        ident,
        cx,
        cz,
        r=16.0,
        pool=None,
        kinds=None,
        seed=1,
        statues=4,
        ring="cypress_tree",
    ):
        """A walled-in garden of delight: a calm round pool, a flagstone walk round it,
        a ring of cypresses, statues on the cardinal points, beds of plants."""
        rng = random.Random(seed)
        pr = pool or r * 0.36
        self.p.lake(
            cx,
            cz,
            round(pr * 2, 1),
            round(pr * 2, 1),
            height=-0.3,
            shape="round",
            calm=True,
        )
        walk = [
            (cx + (pr + 2.4) * math.cos(a), cz + (pr + 2.4) * math.sin(a))
            for a in [2 * math.pi * k / 24 for k in range(24)]
        ]
        self.loop(walk, 2.2, "flagstone")
        for k in range(statues):
            a = 2 * math.pi * k / statues
            self.p.prop(
                "statue",
                round(cx + (pr + 4.6) * math.cos(a), 1),
                round(cz + (pr + 4.6) * math.sin(a), 1),
                0.95,
            )
        n = max(10, int(2 * math.pi * (r - 2) / 4.2))
        for k in range(n):
            a = 2 * math.pi * k / n + 0.13
            if any(
                abs(
                    ((a - (2 * math.pi * j / 4 + math.pi / 4)) + math.pi)
                    % (2 * math.pi)
                    - math.pi
                )
                < 0.2
                for j in range(4)
            ):
                continue
            self.tree(
                ring,
                cx + (r - 2) * math.cos(a),
                cz + (r - 2) * math.sin(a),
                rng.uniform(0.95, 1.15),
            )
        kinds = kinds or (("plant", 4), ("olive_tree", 1), ("palm_tree", 0))
        names, weights = [k for (k, _) in kinds], [w for (_, w) in kinds]
        for k in range(int(r * 1.4)):
            a, d = rng.uniform(0, 2 * math.pi), rng.uniform(pr + 4.0, r - 4.5)
            self.tree(
                rng.choices(names, weights)[0],
                cx + d * math.cos(a),
                cz + d * math.sin(a),
                rng.uniform(0.8, 1.05),
                2.4,
            )
        self.keep_out(Point(cx, cz).buffer(r + 1.5))

    def villa(
        self,
        ident,
        x,
        z,
        rot,
        seed=1,
        style="",
        grove=(("olive_tree", 4), ("cypress_tree", 1)),
        pool=True,
    ):
        """A country house round its peristyle, a pool in the court, an orchard round it."""
        rng = random.Random(seed)
        c, s_ = math.cos(math.radians(rot)), math.sin(math.radians(rot))
        ox, oz = x - (c * 11 - s_ * 9), z - (s_ * 11 + c * 9)
        self.p.block(ident, round(ox, 2), round(oz, 2), 5, 2, rot, alley=2, style=style)
        self.claim(rect(x, z, 26, 22, c, s_))
        if pool:
            self.p.lake(
                round(x, 1),
                round(z, 1),
                5,
                3.2,
                rotation=rot,
                height=-0.25,
                shape="basin",
                calm=True,
            )
            self.street(
                [(x + s_ * 2.6, z - c * 2.6), (x + s_ * 11.5, z - c * 11.5)],
                2.0,
                "flagstone",
                curve=False,
            )
        for k in (-1, 1):
            self.tree("cypress_tree", x + c * 7 * k, z + s_ * 7 * k, 1.0)
        self.p.prop("statue", round(x - s_ * 2.6, 1), round(z + c * 2.6, 1), 0.8)
        names, weights = [k for (k, _) in grove], [w for (_, w) in grove]
        for k in range(26):
            a, d = rng.uniform(0, 2 * math.pi), rng.uniform(17, 30)
            self.tree(
                rng.choices(names, weights)[0],
                x + d * math.cos(a),
                z + d * math.sin(a),
                rng.uniform(0.85, 1.1),
                4.0,
            )
        # The drive from its gate down to the nearest road.
        gate = (x + s_ * 11.5, z - c * 11.5)
        end = (x + s_ * 20, z - c * 20)
        roads = [l for (l, w) in self.road_lines if w >= 4.0]
        if roads:
            near = min(roads, key=lambda l: l.distance(Point(end)))
            q = nearest_points(Point(end), near)[1]
            mid = (
                (end[0] + q.x) / 2 + rng.uniform(-6, 6),
                (end[1] + q.y) / 2 + rng.uniform(-6, 6),
            )
            self.street([gate, end, mid, (q.x, q.y)], 3.0, "stone")
        else:
            self.street([gate, end], 3.0, "stone", curve=False)

    def compound(self, ident, x0, z0, x1, z1, gate_side="s", style="masonry"):
        """A walled yard with one gate (axis-aligned sides)."""
        pts = [(x0, z0), (x1, z0), (x1, z1), (x0, z1)]
        self.wall(ident, pts, closed=True, tower_spacing=0, style=style, clear=1.5)
        self.keep_out(
            box(x0 + 0.5, z0 + 0.5, x1 - 0.5, z1 - 0.5)
        )  # the yard stays open ground
        if gate_side is None:
            return None  # a fold: no way in for anything but sheep
        gx, gz = {
            "s": ((x0 + x1) / 2, z1),
            "n": ((x0 + x1) / 2, z0),
            "w": (x0, (z0 + z1) / 2),
            "e": (x1, (z0 + z1) / 2),
        }[gate_side]
        self.gate(f"{ident}_gate", round(gx / 2) * 2, round(gz / 2) * 2)
        return (round(gx / 2) * 2, round(gz / 2) * 2)

    def troops(
        self, ident, kind, x, z, count=1, facing=0.0, spacing=7.0, cols=4, nation=None
    ):
        """Standing companies in ranks: a drill, a guard, a camp."""
        a = math.radians(facing)
        fx, fz = math.sin(a), math.cos(a)
        rx, rz = fz, -fx
        for k in range(count):
            col, row = k % cols, k // cols
            off = (col - (min(cols, count) - 1) / 2) * spacing
            px, pz = (
                x + rx * off - fx * row * spacing,
                z + rz * off - fz * row * spacing,
            )
            self.p.add(
                "spawn",
                type=kind,
                x=round(px, 1),
                z=round(pz, 1),
                id=f"{ident}_{k}",
                group=ident,
                **self.p.owner(),
                **({"nation": nation} if nation else {}),
            )

    def loop(self, pts, width, style="flagstone"):
        """A closed walk, laid as two halves meeting at both ends (a road whose start is
        its end does not survive the editor)."""
        pts = list(pts)
        if pts[0] != pts[-1]:
            pts.append(pts[0])
        h = len(pts) // 2
        self.street(pts[: h + 1], width, style, curve=False)
        self.street(pts[h:], width, style, curve=False)

    Town.loop = loop
    Town._set_line = _set_line
    Town.tidy_dead_ends = tidy_dead_ends
    Town.route = route
    Town.pool_garden = pool_garden
    Town.villa = villa
    Town.compound = compound
    Town.troops = troops


_city_life(Town)
