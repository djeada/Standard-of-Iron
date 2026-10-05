#!/usr/bin/env python3
"""Write the commander duel reel specs in this directory from one table.

Each reel is one duel_reel_* arena scenario filmed start to finish: two lines
of dialogue, the walk out, then the beats the duel director plays -- clash,
parried string, knockback finisher, standoff, slipped thrust, launcher juggle
and the last exchange. A stage below lists when those beats happen in that
scenario and which way the pair is facing at the time, so every shot can be
framed side-on or down the line of the fight.

Run from the repo root. ``--preview`` writes half-size specs for a fast look.
"""

import argparse
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent

HEADER = {
    "fps": 30,
    "supersample": 1,
    "gameplay_ui": False,
    "audio": True,
    "record_music": False,
    "transition": {"type": "cut"},
    "grade": {
        "contrast": 1.10,
        "saturation": 1.08,
        "grain": 0,
        "vignette": 0.16,
        "sharpen": 0.4,
    },
    "motion_limits": {
        "yaw_degrees_per_second": 40,
        "pitch_degrees_per_second": 16,
        "fov_degrees_per_second": 18,
        "minimum_clip_seconds": 1.2,
        "mean_clip_seconds": 1.6,
    },
    "title": "STANDARD OF IRON",
    "end_card": "steam_demo",
    "end_card_background": "dim",
}


def key(time, distance, pitch, yaw, fov=42, height=1.2, ease=None, roll=0.0):
    result = {
        "time": time,
        "distance": distance,
        "pitch": pitch,
        "yaw": yaw % 360,
        "fov": fov,
        "height": height,
    }
    if roll:
        result["roll"] = roll
    if ease:
        result["ease"] = ease
    return result


ORBIT = 0.4


def move(duration, start, end, ease="smooth"):
    """Two keys; yaw is unwrapped so the camera takes the short way round.

    The authored sweep is centred and scaled by ``ORBIT``: a duel is two
    bodies moving fast in a small frame, and a camera that also swings round
    them leaves the eye nothing still to read the blades against.
    """
    delta = ((end[2] - start[2] + 180) % 360 - 180) * ORBIT
    middle = start[2] + ((end[2] - start[2] + 180) % 360 - 180) * 0.5
    first = key(0, start[0], start[1], middle - delta * 0.5, *start[3:])
    last = key(duration, end[0], end[1], middle + delta * 0.5, *end[3:], ease=ease)
    last["yaw"] = first["yaw"] + delta
    return [first, last]


def one(name, smoothing=0.25, offset=None):
    focus = {"mode": "group", "group": name, "smoothing": smoothing}
    if offset:
        focus["offset"] = offset
    return focus


def both(stage, smoothing=0.3):
    return {
        "mode": "group_pair",
        "group": stage["first"],
        "second_group": stage["second"],
        "smoothing": smoothing,
    }


def turn(a, b):
    """Signed shortest turn from yaw ``a`` to yaw ``b``."""
    return (b - a + 180) % 360 - 180


def side(stage, axis):
    """Yaw square to a fight running along ``axis``, from the lit side.

    A fight has two flanks to film from; the one nearer the stage's
    ``light_yaw`` keeps the key light on the faces and the stage's backdrop
    behind them.
    """
    flanks = ((axis + 90) % 360, (axis + 270) % 360)
    return min(flanks, key=lambda yaw: abs(turn(stage["light_yaw"], yaw)))


def behind(axis, who, swing=0):
    """Yaw that looks down the fight from behind one fighter."""
    return (axis + (180 if who == "first" else 0) + swing) % 360


def shot(stage, name, start, duration, focus, camera, **extra):
    result = {
        "name": name,
        "scenario": stage["scenario"],
        "seed": 7,
        "start": round(start, 2),
        "duration": round(duration, 2),
        "focus": focus,
        "camera": camera,
    }
    result.update(extra)
    return result


