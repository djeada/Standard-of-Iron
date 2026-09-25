#!/usr/bin/env python3
"""Generate the Steam store screenshot missions from the meadow film map.

The store page must show the real game with its HUD, so every screenshot is
filmed by `scripts/capture-steam-screenshots.sh` through the game's --film
mode. This script writes the maps and missions it films. They are copies,
never edits of the shipped assets/maps:

  battle   two dense battle lines a short charge apart
  duel     Scipio and Hannibal with their escorts, close enough to cross blades
  winter   a Roman column walking into an Iron Sepulcher zone in deep snow,
           with grave priests in the opening wave
  oasis    a Carthaginian desert town around a palm-ringed lake: farms, homes
           and a temple already standing, builders raising a new quarter
  construction  the forest-town city with one quarter cleared and a dozen
           builder crews raising a dozen buildings at once

usage: tools/arena/promos/film/steam/generate_steam_fixtures.py
       python3 scripts/format.py --files tools/arena/promos/film/steam/*.json --fix

The JSON is written plainly; the second command puts it in the repository's
Prettier style, which CI checks.
"""

import copy
import json
import math
import random
from pathlib import Path

HERE = Path(__file__).resolve().parent
FILM = HERE.parent
MEADOW = json.loads((FILM / "meadow.map.json").read_text())
MISSION = json.loads((FILM / "commander.mission.json").read_text())
# Grid size of the map currently being generated; set by base_map().
GRID = 96


def grid(x: float, z: float) -> tuple[float, float]:
    """World metres to the map's grid coordinates."""
    offset = GRID / 2 - 0.5
    return round(x + offset, 1), round(z + offset, 1)


def spawn(ident, kind, x, z, player, nation):
    gx, gz = grid(x, z)
    entry = {
        "id": ident,
        "type": kind,
        "x": gx,
        "z": gz,
        "player_id": player,
        "team_id": player,
        "nation": nation,
    }
    if "commander" in kind or "consul" in kind:
        entry["max_population"] = -1
    return entry


def base_map(name: str, size: int = 96) -> dict:
    global GRID
    GRID = size
    data = copy.deepcopy(MEADOW)
    data["grid"] = {"width": size, "height": size, "tile_size": 1.0}
    data["name"] = name
    data["description"] = "Steam store screenshot fixture."
    data["skirmish_hidden"] = True
    data["max_troops_per_player"] = 20000
    data["spawns"] = []
    data["structures"] = []
    data["firecamps"] = []
    data["wildlife"] = []
    return data


def mission(
    ident: str, title: str, map_name: str, objective: str, *, ai_strategy="aggressive"
) -> dict:
    data = copy.deepcopy(MISSION)
    data["id"] = ident
    data["title"] = title
    data["map_path"] = map_name
    # The victory description is the objective banner at the top of the HUD,
    # which every screenshot shows.
    data["victory_conditions"][0]["description"] = objective
    ai = data["ai_setups"][0]
    ai["strategy"] = ai_strategy
    ai["posture"] = "field"
    ai["personality"] = {"aggression": 0.9, "defense": 0.4, "harassment": 0.2}
    return data


