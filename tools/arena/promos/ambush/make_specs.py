#!/usr/bin/env python3
"""Write the capture spec for the night-ambush short (cine_ambush).

    make_specs.py [--preview]   preview: 24 fps, no supersampling, for framing checks
"""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
NIGHT = {"ambient_scale": 1.5, "sun_scale": 1.3, "exposure": 1.05}


def world(name, start, dur, keys, **extra):
    shot = {
        "name": name,
        "scenario": "cine_ambush",
        "start": start,
        "duration": dur,
        "rig": "free",
        "eye_space": "world",
        "look_space": "world",
        "focus": {"mode": "point", "point": [0, 0, 0]},
        "camera": [
            {"time": t, "eye": e, "look": target, "fov": fov}
            for t, e, target, fov in keys
        ],
        "ground_clearance": 0.3,
        "lighting": NIGHT,
    }
    shot.update(extra)
    return shot


def follow(name, group, start, dur, keys, **extra):
    shot = {
        "name": name,
        "scenario": "cine_ambush",
        "start": start,
        "duration": dur,
        "rig": "free",
        "ground_clearance": 0.3,
        "focus": {
            "mode": "group",
            "group": group,
            "smoothing": 0.35,
            "spring": True,
            "dead_zone": 0.4,
        },
        "camera": [
            {"time": t, "eye": e, "look": target, "fov": fov}
            for t, e, target, fov in keys
        ],
        "lighting": NIGHT,
    }
    shot.update(extra)
    return shot


SHOTS = [
    world(
        "road_torches",
        1.0,
        10.0,
        [
            (0, [4.5, 0.8, 3.0], [0, 1.0, 26], 26),
            (10, [4.0, 0.8, 1.0], [0, 1.0, 20], 26),
        ],
    ),
    follow(
        "consul_watch",
        "consul",
        3.0,
        8.0,
        [
            (0, [3.4, 1.5, -4.6], [0, 0.8, 0], 28),
            (8, [3.9, 1.5, -4.0], [0, 0.8, 0], 28),
        ],
    ),
    world(
        "barrow_mist",
        1.0,
        7.0,
        [
            (0, [-3.0, 0.6, 9.0], [-14, 1.2, -3], 22),
            (7, [-4.0, 0.6, 7.5], [-14, 1.2, -3], 21),
        ],
    ),
    world(
        "west_rising",
        5.5,
        8.0,
        [
            (0, [-6.0, 0.55, 6.5], [-14, 0.9, -2], 30),
            (8, [-6.5, 0.55, 5.5], [-14, 0.9, -2], 30),
        ],
    ),
    world(
        "east_rising",
        5.5,
        8.0,
        [
            (0, [8.5, 0.9, 4.5], [14, 0.9, -6], 34),
            (8, [9.0, 0.9, 3.8], [14, 0.9, -6], 34),
        ],
    ),
    world(
        "slope_volley",
        15.5,
        9.0,
        [
            (0, [-27.0, 2.0, 11.0], [-4, 0.8, 8], 32),
            (9, [-26.0, 2.0, 10.0], [-4, 0.8, 7], 32),
        ],
    ),
    world(
        "melee_tele",
        17.0,
        12.0,
        [
            (0, [12.0, 1.6, 12.0], [-1, 0.2, 6], 18),
            (12, [11.0, 1.6, 11.0], [-1, 0.2, 6], 17),
        ],
    ),
    follow(
        "consul_charge",
        "consul",
        19.0,
        12.0,
        [
            (0, [-1.0, 1.15, 3.0], [0, 0.7, -1], 26),
            (12, [1.4, 1.1, 2.8], [0, 0.7, -1], 25),
        ],
        handheld={"degrees": 0.3, "frequency": 0.45},
    ),
    world(
        "east_charge",
        25.0,
        14.0,
        [
            (0, [-9.0, 0.8, 15.0], [12, 0.9, 20], 24),
            (14, [-8.0, 0.8, 13.0], [6, 0.9, 16], 24),
        ],
    ),
    world(
        "crane_battle",
        30.0,
        14.0,
        [
            (0, [22.0, 14.0, 30.0], [-3, 0, 8], 34),
            (14, [15.0, 8.0, 22.0], [-3, 0, 8], 32),
        ],
    ),
    follow(
        "consul_fight",
        "consul",
        34.0,
        12.0,
        [
            (0, [3.8, 1.5, -3.8], [0, 0.7, 0], 30),
            (12, [4.4, 1.5, -3.0], [0, 0.7, 0], 30),
        ],
        handheld={"degrees": 0.3, "frequency": 0.45},
    ),
    world(
        "rear_threat",
        44.0,
        14.0,
        [
            (0, [5.0, 0.9, 14.0], [-6, 0.6, 32], 18),
            (14, [4.0, 0.9, 13.0], [-7, 0.6, 27], 18),
        ],
    ),
    world(
        "last_stand",
        50.0,
        11.0,
        [
            (0, [7.0, 2.6, -9.0], [-4, 0.6, 9], 30),
            (11, [10.0, 5.0, -14.0], [-4, 0.6, 9], 32),
        ],
    ),
]


def main():
    preview = "--preview" in sys.argv
    spec = {
        "width": 1920,
        "height": 1080,
        "fps": 24 if preview else 96,
        "supersample": 1 if preview else 2,
        "gameplay_ui": False,
        "audio": not preview,
        "record_music": False,
        "motion_limits": {
            "yaw_degrees_per_second": 40,
            "pitch_degrees_per_second": 20,
            "fov_degrees_per_second": 20,
            "minimum_clip_seconds": 1.0,
            "mean_clip_seconds": 1.0,
        },
        "id": "ambush_preview" if preview else "cine_ambush",
        "title": "THE NIGHT ROAD",
        "end_card": "steam_demo",
        "shots": SHOTS,
    }
    out = HERE / ("capture_ambush_preview.json" if preview else "capture_ambush.json")
    out.write_text(json.dumps(spec, indent=2) + "\n")
    print(out)


if __name__ == "__main__":
    main()
