#!/usr/bin/env python3
"""Sprinkle cursed gold veins across the shipped maps.

A vein is contested ground: it goes where two players' bases are about equally far
away, on open flat land clear of water, roads, hills, settlements, camps, props and
undead zones. The script is deterministic and idempotent - it removes any existing
`cursed_gold_vein` props before placing, so re-running re-lays the same sites.
`tests/systems/cursed_gold_vein_system_test.cpp` proves every shipped site is clear
with the engine's own clearance check.

Usage: python3 scripts/place-cursed-gold-veins.py [--dry-run] [--reject FILE] [maps...]

The JSON heuristic cannot see runtime scatter (trees, boulders) or slope-forbidden
cells, so the loop is: place, run the test, feed the sites it rejects back through
`--reject FILE` (a JSON object of map name -> [[x, z], ...]) and place again.
`scripts/cursed_gold_vein_rejects.json` holds the sites the shipped layout had to
step around; keep it when re-running so the layout stays stable.
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MAPS = REPO / "assets" / "maps"
SKIPPED = {"map_tutorial.json"}


def seg_dist(px, pz, ax, az, bx, bz):
    dx, dz = bx - ax, bz - az
    if dx == 0 and dz == 0:
        return math.hypot(px - ax, pz - az)
    t = max(0.0, min(1.0, ((px - ax) * dx + (pz - az) * dz) / (dx * dx + dz * dz)))
    return math.hypot(px - (ax + t * dx), pz - (az + t * dz))


def polyline_dist(px, pz, feature):
    pts = [feature["start"]] + list(feature.get("waypoints", [])) + [feature["end"]]
    return min(
        seg_dist(px, pz, a[0], a[1], b[0], b[1])
        for a, b in zip(pts, pts[1:], strict=False)
    )


def rect_dist(px, pz, cx, cz, w, d, rot_deg):
    r = math.radians(-rot_deg)
    lx = (px - cx) * math.cos(r) - (pz - cz) * math.sin(r)
    lz = (px - cx) * math.sin(r) + (pz - cz) * math.cos(r)
    gx = abs(lx) - w * 0.5
    gz = abs(lz) - d * 0.5
    return (
        math.hypot(max(gx, 0.0), max(gz, 0.0)) if (gx > 0 or gz > 0) else -min(-gx, -gz)
    )


def vein_count(size):
    if size <= 100:
        return 1
    if size <= 200:
        return 2
    if size <= 320:
        return 3
    if size <= 700:
        return 4
    return 5


def blockers(d):
    b = []
    for r in d.get("rivers", []):
        b.append(
            lambda px, pz, r=r: polyline_dist(px, pz, r) - r.get("width", 6) * 0.5 - 7
        )
    for r in d.get("roads", []):
        b.append(
            lambda px, pz, r=r: polyline_dist(px, pz, r) - r.get("width", 4) * 0.5 - 4
        )
    for br in d.get("bridges", []):
        b.append(
            lambda px, pz, br=br: seg_dist(px, pz, *br["start"], *br["end"])
            - br.get("width", 8)
            - 6
        )
    for lake in d.get("lakes", []):
        b.append(
            lambda px, pz, lake=lake: rect_dist(
                px, pz, lake["x"], lake["z"], lake["width"], lake["depth"], 0
            )
            - 8
        )
    for t in d.get("terrain", []):
        if t.get("type") in ("hill", "mountain"):
            margin = 8 if t["type"] == "mountain" else 4
            if "radius" in t:
                b.append(
                    lambda px, pz, t=t, m=margin: math.hypot(px - t["x"], pz - t["z"])
                    - t["radius"]
                    - m
                )
            else:
                b.append(
                    lambda px, pz, t=t, m=margin: rect_dist(
                        px,
                        pz,
                        t["x"],
                        t["z"],
                        t["width"],
                        t["depth"],
                        t.get("rotation", 0),
                    )
                    - m
                )
    for s in d.get("settlements", []):
        b.append(lambda px, pz, s=s: math.hypot(px - s["x"], pz - s["z"]) - 48)
    for s in d.get("structures", []):
        if "x" in s and "z" in s:
            b.append(lambda px, pz, s=s: math.hypot(px - s["x"], pz - s["z"]) - 16)
    for s in d.get("landmarks", []):
        b.append(lambda px, pz, s=s: math.hypot(px - s["x"], pz - s["z"]) - 18)
    for s in d.get("spawns", []):
        b.append(lambda px, pz, s=s: math.hypot(px - s["x"], pz - s["z"]) - 12)
    for f in d.get("forests", []):
        b.append(
            lambda px, pz, f=f: math.hypot(px - f["x"], pz - f["z"])
            - f.get("radius", 10)
            - 3
        )
    for z in d.get("undead_zones", []):
        b.append(
            lambda px, pz, z=z: math.hypot(px - z["x"], pz - z["z"])
            - z.get("radius", 8)
            - 10
        )
    for c in d.get("firecamps", []):
        b.append(
            lambda px, pz, c=c: math.hypot(px - c["x"], pz - c["z"])
            - c.get("radius", 3)
            - 6
        )
    for p in d.get("world_props", []):
        if p["type"] == "cursed_gold_vein":
            continue
        reach = 4 + 2.5 * p.get("scale", 1.0)
        b.append(
            lambda px, pz, p=p, reach=reach: math.hypot(px - p["x"], pz - p["z"])
            - reach
        )
    return b


def player_anchors(d):
    anchors = {}
    for s in d.get("settlements", []) + d.get("structures", []) + d.get("spawns", []):
        pid = s.get("player_id")
        if pid is None or pid < 0 or "x" not in s or "z" not in s:
            continue
        anchors.setdefault(pid, []).append((s["x"], s["z"]))
    return anchors


REJECT_RADIUS = 9.0


def place(path, dry_run, rejects):
    text = path.read_text()
    d = json.loads(text)
    indent = 4 if text.startswith("{\n    ") else 2
    w, h = d["grid"]["width"], d["grid"]["height"]
    props = [p for p in d.get("world_props", []) if p["type"] != "cursed_gold_vein"]
    d["world_props"] = props
    want = vein_count(max(w, h))
    checks = blockers(d)
    anchors = player_anchors(d)
    margin = max(10, int(min(w, h) * 0.08))
    step = max(2, min(w, h) // 60)

    candidates = []
    for gx in range(margin, w - margin, step):
        for gz in range(margin, h - margin, step):
            clearance = min(c(gx, gz) for c in checks) if checks else 1e9
            if clearance < 0:
                continue
            if any(math.hypot(gx - rx, gz - rz) < REJECT_RADIUS for rx, rz in rejects):
                continue
            dists = sorted(
                min(math.hypot(gx - ax, gz - az) for ax, az in pts)
                for pts in anchors.values()
            )
            if not dists:
                contest = 0.0
            elif len(dists) == 1:
                contest = abs(dists[0] - 0.3 * max(w, h))
            else:
                contest = abs(dists[0] - dists[1]) + 0.15 * abs(
                    dists[0] - 0.22 * max(w, h)
                )
            candidates.append((contest, -clearance, gx, gz))
    candidates.sort()

    chosen = []
    spacing = max(w, h) / (want + 1.5)
    for _, _, gx, gz in candidates:
        if all(math.hypot(gx - cx, gz - cz) >= spacing for cx, cz in chosen):
            chosen.append((gx, gz))
        if len(chosen) >= want:
            break

    for gx, gz in chosen:
        rotation = round(((gx * 7 + gz * 13) % 628) / 100.0, 2)
        props.append(
            {
                "type": "cursed_gold_vein",
                "x": gx,
                "z": gz,
                "scale": 1.0,
                "rotation": rotation,
            }
        )

    print(f"{path.name}: {len(chosen)}/{want} veins at {chosen}")
    if not dry_run:
        path.write_text(json.dumps(d, indent=indent, ensure_ascii=False) + "\n")
    return len(chosen) == want


def main(argv):
    dry_run = "--dry-run" in argv
    rejects_by_map = {}
    if "--reject" in argv:
        reject_path = Path(argv[argv.index("--reject") + 1])
        if reject_path.exists():
            rejects_by_map = json.loads(reject_path.read_text())
        argv = [a for a in argv if a != str(reject_path)]
    names = [a for a in argv if not a.startswith("--")]
    paths = [MAPS / n for n in names] if names else sorted(MAPS.glob("map_*.json"))
    ok = True
    for path in paths:
        if path.name in SKIPPED:
            print(f"{path.name}: skipped")
            continue
        ok &= place(path, dry_run, rejects_by_map.get(path.name, []))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
