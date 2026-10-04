#!/usr/bin/env python3
"""Write the vertical reel specs in this directory from one table.

Every reel shares a header (1080x1920, grade, end card); each entry below is
only the shots, the hook caption and the music. Run from the repo root.
"""

import json
from pathlib import Path

HERE = Path(__file__).resolve().parent

HEADER = {
    "width": 1080,
    "height": 1920,
    "fps": 30,
    "supersample": 1,
    "gameplay_ui": False,
    "audio": True,
    "record_music": False,
    "transition": {"type": "cut"},
    "grade": {
        "contrast": 1.12,
        "saturation": 1.16,
        "brightness": 0.05,
        "gamma": 1.14,
        "grain": 0,
        "vignette": 0.12,
        "sharpen": 0.4,
    },
    "motion_limits": {
        "yaw_degrees_per_second": 30,
        "pitch_degrees_per_second": 16,
        "fov_degrees_per_second": 10,
        "minimum_clip_seconds": 1.2,
        "mean_clip_seconds": 1.6,
    },
    "title": "STANDARD OF IRON",
    "end_card": "steam_demo",
    "end_card_background": "dim",
}


def key(time, distance, pitch, yaw, fov=46, ease=None, height=1.6):
    result = {
        "time": time,
        "distance": distance,
        "pitch": pitch,
        "yaw": yaw,
        "fov": fov,
        "height": height,
    }
    if ease:
        result["ease"] = ease
    return result


def move(duration, start_pose, end_pose, ease="smooth"):
    return [key(0, *start_pose), key(duration, *end_pose, ease=ease)]


def group(name, smoothing=0.6, offset=None):
    focus = {"mode": "group", "group": name, "smoothing": smoothing}
    if offset:
        focus["offset"] = offset
    return focus


def pair(first, second, smoothing=0.6):
    return {
        "mode": "group_pair",
        "group": first,
        "second_group": second,
        "smoothing": smoothing,
    }


def point(x, z, y=0.0, smoothing=0.8):
    return {"mode": "point", "point": [x, y, z], "smoothing": smoothing}


def battle(radius=30, smoothing=1.0):
    return {"mode": "battle", "engagement_radius": radius, "smoothing": smoothing}


def shot(name, scenario, start, duration, focus, camera, **extra):
    result = {
        "name": name,
        "scenario": scenario,
        "seed": 7,
        "start": start,
        "duration": duration,
        "focus": focus,
        "camera": camera,
    }
    result.update(extra)
    return result


REELS = {}


TWENTY = {"clash": 5.5, "end": 33.0, "verdict": "THE LEGIONARIES WIN."}

S = "trailer_open"
REELS["reel_01_commander"] = {
    "motion_limits": {
        "yaw_degrees_per_second": 30,
        "pitch_degrees_per_second": 16,
        "fov_degrees_per_second": 18,
        "minimum_clip_seconds": 1.2,
        "mean_clip_seconds": 1.6,
    },
    "music": "assets/audio/music/menu/main_theme_standard_of_iron.ogg",
    "music_start": 0.0,
    "shots": [
        shot(
            "the_army",
            S,
            2.0,
            3.0,
            point(-14.0, 23.5),
            move(3.0, (46, 52, 350, 42), (38, 46, 354, 42)),
            gameplay_ui=True,
            caption="WHAT IF YOUR RTS COMMANDER COULD ENTER THE BATTLE?",
        ),
        shot(
            "into_the_saddle",
            S,
            5.0,
            1.6,
            group("scipio", 0.1, [0.64, 0.71, -2.9]),
            [
                key(0, 30, 40, 354, 42, height=1.0),
                key(1.6, 6.23, 17.7, 5.2, 68, ease="smooth", height=0.0),
            ],
            gameplay_ui=True,
        ),
        {
            "name": "commander_view",
            "scenario": S,
            "seed": 7,
            "start": 6.6,
            "duration": 1.9,
            "gameplay_camera": True,
            "gameplay_ui": True,
        },
        shot(
            "the_line_answers",
            S,
            8.5,
            2.2,
            group("scipio", 0.1, [0.64, 0.71, -2.9]),
            [
                key(0, 6.23, 17.7, 5.2, 68, height=0.0),
                key(2.2, 24.0, 28, 12, 48, ease="smooth"),
            ],
            gameplay_ui=True,
            slow_motion=1.2,
        ),
    ],
}