def reel(stage):
    s = stage
    t = s["beats"]
    a = s["axis"]
    opener, replier = s["opener"], s["replier"]
    shots = []

    start, length = 0.3, 2.4
    yaw = behind(a["open"], opener["side"], opener.get("swing", 18))
    shots.append(
        shot(
            s,
            "the_challenge",
            start,
            length,
            one(opener["group"], 0.2),
            move(length, (5.6, 7, yaw, 40, 1.5), (4.4, 6, yaw - 14, 40, 1.4)),
            caption=opener["line"],
        )
    )

    start, length = 2.9, 2.5
    yaw = behind(a["open"], opener["side"], replier.get("swing", -24))
    shots.append(
        shot(
            s,
            "the_answer",
            start,
            length,
            one(replier["group"], 0.2),
            move(length, (4.6, 4, yaw, 38, 1.3), (3.5, 3, yaw + 16, 38, 1.25)),
            caption=replier["line"],
        )
    )

    start, length = 5.7, t["clash"] - 0.25 - 5.7
    yaw = behind(a["open"], opener["side"], -16)
    shots.append(
        shot(
            s,
            "the_walk",
            start,
            length,
            both(s, 0.3),
            move(length, (15, 7, yaw, 40, 1.0), (8.5, 5, yaw + 10, 40, 1.0)),
            caption=s["place"],
        )
    )

    start, length = t["clash"] - 0.25, 1.05
    yaw = side(s, a["open"])
    contact = 0.25 + s.get("clash_contact", 0.28)
    shots.append(
        shot(
            s,
            "the_clash",
            start,
            length,
            both(s, 0.26),
            move(length, (7.4, 6, yaw + 10, 40, 1.15), (5.6, 5, yaw - 6, 40, 1.1)),
            slow_motion=2.6,
            jolts=[{"at": contact, "degrees": 0.9, "decay": 0.3}],
            punch=[{"at": contact * 2.6, "amount": 0.10, "decay": 0.35}],
        )
    )

    start = t["clash"] + 0.8
    length = t["finisher"] - 0.5 - start
    yaw = side(s, a["open"])
    shots.append(
        shot(
            s,
            "the_string",
            start,
            length,
            both(s, 0.26),
            move(length, (7.4, 9, yaw + 14, 42, 1.25), (6.4, 7, yaw - 12, 42, 1.2)),
            caption=s["string_line"],
        )
    )

    start, length = t["finisher"] - 0.5, 1.15
    yaw = behind(a["open"], s["finisher_from"], 50)
    shots.append(
        shot(
            s,
            "the_finisher",
            start,
            length,
            both(s, 0.3),
            move(length, (7.6, 5, yaw, 42, 1.1), (9.4, 7, yaw + 12, 42, 1.2)),
            slow_motion=2.4,
            jolts=[{"at": 0.5, "degrees": 1.1, "decay": 0.35}],
            punch=[{"at": 0.5 * 2.4, "amount": 0.12, "decay": 0.4}],
        )
    )

    start = t["circle"]
    length = min(1.9, t["slip"] - 0.1 - start)
    yaw = s.get("circle_yaw", side(s, a["open"]))
    shots.append(
        shot(
            s,
            "the_circle",
            start,
            length,
            both(s, 0.35),
            move(length, (10.5, 13, yaw - 20, 40, 1.2), (9.0, 10, yaw + 20, 40, 1.2)),
            caption=s["circle_line"],
        )
    )

    start = t["slip"] - 0.1
    length = t["thrust"] - 0.05 - start
    yaw = side(s, a["slip"])
    shots.append(
        shot(
            s,
            "the_press",
            start,
            length,
            both(s, 0.26),
            move(length, (6.8, 6, yaw + 14, 42, 1.2), (6.2, 5, yaw - 8, 42, 1.15)),
        )
    )

    start, length = t["thrust"] - 0.05, 1.15
    shots.append(
        shot(
            s,
            "the_slip",
            start,
            length,
            both(s, 0.3),
            move(length, (7.2, 5, yaw - 8, 42, 1.2), (8.0, 6, yaw - 22, 42, 1.25)),
            slow_motion=2.2,
            caption=s["slip_line"],
        )
    )

    start = t["launcher"] - 0.1
    length = t["slam"] + 0.35 - start
    yaw = side(s, a["juggle"])
    shots.append(
        shot(
            s,
            "the_juggle",
            start,
            length,
            both(s, 0.26),
            move(length, (8.2, 1, yaw - 12, 46, 1.5), (7.4, 3, yaw + 14, 46, 1.5)),
            slow_motion=1.8,
            jolts=[{"at": t["slam"] - start, "degrees": 1.2, "decay": 0.35}],
            punch=[{"at": (t["slam"] - start) * 1.8, "amount": 0.12, "decay": 0.4}],
            caption=s["juggle_line"],
        )
    )

    start, length = (
        t["last_clash"] - 0.2,
        t["second_clash"] + 0.55 - t["last_clash"] + 0.2,
    )
    yaw = side(s, a["last"])
    shots.append(
        shot(
            s,
            "the_bind",
            start,
            length,
            both(s, 0.26),
            move(length, (7.0, 6, yaw + 12, 40, 1.15), (5.8, 5, yaw - 8, 40, 1.1)),
            slow_motion=1.9,
            jolts=[
                {"at": 0.2 + 0.28, "degrees": 0.8, "decay": 0.3},
                {"at": t["second_clash"] - start + 0.38, "degrees": 0.9, "decay": 0.3},
            ],
        )
    )

    start = t["end"] - 0.75
    length = 1.25
    yaw = side(s, a.get("end", a["last"]))
    if s.get("end_from_behind"):
        yaw = behind(a.get("end", a["last"]), s["end_from_behind"], 42)
    shots.append(
        shot(
            s,
            "the_end",
            start,
            length,
            both(s, 0.3),
            move(length, (6.4, 5, yaw - 10, 42, 1.15), (5.6, 4, yaw + 8, 42, 1.1)),
            slow_motion=2.4,
            jolts=[{"at": 0.75, "degrees": 1.3, "decay": 0.4}],
            punch=[{"at": 0.75 * 2.4, "amount": 0.14, "decay": 0.45}],
            **s.get("end_extra", {}),
        )
    )

    if s.get("aftermath", True):
        start, length = t["end"] + 0.7, 2.6
        yaw = s["aftermath_yaw"]
        shots.append(
            shot(
                s,
                "the_field",
                start,
                length,
                one(s["winner"], 0.5),
                move(length, (5.0, 6, yaw - 10, 40, 1.3), (8.5, 9, yaw + 8, 40, 1.5)),
                caption=s["last_line"],
            )
        )

    for earlier, later in zip(shots, shots[1:], strict=False):
        if earlier["start"] + earlier["duration"] > later["start"] + 1e-6:
            raise SystemExit(
                f"{s['id']}: {earlier['name']} runs into {later['name']}; "
                "overlapping shots cost a second simulation pass"
            )

    spec = dict(HEADER)
    spec["id"] = s["id"]
    spec["music"] = s["music"]
    spec["music_start"] = s.get("music_start", 0.0)
    if "lighting" in s:
        for entry in shots:
            entry["lighting"] = s["lighting"]
    spec["shots"] = shots
    if "grade" in s:
        spec["grade"] = dict(HEADER["grade"], **s["grade"])
    return spec