def battle() -> None:
    """Two hosts of 150 cohorts each: about 2,250 soldiers a side."""
    data = base_map("Steam: the clash of lines", 256)
    spawns = data["spawns"]
    columns, ranks = 30, 5
    for rank in range(ranks):
        for col in range(columns):
            x = -101.5 + col * 7
            kind = ("spearman", "swordsman", "swordsman", "spearman", "archer")[rank]
            depth = 7 + rank * 6
            spawns.append(
                spawn(f"rome_{rank}_{col}", kind, x, depth, 1, "roman_republic")
            )
            spawns.append(spawn(f"punic_{rank}_{col}", kind, x, -depth, 2, "carthage"))
    for side, z in ((1, 40), (2, -40)):
        nation = "roman_republic" if side == 1 else "carthage"
        for wing in (-1, 1):
            for col in range(8):
                x = wing * (110 - col * 6)
                spawns.append(
                    spawn(
                        f"horse_{side}_{wing}_{col}",
                        "horse_swordsman",
                        x,
                        z,
                        side,
                        nation,
                    )
                )
    # Behind each line: healers walking the ranks, then the siege train.
    for side, sign in ((1, 1), (2, -1)):
        nation = "roman_republic" if side == 1 else "carthage"
        for col in range(10):
            x = -63 + col * 14
            spawns.append(
                spawn(f"healer_{side}_{col}", "healer", x, sign * 34, side, nation)
            )
        for col in range(6):
            x = -75 + col * 30
            spawns.append(
                spawn(f"ballista_{side}_{col}", "ballista", x, sign * 40, side, nation)
            )
            spawns.append(
                spawn(
                    f"catapult_{side}_{col}",
                    "catapult",
                    x + 15,
                    sign * 45,
                    side,
                    nation,
                )
            )
    # Hannibal's war elephants on both Carthaginian wings.
    for wing in (-1, 1):
        for col in range(5):
            x = wing * (78 + col * 7)
            spawns.append(
                spawn(f"elephant_{wing}_{col}", "elephant", x, -24, 2, "carthage")
            )
    spawns.insert(
        0, spawn("rome_consul", "roman_veteran_consul", 0, 46, 1, "roman_republic")
    )
    spawns.insert(
        1, spawn("hannibal", "carthage_sword_commander", 0, -46, 2, "carthage")
    )
    for side, z, nation in ((1, 64, "roman_republic"), (2, -64, "carthage")):
        gx, gz = grid(0, z)
        data["structures"].append(
            {
                "type": "barracks",
                "x": gx,
                "z": gz,
                "player_id": side,
                "team_id": side,
                "nation": nation,
                "max_population": 20,
            }
        )
    centre = grid(0, 0)
    data["camera"] = {
        "center": [centre[0], 0, centre[1]],
        "distance": 60.0,
        "tilt_deg": 38.0,
        "yaw": 200.0,
        "fov_y": 45.0,
        "near": 1.0,
        "far": 600.0,
    }
    write(
        "battle",
        data,
        mission(
            "steam_battle",
            "Steam: the clash of lines",
            "battle.map.json",
            "Break the Carthaginian line.",
        ),
    )


def duel() -> None:
    """Scipio and Hannibal meet in the gap while 60 cohorts a side close in."""
    data = base_map("Steam: the duel", 192)
    spawns = data["spawns"]
    spawns.append(spawn("scipio", "roman_veteran_consul", 0, 3, 1, "roman_republic"))
    spawns.append(spawn("hannibal", "carthage_sword_commander", 0, -3, 2, "carthage"))
    for rank in range(3):
        for col in range(20):
            x = -66.5 + col * 7
            kind = ("swordsman", "spearman", "archer")[rank]
            depth = 12 + rank * 6
            spawns.append(
                spawn(f"rome_{rank}_{col}", kind, x, depth, 1, "roman_republic")
            )
            spawns.append(spawn(f"punic_{rank}_{col}", kind, x, -depth, 2, "carthage"))
    write(
        "duel",
        data,
        mission(
            "steam_duel", "Steam: the duel", "duel.map.json", "Defeat Hannibal Barca."
        ),
    )


def winter() -> None:
    data = base_map("Steam: the frozen dead", 160)
    mountain = json.loads(
        (FILM.parents[3] / "assets/maps/map_mountain.json").read_text()
    )
    # The whole alpine biome, not just its snow: the meadow's dense grass blades
    # would stand on top of the snow cover, which only the alpine ground draws.
    data["biome"] = copy.deepcopy(mountain["biome"])
    data["biome"]["snow_coverage"] = 0.95
    data["rain"] = {
        "enabled": True,
        "type": "snow",
        "intensity": 0.9,
        "cycle_duration": 600.0,
        "active_duration": 600.0,
        "fade_duration": 1.0,
        "wind_strength": 0.5,
        "wind_direction": 25.0,
    }
    data["environment"] = {
        "start_time": 15.2,
        "time_mode": "locked",
        "day_length_seconds": 2400.0,
        "lighting_profile": "alpine_clear",
    }
    zx, zz = grid(0, -14)
    data["undead_zones"] = [
        {
            "id": "steam_frozen_dead",
            "anchor_type": "ruins",
            "awaken_on": ["unit_enters_radius"],
            "radius": 22.0,
            "leash_radius": 60.0,
            "owner_id": 99,
            "team_id": 99,
            "x": zx,
            "z": zz,
            "fog_density": 0.3,
            "waves": [
                {
                    "trigger": "initial",
                    "units": {
                        "grave_priest": 6,
                        "skeleton_archer": 6,
                        "skeleton_swordsman": 12,
                    },
                },
                {
                    "trigger": "after_clear",
                    "units": {
                        "grave_priest": 4,
                        "skeleton_archer": 4,
                        "skeleton_swordsman": 10,
                    },
                },
            ],
        }
    ]
    spawns = data["spawns"]
    spawns.append(spawn("consul", "roman_veteran_consul", 0, 44, 1, "roman_republic"))
    for row in range(5):
        for col in range(8):
            kind = ("swordsman", "swordsman", "spearman", "spearman", "archer")[row]
            spawns.append(
                spawn(
                    f"rome_{row}_{col}",
                    kind,
                    -24.5 + col * 7,
                    14 + row * 6,
                    1,
                    "roman_republic",
                )
            )
    # The camp the AI side needs so the match does not end on turn one.
    spawns.append(spawn("far_camp", "carthage_sword_commander", 70, 70, 2, "carthage"))
    write(
        "winter",
        data,
        mission(
            "steam_winter",
            "Steam: the frozen dead",
            "winter.map.json",
            "Silence the frozen dead.",
            ai_strategy="defensive",
        ),
        undead=True,
    )