S = "reel_cannae"
REELS["reel_02_cannae"] = {
    "music": "assets/audio/music/combat/combat_dust_of_cannae.ogg",
    "music_start": 0.0,
    "shots": [
        shot(
            "rome_rushes_in",
            S,
            6.0,
            2.6,
            group("rome_hastati", 0.5),
            move(2.6, (11, 7, 12, 48), (9, 8, 4, 48)),
            caption="THE TRAP THAT DESTROYED A ROMAN ARMY.",
        ),
        shot(
            "centre_gives",
            S,
            11.0,
            2.6,
            group("punic_gauls", 0.8),
            move(2.6, (40, 36, 170, 44), (36, 40, 162, 44)),
        ),
        shot(
            "rome_pushes",
            S,
            14.5,
            2.0,
            group("rome_principes", 0.8),
            move(2.0, (24, 20, 205, 46), (21, 22, 196, 46)),
        ),
        shot(
            "wings_close",
            S,
            18.5,
            2.2,
            group("punic_horse_left", 0.5),
            move(2.2, (13, 11, 60, 50), (11, 12, 46, 50)),
            slow_motion=1.3,
        ),
        shot(
            "the_ring",
            "reel_closing_circle",
            12.0,
            7.0,
            point(0.0, -4.0),
            move(7.0, (92, 80, 182, 44), (84, 82, 184, 44)),
            time_lapse=2.0,
        ),
    ],
}

S = "reel_elephant_charge"
REELS["reel_03_elephant"] = {
    "music": "assets/audio/music/combat/combat_carthaginian_dust.ogg",
    "music_start": 0.0,
    "shots": [
        shot(
            "it_comes",
            S,
            5.6,
            2.2,
            group("elephants", 0.3),
            move(2.2, (10, 5, 185, 52), (7.5, 6, 180, 52)),
            caption="ROME HAS A PROBLEM.",
        ),
        shot(
            "impact",
            S,
            8.8,
            1.8,
            pair("elephants", "rome_line", 0.3),
            move(1.8, (10, 12, 120, 50), (9, 13, 108, 50)),
            slow_motion=1.6,
        ),
        shot(
            "through",
            S,
            10.9,
            1.6,
            group("elephants", 0.3),
            move(1.6, (11, 14, 250, 50), (10.5, 14, 246, 50)),
        ),
        shot(
            "aftermath",
            S,
            17.0,
            2.2,
            battle(25, 1.0),
            move(2.2, (26, 52, 185, 44), (30, 58, 180, 44)),
        ),
    ],
}

S = "reel_front_rank"
REELS["reel_04_before_after"] = {
    "music": "assets/audio/music/combat/combat_shield_wall_at_dusk.ogg",
    "music_start": 0.0,
    "shots": [
        shot(
            "ordered",
            S,
            0.8,
            1.9,
            point(0.0, 0.0),
            move(1.9, (46, 32, 200, 44), (45, 32, 200, 44)),
        ),
        shot(
            "thirty_seconds_later",
            S,
            30.0,
            2.6,
            point(0.0, 0.0),
            move(2.6, (45, 32, 200, 44), (44, 32, 200, 44)),
            caption="30 SECONDS LATER...",
        ),
        shot(
            "inside",
            S,
            32.8,
            1.8,
            battle(25, 0.8),
            move(1.8, (9, 12, 150, 50), (8, 13, 138, 50)),
        ),
        shot(
            "wreckage",
            S,
            40.0,
            2.2,
            battle(45, 1.2),
            move(2.2, (14, 14, 200, 46), (13.5, 14, 201, 46)),
        ),
    ],
}

