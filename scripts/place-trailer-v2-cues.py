#!/usr/bin/env python3
"""Place the sound cues for the "Command it. Fight in it." trailer.

Same idea as ``place-trailer-cues.py``: cues are authored against shot names
and offsets within a shot, so retiming or reordering the picture moves them
with it. This spec mixes arena shots (whose length the spec declares) with
clips filmed from the game and the editor (whose length is measured off the
file), so the timeline is computed from both.

    scripts/place-trailer-v2-cues.py
    scripts/promo-edit.py --spec tools/arena/promos/trailer_v2.json \\
        --clips artifacts/promo/trailer_v2
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SPEC = ROOT / "tools/arena/promos/trailer_v2.json"
SFX = "assets/audio/sfx/"
AMB = "assets/audio/ambience/"
VOX = "assets/audio/voices/"


CUES: list[tuple[str, float, str, float]] = [
    ("field_order", 0.05, SFX + "combat/battlefield_distant_mass_01.ogg", 0.62),
    ("field_order", 0.35, SFX + "combat/roman_war_horns_orders.ogg", 0.85),
    ("field_order", 1.10, SFX + "combat/army_march_dirt_mass.ogg", 0.55),
    ("into_the_saddle", 0.15, SFX + "orders/attack_horn_stab.ogg", 0.70),
    ("commander_view", 0.30, SFX + "combat/vanguard_rush.ogg", 0.75),
    ("commander_view", 0.75, SFX + "combat/charge_roar.ogg", 0.85),
    ("the_line_answers", 0.05, SFX + "combat/roman_shield_wall_impact.ogg", 0.90),
    ("build_place", 0.15, AMB + "roman_army_camp_01.ogg", 0.40),
    ("build_place", 0.90, SFX + "build/placement_confirmed.ogg", 0.60),
    ("build_place", 2.30, SFX + "build/construction_started.ogg", 0.55),
    ("build_place", 4.60, SFX + "build/construction_complete.ogg", 0.55),
    ("rome_builds", 0.20, AMB + "mediterranean_city_market.ogg", 0.42),
    ("build_rise", 0.10, SFX + "build/construction_complete.ogg", 0.50),
    ("meet_scipio", 0.10, SFX + "ui/command_accept.ogg", 0.55),
    ("meet_scipio", 0.30, VOX + "commanders/publius_cornelius_scipio.ogg", 0.95),
    ("scipio_strikes", 0.55, SFX + "combat/gladius_shield_impacts_close.ogg", 0.85),
    ("scipio_strikes", 1.35, SFX + "combat/sword_hit_01.ogg", 0.80),
    ("hannibal_intro", 0.20, VOX + "carthage/hannibal.ogg", 0.95),
    ("battle_high", 0.10, SFX + "combat/spearmen_formation_advance.ogg", 0.72),
    ("battle_high", 1.60, SFX + "combat/army_march_dirt_mass.ogg", 0.60),
    ("battle_high", 4.20, SFX + "combat/blade_clash_01.ogg", 0.80),
    ("bridge_storm", 0.20, SFX + "combat/roman_shield_wall_impact.ogg", 0.92),
    ("battle_cavalry", 0.15, SFX + "combat/horse_gallop_close_pass.ogg", 0.85),
    ("battle_cavalry", 1.60, SFX + "combat/battlefield_crowd_chaos.ogg", 0.55),
    ("turn_it_yourself", 0.30, SFX + "combat/blade_clash_02.ogg", 0.82),
    ("turn_it_yourself", 2.10, SFX + "combat/vanguard_rush.ogg", 0.85),
    ("turn_it_yourself", 3.40, SFX + "combat/guard_break.ogg", 0.80),
    ("turn_it_yourself", 6.20, SFX + "combat/soldiers_victory_cheer.ogg", 0.62),
    ("cold_march", 0.10, AMB + "weather_snow.ogg", 0.45),
    ("cold_march", 0.60, AMB + "mountain_camp_night.ogg", 0.50),
    ("grave_priests", 0.30, SFX + "combat/guard_break.ogg", 0.70),
    ("surrounded", 0.20, SFX + "combat/battlefield_crowd_chaos.ogg", 0.60),
    ("surrounded", 1.40, SFX + "combat/human_death_cry.ogg", 0.80),
    ("editor_route", 0.20, SFX + "build/placement_confirmed.ogg", 0.45),
    ("editor_mission", 0.20, SFX + "ui/click_confirm.ogg", 0.40),
    ("new_route", 0.10, SFX + "combat/army_march_dirt_mass.ogg", 0.62),
    ("lines_collide", 0.10, SFX + "combat/roman_shield_wall_impact.ogg", 0.95),
    ("lines_collide", 1.20, SFX + "combat/battlefield_crowd_chaos.ogg", 0.62),
]


def clip_seconds(path: Path) -> float:
    probe = subprocess.run(
        [
            "ffprobe",
            "-v",
            "error",
            "-show_entries",
            "format=duration",
            "-of",
            "csv=p=0",
            str(path),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    try:
        return float(probe.stdout.strip())
    except ValueError:
        return 0.0


def shot_slow_motion(shot: dict) -> float:
    if "time_lapse" in shot and "slow_motion" not in shot:
        return 1.0 / max(1.0, float(shot["time_lapse"]))
    return float(shot.get("slow_motion", 1.0))


def shot_length(shot: dict) -> float:
    """An external clip is as long as its file; an arena shot as long as authored."""
    if "clip" in shot:
        path = Path(shot["clip"])
        return clip_seconds(path if path.is_absolute() else ROOT / path)
    return float(shot["duration"]) * shot_slow_motion(shot)


def timeline(spec: dict) -> tuple[dict[str, float], float]:
    default = spec.get("transition", {"type": "cut"})
    starts: dict[str, float] = {}
    end = 0.0
    for index, shot in enumerate(spec["shots"]):
        join = shot.get("transition", default) or {"type": "cut"}
        overlap = 0.0
        if index > 0 and join.get("type") != "cut":
            overlap = float(join.get("duration", 0.35))
        start = max(0.0, end - overlap)
        starts[shot["name"]] = start
        end = start + shot_length(shot)
    return starts, end


def main() -> int:
    spec = json.loads(SPEC.read_text())
    starts, total = timeline(spec)

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
                f"place-trailer-v2-cues: {path} on '{name}' falls past the cut",
                file=sys.stderr,
            )
            continue
        cues.append({"file": path, "at": round(at, 3), "gain": gain})

    for name in sorted(missing_shots):
        print(f"place-trailer-v2-cues: no shot named '{name}'", file=sys.stderr)
    for path in sorted(missing_files):
        print(f"place-trailer-v2-cues: missing audio {path}", file=sys.stderr)
    if missing_shots or missing_files:
        return 1

    cues.sort(key=lambda cue: cue["at"])
    spec["sfx"] = cues
    SPEC.write_text(json.dumps(spec, indent=2) + "\n")
    print(f"place-trailer-v2-cues: placed {len(cues)} cue(s) across {total:.1f}s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
