#!/usr/bin/env python3
"""Generate forest_town.map.json, the trailer's "a city at work" film map.

A Carthaginian town in a wide forest clearing, filmed in the real game so the
footage carries the HUD: housing blocks on a street grid, an agora of four
markets, a temple, watchtowers, farms, and three work yards (lumber, quarry,
iron) whose builders haul loads to the barracks beside them. Campfires line
the streets for the afternoon light. The enemy camp exists only so the match
has an opponent; it sits in the far corner behind the forest.

    python3 tools/arena/promos/film/generate_forest_town.py
"""
from __future__ import annotations

import json
import math
import random
from pathlib import Path

OUT = Path(__file__).resolve().parent / "forest_town.map.json"
SIZE = 220
CX = CZ = 110.0
PLAYER = 1
ENEMY = 2

rng = random.Random(20260916)
structures: list[dict] = []
spawns: list[dict] = []
props: list[dict] = []
firecamps: list[dict] = []
roads: list[dict] = []
occupied: list[tuple[float, float, float]] = []


def free(x: float, z: float, radius: float) -> bool:
    return all(math.hypot(x - ox, z - oz) >= radius + orad for ox, oz, orad in occupied)


def structure(
    kind: str,
    x: float,
    z: float,
    rotation: float,
    owner: int = PLAYER,
    footprint: float = 3.6,
    **extra,
) -> None:
    ident = f"{kind}_{len(structures):03d}"
    entry = {
        "id": ident,
        "type": kind,
        "x": round(x, 2),
        "z": round(z, 2),
        "rotation": rotation,
        "player_id": owner,
        "team_id": owner,
    }
    entry.update(extra)
    structures.append(entry)
    occupied.append((x, z, footprint))


def spawn(kind: str, x: float, z: float, owner: int = PLAYER, **extra) -> None:
    entry = {
        "id": f"{kind}_{len(spawns):03d}",
        "type": kind,
        "x": round(x, 2),
        "z": round(z, 2),
        "player_id": owner,
    }
    entry.update(extra)
    spawns.append(entry)


def prop(
    kind: str, x: float, z: float, rotation: float = 0.0, scale: float = 1.0
) -> None:
    props.append(
        {
            "id": f"{kind}_{len(props):03d}",
            "type": kind,
            "x": round(x, 2),
            "z": round(z, 2),
            "rotation": round(rotation, 1),
            "scale": scale,
        }
    )
    occupied.append((x, z, 1.2 * scale))


def fire(x: float, z: float, intensity: float = 1.0, radius: float = 3.0) -> None:
    firecamps.append(
        {
            "id": f"fire_{len(firecamps):02d}",
            "x": round(x, 2),
            "z": round(z, 2),
            "intensity": intensity,
            "radius": radius,
        }
    )
    occupied.append((x, z, 1.6))


def road(
    x0: float, z0: float, x1: float, z1: float, width: float, style: str = "stone"
) -> None:
    roads.append(
        {
            "id": f"street_{len(roads):02d}",
            "start": [x0, z0],
            "end": [x1, z1],
            "width": width,
            "style": style,
        }
    )


road(CX - 64, CZ, CX + 64, CZ, 6.0)
road(CX, CZ - 60, CX, CZ + 60, 6.0)
for offset in (-30.0, 30.0):
    road(CX - 56, CZ + offset, CX + 56, CZ + offset, 3.6, "default")
    road(CX + offset, CZ - 52, CX + offset, CZ + 52, 3.6, "default")


for dx, dz, rot in ((-11, -9, 0), (11, -9, 0), (-11, 9, 180), (11, 9, 180)):
    structure("marketplace", CX + dx, CZ + dz, rot, footprint=4.6)
for dx, dz in ((-5, -4), (5, 4), (-5, 4), (5, -4)):
    fire(CX + dx, CZ + dz, 1.05, 3.2)
for dx, dz, rot in ((-17, 0, 90), (17, 0, 270)):
    prop("supply_cart", CX + dx, CZ + dz + 3.5, rot)


