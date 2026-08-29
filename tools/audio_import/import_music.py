"""Bring generated music renders up to the shipped music conventions.

Usage:
    python3 tools/audio_music/import_music.py --source DIR [--out DIR] [--dry-run]

Format follows `assets/audio/music`: Vorbis, 32 kHz (the mixer rate, `tools/audio_synth/dsp.py`
SR), mono, `-q:a 4` -- nominal 78 kbps, the same quality constant as
`tools/audio_field/sources.py`.

Level is the part that is easy to get wrong. `game/audio/audio_mastering.cpp` normalises music
to -15 LUFS but grants only +/-6 dB of authority, further capped by
`ceiling_db - peak + MAX_LIMITING_DB`, so a file peaking at -1 dBTP can be lifted at most 4 dB.
A render baked below about -19 LUFS therefore plays under the rest of the library for good, and
no runtime setting recovers it.

So: pure linear gain toward the house level, capped so true peak stays at -1.0 dBTP
(`Profile::ceiling_db` for `Material::Music`), and a look-ahead limiter allowed to close at
most LIMIT_HEADROOM_DB of what is left. Limiting is bounded on purpose -- these renders carry
12-20 LU of range and crushing that to hit a number is the worse trade. The script reports the
level each track will actually play at in game, and flags any that land short.

The shipped files are deliberately not mastered to the +0.1..+2.8 dBTP the previous set
decoded at; `docs/AUDIO_MASTERING.md` names those peaks as a defect.

BATCHES records what each import renamed, so provenance survives the source folder being
deleted. Add a new entry per batch rather than editing an old one.
"""

import argparse
import json
import pathlib
import re
import subprocess
import sys

RATE, QUALITY, TARGET_LUFS, CEILING_DBTP = 32000, 4, -14.1, -1.0

RUNTIME_FLOOR_LUFS, LIMIT_HEADROOM_DB, LIMITER_CEILING = -19.0, 3.0, 0.871


BATCHES = {
    "2026-08-punic": {
        "Standard_of_Iron_Main_Theme_2026-08-28T210251": "menu/main_theme_standard_of_iron",
        "Standard_of_Iron_Main_Theme_ALT": "menu/main_theme_standard_of_iron_alt",
        "Hannibals_Ascent_2026-08-28T204617": "campaign/campaign_hannibals_ascent",
        "The_Crossing_of_the_Alps_2026-08-28T204617": "campaign/campaign_crossing_of_the_alps",
        "The_Dust_of_Cannae_2026-08-28T210020": "combat/combat_dust_of_cannae",
        "Dust_of_Trasimene_2026-08-28T210020": "combat/combat_dust_of_trasimene",
        "Cavalry_of_Carthage_2026-08-28T205444": "combat/combat_cavalry_of_carthage",
        "Carthaginian_Dust_2026-08-28T205446": "combat/combat_carthaginian_dust",
        "The_Shield_Wall_at_Dusk_2026-08-28T210210": "combat/combat_shield_wall_at_dusk",
        "The_Last_Defensive_Wall_2026-08-28T210210": "combat/combat_last_defensive_wall",
        "Ancient_Preparations_2026-08-28T205218": "base/base_ancient_preparations",
        "Campfire_Shadows_of_Carthage_2026-08-28T204014": "base/base_campfire_shadows_carthage",
        "Uneasy_Rest_at_the_Punic_Camp_2026-08-28T204959": "base/base_uneasy_rest_punic_camp",
        "Uneasy_Rest_at_the_Punic_Camp_2026-08-28T204014": "base/base_uneasy_rest_punic_camp_alt",
        "Hearth_and_Harbor_of_Old_2026-08-28T204417": "base/base_hearth_and_harbor",
        "Sunlight_Over_the_Olive_Groves_2026-08-28T204417": "base/base_sunlight_olive_groves",
        "Echoes_of_the_Legion_at_Dusk_2026-08-28T205219": "base/base_legion_at_dusk",
        "Sentinels_of_the_Peak_2026-08-28T210721": "base/base_sentinels_of_the_peak",
        "Guardians_of_the_Forest_Throne_2026-08-29T075035": "events/skeletons_awaken",
        "Ancient_Peak_Fires_2026-08-28T210704": "base/base_ancient_peak_fires",
        "Echoes_of_the_Ancient_Outpost_2026-08-28T210704": "base/base_echoes_ancient_outpost",
        "Triumph_of_Carthage_2026-08-28T210547": "stingers/victory_carthage_triumph",
        "The_Gates_of_Carthage_2026-08-28T210547": "stingers/victory_fanfare",
        "Echo_of_Defeat_2026-08-28T210512": "stingers/defeat_echo",
        "Tragic_Silence_on_the_Field_2026-08-28T210427": "stingers/defeat_tragic_silence_field",
        "The_Quiet_Field_2026-08-28T210427": "stingers/defeat_quiet_field",
        "Signal_Into_Wind_2026-08-28T210511": "stingers/retreat_signal_into_wind",
    },
}