def write(name: str, map_data: dict, mission_data: dict, *, undead=False) -> None:
    if not undead:
        map_data.pop("undead_zones", None)
    mission_data["include_ambient_undead"] = False
    (HERE / f"{name}.map.json").write_text(json.dumps(map_data, indent=2) + "\n")
    (HERE / f"{name}.mission.json").write_text(
        json.dumps(mission_data, indent=2) + "\n"
    )
    print(f"wrote {name}.map.json, {name}.mission.json")


def town() -> None:
    """The forest-town trailer city (253 homes), with a real objective."""
    data = json.loads((FILM / "forest_town.map.json").read_text())
    story = json.loads((FILM / "forest_town.mission.json").read_text())
    story["id"] = "steam_town"
    story["title"] = "Steam: a city at work"
    story["map_path"] = "town.map.json"
    story["victory_conditions"][0]["description"] = "Grow the colony into a city."
    for condition in story.get("defeat_conditions", []):
        condition["description"] = "The city falls."
    (HERE / "town.action.json").write_text(
        (FILM / "forest_town.action.json").read_text()
    )
    write("town", data, story, undead=True)


FIXTURE_TEXT = {
    "summary": "A Steam store screenshot.",
    "teaching_goal": "Build the town.",
    "narrative_intent": "Show the town at work.",
    "historical_context": "Carthaginian North Africa.",
    "theme": "A town at work.",
}


def town_mission(ident: str, title: str, map_name: str, objective: str) -> dict:
    story = json.loads((FILM / "forest_town.mission.json").read_text())
    story.update(FIXTURE_TEXT)
    story["id"] = ident
    story["title"] = title
    story["map_path"] = map_name
    story["victory_conditions"][0]["description"] = objective
    for condition in story.get("defeat_conditions", []):
        condition["description"] = "The town falls."
    story["player_setup"]["starting_resources"] = {
        "gold": 4200,
        "food": 3600,
        "wood": 5200,
        "stone": 3800,
        "iron": 1500,
    }
    return story


def build_orders(sites, crews, start: float, step: float) -> list[dict]:
    """Select each crew with a small box and order its site, one after another.

    `sites` are (kind, x, z) in world metres and `crews` gives where each crew
    stands relative to its site. Returns film actions timed from `start`.
    """
    actions = []
    for index, ((kind, x, z), crew) in enumerate(zip(sites, crews, strict=False)):
        at = start + index * step
        cx, cz = x + crew[0], z + crew[1]
        actions += [
            act(at, "select_area_world", f"{cx - 3},{cz - 3},{cx + 3},{cz + 3}"),
            act(at + step * 0.35, "build_start_world", f"{kind},{x},{z}"),
            act(at + step * 0.7, "build_place_world", f"{x},{z}"),
        ]
    return actions


def hero_camera(at: float, view: str, zoom: float, yaw: float, pitch: float):
    """Move to the hero framing: centre, zoom in, then orbit to a low angle.

    Pitch is negative looking down; a positive orbit lifts the view toward the
    horizon until the RTS limit for that distance stops it.
    """
    return [
        act(at, "camera_look_at", view),
        act(at + 0.01, "camera_orbit", f"{yaw},{pitch}"),
        act(at + 0.4, "camera_zoom", str(zoom)),
        act(at + 1.2, "camera_orbit", f"0,{pitch}"),
    ]


def act(at: float, action: str, argument: str = "") -> dict:
    return {"at": round(at, 2), "action": action, "argument": argument}


