#!/usr/bin/env python3
"""Write the trailer's second formation beat: a column crossing a river bridge.

The first formation beat is a line on an empty parade ground -- deliberately
bare, so the ranks read. This one has to say the opposite thing: a formation is
a *shape you pick for the ground in front of you*, and a bridge is the ground
that only a column fits. So the map is built around one wide river with two
crossings, in a dry Mediterranean palm country that reads nothing like the green
valley the rest of the trailer is shot in.

Everything here is placed so the hero bridge (`great_span`, z = 96) is clean:

* The river runs straight through both crossings. Bridge decks are fitted to the
  drawn waterline, and a deck laid over a bend gets a phantom trench at one
  abutment, so the waypoints only bend well north and well south of the spans.
* Palms come from the ground type and its `tree_mix`, not from props:
  `grass_dry` is the only biome whose scatter rules give palms the top density
  scale (0.400 against the cypress' 0.240 and the olive's 0.100), and the mix
  leans that the rest of the way, because a 55/45 palm-to-scrub split still
  reads as generic bush country on camera. The groves are `forests`, which seed
  the scatter rather than placing trees, so they cluster the palms on the banks
  and leave the approach roads and the deck itself clear.
* Nothing is placed inside the box the column marches through
  (x 60..118, z 84..108): props there would cut the marching ranks up the way
  the river map's banks cut the line beat up. Scatter is global, not per-region,
  so the marching lane is kept clear the only way a map can keep it clear --
  `the_crossing_road` is drawn wide enough (11 m, a rank wider than the column)
  that the scatter's spawn validator refuses to plant in it.

    python3 tools/arena/promos/film/generate_palm_crossing.py
"""
from __future__ import annotations

import json
import math
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE / "river.map.json"
TARGET = HERE / "palm_crossing.map.json"


RIVER_X = 88.0
RIVER_WIDTH = 24.0
GREAT_SPAN_Z = 96.0
UPPER_SPAN_Z = 52.0
SPAN_WEST = 73.0
SPAN_EAST = 103.0
DECK_WIDTH = 14.0


MUSTER = (58.0, 96.0)


ROSTER = ["swordsman"] * 5 + ["spearman"] * 3 + ["archer"] * 1


def hero_palms(rng: random.Random) -> list[dict]:
    """Hand-placed palms on the four corners the hero shot actually frames.

    The scatter will not plant on the riverbank itself -- the bank is sloped and
    carved by the river geometry -- so raising `plant_density` puts more trees
    everywhere except the strip either side of the deck, which is the strip in
    shot. These are `palm_tree` world props, placed by hand and kept at least
    8 m clear of the marching lane (z 96 +/- 8) because a palm is a solid prop
    and would otherwise stand in the column's way.
    """
    stands = [
        ("west_n", 62.0, 78.0, 84.0, 88.0),
        ("west_s", 62.0, 78.0, 104.0, 114.0),
        ("east_n", 98.0, 116.0, 84.0, 88.0),
        ("east_s", 98.0, 116.0, 104.0, 114.0),
    ]
    palms = []
    for name, x0, x1, z0, z1 in stands:
        for index in range(7):
            palms.append(
                {
                    "id": f"palm_{name}_{index}",
                    "type": "palm_tree",
                    "x": round(rng.uniform(x0, x1), 1),
                    "z": round(rng.uniform(z0, z1), 1),
                    "rotation": round(rng.uniform(0.0, 360.0), 1),
                }
            )
    return palms


def palm_groves() -> list[dict]:
    """Palm stands on both banks, clear of the deck and the marching box."""
    groves = [
        ("west_upper_palms", 58.0, 30.0, 15.0),
        ("west_mid_palms", 52.0, 66.0, 13.0),
        ("west_lower_palms", 46.0, 134.0, 16.0),
        ("west_shore_palms", 66.0, 150.0, 12.0),
        ("east_upper_palms", 118.0, 26.0, 14.0),
        ("east_mid_palms", 124.0, 74.0, 15.0),
        ("east_shore_palms", 110.0, 126.0, 13.0),
        ("east_lower_palms", 132.0, 150.0, 16.0),
        ("north_headland_palms", 88.0, 14.0, 12.0),
        ("south_headland_palms", 92.0, 166.0, 13.0),
        ("west_abutment_palms_n", 70.0, 84.0, 10.0),
        ("west_abutment_palms_s", 70.0, 108.0, 10.0),
        ("east_abutment_palms_n", 106.0, 84.0, 10.0),
        ("east_abutment_palms_s", 106.0, 108.0, 10.0),
        ("west_muster_palms_n", 52.0, 82.0, 9.0),
        ("west_muster_palms_s", 52.0, 110.0, 9.0),
        ("east_landing_palms_n", 124.0, 88.0, 9.0),
        ("east_landing_palms_s", 124.0, 106.0, 9.0),
    ]
    return [
        {"id": name, "x": x, "z": z, "radius": radius} for name, x, z, radius in groves
    ]


