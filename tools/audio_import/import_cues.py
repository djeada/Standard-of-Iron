"""Bring vendor-generated one-shot cues up to the shipped sfx conventions.

Usage:
    python3 tools/audio_import/import_cues.py --source DIR [--dry-run]

Generated cues arrive padded to a round duration with the sound sitting somewhere inside
the padding, at whatever level the service felt like. Three things have to happen before
they are shippable.

**Trim.** The sound is located from the loudest transient outward, not from the first
non-silent sample, because these renders often open with room tone seconds before the hit.
A cue that starts with silence reads in game as the game reacting late.

**Level.** Effect cues are deliberately *not* loudness-normalised at runtime -- the level in
the file is the design decision (docs/AUDIO_MASTERING.md). So each import is matched to the
RMS of the file it replaces, which keeps the existing mix intact, and ceilinged at
-1.9 dBFS the way the CC0 battle cues were.

**Format.** Vorbis, 48 kHz, mono, -q:a 4 -- what tools/audio_field/sources.py produces and
what every recorded cue in the tree already is. (The older synthesised cues are 32 kHz; new
recorded-quality material should not be down-rated to match them.)
"""

import argparse
import pathlib
import subprocess
import sys

import numpy as np

SR = 48000
QUALITY = 4
CEILING_DBFS = -1.9
PRE_ROLL_S = 0.005


SILENCE_FRACTION = 0.03
TAIL_PAD_S = 0.05
FADE_IN_S = 0.003
FADE_OUT_S = 0.030


PLAN = {
    "Stiff_Leather_Tap": ("sfx/ui/click_confirm.ogg", None, None),
    "Fingertip_and_Parchment": ("sfx/ui/hover_brush.ogg", None, None),
    "Shield_Boss_Knock": ("sfx/ui/select_unit.ogg", None, None),
    "Leather_Lift_Brush": ("sfx/ui/deselect.ogg", None, None),
    "Toggle_Latch_UI_Click": ("sfx/ui/toggle_latch.ogg", None, None),
    "Wooden_Box_Closure": ("sfx/ui/back_cancel.ogg", None, None),
    "Bronze_Seal_Press": ("sfx/ui/confirm_seal.ogg", None, None),
    "Shield_Thud_Error": ("sfx/ui/error_thud.ogg", None, None),
    "Leather_Shield_Impact": ("sfx/ui/error_thud_v2.ogg", None, None),
    "Spear_and_Shield_Impact": ("sfx/ui/command_accept.ogg", None, None),
    "Command_Refuse": ("sfx/ui/command_refuse.ogg", None, None),
    "No_No_No": ("sfx/ui/command_refuse_v2.ogg", None, None),
    "Roman_War_Horn_Blast": ("sfx/orders/attack_horn_stab.ogg", None, None),
    "The_Patrol_Horn": ("sfx/orders/patrol_horn_two_note.ogg", None, None),
    "Damped_War_Drum_Stroke": ("sfx/orders/stop_drum.ogg", None, None),
    "Shields_Planted": ("sfx/orders/hold_shields_plant.ogg", None, None),
    "Guard_Spear_Taps": ("sfx/orders/guard_spear_taps.ogg", None, None),
    "Standard_Pole_Placement": (
        "sfx/orders/formation_standard_planted.ogg",
        None,
        None,
    ),
    "Resonance_in_the_Courtyard": ("sfx/build/unit_ready_bell.ogg", None, None),
    "Hammers_and_Nails": ("sfx/build/placement_confirmed.ogg", None, None),
    "Unit_Lost_Signal": (
        "sfx/alerts/unit_lost.ogg",
        None,
        "sfx/alerts/objective_failed.ogg",
    ),
    "Objective_Failed": ("sfx/alerts/objective_failed.ogg", None, None),
    "Horn_of_the_Enemy": ("sfx/alerts/enemy_reinforcements_warning.ogg", None, None),
    "No_Peasants_Remain": ("sfx/alerts/population_limit_horn.ogg", None, None),
    "The_Demand_of_the_Builders": ("sfx/alerts/low_resources_click.ogg", None, None),
    "The_Commanders_Folly": (
        "sfx/alerts/commander_message.ogg",
        None,
        "sfx/alerts/objective_failed.ogg",
    ),
    "Rise_of_the_Ossuary": (
        "sfx/undead/skeletons_rise.ogg",
        None,
        "sfx/alerts/objective_failed.ogg",
    ),
    "Armored_Soldier_Stagger": (
        "sfx/combat/stagger.ogg",
        None,
        "sfx/combat/shield_block.ogg",
    ),
    "Economy_Income_Tick": (
        "sfx/economy/income_tick.ogg",
        None,
        "sfx/ui/click_confirm.ogg",
    ),
    "Charge_of_the_War_Elephant": (
        "sfx/combat/elephant_trumpet_charge.ogg",
        None,
        "sfx/combat/elephant_charge_carthage.ogg",
    ),
    "Distressed_War_Elephant": (
        "sfx/combat/elephant_trumpet_panic.ogg",
        None,
        "sfx/combat/elephant_panic.ogg",
    ),
    "Pastoral_Sheep_Bleat": (
        "sfx/wildlife/sheep_bleat.ogg",
        None,
        "sfx/wildlife/wolf_howl_distant.ogg",
    ),
    "Morning_Bird_Chirp": (
        "sfx/wildlife/bird_chirp.ogg",
        None,
        "sfx/wildlife/wolf_howl_distant.ogg",
    ),
    "HoresForest_Path_Walk": (
        "sfx/movement/hooves_walk.ogg",
        None,
        "sfx/movement/footstep_grass_01.ogg",
    ),
    "Galloping_Hooves": (
        "sfx/movement/hooves_gallop.ogg",
        None,
        "sfx/movement/footstep_run_01.ogg",
    ),
    "Timber_Structure_Inferno": (
        "sfx/build/building_burning.ogg",
        None,
        "ambience/camp_fire_night.ogg",
    ),
}