S = "promo_last_stand"
REELS["reel_05_hold_the_line"] = {
    "music": "assets/audio/music/combat/combat_last_defensive_wall.ogg",
    "music_start": 0.0,
    "shots": [
        shot(
            "they_come",
            S,
            1.4,
            2.3,
            group("punic_horde", 0.4),
            move(2.3, (17, 4, 150, 34), (16, 5, 152, 34)),
            caption="WOULD YOU HOLD HERE?",
        ),
        shot(
            "the_line_waits",
            S,
            3.2,
            1.4,
            group("roman_line", 0.4),
            move(1.4, (6, 7, 184, 50), (5.2, 8, 178, 50)),
        ),
        shot(
            "impact",
            S,
            4.6,
            1.3,
            pair("roman_line", "punic_horde", 0.3),
            move(1.3, (10, 14, 118, 52), (8.5, 17, 106, 52)),
            slow_motion=2.2,
            shake=0.03,
        ),
        shot(
            "it_bends",
            S,
            8.0,
            2.2,
            pair("roman_line", "punic_horde", 0.6),
            move(2.2, (14, 20, 150, 46), (12, 18, 172, 46)),
        ),
        shot(
            "it_holds",
            S,
            14.5,
            2.6,
            {"mode": "all", "smoothing": 0.8},
            move(2.6, (16, 14, 204, 44), (32, 32, 224, 44)),
            slow_motion=1.2,
        ),
    ],
}

S = "reel_cavalry_flank"
REELS["reel_06_cavalry_flank"] = {
    "music": "assets/audio/music/combat/combat_cavalry_of_carthage.ogg",
    "music_start": 0.0,
    "shots": [
        shot(
            "locked",
            S,
            6.0,
            2.2,
            pair("rome_front", "carthage_front", 0.5),
            move(2.2, (10, 12, 110, 48), (9.5, 12, 106, 48)),
            caption="THEY NEVER SAW THE CAVALRY COMING.",
        ),
        shot(
            "behind_them",
            S,
            11.5,
            2.8,
            group("rome_horse", 0.6),
            move(2.8, (16, 10, 304, 46), (15, 10, 308, 46)),
        ),
        shot(
            "into_the_rear",
            S,
            16.0,
            2.2,
            pair("rome_horse", "carthage_archers", 0.4),
            move(2.2, (12, 12, 20, 48), (11.5, 12, 16, 48)),
            slow_motion=1.4,
        ),
        shot(
            "collapse",
            S,
            23.0,
            2.4,
            battle(30, 1.0),
            move(2.4, (30, 52, 200, 44), (31, 53, 199, 44)),
        ),
    ],
}

S = "reel_twenty_a_side"
REELS["reel_07_twenty_v_twenty"] = {
    "music": "assets/audio/music/combat/combat_cavalry_of_carthage.ogg",
    "music_start": 4.0,
    "shots": [
        shot(
            "face_off",
            S,
            0.3,
            2.2,
            pair("legionaries", "swordsmen", 0.1),
            move(2.2, (20, 14, 200, 44), (19, 14, 200, 44)),
            caption="20 LEGIONARIES VS 20 SWORDSMEN. WHO WINS?",
        ),
        shot(
            "clash",
            S,
            TWENTY["clash"],
            1.8,
            pair("legionaries", "swordsmen", 0.6),
            move(1.8, (9, 11, 200, 48), (8.5, 11, 206, 48)),
            slow_motion=1.4,
        ),
        shot(
            "cut_a",
            S,
            TWENTY["clash"] + 3.0,
            1.4,
            battle(15, 0.6),
            move(1.4, (6.5, 10, 120, 50), (6.3, 10, 116, 50)),
        ),
        shot(
            "cut_b",
            S,
            TWENTY["clash"] + 8.0,
            1.4,
            battle(15, 0.6),
            move(1.4, (7, 13, 300, 50), (6.8, 13, 304, 50)),
        ),
        shot(
            "result",
            S,
            TWENTY["end"] - 1.2,
            3.0,
            battle(15, 1.2),
            move(3.0, (14, 30, 250, 44), (13, 32, 254, 44)),
            freeze=1.4,
            freeze_text=TWENTY["verdict"],
        ),
    ],
}

