#!/usr/bin/env python3
"""Write the formation film's map: one big Roman army standing in a loose mob.

The trailer's formation beat has to read as a rabble becoming a line, so the
army starts scattered -- no ranks, no shared facing, types interleaved -- and it
stands on an empty plain. The river map's banks, roads, woods and hill cut the
ranks up and hide the moment the line lands, so everything but the ground is
stripped out here, procedural scatter included: the first cut of this beat had a
pair of cypresses and a boulder standing in the middle of the deployment because
the biome generates its own props even when every feature array is empty.

The mob is deliberately built as knots rather than an even cloud. An even
scatter of forty squads reads as a texture; three or four clumps with stragglers
between them reads as a crowd that has not been told what to do yet, which is
the "before" the beat needs.

    python3 tools/arena/promos/film/generate_formation_field.py
"""
from __future__ import annotations

import json
import math
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE / "river.map.json"
TARGET = HERE / "formation_field.map.json"


CENTRE = (88.0, 100.0)


KNOTS = (
    (-16.0, -5.0, 5.5, 0.30),
    (2.0, 3.0, 6.5, 0.32),
    (17.0, -3.0, 5.0, 0.22),
    (0.0, 0.0, 15.0, 0.16),
)


ROSTER = (
    ["swordsman"] * 16 + ["spearman"] * 11 + ["archer"] * 8 + ["horse_swordsman"] * 4
)


def main() -> int:
    world = json.loads(SOURCE.read_text())
    rng = random.Random(20260918)

    kept = []
    for spawn in world["spawns"]:
        if spawn.get("player_id") == 1:
            continue

        spawn = dict(spawn)
        spawn["x"] = min(168.0, float(spawn["x"]) + 60.0)
        spawn["z"] = max(8.0, float(spawn["z"]) - 35.0)
        kept.append(spawn)
    commander = next(
        spawn for spawn in world["spawns"] if spawn.get("id") == "p1_commander"
    )
    commander = dict(commander)
    commander["x"] = CENTRE[0] - 3.0
    commander["z"] = CENTRE[1] - 9.0
    army = [commander]

    def place() -> tuple[float, float]:
        roll = rng.random()

        for dx, dz, sigma, share in KNOTS:
            roll -= share
            if roll <= 0.0:
                break
        radius = abs(rng.gauss(0.0, sigma))
        angle = rng.uniform(0.0, 2.0 * math.pi)
        return (
            CENTRE[0] + dx + math.cos(angle) * radius,
            CENTRE[1] + dz + math.sin(angle) * radius * 0.7,
        )

    order = ROSTER[:]
    rng.shuffle(order)
    for index, unit_type in enumerate(order):
        x, z = place()
        army.append(
            {
                "id": f"p1_{unit_type}_{index}",
                "type": unit_type,
                "x": round(min(124.0, max(52.0, x)), 1),
                "z": round(min(126.0, max(74.0, z)), 1),
                "player_id": 1,
                "team_id": 1,
                "nation": "roman_republic",
            }
        )

    for field in (
        "terrain",
        "forests",
        "rivers",
        "lakes",
        "bridges",
        "roads",
        "landmarks",
        "structures",
        "world_props",
        "firecamps",
    ):
        world[field] = []
    world["wildlife"] = {"enabled": False}

    world["terrain"] = [
        {
            "id": "parade_ground",
            "type": "flat",
            "x": 88.0,
            "z": 96.0,
            "width": 148.0,
            "depth": 110.0,
            "description": "The field the army forms up on.",
        }
    ]

    world["environment"]["fog_density"] = 0.005
    world["environment"]["exposure"] = 1.32

    world["biome"]["plant_density"] = 0.0
    world["biome"]["procedural_trees_enabled"] = False
    world["biome"]["procedural_boulders_enabled"] = False
    world["biome"]["procedural_iron_ore_enabled"] = False
    world["biome"]["patch_density"] = 1.25
    world["biome"]["moisture_level"] = 0.55
    world["biome"]["grass_saturation"] = 0.74
    world["biome"]["grass_primary"] = [0.24, 0.38, 0.20]
    world["biome"]["grass_secondary"] = [0.33, 0.45, 0.24]
    world["name"] = "Trailer: the forming field"
    world["description"] = "Filming fixture: a loose army that forms a line."

    world["spawns"] = army + kept
    TARGET.write_text(json.dumps(world, indent=2) + "\n")
    xs = [s["x"] for s in army]
    zs = [s["z"] for s in army]
    print(
        f"wrote {TARGET.name}: {len(army)} Roman units over "
        f"x {min(xs):.0f}..{max(xs):.0f}, z {min(zs):.0f}..{max(zs):.0f}; "
        f"{len(kept)} other spawns"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