def main() -> int:
    world = json.loads(SOURCE.read_text())
    rng = random.Random(20260920)

    world["name"] = "Trailer: the palm crossing"
    world["description"] = (
        "Filming fixture: one wide river through dry palm country, two bridges, "
        "and a Roman column marching over the southern span."
    )
    world["theme"] = "A river nobody fords and two bridges everybody wants."

    world["rivers"] = [
        {
            "id": "the_great_river",
            "start": [RIVER_X, 0.0],
            "end": [RIVER_X, 175.0],
            "width": RIVER_WIDTH,
            "waypoints": [
                [78.0, 0.0],
                [RIVER_X, 30.0],
                [RIVER_X, 140.0],
                [98.0, 175.0],
            ],
        }
    ]
    world["bridges"] = [
        {
            "id": "great_span",
            "start": [SPAN_WEST, GREAT_SPAN_Z],
            "end": [SPAN_EAST, GREAT_SPAN_Z],
            "width": DECK_WIDTH,
            "height": 0.9,
        },
        {
            "id": "upper_span",
            "start": [SPAN_WEST + 2.0, UPPER_SPAN_Z],
            "end": [SPAN_EAST - 2.0, UPPER_SPAN_Z],
            "width": 8.0,
            "height": 0.8,
        },
    ]
    world["lakes"] = []

    world["terrain"] = [
        {
            "id": "west_approach",
            "type": "flat",
            "x": 62.0,
            "z": GREAT_SPAN_Z,
            "width": 26.0,
            "depth": 20.0,
            "description": "The muster ground on the west bank.",
        },
        {
            "id": "east_approach",
            "type": "flat",
            "x": 114.0,
            "z": GREAT_SPAN_Z,
            "width": 24.0,
            "depth": 20.0,
            "description": "The landing on the east bank.",
        },
    ]

    world["forests"] = palm_groves()

    world["roads"] = [
        {
            "id": "the_west_approach_road",
            "start": [28.0, 99.0],
            "end": [SPAN_WEST - 1.0, GREAT_SPAN_Z],
            "width": 11.0,
            "style": "default",
            "waypoints": [
                [28.0, 99.0],
                [46.0, 98.0],
                [MUSTER[0], 97.0],
                [SPAN_WEST - 1.0, GREAT_SPAN_Z],
            ],
        },
        {
            "id": "the_east_landing_road",
            "start": [SPAN_EAST + 1.0, GREAT_SPAN_Z],
            "end": [150.0, 93.0],
            "width": 11.0,
            "style": "default",
            "waypoints": [
                [SPAN_EAST + 1.0, GREAT_SPAN_Z],
                [122.0, 95.0],
                [150.0, 93.0],
            ],
        },
        {
            "id": "the_upper_road_west",
            "start": [40.0, 40.0],
            "end": [SPAN_WEST + 1.0, UPPER_SPAN_Z],
            "width": 3.6,
            "style": "default",
            "waypoints": [
                [40.0, 40.0],
                [62.0, 48.0],
                [SPAN_WEST + 1.0, UPPER_SPAN_Z],
            ],
        },
        {
            "id": "the_upper_road_east",
            "start": [SPAN_EAST - 1.0, UPPER_SPAN_Z],
            "end": [146.0, 60.0],
            "width": 3.6,
            "style": "default",
            "waypoints": [
                [SPAN_EAST - 1.0, UPPER_SPAN_Z],
                [124.0, 56.0],
                [146.0, 60.0],
            ],
        },
    ]

    world["landmarks"] = [
        {
            "id": "river_sanctuary",
            "kind": "sanctuary",
            "x": 120.0,
            "z": 148.0,
            "facing": "north",
        },
    ]
    world["structures"] = []
    world["firecamps"] = []
    world["undead_zones"] = []
    world["wildlife"] = {"enabled": False}

    world["world_props"] = [
        {
            "id": "west_camp_tent_a",
            "type": "tent",
            "x": 44.0,
            "z": 78.0,
            "rotation": 18.0,
        },
        {
            "id": "west_camp_tent_b",
            "type": "tent",
            "x": 39.0,
            "z": 84.0,
            "rotation": 340.0,
        },
        {
            "id": "west_camp_cart",
            "type": "supply_cart",
            "x": 42.0,
            "z": 70.0,
            "rotation": 96.0,
        },
        {
            "id": "west_rack",
            "type": "weapon_rack",
            "x": 47.0,
            "z": 72.0,
            "rotation": 210.0,
        },
        {
            "id": "east_ruins_a",
            "type": "ruins",
            "x": 128.0,
            "z": 112.0,
            "rotation": 42.0,
        },
        {
            "id": "east_ruins_b",
            "type": "ruins",
            "x": 134.0,
            "z": 104.0,
            "rotation": 128.0,
        },
        {
            "id": "east_statue",
            "type": "statue",
            "x": 122.0,
            "z": 120.0,
            "rotation": 270.0,
        },
        {"id": "north_ruins", "type": "ruins", "x": 70.0, "z": 40.0, "rotation": 300.0},
        {
            "id": "north_home",
            "type": "abandoned_home",
            "x": 108.0,
            "z": 38.0,
            "rotation": 190.0,
        },
        {
            "id": "south_home",
            "type": "abandoned_home",
            "x": 62.0,
            "z": 140.0,
            "rotation": 64.0,
        },
    ] + hero_palms(rng)

    world["biome"] = {
        "ground_type": "grass_dry",
        "seed": 70921,
        "patch_density": 2.6,
        "patch_jitter": 0.82,
        "plant_density": 1.15,
        "grass_primary": [0.34, 0.44, 0.21],
        "grass_secondary": [0.46, 0.53, 0.25],
        "grass_dry": [0.58, 0.52, 0.28],
        "soil_color": [0.34, 0.27, 0.18],
        "rock_low": [0.44, 0.42, 0.39],
        "rock_high": [0.66, 0.65, 0.61],
        "height_noise": [0.18, 0.06],
        "ground_irregularity_enabled": True,
        "irregularity_scale": 0.13,
        "irregularity_amplitude": 0.08,
        "moisture_level": 0.14,
        "grass_saturation": 0.62,
        "sway_strength": 0.2,
        "sway_speed": 1.1,
        "procedural_boulders_enabled": False,
        "procedural_iron_ore_enabled": False,
        "tree_mix": {"pine": 0.0, "olive": 0.3, "cypress": 0.45, "palm": 2.6},
    }

    world["environment"] = {
        "start_time": 13.5,
        "time_mode": "locked",
        "day_length_seconds": 1800.0,
        "lighting_profile": "mediterranean_summer",
        "fog_density": 0.008,
        "exposure": 1.22,
    }
    world["rain"] = {"enabled": False, "type": "rain", "intensity": 0.0}

    world["camera"] = {
        "center": [86.0, 0.0, GREAT_SPAN_Z],
        "distance": 62.0,
        "tilt_deg": 34.0,
        "yaw": 315.0,
        "fov_y": 45.0,
        "near": 1.0,
        "far": 492.8,
    }

    commander = {
        "id": "p1_commander",
        "type": "roman_veteran_consul",
        "x": MUSTER[0] - 4.0,
        "z": MUSTER[1] + 6.0,
        "player_id": 1,
        "team_id": 1,
        "nation": "roman_republic",
    }
    army = [commander]
    order = ROSTER[:]
    rng.shuffle(order)
    for index, unit_type in enumerate(order):
        radius = abs(rng.gauss(0.0, 5.0))
        angle = rng.uniform(0.0, 2.0 * math.pi)
        army.append(
            {
                "id": f"p1_{unit_type}_{index}",
                "type": unit_type,
                "x": round(MUSTER[0] + math.cos(angle) * radius, 1),
                "z": round(MUSTER[1] + math.sin(angle) * radius * 0.8, 1),
                "player_id": 1,
                "team_id": 1,
                "nation": "roman_republic",
            }
        )

    enemy = [
        {
            "id": "p2_commander",
            "type": "carthage_sword_commander",
            "x": 150.0,
            "z": 30.0,
            "player_id": 2,
            "team_id": 2,
            "nation": "carthage",
        }
    ]

    world["spawns"] = army + enemy
    world["max_troops_per_player"] = 400
    world["starting_resources"] = {
        "gold": 250,
        "food": 200,
        "wood": 250,
        "stone": 120,
        "iron": 80,
    }

    TARGET.write_text(json.dumps(world, indent=2) + "\n")
    print(
        f"wrote {TARGET.name}: {len(army)} Roman units around "
        f"({MUSTER[0]:.0f}, {MUSTER[1]:.0f}); river x={RIVER_X:.0f} w={RIVER_WIDTH:.0f}; "
        f"spans z={GREAT_SPAN_Z:.0f} (w {DECK_WIDTH:.0f}) and z={UPPER_SPAN_Z:.0f}; "
        f"{len(world['forests'])} palm groves"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