S = "trailer_siege_walls"
REELS["reel_09_siege_gate"] = {
    "music": "assets/audio/music/combat/combat_siege_at_dawn.ogg",
    "music_start": 0.0,
    "shots": [
        shot(
            "in_flight",
            S,
            14.6,
            2.2,
            point(0.0, -12.0, 2.0),
            move(2.2, (26, 12, 8, 48), (24, 12, 6, 48)),
            caption="WHY FIGHT THE WALL WHEN YOU CAN REMOVE IT?",
        ),
        shot(
            "impact",
            S,
            18.9,
            2.0,
            point(0.0, -14.0, 1.5),
            move(2.0, (22, 22, 14, 48), (21, 22, 13, 48)),
            slow_motion=1.5,
            shake=0.03,
        ),
        shot(
            "the_breach",
            S,
            21.0,
            2.0,
            point(0.0, -14.0),
            move(2.0, (28, 18, 4, 46), (27, 18, 5, 46)),
        ),
        shot(
            "through_it",
            S,
            26.0,
            3.0,
            group("siege_assault", 0.6),
            move(3.0, (13, 12, 18, 48), (12, 12, 14, 48)),
        ),
    ],
}

S = "reel_fog_ambush"
REELS["reel_11_fog_ambush"] = {
    "music": "assets/audio/music/combat/combat_dust_of_trasimene.ogg",
    "music_start": 0.0,
    "shots": [
        shot(
            "in_the_mist",
            S,
            3.0,
            2.4,
            group("carthage_left", 0.1),
            move(2.4, (30, 4, 100, 26), (28, 4, 98, 26)),
            caption="ROME WALKED INTO THE FOG.",
        ),
        shot(
            "the_column",
            S,
            5.6,
            2.4,
            group("rome_column", 0.8),
            move(2.4, (14, 10, 200, 46), (12, 11, 192, 46)),
        ),
        shot(
            "they_emerge",
            S,
            11.0,
            2.0,
            group("carthage_left", 0.5, [4.0, 0.0, 0.0]),
            move(2.0, (6, 7, 262, 50), (5.5, 8, 268, 50)),
        ),
        shot(
            "the_attack",
            S,
            16.5,
            2.2,
            pair("carthage_left", "rome_column", 0.4),
            move(2.2, (10, 12, 140, 50), (9, 13, 128, 50)),
            slow_motion=1.3,
            shake=0.03,
        ),
        shot(
            "surrounded",
            S,
            24.0,
            2.2,
            battle(25, 1.0),
            move(2.2, (22, 36, 200, 44), (26, 42, 196, 44)),
        ),
    ],
}

S = "trailer_city_battle"
REELS["reel_12_zoom"] = {
    "music": "assets/audio/music/menu/main_theme_iron_kingdom.ogg",
    "music_start": 0.0,
    "motion_limits": {
        "yaw_degrees_per_second": 30,
        "pitch_degrees_per_second": 16,
        "fov_degrees_per_second": 10,
        "minimum_clip_seconds": 1.2,
        "mean_clip_seconds": 1.2,
    },
    "shots": [
        shot(
            "one_soldier",
            S,
            22.0,
            8.6,
            group("scipio", 0.3),
            [
                key(0, 4.2, 10, 120, 50),
                key(2.0, 9.0, 16, 124, 50, ease="smooth"),
                key(5.0, 45.0, 38, 132, 48, ease="smooth"),
                key(8.6, 130.0, 58, 140, 46, ease="smooth"),
            ],
            caption="FROM ONE SOLDIER... TO THE WHOLE BATTLEFIELD.",
        ),
    ],
}

S = "reel_closing_circle"
REELS["reel_13_encirclement"] = {
    "music": "assets/audio/music/combat/combat_dust_of_cannae.ogg",
    "music_start": 20.0,
    "shots": [
        shot(
            "the_blob",
            S,
            1.5,
            6.0,
            group("rome_block", 1.5),
            move(6.0, (72, 82, 180, 44), (70, 82, 181, 44)),
            time_lapse=2.0,
            caption="YOU KNOW IT'S OVER WHEN THE CIRCLE CLOSES.",
        ),
        shot(
            "closing",
            S,
            7.5,
            9.0,
            group("rome_block", 1.5),
            move(9.0, (70, 82, 181, 44), (62, 82, 182, 44)),
            time_lapse=2.5,
        ),
        shot(
            "closed",
            S,
            17.0,
            3.2,
            group("rome_block", 1.5),
            move(3.2, (62, 82, 182, 44), (58, 83, 183, 44)),
        ),
    ],
}

