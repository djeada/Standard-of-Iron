#!/usr/bin/env python3
"""Write cut_ambush.json, the edit, looks, captions and sound for the night-ambush
short, reusing the cinematic trailer's grade and end card."""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
TRAILER = json.loads((HERE.parent / "cinematic" / "cut.json").read_text())
C = "cine_ambush/"
SFX = "assets/audio/sfx/"
MUS = "assets/audio/music/"


def ev(name, clip, at, dur, **extra):
    event = {
        "name": name,
        "clip": C + clip,
        "in": at,
        "dur": dur,
        "look": "night",
        "game_audio": -15,
    }
    event.update(extra)
    return event


EVENTS = [
    ev("march", "road_torches", 2.0, 4.6, fx={"embers": 0.35}),
    ev("consul", "consul_watch", 1.5, 2.6),
    ev("barrow", "barrow_mist", 0.6, 3.0),
    ev("march2", "road_torches", 6.6, 1.9, fx={"embers": 0.35}),
    ev("barrow2", "barrow_mist", 4.4, 1.5),
    ev("rising", "west_rising", 1.3, 3.2, hits=[0.15]),
    ev("east", "west_rising", 4.4, 1.8, hits=[0.0]),
    ev("volley", "slope_volley", 1.0, 2.6),
    ev("melee", "melee_tele", 2.0, 2.4, hits=[0.3]),
    ev("charge", "consul_charge", 1.0, 3.0, hits=[1.2]),
    ev("flank", "east_charge", 5.0, 2.4),
    ev("melee2", "melee_tele", 6.5, 1.6, hits=[0.4]),
    ev("crane", "crane_battle", 1.0, 3.6),
    ev("fight", "consul_fight", 1.0, 2.0, hits=[0.6]),
    ev("rear", "rear_threat", 4.0, 2.4),
    ev("x1", "melee_tele", 9.0, 0.8, hits=[0.0]),
    ev("x2", "consul_charge", 7.0, 0.8, hits=[0.0]),
    ev("x3", "crane_battle", 8.0, 0.9, hits=[0.0]),
    ev("x4", "east_charge", 11.0, 0.8, hits=[0.0]),
    ev("x5", "slope_volley", 6.0, 0.8, hits=[0.0]),
    ev("dawn", "last_stand", 2.0, 5.6),
]

ENDCARD = json.loads(json.dumps(TRAILER["events"][-1]))
ENDCARD["dur"] = 10.0
ENDCARD["card"]["lines"][0]["at"] = 0.6

CAPTIONS = [
    {
        "text": "ITALY, 217 BC",
        "sub": "A LEGION TAKES THE OLD ROAD BY NIGHT",
        "at": "march+0.5",
        "dur": 3.4,
        "size": 52,
    },
    {
        "text": "NO SHEPHERD WALKS IT AFTER DARK",
        "at": "barrow+0.2",
        "dur": 2.6,
        "size": 44,
    },
    {"text": "THE DEAD WERE WAITING", "at": "rising+0.6", "dur": 2.4, "size": 52},
    {"text": "ARROWS OUT OF THE DARK", "at": "volley+0.2", "dur": 2.2, "size": 48},
    {"text": "THE CONSUL TAKES THE FRONT", "at": "charge+0.3", "dur": 2.4, "size": 48},
    {"text": "HOLD THE LINE", "at": "crane+0.3", "dur": 2.8, "size": 56},
    {
        "text": "BY DAWN, THE ROAD WAS SILENT AGAIN",
        "at": "dawn+1.0",
        "dur": 3.8,
        "size": 46,
    },
]


def fx(at, path, gain, **extra):
    cue = {"at": at, "file": SFX + path, "gain": gain}
    cue.update(extra)
    return cue


def synth(at, kind, gain, **extra):
    cue = {"at": at, "synth": kind, "gain": gain}
    cue.update(extra)
    return cue