AUDIO = pathlib.Path("assets/audio")


def decode(path: pathlib.Path) -> np.ndarray:
    raw = subprocess.run(
        [
            "ffmpeg",
            "-v",
            "error",
            "-i",
            str(path),
            "-ac",
            "1",
            "-ar",
            str(SR),
            "-f",
            "f32le",
            "-",
        ],
        capture_output=True,
        check=True,
    ).stdout
    return np.frombuffer(raw, dtype=np.float32).astype(np.float64)


def envelope(x: np.ndarray, hop: int = 240) -> np.ndarray:
    n = (len(x) - hop) // hop
    return np.sqrt(
        np.array([(x[i * hop : i * hop + hop] ** 2).mean() for i in range(n)])
    )


def locate(x: np.ndarray, _unused: object = None) -> tuple[int, int]:
    """Trim the padding the generator added. Keep everything else.

    Generated renders arrive padded to a round duration with the sound somewhere inside.
    Only that padding is removed: from the first substantial onset to the last sample still
    above the noise floor, plus a little air either side.

    This deliberately has no maximum length. An earlier version capped each cue and picked
    the window around the loudest transient, which silently threw away most of what was
    generated -- `Horn_of_the_Enemy` shipped as its noisy tail instead of the horn, and the
    Sepulcher awakening shipped 4 seconds of a 10 second rise. The render is the sound; if
    it is too long for a cue, regenerate it shorter rather than cutting it here.
    """
    hop = 240
    env = envelope(x, hop)
    if len(env) == 0:
        return 0, len(x)

    peak = env.max()
    floor = peak * SILENCE_FRACTION
    above = np.flatnonzero(env >= floor)
    if len(above) == 0:
        return 0, len(x)
    onset, last = int(above[0]), int(above[-1])
    s = max(0, int(onset * hop - PRE_ROLL_S * SR))
    e = min(len(x), int((last + 1) * hop + TAIL_PAD_S * SR))
    return s, e