STAGES = [
    {
        "id": "duel_01_old_enemies",
        "scenario": "duel_reel_old_enemies",
        "first": "scipio",
        "second": "hannibal",
        "music": "assets/audio/music/combat/combat_dust_of_cannae.ogg",
        "opener": {
            "group": "scipio",
            "side": "first",
            "line": "HANNIBAL. YOU AGAIN.",
        },
        "replier": {
            "group": "hannibal",
            "line": "SCIPIO. STILL CHASING ME?",
        },
        "place": "ZAMA. THE LAST MEETING.",
        "string_line": "SIXTEEN YEARS OF WAR",
        "circle_line": "NEITHER GIVES GROUND",
        "slip_line": "TOO SLOW, CARTHAGINIAN",
        "juggle_line": "HANNIBAL ANSWERS",
        "last_line": "ROME REMEMBERS",
        "finisher_from": "first",
        "winner": "scipio",
        "aftermath_yaw": 90,
        "light_yaw": 120,
        "lighting": {"ambient_scale": 1.4},
        "beats": {
            "clash": 8.40,
            "finisher": 11.57,
            "circle": 12.3,
            "slip": 14.17,
            "thrust": 16.10,
            "launcher": 20.37,
            "slam": 22.67,
            "last_clash": 25.30,
            "second_clash": 26.03,
            "end": 29.23,
        },
        "axis": {"open": 270, "slip": 184, "juggle": 124, "last": 35},
    },
    {
        "id": "duel_02_alpine_pass",
        "scenario": "duel_reel_alpine_pass",
        "first": "fabius",
        "second": "hannibal",
        "music": "assets/audio/music/campaign/campaign_crossing_of_the_alps.ogg",
        "opener": {
            "group": "fabius",
            "side": "first",
            "line": "NO FURTHER, CARTHAGINIAN.",
        },
        "replier": {
            "group": "hannibal",
            "line": "I CROSSED THE ALPS FOR THIS?",
        },
        "place": "THE HIGH PASS",
        "string_line": "SPEAR AGAINST SWORD",
        "circle_line": "THE SNOW TAKES NO SIDE",
        "slip_line": "FABIUS SLIPS THE POINT",
        "juggle_line": "HANNIBAL BREAKS HIS GUARD",
        "last_line": "THE ROAD TO ROME IS OPEN",
        "finisher_from": "first",
        "winner": "hannibal",
        "aftermath_yaw": 0,
        "light_yaw": 0,
        "beats": {
            "clash": 7.93,
            "finisher": 11.03,
            "circle": 12.2,
            "slip": 13.93,
            "thrust": 15.87,
            "launcher": 19.47,
            "slam": 21.77,
            "last_clash": 24.33,
            "second_clash": 25.10,
            "end": 34.30,
        },
        "axis": {"open": 75, "slip": 333, "juggle": 315, "last": 232, "end": 142},
    },
    {
        "id": "duel_03_night_raid",
        "scenario": "duel_reel_night_raid",
        "first": "scipio",
        "second": "hanno",
        "music": "assets/audio/music/combat/combat_shield_wall_at_dusk.ogg",
        "opener": {
            "group": "hanno",
            "side": "second",
            "line": "ROMAN. WRONG CAMP.",
        },
        "replier": {
            "group": "scipio",
            "line": "I CAME FOR YOU, HANNO.",
        },
        "place": "THE TREBIA. AFTER DARK.",
        "string_line": "ONE CONSUL. ONE CAMP.",
        "circle_line": "NO ONE IS COMING",
        "slip_line": "SCIPIO SLIPS THE SPEAR",
        "juggle_line": "HANNO SENDS HIM UP",
        "last_line": "",
        "finisher_from": "first",
        "winner": "scipio",
        "aftermath": False,
        "aftermath_yaw": 270,
        "light_yaw": 270,
        "circle_yaw": 310,
        "end_from_behind": "first",
        "end_extra": {"freeze": 1.4, "freeze_text": "WHO WALKS OUT?"},
        "beats": {
            "clash": 8.37,
            "finisher": 11.53,
            "circle": 12.3,
            "slip": 14.57,
            "thrust": 16.47,
            "launcher": 20.03,
            "slam": 22.40,
            "last_clash": 25.13,
            "second_clash": 25.87,
            "end": 29.07,
        },
        "axis": {"open": 0, "slip": 244, "juggle": 230, "last": 140},
    },
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preview", action="store_true", help="write 540x960 specs")
    parser.add_argument("--out", type=Path, default=HERE, help="directory to write to")
    args = parser.parse_args()
    width, height = (540, 960) if args.preview else (1080, 1920)
    for stage in STAGES:
        spec = reel(stage)
        spec["width"] = width
        spec["height"] = height
        path = args.out / f"{stage['id']}.json"
        path.write_text(json.dumps(spec, indent=2) + "\n")
        print(path)


if __name__ == "__main__":
    main()