S = "reel_sepulcher_reveal"
REELS["reel_15_sepulcher"] = {
    "music": "assets/audio/music/events/skeletons_awaken.ogg",
    "music_start": 0.0,
    "shots": [
        shot(
            "just_a_battle",
            S,
            0.8,
            2.1,
            group("rome_line", 0.1, [0.0, 0.0, 5.0]),
            move(2.1, (8, 8, 180, 40), (7.5, 8, 180, 40)),
            caption="THIS WAS SUPPOSED TO BE A HISTORICAL RTS.",
        ),
        shot(
            "the_ground_wakes",
            S,
            2.9,
            2.0,
            point(-10.0, 0.0),
            move(2.0, (20, 16, 100, 48), (19, 17, 98, 48)),
            slow_motion=1.5,
            shake=0.03,
        ),
        shot(
            "they_rise",
            S,
            5.2,
            2.4,
            point(-12.0, 0.0, 1.0),
            move(2.4, (12, 8, 120, 50), (11.5, 8, 117, 50)),
        ),
        shot(
            "everyone_turns",
            S,
            8.6,
            2.4,
            point(-8.0, 0.0),
            move(2.4, (30, 34, 110, 44), (32, 37, 108, 44)),
        ),
    ],
}

S = "promo_commander_rally"
REELS["reel_16_commander_rally"] = {
    "music": "assets/audio/music/menu/main_theme_standard_of_iron_alt.ogg",
    "music_start": 0.0,
    "shots": [
        shot(
            "the_line_breaks",
            S,
            11.0,
            2.4,
            pair("roman_line", "punic_horde", 1.4),
            move(2.4, (30, 12, 70, 40), (28.5, 11, 72, 40)),
            caption="SOMETIMES THE ARMY NEEDS YOU THERE.",
        ),
        shot(
            "he_watches",
            S,
            13.8,
            1.6,
            point(0.5, -15.5, 7.0, 2.2),
            move(2.0, (12.5, 4, 138, 36), (12, 4, 141, 36)),
        ),
        shot(
            "the_signal",
            S,
            16.4,
            2.2,
            point(0.5, -15.5, 7.0, 2.6),
            move(2.2, (7.2, -5, 336, 33), (6.9, -6, 332, 33)),
            slow_motion=1.15,
        ),
        shot(
            "they_reform",
            S,
            21.4,
            1.9,
            pair("roman_line", "punic_horde", 0.9),
            move(1.9, (9, 11, 30, 34), (8.7, 11, 34, 34)),
            slow_motion=1.4,
        ),
        shot(
            "counterattack",
            S,
            22.6,
            2.2,
            group("roman_horse", 1.1),
            move(2.2, (22, 12, 108, 40), (21, 12, 112, 40)),
        ),
        shot(
            "they_break",
            S,
            25.8,
            1.6,
            pair("roman_horse", "punic_horde", 1.0),
            move(2.0, (19, 10, 100, 40), (18, 10, 104, 40)),
        ),
    ],
}


def three_ways(name, scenario, start, duration, caption, lapse):
    return shot(
        name,
        scenario,
        start,
        duration,
        point(0.0, 2.0),
        move(duration, (40, 62, 180, 44), (38, 64, 184, 44)),
        time_lapse=lapse,
        caption=caption,
    )


REELS["reel_17_three_ways"] = {
    "music": "assets/audio/music/base/base_march_of_the_old_gods.ogg",
    "music_start": 0.0,
    "shots": [
        shot(
            "the_enemy",
            "reel_attack_frontal",
            0.4,
            1.8,
            group("carthage_line", 0.5),
            move(1.8, (12, 7, 190, 48), (10.5, 8, 184, 48)),
            caption="WHICH ONE WOULD YOU CHOOSE?",
        ),
        three_ways("frontal", "reel_attack_frontal", 1.0, 6.5, "1. FRONTAL", 2.5),
        three_ways("flank", "reel_attack_flank", 1.0, 7.6, "2. FLANK", 2.7),
        three_ways("encircle", "reel_attack_encircle", 1.0, 9.0, "3. ENCIRCLE", 2.8),
    ],
}

