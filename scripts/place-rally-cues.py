#!/usr/bin/env python3
"""Place the commander-rally reel's sound cues on the moments that happen.

    build/bin/arena_app --batch --scenario promo_commander_rally \
      --artifact-dir artifacts/arena/rally
    python3 scripts/place-rally-cues.py \
      artifacts/arena/rally/promo_commander_rally/trace.jsonl \
      tools/arena/promos/commander_rally.json

Reads the scenario's own trace, finds the losses on each side, the commander's
signature strikes and the moment the aura is called, maps every one through the
shot that shows it (``start`` plus ``slow_motion``), and writes the cue list back
into the spec. Hand-typed cue times rot the moment a shot is retimed -- see
``scripts/place-promo-cues.py`` for what that looked like on the wolf reel -- so
re-run this after any retime.
"""
import json
import sys


def shot_slow_motion(shot: dict) -> float:
    """`time_lapse` is `slow_motion` written for values below one."""
    if "time_lapse" in shot and "slow_motion" not in shot:
        return 1.0 / max(1.0, float(shot["time_lapse"]))
    return float(shot.get("slow_motion", 1.0))


trace_path, spec_path = sys.argv[1], sys.argv[2]

alive = {}
losses = []
signatures = []
aura = None
for line in open(trace_path):
    frame = json.loads(line)
    now = round(frame["time_seconds"], 2)
    live = {}
    for unit in frame.get("units", []):
        group = unit["group"]
        if unit["health"] > 0:
            live[group] = live.get(group, 0) + 1
        if group == "roman_consul":
            if unit.get("combat_action_id", 0) >= 16:
                signatures.append(now)
            if aura is None and unit.get("commander_aura_active"):
                aura = now
    for group, was in alive.items():
        if live.get(group, 0) < was:
            losses.append((now, group))
    alive = live

spec = json.load(open(spec_path))
DEFAULT = spec.get("transition", {"type": "cut", "duration": 0.0})

shots = []
finished = 0.0
for index, shot in enumerate(spec["shots"]):
    slow = shot_slow_motion(shot)
    if index > 0:
        join = shot.get("transition", DEFAULT)
        if join.get("type") != "cut":
            finished -= float(join.get("duration", 0.0))
    shots.append((shot["name"], finished, shot["start"], shot["duration"], slow))
    finished += shot["duration"] * slow
total = finished


QUIET = {"after"}


def to_finished(scenario_seconds, name_filter=None):
    """Scenario seconds -> finished seconds, or None when no shot shows it."""
    for name, start, window_start, duration, slow in shots:
        if window_start <= scenario_seconds < window_start + duration:
            if name in QUIET:
                return None
            if name_filter and name != name_filter:
                continue
            return start + (scenario_seconds - window_start) * slow
    return None


def shot_start(name):
    for shot_name, start, _ws, _d, _s in shots:
        if shot_name == name:
            return start
    return None


COMBAT = "assets/audio/sfx/combat/"
ORDERS = "assets/audio/sfx/orders/"
cues = []


def add(path, at, gain):
    if at is None or at < 0 or at > total:
        return
    cues.append({"file": path, "at": round(at, 2), "gain": gain})


def after_shot(name, offset=0.0):
    """A cue offset into a named shot, or None when the reel no longer has it."""
    start = shot_start(name)
    return None if start is None else start + offset


add(COMBAT + "battlefield_distant_mass_01.ogg", 0.1, 1.20)
add(COMBAT + "roman_shield_wall_impact.ogg", after_shot("the_field", 0.25), 1.80)
add(COMBAT + "gladius_shield_impacts_close.ogg", after_shot("the_field", 0.9), 1.35)
add(COMBAT + "battlefield_crowd_chaos.ogg", after_shot("the_field", 1.6), 1.10)


add(ORDERS + "attack_horn_stab.ogg", to_finished(aura), 2.30)
add(COMBAT + "roman_war_horns_orders.ogg", to_finished(aura), 1.70)
add(COMBAT + "charge_roar.ogg", after_shot("behind_him", 0.1), 1.85)
add(ORDERS + "run_kit_rattle.ogg", after_shot("behind_him", 0.5), 1.15)

add(COMBAT + "horse_gallop_close_pass.ogg", after_shot("the_charge", 0.05), 1.65)
add(COMBAT + "horse_gallop_close_pass.ogg", after_shot("the_charge", 1.2), 1.45)
add(COMBAT + "roman_shield_wall_impact.ogg", after_shot("the_charge", 2.3), 1.90)
add(COMBAT + "vanguard_rush.ogg", after_shot("they_break", 0.2), 1.25)
add(COMBAT + "soldiers_victory_cheer.ogg", after_shot("after", 0.7), 1.20)


last = -9.0
for index, at in enumerate(signatures):
    finished_at = to_finished(at, "signature")
    if finished_at is None or finished_at - last < 0.30:
        continue
    last = finished_at
    add(COMBAT + f"blade_clash_0{(index % 3) + 1}.ogg", finished_at, 2.20)
    add(COMBAT + "sword_hit_01.ogg", finished_at + 0.06, 1.60)


last = -9.0
for at, _group in losses:
    finished_at = to_finished(at)
    if finished_at is None:
        continue
    if finished_at - last < 0.5:
        continue
    last = finished_at
    add(COMBAT + "human_death_cry.ogg", finished_at - 0.05, 2.40)

cues.sort(key=lambda cue: cue["at"])
spec["sfx"] = cues
json.dump(spec, open(spec_path, "w"), indent=2)
print(f"placed {len(cues)} cues across {total:.2f} s (aura at scenario {aura}s)")
