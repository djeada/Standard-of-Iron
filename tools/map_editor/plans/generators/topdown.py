#!/usr/bin/env python3
"""topdown.py <map.json> <out.png> [scale] : quick plan sketch of a map (north up, east right)."""
import json, math, sys
from PIL import Image, ImageDraw

sys.path.insert(0, __import__("os").path.dirname(__file__))
from planlib import Water

m = json.load(open(sys.argv[1]))
S = float(sys.argv[3]) if len(sys.argv) > 3 else 3.0
W, H = m["grid"]["width"], m["grid"]["height"]
im = Image.new("RGB", (int(W * S), int(H * S)), (120, 150, 90))
d = ImageDraw.Draw(im)


def P(x, z):
    return ((x + W / 2) * S, (z + H / 2) * S)


# terrain rises
for t in m.get("terrain", []):
    if t.get("type") in ("flat", "hill", "mountain"):
        w, dp = t.get("width", t.get("radius", 10) * 2), t.get(
            "depth", t.get("radius", 10) * 2
        )
        x, z = t["x"], t["z"]
        d.ellipse(
            [P(x - w / 2, z - dp / 2), P(x + w / 2, z + dp / 2)],
            fill=(150, 160, 110) if t["type"] != "mountain" else (130, 120, 110),
        )
# lakes (sampled)
waters = [
    Water(
        l["x"],
        l["z"],
        l["width"],
        l["depth"],
        l.get("rotation", 0),
        l.get("shape", "natural"),
    )
    for l in m.get("lakes", [])
]
px = im.load()
step = max(1, int(S))
for j in range(0, im.height, 2):
    for i in range(0, im.width, 2):
        x, z = i / S - W / 2, j / S - H / 2
        if any(w.wet(x, z) for w in waters):
            for a in range(2):
                for b in range(2):
                    if i + a < im.width and j + b < im.height:
                        px[i + a, j + b] = (60, 110, 160)


def poly(pts, width, fill):
    for a, b in zip(pts, pts[1:]):
        d.line([P(*a), P(*b)], fill=fill, width=max(1, int(width * S)))


for r in m.get("rivers", []):
    pts = [r["start"]] + r.get("waypoints", []) + [r["end"]]
    poly(pts, r.get("width", 6), (60, 110, 160))
for r in m.get("roads", []):
    pts = [r["start"]] + r.get("waypoints", []) + [r["end"]]
    col = {"flagstone": (214, 204, 180), "stone": (150, 148, 140)}.get(
        r.get("style"), (170, 150, 110)
    )
    poly(pts, r.get("width", 3), col)
for r in m.get("bridges", []):
    poly([r["start"], r["end"]], r.get("width", 8), (200, 200, 195))
SIZE = {
    "home": (4.3, 4.4),
    "marketplace": (5.5, 5.5),
    "temple": (12.4, 9.2),
    "farm": (13.6, 13.6),
    "barracks": (4, 4),
    "defense_tower": (3, 3),
    "wall_gate": (6, 2),
}
COL = {
    "home": (196, 96, 60),
    "tenement": (196, 96, 60),
    "portico": (232, 170, 90),
    "basilica": (240, 140, 60),
    "dock": (120, 80, 50),
    "temple": (245, 245, 235),
    "farm": (190, 180, 90),
    "barracks": (140, 60, 60),
    "defense_tower": (90, 90, 100),
    "wall_gate": (60, 60, 70),
}
n = {}
for s in m.get("structures", []):
    t = s["type"]
    if t == "wall_segment":
        poly([s["start"], s["end"]], 1.6, (70, 70, 80))
        continue
    w, dp = SIZE.get(t, (4, 4))
    a = math.radians(s.get("rotation", 0))
    c, sn = math.cos(a), math.sin(a)
    pts = []
    for u, v in (
        (-w / 2, -dp / 2),
        (w / 2, -dp / 2),
        (w / 2, dp / 2),
        (-w / 2, dp / 2),
    ):
        pts.append(P(s["x"] + c * u + sn * v, s["z"] - sn * u + c * v))
    key = s.get("style") or t
    n[key] = n.get(key, 0) + 1
    d.polygon(pts, fill=COL.get(key, (200, 0, 200)), outline=(60, 40, 30))
    # front tick
    fx, fz = s["x"] + sn * dp / 2, s["z"] + c * dp / 2
    d.line([P(s["x"], s["z"]), P(fx, fz)], fill=(30, 30, 30), width=1)
for q in m.get("world_props", []):
    col = {
        "statue": (255, 255, 255),
        "cypress_tree": (30, 80, 40),
        "olive_tree": (90, 120, 70),
    }.get(q["type"], (60, 100, 50))
    x, y = P(q["x"], q["z"])
    d.ellipse([x - 1.5, y - 1.5, x + 1.5, y + 1.5], fill=col)
for f in m.get("forests", []):
    x, z, r = f["x"], f["z"], f.get("radius", 10)
    d.ellipse([P(x - r, z - r), P(x + r, z + r)], outline=(40, 90, 50), width=2)
im.save(sys.argv[2])
print(n, "total", sum(n.values()))