def write_actions(name: str, actions: list[dict]) -> None:
    actions = sorted(actions, key=lambda a: a["at"])
    document = {
        "version": 1,
        "name": f"steam_{name}",
        "loop_seconds": 0.0,
        "required_coverage": ["build_panel"],
        "actions": actions,
    }
    (HERE / f"{name}.action.json").write_text(json.dumps(document, indent=2) + "\n")


class Town:
    """Grid-space authoring helpers for a hand-laid town map."""

    def __init__(self, seed: int) -> None:
        self.rng = random.Random(seed)
        self.structures: list[dict] = []
        self.spawns: list[dict] = []
        self.props: list[dict] = []
        self.fires: list[dict] = []
        self.roads: list[dict] = []
        self.occupied: list[tuple[float, float, float]] = []

    def free(self, x: float, z: float, radius: float) -> bool:
        return all(
            math.hypot(x - ox, z - oz) >= radius + orad
            for ox, oz, orad in self.occupied
        )

    def structure(self, kind, x, z, rotation=0.0, owner=1, footprint=3.6, **extra):
        entry = {
            "id": f"{kind}_{len(self.structures):03d}",
            "type": kind,
            "x": round(x, 2),
            "z": round(z, 2),
            "rotation": rotation,
            "player_id": owner,
            "team_id": owner,
        }
        entry.update(extra)
        self.structures.append(entry)
        self.occupied.append((x, z, footprint))

    def spawn(self, kind, x, z, owner=1, **extra):
        entry = {
            "id": f"{kind}_{len(self.spawns):03d}",
            "type": kind,
            "x": round(x, 2),
            "z": round(z, 2),
            "player_id": owner,
        }
        entry.update(extra)
        self.spawns.append(entry)

    def prop(self, kind, x, z, rotation=None, scale=1.0, solid=1.2):
        if rotation is None:
            rotation = self.rng.uniform(0, 360)
        self.props.append(
            {
                "id": f"{kind}_{len(self.props):03d}",
                "type": kind,
                "x": round(x, 2),
                "z": round(z, 2),
                "rotation": round(rotation, 1),
                "scale": scale,
            }
        )
        self.occupied.append((x, z, solid * scale))

    def fire(self, x, z, intensity=1.0, radius=3.0):
        self.fires.append(
            {
                "id": f"fire_{len(self.fires):02d}",
                "x": round(x, 2),
                "z": round(z, 2),
                "intensity": intensity,
                "radius": radius,
            }
        )
        self.occupied.append((x, z, 1.6))

    def road(self, points, width, style="default"):
        entry = {
            "id": f"street_{len(self.roads):02d}",
            "start": list(points[0]),
            "end": list(points[-1]),
            "width": width,
            "style": style,
        }
        if len(points) > 2:
            entry["waypoints"] = [list(p) for p in points]
        self.roads.append(entry)


# The oasis: a 200-tile desert map with a palm-ringed lake at its heart. Grid
# coordinates; world = grid - 99.5. The hero camera looks north-east across the
# lake at the temple, with the fields on the near shore and the new quarter the
# builders are raising on the east bank.
OASIS_SIZE = 200
OASIS_LAKE = (96.0, 104.0)
OASIS_LAKE_W, OASIS_LAKE_D, OASIS_LAKE_ROT = 42.0, 30.0, 25.0
# Construction sites for the builders, in world coordinates: (kind, x, z).
OASIS_SITES = [
    ("defense_tower", 32.0, -6.0),
    ("home", 48.0, -6.0),
    ("marketplace", 36.0, 12.0),
    ("home", 52.0, 12.0),
    ("home", 34.0, 28.0),
    ("home", 50.0, 28.0),
    ("temple", 44.0, -24.0),
]
# Field-hand crews, relative to the lake centre (grid tiles).
OASIS_FIELD_HANDS = [(-30.0, 18.0), (-2.0, 41.0), (-48.0, -2.0)]
OASIS_ORDER_VIEW = "40,2"
# Hero framing once the orders are out: look-at, zoom, orbit yaw and pitch.
OASIS_HERO = ("26,-2", 2.7, 0.0, 40.0)
# Hero camera: world target x, z, distance, tilt, yaw (yaw 45 sits south-east).
OASIS_CAMERA = (22.0, 2.0, 95.0, 42.0, 45.0)
# The film opens at 31.7 m; each -1 of camera_zoom widens it by 15 %.
OASIS_ZOOM = -12
# Where each crew stands at the start, relative to its site (world metres).
OASIS_CREW = (0.0, 7.5)