for bx in (-45.0, -15.0, 15.0, 45.0):
    for bz in (-45.0, -15.0, 15.0, 45.0):
        if abs(bx) == 15.0 and abs(bz) == 15.0:
            continue
        garden_x, garden_z = CX + bx, CZ + bz
        if bx == 45.0 and bz == 45.0:

            fire(garden_x - 12, garden_z - 12, 0.9, 2.6)
            fire(garden_x + 12, garden_z + 12, 0.9, 2.6)
            prop("supply_cart", garden_x - 11, garden_z + 10, 45.0)
            continue
        prop(
            rng.choice(("olive_tree", "cypress_tree")),
            garden_x - 1.5,
            garden_z,
            rng.uniform(0, 360),
            1.05,
        )
        fire(garden_x + 1.8, garden_z + 0.5, 0.9, 2.6)
        for gx in (-11.0, -5.5, 0.0, 5.5, 11.0):
            for gz in (-11.0, -5.5, 0.0, 5.5, 11.0):
                if abs(gx) < 1.0 and abs(gz) < 1.0:
                    continue
                x, z = garden_x + gx, garden_z + gz
                facing = 0 if gz < 0 else (180 if gz > 0 else (90 if gx < 0 else 270))
                if free(x, z, 2.2):
                    structure("home", x, z, facing, footprint=2.2)


structure("temple", CX + 22, CZ - 22, 225, footprint=7.0)
for dx, dz in ((16, -16), (28, -16), (16, -28)):
    fire(CX + dx, CZ + dz, 1.1, 3.4)


for sx, sz in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
    structure("defense_tower", CX + sx * 62, CZ + sz * 58, 0, footprint=3.0)


yards = {
    "lumber": ((CX - 60, CZ + 2), 90),
    "quarry": ((CX + 2, CZ - 58), 0),
    "mine": ((CX + 60, CZ - 2), 270),
}
for _name, ((x, z), rotation) in yards.items():
    structure("barracks", x, z, rotation, footprint=6.0, max_population=200)
    fire(x + 6, z + 7, 1.0, 3.0)

for i in range(16):
    prop(
        "pine_tree" if i % 2 else "olive_tree",
        CX - 90 + rng.uniform(-3, 3),
        CZ - 22 + i * 3.2,
        rng.uniform(0, 360),
        1.15,
    )
for i in range(12):
    prop(
        "boulder",
        CX - 16 + i * 3.0,
        CZ - 88 + rng.uniform(-2, 2),
        rng.uniform(0, 360),
        1.2,
    )
for i in range(10):
    prop(
        "iron_ore",
        CX + 90 + rng.uniform(-2, 2),
        CZ - 16 + i * 3.2,
        rng.uniform(0, 360),
        1.05,
    )


for t in range(-54, 55, 12):
    if abs(t) < 8:
        continue
    prop("cypress_tree", CX + t, CZ - 5.2, 0.0, 0.95)
    prop("olive_tree", CX + 5.2, CZ + t, rng.uniform(0, 360), 0.95)


for i in range(4):
    structure("farm", CX - 36 + i * 24, CZ + 70, 0, footprint=9.0)


for name, ((x, z), _rotation) in yards.items():
    out_x = -18 if name == "lumber" else (18 if name == "mine" else 0)
    out_z = -18 if name == "quarry" else 0
    for _i in range(6):
        spawn("builder", x + out_x + rng.uniform(-5, 5), z + out_z + rng.uniform(-5, 5))
for i in range(4):
    spawn("builder", CX - 36 + i * 24, CZ + 60)
for i in range(4):
    spawn("builder", CX + 33 + i * 3.0, CZ + 32)


for i in range(18):
    angle = i / 18 * math.tau
    spawn(
        "civilian",
        CX + math.cos(angle) * 14,
        CZ + math.sin(angle) * 12,
        behavior="guard",
        guard_radius=14.0,
    )
for _i in range(16):
    spawn(
        "civilian",
        CX + rng.uniform(-50, 50),
        CZ + rng.choice((-30, 30)) + rng.uniform(-2, 2),
        behavior="guard",
        guard_radius=18.0,
    )


for t in (-50, -38, -26, 26, 38, 50):
    fire(CX + t, CZ + 4.5, 0.95, 2.8)
    fire(CX - 4.5, CZ + t, 0.95, 2.8)
    fire(CX + t, CZ + 30 + 3.8, 0.85, 2.4)
    fire(CX + t, CZ - 30 - 3.8, 0.85, 2.4)