S = "reel_bad_decision"
REELS["reel_18_bad_decision"] = {
    "music": "assets/audio/music/combat/combat_shield_wall_at_dusk.ogg",
    "music_start": 10.0,
    "shots": [
        shot(
            "even",
            S,
            3.0,
            2.2,
            point(-8.0, 0.0),
            move(2.2, (36, 42, 180, 44), (34, 44, 184, 44)),
            caption="ONE COMMAND LOST THE BATTLE.",
        ),
        shot(
            "the_order",
            S,
            5.8,
            2.6,
            point(-6.0, -8.0),
            move(2.6, (34, 46, 180, 44), (33, 47, 181, 44)),
            caption="THIS ONE.",
        ),
        shot(
            "the_gap",
            S,
            10.2,
            2.4,
            point(-22.0, 6.0),
            move(2.4, (24, 30, 250, 46), (23, 30, 250, 46)),
        ),
        shot(
            "the_flank_goes",
            S,
            14.0,
            2.2,
            point(-10.0, -2.0),
            move(2.2, (16, 20, 250, 48), (15.5, 20, 249, 48)),
            slow_motion=1.3,
        ),
        shot(
            "defeat",
            S,
            24.0,
            6.0,
            point(-6.0, 0.0),
            move(6.0, (26, 48, 180, 44), (30, 54, 184, 44)),
            time_lapse=2.0,
        ),
    ],
}

for plan, scenario in (
    ("a", "reel_formation_piecemeal"),
    ("b", "reel_formation_combined"),
):
    REELS[f"reel_08_formation_{plan}"] = {
        "music": "assets/audio/music/combat/combat_dust_of_trasimene.ogg",
        "music_start": 0.0,
        "shots": [
            shot(
                "split",
                scenario,
                0.6,
                2.6,
                point(0.0, 0.0),
                move(2.6, (66, 62, 180, 44), (65, 62, 180, 44)),
            ),
            shot(
                "play",
                scenario,
                3.2,
                32.0,
                point(0.0, 2.0),
                move(32.0, (34, 52, 180, 44), (33, 53, 181, 44)),
                time_lapse=8.0,
            ),
        ],
    }


REELS["reel_10_campaign"] = {
    "music": "assets/audio/music/campaign/campaign_crossing_of_the_alps.ogg",
    "music_start": 0.0,
    "shots": [
        shot(
            "the_alps",
            "trailer_highland",
            8.0,
            2.4,
            group("pass_column", 0.2),
            move(2.4, (24, 12, 200, 44), (23, 12, 202, 44)),
            caption="TAKE HANNIBAL FROM THE RHONE TO ZAMA.",
        ),
        shot(
            "the_rhone",
            "trailer_bridge_defense",
            5.0,
            1.8,
            group("bridge_storm", 0.2),
            move(1.8, (18, 16, 70, 44), (17, 16, 72, 44)),
            caption="THE RHONE.",
        ),
        shot(
            "trebia",
            "promo_storm_charge",
            3.4,
            1.8,
            group("roman_cavalry", 0.2),
            move(1.8, (12, 8, 96, 50), (11, 9, 100, 50)),
            caption="TREBIA.",
        ),
        shot(
            "trasimene",
            "reel_fog_ambush",
            16.5,
            1.8,
            pair("carthage_left", "rome_column", 0.2),
            move(1.8, (12, 12, 140, 48), (11.5, 12, 138, 48)),
            caption="TRASIMENE.",
        ),
        shot(
            "cannae",
            "reel_closing_circle",
            16.0,
            1.8,
            group("rome_block", 0.5),
            move(1.8, (60, 82, 182, 44), (58, 82, 182, 44)),
            caption="CANNAE.",
        ),
        shot(
            "zama",
            "reel_elephant_charge",
            8.6,
            2.2,
            pair("elephants", "rome_line", 0.2),
            move(2.2, (12, 12, 120, 48), (11, 12, 116, 48)),
            caption="ZAMA.",
        ),
    ],
}


def main():
    for reel_id, body in REELS.items():
        spec = dict(HEADER)
        spec["id"] = reel_id
        spec.update({k: v for k, v in body.items() if k != "shots"})
        spec["shots"] = body["shots"]
        (HERE / f"{reel_id}.json").write_text(json.dumps(spec, indent=2) + "\n")
        print(reel_id, len(spec["shots"]), "shots")


if __name__ == "__main__":
    main()
