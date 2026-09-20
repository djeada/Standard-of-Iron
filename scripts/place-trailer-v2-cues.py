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
    ("city_reveal", 0.05, SFX + "combat/battlefield_distant_mass_01.ogg", 0.62),
    ("city_reveal", 0.35, SFX + "combat/roman_war_horns_orders.ogg", 0.85),
    ("command_view", 0.60, SFX + "ui/command_accept.ogg", 0.50),
    ("ballista_volley", 0.30, SFX + "combat/arrows_many_overhead.ogg", 0.55),
    ("engines_loose", 0.20, SFX + "combat/siege_launch.ogg", 0.80),
    ("into_the_saddle", 0.15, SFX + "orders/attack_horn_stab.ogg", 0.70),
    ("scipio_pov", 0.05, SFX + "combat/charge_roar.ogg", 0.85),
    ("scipio_pov", 0.90, SFX + "combat/gladius_shield_impacts_close.ogg", 0.85),
    ("scipio_pov", 2.40, SFX + "combat/roman_shield_wall_impact.ogg", 0.80),
    ("town_wide", 0.20, AMB + "mediterranean_city_market.ogg", 0.40),
    ("town_crews", 0.70, SFX + "ui/click_confirm.ogg", 0.40),
    ("town_build", 0.80, SFX + "build/placement_confirmed.ogg", 0.55),
    ("town_build", 2.20, SFX + "build/construction_started.ogg", 0.50),
    ("town_busy", 1.40, SFX + "build/construction_complete.ogg", 0.55),
    # The formation beat, two orders and two payoffs: the order shots are cued
    # off what is happening in the panel (select, pick, drag, confirm) and the
    # payoffs off the ranks (march, halt, plant).
    ("form_the_line", 0.15, SFX + "ui/command_accept.ogg", 0.50),
    ("form_the_line", 1.00, SFX + "orders/formation_pole_shift.ogg", 0.60),
    ("form_the_line", 3.05, SFX + "combat/army_march_dirt_mass.ogg", 0.55),
    ("battle_line", 0.20, SFX + "combat/army_march_dirt_mass.ogg", 0.50),
    ("battle_line", 2.40, SFX + "combat/army_march_dirt_mass.ogg", 0.45),
    ("battle_line", 3.40, SFX + "orders/hold_shields_plant.ogg", 0.70),
    ("battle_line", 3.95, SFX + "orders/formation_standard_planted.ogg", 0.65),
    ("bridge_order", 0.15, SFX + "ui/command_accept.ogg", 0.50),
    ("bridge_order", 0.60, SFX + "ui/click_confirm.ogg", 0.45),
    ("bridge_order", 2.85, SFX + "orders/formation_pole_shift.ogg", 0.60),
    ("bridge_column", 0.10, SFX + "combat/army_march_dirt_mass.ogg", 0.50),
    ("bridge_column", 2.00, SFX + "combat/army_march_dirt_mass.ogg", 0.45),
    ("bridge_column", 4.00, SFX + "orders/hold_shields_plant.ogg", 0.60),
    ("editor_bridge", 0.30, SFX + "ui/click_confirm.ogg", 0.40),
    ("editor_bridge", 1.20, SFX + "build/placement_confirmed.ogg", 0.45),
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