spawn("carthage_sword_commander", CX + 2, CZ + 16)
structure("barracks", 16, 16, 45, owner=ENEMY, footprint=6.0, max_population=60)
spawn("roman_veteran_consul", 22, 22, owner=ENEMY)


forests = []
for i in range(16):
    angle = i / 16 * math.tau
    radius = 96 + (i % 3) * 5
    forests.append(
        {
            "id": f"ring_{i:02d}",
            "x": round(CX + math.cos(angle) * radius, 1),
            "z": round(CZ + math.sin(angle) * radius, 1),
            "radius": 16.0 + (i % 4) * 2.0,
        }
    )
for i, (x, z) in enumerate(
    ((CX - 84, CZ - 50), (CX + 84, CZ + 50), (CX - 60, CZ + 88), (CX + 60, CZ - 90))
):
    forests.append({"id": f"grove_{i}", "x": x, "z": z, "radius": 12.0})

document = {
    "name": "Forest Town (trailer film)",
    "description": "Trailer film fixture: a Carthaginian town at work in a forest clearing.",
    "theme": "forest",
    "coord_system": "grid",
    "grid": {"width": SIZE, "height": SIZE, "tile_size": 1.0},
    "max_troops_per_player": 900,
    "camera": {
        "center": [CX, 0, CZ],
        "distance": 150.0,
        "tilt_deg": 50.0,
        "yaw": 225.0,
        "fov_y": 45.0,
        "near": 1.0,
        "far": 600.0,
    },
    "time_of_day": "afternoon",
    "environment": {
        "start_time": 15.6,
        "time_mode": "locked",
        "day_length_seconds": 2400.0,
        "lighting_profile": "mediterranean_summer",
    },
    "biome": {
        "ground_type": "forest_mud",
        "seed": 51844,
        "patch_density": 5.2,
        "patch_jitter": 0.9,
        "plant_density": 1.05,
        "grass_primary": [0.16, 0.33, 0.17],
        "grass_secondary": [0.23, 0.42, 0.21],
        "grass_dry": [0.42, 0.36, 0.22],
        "soil_color": [0.14, 0.11, 0.08],
        "height_noise": [0.18, 0.06],
        "ground_irregularity_enabled": True,
        "irregularity_scale": 0.14,
        "irregularity_amplitude": 0.06,
    },
    "rain": {"enabled": False},
    "terrain": [
        {
            "id": "town_clearing",
            "type": "flat",
            "x": CX,
            "z": CZ,
            "width": 190.0,
            "depth": 190.0,
            "description": "The town floor.",
        }
    ],
    "forests": forests,
    "rivers": [],
    "lakes": [],
    "bridges": [],
    "roads": roads,
    "landmarks": [],
    "structures": structures,
    "spawns": spawns,
    "world_props": props,
    "firecamps": firecamps,
    "undead_zones": [],
    "wildlife": {
        "enabled": True,
        "seed": 51844,
        "near_simulation_radius": 60.0,
        "far_simulation_radius": 120.0,
        "sheep": {
            "enabled": True,
            "groups": 2,
            "group_size_min": 5,
            "group_size_max": 7,
            "roam_radius": 10.0,
            "respawn": False,
            "spawn_areas": [
                {"x": CX - 20, "z": CZ + 84, "radius": 8.0},
                {"x": CX + 30, "z": CZ + 84, "radius": 8.0},
            ],
        },
    },
    "starting_resources": {
        "gold": 3000,
        "food": 2000,
        "wood": 2500,
        "stone": 1800,
        "iron": 900,
    },
    "victory": {
        "type": "elimination",
        "key_structures": ["barracks"],
        "defeat_conditions": [
            "no_key_structures",
            "no_commander",
            "only_commander_remaining",
        ],
    },
}
OUT.write_text(json.dumps(document, indent=2) + "\n")
print(
    f"wrote {OUT.name}: {len(structures)} structures, {len(spawns)} spawns, "
    f"{len(props)} props, {len(firecamps)} fires, {len(forests)} forests"
)