def oasis() -> None:
    town = Town(20260925)
    size = OASIS_SIZE
    off = size / 2 - 0.5
    lx, lz = OASIS_LAKE

    def lake_distance(x, z):
        """Normalised distance from the lake centre (1.0 = the shoreline)."""
        a = math.radians(OASIS_LAKE_ROT)
        dx, dz = x - lx, z - lz
        u = dx * math.cos(a) + dz * math.sin(a)
        v = -dx * math.sin(a) + dz * math.cos(a)
        return math.hypot(u / (OASIS_LAKE_W / 2), v / (OASIS_LAKE_D / 2))

    # Keep the water, and a shore walk around it, free of buildings.
    for i in range(72):
        t = i / 72 * math.tau
        a = math.radians(OASIS_LAKE_ROT)
        u, v = math.cos(t) * OASIS_LAKE_W / 2, math.sin(t) * OASIS_LAKE_D / 2
        town.occupied.append(
            (
                lx + u * math.cos(a) - v * math.sin(a),
                lz + u * math.sin(a) + v * math.cos(a),
                3.0,
            )
        )
    town.occupied.append((lx, lz, 12.0))

    # Keep the builders' ground clear: every site and its crew.
    for kind, wx, wz in OASIS_SITES:
        town.occupied.append((wx + off, wz + off, 9.0 if kind == "temple" else 6.5))

    # Palms: loose groves on the shore with gaps between them, so the water
    # shows through, then single trees wandering off into the sand.
    def shore_point(t, r):
        a = math.radians(OASIS_LAKE_ROT)
        u = math.cos(t) * OASIS_LAKE_W / 2 * r
        v = math.sin(t) * OASIS_LAKE_D / 2 * r
        return lx + u * math.cos(a) - v * math.sin(a), lz + u * math.sin(
            a
        ) + v * math.cos(a)

    groves = 10
    for g in range(groves):
        centre = (g + town.rng.uniform(-0.2, 0.2)) / groves * math.tau
        for i in range(town.rng.randint(4, 8)):
            t = centre + town.rng.gauss(0.0, 0.16)
            x, z = shore_point(t, town.rng.uniform(1.07, 1.4))
            town.props.append(
                {
                    "id": f"palm_grove_{g}_{i}",
                    "type": "palm_tree",
                    "x": round(x, 2),
                    "z": round(z, 2),
                    "rotation": round(town.rng.uniform(0, 360), 1),
                    "scale": round(town.rng.uniform(0.9, 1.35), 2),
                }
            )
    for i in range(16):
        x, z = shore_point(town.rng.uniform(0, math.tau), town.rng.uniform(1.45, 2.0))
        if town.free(x, z, 1.5):
            town.props.append(
                {
                    "id": f"palm_single_{i:02d}",
                    "type": "palm_tree",
                    "x": round(x, 2),
                    "z": round(z, 2),
                    "rotation": round(town.rng.uniform(0, 360), 1),
                    "scale": round(town.rng.uniform(0.9, 1.25), 2),
                }
            )

    # The temple on the north shore, looking over the water.
    town.structure("temple", lx + 4, lz - 30, 0, footprint=8.0)
    for dx in (-8, 8):
        town.fire(lx + 4 + dx, lz - 22, 1.05, 3.2)
        town.prop("statue", lx + 4 + dx * 1.3, lz - 36, 180.0, 1.0)

    # Irrigated fields on the south and west shores.
    for fx, fz, rot in (
        (lx - 42, lz + 8, 90),
        (lx - 42, lz + 26, 90),
        (lx - 24, lz + 30, 0),
        (lx - 6, lz + 32, 0),
        (lx + 12, lz + 32, 0),
        (lx - 40, lz - 12, 90),
        (lx - 24, lz + 48, 0),
        (lx - 6, lz + 50, 0),
        (lx + 12, lz + 50, 0),
    ):
        town.structure("farm", fx, fz, rot, footprint=8.5)
    # Field hands: builder crews harvesting, and villagers on the paths.
    for fx, fz in OASIS_FIELD_HANDS:
        town.spawn("builder", lx + fx, lz + fz)

    # The old town: courtyard homes on a street grid west and north of the lake.
    town.road([(lx - 70, lz - 44), (lx + 70, lz - 44)], 5.0)
    town.road([(lx - 60, lz - 70), (lx - 60, lz + 60)], 4.0)
    town.road([(lx + 60, lz - 70), (lx + 60, lz + 60)], 4.0)
    town.road([(lx - 60, lz + 40), (lx + 60, lz + 40)], 3.6)
    for bx, bz in (
        (lx - 40, lz - 60),
        (lx - 14, lz - 62),
        (lx + 28, lz - 62),
        (lx - 76, lz - 26),
        (lx - 76, lz + 4),
        (lx - 76, lz + 34),
        (lx + 54, lz - 62),
    ):
        town.prop("palm_tree", bx + 0.5, bz - 0.5, scale=1.1, solid=1.0)
        town.fire(bx + 2.5, bz + 1.0, 0.9, 2.6)
        for gx in (-10.0, -5.0, 0.0, 5.0, 10.0):
            for gz in (-8.0, -3.0, 2.0, 7.0):
                if abs(gx) < 1 and gz in (-3.0, 2.0):
                    continue
                x, z = bx + gx + town.rng.uniform(-0.4, 0.4), bz + gz
                facing = 0 if gz < 0 else 180
                if town.free(x, z, 2.1):
                    town.structure("home", x, z, facing, footprint=2.1)

    # Markets at the crossroads by the temple.
    for dx, rot in ((-20, 90), (26, 270)):
        town.structure("marketplace", lx + dx, lz - 46, rot, footprint=4.6)
        town.prop("supply_cart", lx + dx + 4, lz - 40, rot)

    # East bank: a finished street the new quarter continues.
    for z in range(-52, 54, 7):
        x = lx + 66
        if town.free(x, lz + z, 2.1):
            town.structure("home", x, lz + z, 270, footprint=2.1)
        if town.free(x + 7, lz + z, 2.1):
            town.structure("home", x + 7, lz + z, 90, footprint=2.1)

    # Barracks and towers: the key structure and the edges of the town.
    town.structure("barracks", lx + 70, lz + 60, 270, footprint=6.0, max_population=200)
    town.fire(lx + 64, lz + 67, 1.0, 3.0)
    for tx, tz in ((lx - 86, lz - 70), (lx + 84, lz - 72), (lx - 84, lz + 60)):
        town.structure("defense_tower", tx, tz, 0, footprint=3.0)

    # The new quarter's builders, each crew beside its site.
    for _kind, wx, wz in OASIS_SITES:
        town.spawn("builder", wx + off + OASIS_CREW[0], wz + off + OASIS_CREW[1])
    # Stone and timber stacked for them.
    for dx, dz in ((58, -22), (60, 30), (18, -22)):
        town.prop("supply_cart", dx + off, dz + off, None)
    for i in range(6):
        town.prop(
            "boulder",
            64 + off + town.rng.uniform(-3, 3),
            -30 + i * 2.5 + off,
            None,
            0.8,
        )

    # Villagers along the shore and in the streets.
    for i in range(22):
        t = i / 22 * math.tau
        a = math.radians(OASIS_LAKE_ROT)
        u, v = (
            math.cos(t) * OASIS_LAKE_W / 2 * 1.45,
            math.sin(t) * OASIS_LAKE_D / 2 * 1.45,
        )
        town.spawn(
            "civilian",
            lx + u * math.cos(a) - v * math.sin(a),
            lz + u * math.sin(a) + v * math.cos(a),
            behavior="guard",
            guard_radius=12.0,
        )
    for _ in range(12):
        town.spawn(
            "civilian",
            lx + town.rng.uniform(-60, 60),
            lz - 44 + town.rng.uniform(-2, 2),
            behavior="guard",
            guard_radius=16.0,
        )
    for t in (-50, -30, -10, 10, 30, 50):
        town.fire(lx + t, lz - 40, 0.85, 2.4)

    town.spawn("carthage_sword_commander", lx + 10, lz - 38)
    town.structure("barracks", 16, 184, 45, owner=2, footprint=6.0, max_population=60)
    town.spawn("roman_veteran_consul", 22, 178, owner=2, nation="roman_republic")

    forests = []
    for i in range(14):
        angle = i / 14 * math.tau
        radius = 88 + (i % 3) * 4
        forests.append(
            {
                "id": f"palm_belt_{i:02d}",
                "x": round(lx + math.cos(angle) * radius, 1),
                "z": round(lz + math.sin(angle) * radius, 1),
                "radius": 11.0 + (i % 3) * 2.0,
            }
        )

    document = {
        "name": "Steam: the oasis",
        "description": "Steam store screenshot fixture: a desert town around an oasis.",
        "theme": "Water, palms and a town that grows around them.",
        "skirmish_hidden": True,
        "coord_system": "grid",
        "grid": {"width": size, "height": size, "tile_size": 1.0},
        "max_troops_per_player": 900,
        # From the south-east: the new quarter in front, the lake beyond it,
        # the temple and the old town on the far shore.
        "camera": {
            "center": [OASIS_CAMERA[0] + off, 0, OASIS_CAMERA[1] + off],
            "distance": OASIS_CAMERA[2],
            "tilt_deg": OASIS_CAMERA[3],
            "yaw": OASIS_CAMERA[4],
            "fov_y": 45.0,
            "near": 1.0,
            "far": 600.0,
        },
        "time_of_day": "afternoon",
        "environment": {
            "start_time": 16.0,
            "time_mode": "locked",
            "day_length_seconds": 2400.0,
            "lighting_profile": "mediterranean_summer",
            "fog_density": 0.006,
        },
        "biome": {
            "ground_type": "grass_dry",
            "seed": 60925,
            "patch_density": 2.2,
            "patch_jitter": 0.86,
            "plant_density": 0.3,
            "grass_primary": [0.55, 0.49, 0.27],
            "grass_secondary": [0.62, 0.54, 0.31],
            "grass_dry": [0.74, 0.62, 0.40],
            "soil_color": [0.66, 0.51, 0.31],
            "rock_low": [0.58, 0.47, 0.33],
            "rock_high": [0.78, 0.68, 0.52],
            "height_noise": [0.16, 0.05],
            "ground_irregularity_enabled": True,
            "irregularity_scale": 0.12,
            "irregularity_amplitude": 0.06,
            "moisture_level": 0.08,
            "grass_saturation": 0.55,
            "sway_strength": 0.18,
            "sway_speed": 1.0,
            "procedural_boulders_enabled": False,
            "procedural_iron_ore_enabled": False,
            "tree_mix": {"pine": 0.0, "olive": 0.25, "cypress": 0.1, "palm": 3.0},
        },
        "rain": {"enabled": False},
        "terrain": [],
        "forests": forests,
        "rivers": [],
        "lakes": [
            {
                "id": "the_oasis",
                "x": lx,
                "z": lz,
                "width": OASIS_LAKE_W,
                "depth": OASIS_LAKE_D,
                "rotation": OASIS_LAKE_ROT,
            }
        ],
        "bridges": [],
        "roads": town.roads,
        "landmarks": [],
        "structures": town.structures,
        "spawns": town.spawns,
        "world_props": town.props,
        "firecamps": town.fires,
        "undead_zones": [],
        "wildlife": {
            "enabled": True,
            "seed": 60925,
            "near_simulation_radius": 60.0,
            "far_simulation_radius": 120.0,
            "sheep": {
                "enabled": True,
                "groups": 2,
                "group_size_min": 5,
                "group_size_max": 7,
                "roam_radius": 8.0,
                "respawn": False,
                "spawn_areas": [
                    {"x": lx - 60, "z": lz + 60, "radius": 6.0},
                    {"x": lx + 30, "z": lz + 64, "radius": 6.0},
                ],
            },
        },
        "starting_resources": {
            "gold": 4200,
            "food": 3600,
            "wood": 5200,
            "stone": 3800,
            "iron": 1500,
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
    story = town_mission(
        "steam_oasis",
        "Steam: the oasis",
        "oasis.map.json",
        "Grow the oasis into a city.",
    )
    story["terrain_type"] = "desert"
    write("oasis", document, story, undead=True)
    # The field hands start on the harvest, seen from over the lake.
    actions = [
        act(0.05, "camera_look_at", f"{lx - off},{lz - off}"),
        act(0.06, "camera_zoom", str(OASIS_ZOOM)),
    ]
    for index, (fx, fz) in enumerate(OASIS_FIELD_HANDS):
        wx, wz = fx + lx - off, fz + lz - off
        at = 0.3 + index * 0.3
        actions += [
            act(at, "select_area_world", f"{wx - 3},{wz - 3},{wx + 3},{wz + 3}"),
            act(at + 0.1, "auto_gather"),
        ]
    actions.append(act(1.3, "camera_look_at", OASIS_ORDER_VIEW))
    actions += build_orders(OASIS_SITES, [OASIS_CREW] * len(OASIS_SITES), 1.6, 0.6)
    actions += hero_camera(1.6 + len(OASIS_SITES) * 0.6 + 0.3, *OASIS_HERO)
    write_actions("oasis", actions)
    print(
        f"  oasis: {len(town.structures)} structures, {len(town.spawns)} spawns, "
        f"{len(town.props)} props"
    )


# The construction boom: the forest-town city with the two northern blocks of
# the west side cleared for a new quarter. World coordinates (forest town is
# 220 tiles; world = grid - 109.5). Twelve sites, each with its own crew.
BOOM_CLEAR = (-60.0, -60.0, -2.0, -32.0)
BOOM_SITES = [
    ("temple", -50.0, -51.0),
    ("defense_tower", -39.0, -52.0),
    ("marketplace", -21.0, -52.0),
    ("home", -11.0, -53.0),
    ("home", -54.0, -40.0),
    ("barracks", -43.0, -40.0),
    ("home", -21.0, -40.0),
    ("temple", -10.0, -40.0),
    ("home", -54.0, -23.0),
    ("marketplace", -43.0, -23.0),
    ("home", -20.0, -23.0),
    ("defense_tower", -10.0, -23.0),
]
BOOM_CAMERA = (-26.0, -32.0, 80.0, 42.0, 225.0)
BOOM_ZOOM = -10
BOOM_ORDER_VIEW = "-32,-38"
BOOM_HERO = ("-30,-34", 2.0, 0.0, 40.0)
# Each crew waits beside its site: north rows below it, the south row above.
BOOM_CREWS = [(0.0, 5.5)] * 8 + [(0.0, -5.0)] * 4


def construction() -> None:
    data = json.loads((FILM / "forest_town.map.json").read_text())
    off = data["grid"]["width"] / 2 - 0.5
    x0, z0, x1, z1 = BOOM_CLEAR
    z1_ = max(z for _k, _x, z in BOOM_SITES) + 6

    def cleared(entry) -> bool:
        wx, wz = entry["x"] - off, entry["z"] - off
        return x0 <= wx <= x1 and z0 <= wz <= z1_

    data["name"] = "Steam: the construction boom"
    data["description"] = "Steam store screenshot fixture: a new quarter rising."
    data["skirmish_hidden"] = True
    data["structures"] = [s for s in data["structures"] if not cleared(s)]
    data["world_props"] = [p for p in data["world_props"] if not cleared(p)]
    data["firecamps"] = [f for f in data["firecamps"] if not cleared(f)]
    data["spawns"] = [s for s in data["spawns"] if not cleared(s)]
    for index, ((_kind, wx, wz), (dx, dz)) in enumerate(
        zip(BOOM_SITES, BOOM_CREWS, strict=False)
    ):
        data["spawns"].append(
            {
                "id": f"boom_crew_{index:02d}",
                "type": "builder",
                "x": round(wx + off + dx, 2),
                "z": round(wz + off + dz, 2),
                "player_id": 1,
            }
        )
    for dx, dz in ((-58, -30), (-4, -30), (-30, -60)):
        data["world_props"].append(
            {
                "id": f"boom_cart_{dx}_{dz}",
                "type": "supply_cart",
                "x": dx + off,
                "z": dz + off,
                "rotation": 30.0,
                "scale": 1.0,
            }
        )
    # From the north-west: the new quarter in front, the city behind it.
    data["camera"] = {
        "center": [BOOM_CAMERA[0] + off, 0, BOOM_CAMERA[1] + off],
        "distance": BOOM_CAMERA[2],
        "tilt_deg": BOOM_CAMERA[3],
        "yaw": BOOM_CAMERA[4],
        "fov_y": 45.0,
        "near": 1.0,
        "far": 600.0,
    }
    data["starting_resources"] = {
        "gold": 4200,
        "food": 3600,
        "wood": 5200,
        "stone": 3800,
        "iron": 1500,
    }
    story = town_mission(
        "steam_construction",
        "Steam: the construction boom",
        "construction.map.json",
        "Raise the new quarter.",
    )
    write("construction", data, story, undead=True)
    actions = [
        act(0.05, "camera_look_at", BOOM_ORDER_VIEW),
        act(0.06, "camera_zoom", str(BOOM_ZOOM)),
    ]
    actions += build_orders(BOOM_SITES, BOOM_CREWS, 0.6, 0.5)
    actions += hero_camera(0.6 + len(BOOM_SITES) * 0.5 + 0.3, *BOOM_HERO)
    write_actions("construction", actions)


if __name__ == "__main__":
    oasis()
    construction()
    town()
    battle()
    duel()
    winter()