FX = [
    fx("march", "combat/army_march_dirt_mass.ogg", -9, lowpass=3500, distance=10),
    fx(
        "march+1.2",
        "combat/roman_war_horns_orders.ogg",
        -14,
        distance=140,
        reverb=0.7,
        decay=4.0,
        pan=-0.3,
    ),
    fx("consul+0.2", "orders/move_kit_shuffle.ogg", -10),
    fx(
        "barrow+0.3",
        "combat/magic_arc_discharge.ogg",
        -16,
        distance=60,
        reverb=0.9,
        decay=5,
    ),
    synth("barrow2", "drone", -14, length=1.6),
    synth("rising-1.6", "riser", -6, length=1.6, top_hz=7000),
    fx("rising-0.4", "undead/skeletons_rise.ogg", 0, reverb=0.4),
    synth("rising", "impact", -3, weight=1.4, brightness=0.15, seed=11),
    fx("east+0.1", "undead/skeletons_rise.ogg", -5, reverb=0.5, pan=0.4),
    fx("east+0.4", "alerts/enemy_spotted_horn.ogg", -12, distance=80, reverb=0.6),
    fx("volley-0.2", "combat/arrows_overhead_dark.ogg", -3),
    fx("volley+1.4", "combat/arrow_impact_02.ogg", -6, pan=-0.2),
    fx("volley+1.9", "combat/arrow_impact_03.ogg", -7, pan=0.25),
    fx("melee", "combat/blade_clash_01.ogg", -3),
    fx("melee+0.3", "combat/roman_shield_wall_impact.ogg", -4),
    fx("melee+1.1", "combat/sword_hit_02.ogg", -6, pan=0.3),
    synth("charge-0.4", "whoosh", -9, length=0.6, seed=21),
    fx("charge+0.1", "orders/commander_rally.ogg", -6),
    fx("charge+1.2", "combat/blade_clash_03.ogg", -2),
    fx("charge+1.9", "combat/human_death_cry_v2.ogg", -12, distance=15),
    fx("flank", "combat/charge_roar.ogg", -10, distance=40, reverb=0.4, pan=0.4),
    fx("melee2+0.4", "combat/gladius_shield_impacts_close.ogg", -3),
    synth("crane-0.5", "whoosh", -8, length=1.2, seed=22),
    fx("crane+0.2", "combat/battlefield_crowd_chaos.ogg", -8),
    fx("fight+0.6", "combat/blade_clash_02.ogg", -2),
    fx("fight+0.9", "combat/fireball_impact.ogg", -5, pan=-0.3, reverb=0.3),
    fx("rear+0.3", "combat/arrows_many_overhead.ogg", -8),
    synth("x1-1.4", "riser", -7, length=1.4, top_hz=9000),
]
for index, name in enumerate(["x1", "x2", "x3", "x4", "x5"]):
    FX.append(synth(f"{name}-0.3", "whoosh", -12, length=0.45, seed=60 + index))
    FX.append(fx(name, f"combat/sword_hit_0{index % 4 + 1}.ogg", -3))
FX += [
    fx("x3+0.1", "combat/fireball_impact.ogg", -3, reverb=0.25),
    synth("dawn", "reverse", -10, length=1.0),
    fx("dawn+0.4", "combat/aftermath_battlefield.ogg", -9),
    fx("dawn+2.6", "combat/soldiers_victory_cheer.ogg", -16, distance=60, reverb=0.6),
    synth("endcard+0.45", "boom", -4, seed=78),
]

AUDIO = {
    "music": [
        {
            "file": MUS + "combat/combat_shield_wall_at_dusk.ogg",
            "src_in": 1.0,
            "at": "march",
            "until": "rising+0.1",
            "gain": -1,
            "fade_out": 0.4,
            "gain_curve": [["march", -10], ["barrow", -6], ["barrow2", -2]],
        },
        {
            "file": MUS + "events/skeletons_awaken.ogg",
            "src_in": 24.6,
            "at": "rising-0.05",
            "until": "charge+0.2",
            "gain": 0,
            "fade_in": 0.05,
            "fade_out": 0.5,
        },
        {
            "file": MUS + "combat/combat_last_defensive_wall.ogg",
            "src_in": 26.0,
            "at": "charge-0.1",
            "until": "dawn+0.4",
            "gain": 0,
            "fade_in": 0.3,
            "fade_out": 1.2,
        },
        {
            "file": MUS + "menu/main_theme_iron_kingdom.ogg",
            "src_in": 42.0,
            "at": "dawn",
            "until": "endcard+9.8",
            "gain": 0,
            "fade_in": 1.2,
            "fade_out": 2.5,
            "gain_curve": [["dawn", -8], ["endcard-0.5", 0]],
        },
    ],
    "fx": FX,
    "beds": [
        {
            "file": "assets/audio/ambience/camp_fire_night.ogg",
            "at": 0.0,
            "until": "barrow",
            "gain": -14,
            "fade_out": 0.5,
        },
        {
            "file": "assets/audio/ambience/forest_ambush.ogg",
            "at": "barrow",
            "until": "melee",
            "gain": -12,
            "fade_out": 0.3,
        },
        {
            "file": "assets/audio/sfx/combat/battlefield_crowd_chaos.ogg",
            "at": "melee",
            "until": "dawn",
            "gain": -12,
            "fade_out": 0.6,
        },
        {
            "file": "assets/audio/ambience/mountain_camp_night.ogg",
            "at": "dawn",
            "until": "endcard+2.0",
            "gain": -15,
            "fade_out": 1.5,
        },
    ],
    "duck": TRAILER["audio"]["duck"],
    "bus_gain": TRAILER["audio"]["bus_gain"],
    "master": TRAILER["audio"]["master"],
}

LOOKS = json.loads(json.dumps(TRAILER["looks"]))


LOOKS["moonlit"] = dict(
    LOOKS["night"],
    exposure=0.15,
    curve=[[0, 0.03], [0.25, 0.22], [0.5, 0.5], [0.75, 0.78], [1, 0.96]],
    saturation=0.95,
)
for event in EVENTS:
    event["look"] = "moonlit"

cut = {
    "fps": TRAILER["fps"],
    "subframes": TRAILER["subframes"],
    "shutter": TRAILER["shutter"],
    "scope": TRAILER["scope"],
    "looks": LOOKS,
    "events": EVENTS + [ENDCARD],
    "captions": CAPTIONS,
    "audio": AUDIO,
}
out = HERE / "cut_ambush.json"
out.write_text(json.dumps(cut, indent=2) + "\n")
print(out, sum(e["dur"] for e in cut["events"]), "s")
