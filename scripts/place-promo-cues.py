#!/usr/bin/env python3
"""Place a promo reel's sound cues on the moments that actually happen.

    build/bin/arena_app --batch --scenario promo_wolf_attack \
      --artifact-dir artifacts/events
    scripts/place-promo-cues.py artifacts/events/promo_wolf_attack/trace.jsonl \
      tools/arena/promos/wolf_attack.json

Reads the scenario's own trace, finds every bite, death and the cavalry melee,
maps each through the shot that shows it (`start` plus `slow_motion`), and
writes the cue list back into the spec.

**Hand-typed cue times rot.** Every time a shot was retimed during this reel the
sound stayed where it was, and the result was a death scream three seconds
before anyone died and bite sounds over an empty field -- which reads as "there
is no audio" rather than as "the audio is late". Re-running this after any
retime is the whole point of it.

Two things it refuses to do quietly:

* an event with no shot showing it gets a warning rather than a dropped cue, because
  a death that falls in the gap between two shots is exactly how the scream went
  missing;
* nothing is placed inside a shot named in ``QUIET`` (the aftermath), where a
  wolf snapping at nothing was audible over the closing music.
"""
import json
import sys

trace, spec_path = sys.argv[1], sys.argv[2]
prev = {}
alive = None
bites = []
deaths = []
melee = []
for line in open(trace):
    d = json.loads(line)
    t = d["time_seconds"]
    prey = [u for u in d["units"] if u.get("group") == "villagers"]
    rid = [u for u in d["units"] if u.get("group") == "riders"]
    ids = {u["entity_id"] for u in prey}
    if alive is not None:
        deaths += [round(t, 2)] * len(alive - ids)
    alive = ids
    for u in prey:
        k, h = u["entity_id"], u["health"]
        if k in prev and h < prev[k]:
            bites.append(round(t, 2))
        prev[k] = h
    if any(u.get("melee_lock") for u in rid):
        melee.append(round(t, 2))

thin = []
for b in bites:
    if not thin or b - thin[-1] >= 0.45:
        thin.append(b)

spec = json.load(open(spec_path))
DEF = spec.get("transition", {"type": "dissolve", "duration": 0.4})
shots = []
t = 0.0
for i, s in enumerate(spec["shots"]):
    slow = s.get("slow_motion", 1.0)
    if i > 0:
        tr = s.get("transition", DEF)
        t -= 0.0 if tr["type"] == "cut" else float(tr["duration"])
    shots.append((s["name"], t, s["start"], s["duration"], slow))
    t += s["duration"] * slow
total = t

QUIET = {"after"}


def to_fin(scn, allow_quiet=False):
    """Scenario seconds -> finished seconds, or None when no shot shows it."""
    for name, f, ss, dur, slow in shots:
        if ss <= scn < ss + dur:
            if name in QUIET and not allow_quiet:
                return None
            return f + (scn - ss) * slow
    return None


WILD = "assets/audio/sfx/wildlife/"
COMBAT = "assets/audio/sfx/combat/"
ORDERS = "assets/audio/sfx/orders/"
cues = []


def add(f, at, gain):
    if at is None or at < 0 or at > total:
        return
    cues.append({"file": f, "at": round(at, 2), "gain": gain})


add(WILD + "wolf_howl_distant.ogg", 0.35, 1.30)
add(WILD + "wolf_growl_low.ogg", to_fin(5.2), 1.95)
add(WILD + "wolf_pack_attack.ogg", to_fin(6.05), 1.45)

last = -9.0
for i, b in enumerate(thin):
    fin = to_fin(b)
    if fin is None or fin - last < 0.42:
        continue
    last = fin
    snap = i % 2 == 0
    add(
        (WILD + "wolf_bite_snap.ogg") if snap else (WILD + "wolf_snarl_bark.ogg"),
        fin,
        1.85 if snap else 1.50,
    )

for scn in (6.5, 7.6):
    add(ORDERS + "run_kit_rattle.ogg", to_fin(scn), 1.35)


for d in deaths:
    fin = to_fin(d)
    if fin is None:
        print(f"  WARNING: death at scenario {d}s is not inside any shot - no scream")
        continue
    add(COMBAT + "human_death_cry.ogg", fin - 0.08, 3.60)

for name, f, _ss, _dur, _slow in shots:
    if name == "turn_out":
        add(ORDERS + "attack_horn_stab.ogg", f + 0.05, 2.20)
        add(ORDERS + "run_kit_rattle.ogg", f + 0.35, 1.20)
    if name == "the_charge":
        add(COMBAT + "roman_cavalry_charge.ogg", f - 0.10, 1.55)
        add(COMBAT + "horse_gallop_close_pass.ogg", f + 0.90, 1.50)
    if name == "run_them_down":
        add(COMBAT + "numidian_cavalry_chase.ogg", f, 1.20)
    if name == "after":
        add(COMBAT + "soldiers_victory_cheer.ogg", f + 0.60, 1.10)

if melee:
    for k, off in enumerate((0.15, 0.85, 1.55, 2.4, 3.2)):
        add(
            (
                (WILD + "wolf_bite_snap.ogg")
                if k % 2 == 0
                else (WILD + "wolf_snarl_bark.ogg")
            ),
            to_fin(melee[0] + off),
            1.70 if k % 2 == 0 else 1.40,
        )

cues.sort(key=lambda c: c["at"])
spec["sfx"] = cues
json.dump(spec, open(spec_path, "w"), indent=2)
print(
    f"{len(cues)} cues on real events; {len(thin)} bites, {len(deaths)} deaths, "
    f"melee from {melee[0] if melee else '-'}"
)
