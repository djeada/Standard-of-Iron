#!/usr/bin/env python3
"""Place the trailer's sound cues on the finished timeline and write them back.

The trailer is cut from eighteen scenarios plus a rendered act card, so the
cue-per-scenario-event approach ``place-promo-cues.py`` uses for a single-scene
reel does not apply: there is no one trace to read. What the cues actually hang
off here is *editorial* -- the act cards, the cut into the charge, the frame the
lines meet -- so they are authored against shot names and offsets within a shot,
and this script resolves them onto the blended timeline.

Authoring against shot names is the point. Retiming a shot, reordering the
acts or changing a transition moves every cue with it, so the cue list cannot
rot the way hand-typed absolute times do. Re-run this after any change to the
picture, before the edit:

    scripts/place-trailer-cues.py
    scripts/promo-edit.py --spec tools/arena/promos/trailer.json \\
        --clips artifacts/promo/trailer

Each entry in ``CUES`` is ``(shot name, offset, audio path, gain)``. The offset
is measured in finished seconds from the start of that shot, so it already
accounts for slow motion; a negative offset places the cue before the cut,
which is how an impact is made to land *on* it rather than after it. A cue
naming a shot or a file that does not exist fails the run rather than being
dropped, because a silently missing hit is indistinguishable from a mix that
was never checked.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SPEC = (
    Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "tools/arena/promos/trailer.json"
)

SFX = "assets/audio/sfx/"
AMB = "assets/audio/ambience/"


CUES: list[tuple[str, float, str, float]] = [
    ("card_open", 0.05, SFX + "combat/roman_war_horns_orders.ogg", 0.80),
    ("valley_reveal", 0.10, AMB + "mediterranean_plains.ogg", 0.42),
    ("works_yard", 0.20, AMB + "siege_camp.ogg", 0.40),
    ("works_yard", 0.60, SFX + "build/construction_started.ogg", 0.55),
    ("works_wall", 0.30, SFX + "build/placement_confirmed.ogg", 0.50),
    ("works_caravan", 0.25, AMB + "roman_road.ogg", 0.45),
    ("sanctuary_road", 0.20, AMB + "mediterranean_plains.ogg", 0.38),
    ("sanctuary_healers", 0.40, SFX + "combat/heal_bind_wound.ogg", 0.55),
    ("punic_town", 0.20, AMB + "mediterranean_city_market.ogg", 0.45),
    ("mill_town", 0.20, AMB + "river_crossing.ogg", 0.48),
    ("flock", 0.35, SFX + "wildlife/wolf_howl_distant.ogg", 0.45),
    ("wolf_rush", -0.15, SFX + "wildlife/wolf_pack_attack.ogg", 0.95),
    ("wolf_rush", 1.30, SFX + "wildlife/wolf_bite_snap.ogg", 0.85),
    ("card_legion", 0.05, SFX + "combat/roman_war_horns_orders.ogg", 0.85),
    ("gate_open", 0.20, AMB + "roman_army_camp_01.ogg", 0.45),
    ("gate_open", 0.55, SFX + "build/gate_open.ogg", 0.80),
    ("gate_horse", 0.20, SFX + "combat/horse_gallop_close_pass.ogg", 0.75),
    ("gate_siege", 0.25, SFX + "combat/army_march_dirt_mass.ogg", 0.65),
    ("iron_line", 0.15, SFX + "combat/spearmen_formation_advance.ogg", 0.78),
    ("punic_crescent", 0.20, SFX + "combat/carthage_prepare_battle.ogg", 0.75),
    ("high_pass", 0.10, AMB + "alpine_mountain_pass.ogg", 0.55),
    ("high_pass", 0.60, AMB + "weather_snow.ogg", 0.42),
    ("wood_march", 0.10, AMB + "weather_rain.ogg", 0.48),
    ("wood_ambush", -0.20, SFX + "combat/vanguard_rush.ogg", 0.90),
    ("wood_ambush", 1.40, SFX + "combat/blade_clash_01.ogg", 0.82),
    ("card_battle", 0.05, SFX + "combat/charge_roar.ogg", 0.90),
    ("hosts_face", 0.15, SFX + "combat/battlefield_distant_mass_01.ogg", 0.55),
    ("bridge_hold", 0.20, SFX + "combat/spearmen_formation_advance.ogg", 0.70),
    ("bridge_storm", -0.20, SFX + "combat/roman_shield_wall_impact.ogg", 0.95),
    ("bridge_storm", 1.90, SFX + "combat/gladius_shield_impacts_close.ogg", 0.80),
    ("siege_range", 0.30, SFX + "combat/siege_launch.ogg", 0.85),
    ("siege_range", 1.60, SFX + "combat/stone_impact_01.ogg", 0.85),
    ("wall_falls", -0.20, SFX + "combat/elephant_charge_carthage.ogg", 0.95),
    ("wall_falls", 2.10, SFX + "build/building_destroyed.ogg", 0.85),
    ("storm_charge", 0.10, AMB + "storm.ogg", 0.50),
    ("storm_charge", 0.70, SFX + "combat/roman_cavalry_charge.ogg", 0.85),
    ("lines_collide", -0.20, SFX + "combat/roman_shield_wall_impact.ogg", 1.00),
    ("lines_collide", 1.40, SFX + "combat/battlefield_crowd_chaos.ogg", 0.62),
    ("consuls_duel", 0.30, SFX + "combat/blade_clash_02.ogg", 0.88),
    ("consuls_duel", 1.90, SFX + "combat/sword_hit_03.ogg", 0.82),
    ("pov_wade", 0.20, SFX + "combat/blade_clash_04.ogg", 0.85),
    ("pov_wade", 2.40, SFX + "combat/shield_bash.ogg", 0.80),
    ("card_night", 0.05, SFX + "combat/siege_impact.ogg", 0.55),
    ("cold_march", 0.15, AMB + "mountain_camp_night.ogg", 0.55),
    ("cold_march", 0.70, AMB + "weather_snow.ogg", 0.45),
    ("grave_priests", 0.30, AMB + "forest_ambush.ogg", 0.55),
    ("surrounded", -0.20, SFX + "combat/guard_break.ogg", 0.78),
    ("surrounded", 1.60, SFX + "combat/battlefield_crowd_chaos.ogg", 0.52),
    ("last_stand", 0.30, SFX + "combat/blade_clash_03.ogg", 0.88),
    ("last_stand", 2.10, SFX + "combat/shield_block.ogg", 0.80),
    ("last_breath", 0.40, SFX + "combat/guard_break.ogg", 0.85),
    ("last_breath", 2.20, SFX + "combat/armour_hit_02.ogg", 0.85),
    ("last_breath", 3.30, SFX + "combat/human_death_cry.ogg", 0.90),
]


def timeline(spec: dict) -> dict[str, float]:
    """Finished start second of every shot, on the blended timeline."""
    default = spec.get("transition", {"type": "dissolve", "duration": 0.35})
    starts: dict[str, float] = {}
    end = 0.0
    for index, shot in enumerate(spec["shots"]):
        join = shot.get("transition", default) or {"type": "cut"}
        overlap = 0.0
        if index > 0 and join.get("type") != "cut":
            overlap = float(join.get("duration", 0.35))
        start = max(0.0, end - overlap)
        starts[shot["name"]] = start
        end = start + float(shot["duration"]) * float(shot.get("slow_motion", 1.0))
    starts["__total__"] = end
    return starts


def main() -> int:
    spec = json.loads(SPEC.read_text())
    starts = timeline(spec)
    total = starts.pop("__total__")

    cues = []
    missing_shots: set[str] = set()
    missing_files: set[str] = set()
    for name, offset, path, gain in CUES:
        if name not in starts:
            missing_shots.add(name)
            continue
        if not (ROOT / path).is_file():
            missing_files.add(path)
            continue
        at = max(0.0, starts[name] + offset)
        if at >= total:
            print(
                f"place-trailer-cues: {path} on '{name}' falls past the cut",
                file=sys.stderr,
            )
            continue
        cues.append({"file": path, "at": round(at, 3), "gain": gain})

    for name in sorted(missing_shots):
        print(f"place-trailer-cues: no shot named '{name}'", file=sys.stderr)
    for path in sorted(missing_files):
        print(f"place-trailer-cues: missing audio {path}", file=sys.stderr)
    if missing_shots or missing_files:
        return 1

    cues.sort(key=lambda cue: cue["at"])
    spec["sfx"] = cues
    SPEC.write_text(json.dumps(spec, indent=2) + "\n")
    print(f"place-trailer-cues: placed {len(cues)} cue(s) across {total:.1f}s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