def shape(x: np.ndarray) -> np.ndarray:
    """Fade both ends so the file can never start or end on a step."""
    y = x.copy()
    fi = min(int(FADE_IN_S * SR), len(y) // 4)
    fo = min(int(FADE_OUT_S * SR), len(y) // 3)
    if fi > 0:
        y[:fi] *= np.linspace(0.0, 1.0, fi)
    if fo > 0:
        y[-fo:] *= np.linspace(1.0, 0.0, fo)
    return y


def rms(x: np.ndarray) -> float:
    return float(np.sqrt((x**2).mean()))


def write(y: np.ndarray, dest: pathlib.Path) -> int:
    dest.parent.mkdir(parents=True, exist_ok=True)
    pcm = np.clip(y, -1.0, 1.0).astype(np.float32).tobytes()
    subprocess.run(
        [
            "ffmpeg",
            "-y",
            "-v",
            "error",
            "-f",
            "f32le",
            "-ar",
            str(SR),
            "-ac",
            "1",
            "-i",
            "pipe:0",
            "-c:a",
            "libvorbis",
            "-q:a",
            str(QUALITY),
            str(dest),
        ],
        input=pcm,
        check=True,
    )
    return dest.stat().st_size


def level_to(y: np.ndarray, reference: pathlib.Path | None) -> tuple[np.ndarray, str]:
    note = "as generated"
    if reference is not None and reference.exists():
        target = rms(decode(reference))
        cur = rms(y)
        if cur > 0 and target > 0:
            y = y * (target / cur)
            note = f"matched {reference.name} ({20 * np.log10(target / cur):+.1f} dB)"
    peak = np.max(np.abs(y)) if len(y) else 0.0
    ceiling = 10 ** (CEILING_DBFS / 20)
    if peak > ceiling:
        y = y * (ceiling / peak)
        note += f", ceilinged {20 * np.log10(ceiling / peak):+.1f} dB"
    return y, note


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("--source", required=True, type=pathlib.Path)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument(
        "--only",
        default=None,
        help="reprocess a single source stem instead of the batch",
    )
    args = ap.parse_args()

    by_stem = {}
    for p in sorted(args.source.iterdir()):
        if p.suffix.lower() not in (".flac", ".wav", ".mp3"):
            continue
        stem = p.stem.rsplit("_2026-", 1)[0]

        if stem not in by_stem or p.suffix.lower() == ".flac":
            by_stem[stem] = p

    rows, missing = [], []
    for stem, (dest_rel, _cap, ref_rel) in PLAN.items():
        if args.only and stem != args.only:
            continue
        src = by_stem.get(stem)
        if src is None:
            missing.append(stem)
            continue
        x = decode(src)
        s, e = locate(x)
        y = shape(x[s:e])
        ref = AUDIO / (ref_rel if ref_rel else dest_rel)
        y, note = level_to(y, ref)
        dest = AUDIO / dest_rel
        size = 0 if args.dry_run else write(y, dest)
        rows.append((dest_rel, len(x) / SR, len(y) / SR, s / SR, note, size))

    print(f"{'destination':44} {'src':>5} {'out':>5} {'from':>5} {'KiB':>5}  level")
    for rel, raw_s, out_s, at, note, size in sorted(rows):
        print(
            f"{rel:44} {raw_s:5.1f} {out_s:5.2f} {at:5.2f} {size / 1024:5.0f}  {note}"
        )
    print(f"\n{len(rows)} cues written")
    if missing and not args.only:
        print("no source found for:", ", ".join(sorted(missing)), file=sys.stderr)
    unused = [] if args.only else sorted(set(by_stem) - set(PLAN))
    if unused:
        print("source not used by any plan entry:", ", ".join(unused), file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
