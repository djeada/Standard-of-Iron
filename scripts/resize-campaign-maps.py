#!/usr/bin/env python3
"""Resize campaign map grids and all dependent authored coordinates.

The operation is dry-run by default. It scales map JSON and the matching mission
setup together so units, structures, waves, terrain, roads, and water retain the
same authored relationship on the larger battlefield.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any, Sequence


MAPS = (
    (
        "assets/maps/map_crossing_rhone.json",
        "assets/missions/crossing_the_rhone.json",
        650,
    ),
    (
        "assets/maps/map_crossing_alps.json",
        "assets/missions/crossing_the_alps.json",
        650,
    ),
    (
        "assets/maps/map_battle_ticino.json",
        "assets/missions/battle_of_ticino.json",
        650,
    ),
    (
        "assets/maps/map_battle_trebia.json",
        "assets/missions/battle_of_trebia.json",
        650,
    ),
    (
        "assets/maps/map_battle_cannae.json",
        "assets/missions/battle_of_cannae.json",
        650,
    ),
    (
        "assets/maps/map_campania_campaign.json",
        "assets/missions/campania_campaign.json",
        650,
    ),
    ("assets/maps/map_battle_zama.json", "assets/missions/battle_of_zama.json", 800),
)


def number(value: float) -> int | float:
    rounded = round(value, 2)
    return int(rounded) if abs(rounded - round(rounded)) < 1.0e-7 else rounded


def scale_point(
    point: Sequence[float], scale_x: float, scale_z: float
) -> list[int | float]:
    return [number(float(point[0]) * scale_x), number(float(point[1]) * scale_z)]


def scale_xz_object(item: dict[str, Any], scale_x: float, scale_z: float) -> None:
    if isinstance(item.get("x"), (int, float)) and isinstance(
        item.get("z"), (int, float)
    ):
        item["x"] = number(float(item["x"]) * scale_x)
        item["z"] = number(float(item["z"]) * scale_z)


def scale_map(definition: dict[str, Any], target: int) -> tuple[float, float]:
    grid = definition["grid"]
    old_width, old_height = int(grid["width"]), int(grid["height"])
    if old_width == target and old_height == target:
        return 1.0, 1.0
    scale_x = (target - 1) / max(old_width - 1, 1)
    scale_z = (target - 1) / max(old_height - 1, 1)
    scale_mean = math.sqrt(scale_x * scale_z)
    grid["width"] = target
    grid["height"] = target

    camera = definition.get("camera") or {}
    if isinstance(camera.get("center"), list) and len(camera["center"]) >= 3:
        camera["center"][0] = number(float(camera["center"][0]) * scale_x)
        camera["center"][2] = number(float(camera["center"][2]) * scale_z)
    if isinstance(camera.get("distance"), (int, float)):
        camera["distance"] = number(
            max(float(camera["distance"]) * scale_mean, target * 0.42)
        )
    camera["far"] = max(float(camera.get("far", 0.0)), target * 2.25)

    for feature in definition.get("terrain") or []:
        scale_xz_object(feature, scale_x, scale_z)
        if isinstance(feature.get("width"), (int, float)):
            feature["width"] = number(float(feature["width"]) * scale_x)
        if isinstance(feature.get("depth"), (int, float)):
            feature["depth"] = number(float(feature["depth"]) * scale_z)
        if isinstance(feature.get("radius"), (int, float)):
            feature["radius"] = number(float(feature["radius"]) * scale_mean)
        for entrance in feature.get("entrances") or []:
            scale_xz_object(entrance, scale_x, scale_z)

    for lake in definition.get("lakes") or []:
        scale_xz_object(lake, scale_x, scale_z)
        if isinstance(lake.get("width"), (int, float)):
            lake["width"] = number(float(lake["width"]) * scale_x)
        if isinstance(lake.get("depth"), (int, float)):
            lake["depth"] = number(float(lake["depth"]) * scale_z)
        if isinstance(lake.get("radius"), (int, float)):
            lake["radius"] = number(float(lake["radius"]) * scale_mean)

    for key in ("rivers", "roads", "bridges", "walls"):
        for feature in definition.get(key) or []:
            for point_key in ("start", "end"):
                if isinstance(feature.get(point_key), list):
                    feature[point_key] = scale_point(
                        feature[point_key], scale_x, scale_z
                    )
            if isinstance(feature.get("waypoints"), list):
                feature["waypoints"] = [
                    scale_point(point, scale_x, scale_z)
                    for point in feature["waypoints"]
                ]
            if key == "rivers" and isinstance(feature.get("width"), (int, float)):
                feature["width"] = number(max(3.0, float(feature["width"]) * 1.4))
            elif key == "roads" and isinstance(feature.get("width"), (int, float)):
                multiplier = 1.25 if float(feature["width"]) >= 3.4 else 1.18
                feature["width"] = number(
                    max(3.0, float(feature["width"]) * multiplier)
                )
            elif key == "bridges" and isinstance(feature.get("width"), (int, float)):
                feature["width"] = number(max(4.0, float(feature["width"]) * 1.25))

    for key in ("buildings", "spawns", "firecamps", "world_props", "undead_zones"):
        for item in definition.get(key) or []:
            scale_xz_object(item, scale_x, scale_z)
            if key == "undead_zones":
                for radius_key in ("radius", "leash_radius"):
                    if isinstance(item.get(radius_key), (int, float)):
                        item[radius_key] = number(float(item[radius_key]) * scale_mean)

    return scale_x, scale_z


def scale_mission(value: Any, scale_x: float, scale_z: float) -> None:
    if isinstance(value, dict):
        scale_xz_object(value, scale_x, scale_z)
        for child in value.values():
            scale_mission(child, scale_x, scale_z)
    elif isinstance(value, list):
        for child in value:
            scale_mission(child, scale_x, scale_z)


def process(
    repo: Path, map_relative: str, mission_relative: str, target: int, write: bool
) -> None:
    map_path, mission_path = repo / map_relative, repo / mission_relative
    map_definition = json.loads(map_path.read_text(encoding="utf-8"))
    mission_definition = json.loads(mission_path.read_text(encoding="utf-8"))
    old_grid = dict(map_definition["grid"])
    scale_x, scale_z = scale_map(map_definition, target)
    if scale_x == 1.0 and scale_z == 1.0:
        print(f"{map_relative}: already {target}x{target}")
        return
    scale_mission(mission_definition, scale_x, scale_z)
    print(
        f"{map_relative}: {old_grid['width']}x{old_grid['height']} -> "
        f"{target}x{target} (x {scale_x:.3f}, z {scale_z:.3f})"
    )
    if write:
        map_path.write_text(
            json.dumps(map_definition, indent=2) + "\n", encoding="utf-8"
        )
        mission_path.write_text(
            json.dumps(mission_definition, indent=2) + "\n", encoding="utf-8"
        )


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--write", action="store_true", help="write resized map and mission JSON"
    )
    args = parser.parse_args(argv)
    repo = Path(__file__).resolve().parents[1]
    for map_relative, mission_relative, target in MAPS:
        process(repo, map_relative, mission_relative, target, args.write)
    if not args.write:
        print("dry-run: pass --write to update files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