def loudness(path):
    proc = subprocess.run(
        [
            "ffmpeg",
            "-v",
            "info",
            "-i",
            str(path),
            "-af",
            "loudnorm=print_format=json",
            "-f",
            "null",
            "-",
        ],
        capture_output=True,
        text=True,
    )
    blob = re.findall(r"\{[^{}]*\"input_i\".*?\}", proc.stderr, re.S)[-1]
    data = json.loads(blob)
    return float(data["input_i"]), float(data["input_tp"])


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument(
        "--source",
        required=True,
        type=pathlib.Path,
        help="directory of master renders (FLAC/WAV)",
    )
    ap.add_argument(
        "--out",
        type=pathlib.Path,
        default=pathlib.Path("assets/audio/music"),
        help="destination, default assets/audio/music",
    )
    ap.add_argument(
        "--batch",
        default="2026-08-punic",
        choices=sorted(BATCHES),
        help="which rename map to apply",
    )
    ap.add_argument(
        "--work",
        type=pathlib.Path,
        default=pathlib.Path("/tmp/soi-music-import"),
        help="scratch directory for the mono intermediates",
    )
    ap.add_argument(
        "--dry-run",
        action="store_true",
        help="measure and report without writing any .ogg",
    )
    args = ap.parse_args()

    name_map = BATCHES[args.batch]
    args.work.mkdir(parents=True, exist_ok=True)
    rows = []
    for src in sorted(args.source.iterdir()):
        if src.is_dir() or src.suffix.lower() not in (".flac", ".wav"):
            continue
        dest_stem = name_map.get(src.stem)
        if dest_stem is None:
            print(f"no mapping for {src.name}, skipped", file=sys.stderr)
            continue

        mono = args.work / (src.stem + ".wav")
        subprocess.run(
            [
                "ffmpeg",
                "-y",
                "-v",
                "error",
                "-i",
                str(src),
                "-ac",
                "1",
                "-ar",
                str(RATE),
                "-c:a",
                "pcm_f32le",
                str(mono),
            ],
            check=True,
        )
        lufs, peak = loudness(mono)
        gain = min(TARGET_LUFS - lufs, CEILING_DBTP - peak)
        limited = max(0.0, min(LIMIT_HEADROOM_DB, RUNTIME_FLOOR_LUFS - (lufs + gain)))
        chain = f"volume={gain + limited:.2f}dB"
        if limited > 0.0:
            chain += (
                f",alimiter=limit={LIMITER_CEILING}:attack=5:release=60"
                f":level=disabled"
            )
        out = args.out / (dest_stem + ".ogg")
        if args.dry_run:
            rows.append((dest_stem + ".ogg", lufs, gain, limited, None, None, None, 0))
            continue
        out.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            [
                "ffmpeg",
                "-y",
                "-v",
                "error",
                "-i",
                str(mono),
                "-af",
                chain,
                "-c:a",
                "libvorbis",
                "-q:a",
                str(QUALITY),
                "-ac",
                "1",
                "-ar",
                str(RATE),
                str(out),
            ],
            check=True,
        )
        final_i, final_tp = loudness(out)

        in_game = final_i + min(6.0, -15.0 - final_i, -1.0 - final_tp + 4.0)
        rows.append(
            (
                dest_stem + ".ogg",
                lufs,
                gain,
                limited,
                final_i,
                final_tp,
                in_game,
                out.stat().st_size,
            )
        )

    print(
        f"{'track':46} {'monoI':>6} {'gain':>6} {'lim':>5} {'outI':>6} {'outTP':>6} "
        f"{'ingame':>7} {'KiB':>6}"
    )
    short = 0
    for name, lufs, gain, limited, fi, ftp, ig, size in sorted(rows):
        if fi is None:
            print(f"{name:46} {lufs:6.1f} {gain:+6.1f} {limited:5.1f}  (dry run)")
            continue
        flag = ""
        if ig < -15.8:
            flag = f"  <- plays {abs(ig + 15.0):.1f} dB under the library"
            short += 1
        print(
            f"{name:46} {lufs:6.1f} {gain:+6.1f} {limited:5.1f} {fi:6.1f} {ftp:6.1f} "
            f"{ig:7.1f} {size / 1024:6.0f}{flag}"
        )
    if short:
        print(
            f"\n{short} track(s) land short; re-render them louder if it matters.",
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
