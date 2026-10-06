"""Map-level fields of a freshly generated map: grid, biome, camera, rules."""

from __future__ import annotations

import copy
from typing import Any

from .schema import load_biomes

ARRAYS = (
    "terrain",
    "lakes",
    "rivers",
    "roads",
    "bridges",
    "forests",
    "settlements",
    "landmarks",
    "dressing",
    "structures",
    "spawns",
    "world_props",
)

VICTORY = {
    "defeat_conditions": ["no_commander", "only_commander_remaining"],
    "key_structures": ["barracks"],
    "min_count": 1,
    "type": "capture_structures",
}
STARTING_RESOURCES = {"food": 200, "gold": 300, "iron": 150, "stone": 50, "wood": 100}


def fresh_map(width: int, height: int, params: dict[str, Any]) -> dict[str, Any]:
    biome_key = str(params["general.biome"])
    template = copy.deepcopy(load_biomes()[biome_key])
    biome = template["biome"]
    roughness = float(params["terrain.roughness"])
    noise = biome.get("height_noise")
    if isinstance(noise, list) and len(noise) == 2:
        scale = 0.4 + 1.2 * roughness
        biome["height_noise"] = [
            round(float(noise[0]) * scale, 3),
            round(float(noise[1]) * scale, 3),
        ]
    extent = max(width, height)
    definition: dict[str, Any] = {
        "name": "Generated battlefield",
        "coord_system": "grid",
        "grid": {"width": width, "height": height, "tile_size": 1},
        "biome": biome,
        "environment": template["environment"],
        "rain": template["rain"],
        "wildlife": template["wildlife"],
        "camera": {
            "center": [round(width * 0.5, 2), 0, round(height * 0.5, 2)],
            "distance": round(extent * 0.42, 1),
            "far": round(extent * 2.25, 1),
            "fov_y": 45,
            "near": 1,
            "tilt_deg": 48,
            "yaw": 225,
        },
        "victory": copy.deepcopy(VICTORY),
        "starting_resources": dict(STARTING_RESOURCES),
        "max_troops_per_player": 2000,
    }
    for key in ARRAYS:
        definition[key] = []
    return definition
