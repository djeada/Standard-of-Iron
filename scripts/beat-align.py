#!/usr/bin/env python3
"""Put a promo spec's cuts on a track's beat.

A short-form reel is cut *to* the music: every cut, and every punch-in inside a
shot, lands on a beat. A spec authored by eye does not do that, and the
difference is the whole reason a reel reads as an edit rather than as a trailer
with music over it.

    python3 tools/audio_synth/reel_score.py --bpm 140 --bars 22 \
        --out artifacts/promo/reel_beat.ogg
    python3 scripts/beat-align.py tools/arena/promos/commander_rally.json \
        --track artifacts/promo/reel_beat.ogg

Each shot's *finished* length (``duration * slow_motion``) is quantised to a
whole number of beats, so the joins land on the grid without the editor having
to do arithmetic, and ``punch`` marks are written onto the beats inside each
shot. Because the length changes, the clips have to be re-captured afterwards.

The grid comes from the track's ``.grid.json`` sidecar when the track was
synthesised here, and is detected from the audio otherwise. ``--detect`` reports
what it found without touching the spec, which is also how the detector is
checked against a track whose tempo is known by construction.
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from pathlib import Path


def shot_slow_motion(shot: dict) -> float:
    """`time_lapse` is `slow_motion` written for values below one."""
    if "time_lapse" in shot and "slow_motion" not in shot:
        return 1.0 / max(1.0, float(shot["time_lapse"]))
    return float(shot.get("slow_motion", 1.0))


SAMPLE_RATE = 8000
HOP = 128
MIN_BPM, MAX_BPM = 70.0, 190.0

MIN_CONFIDENCE = 2.0


MIN_ONSET_STRENGTH = 0.002


def fail(message: str) -> None:
    print(f"beat-align: {message}", file=sys.stderr)
    raise SystemExit(1)


def onset_flux(track: Path) -> list[float]:
    """Half-wave rectified energy rise per frame: where a hit starts."""
    result = subprocess.run(
        [
            "ffmpeg",
            "-v",
            "error",
            "-i",
            str(track),
            "-ac",
            "1",
            "-ar",
            str(SAMPLE_RATE),
            "-f",
            "s16le",
            "-",
        ],
        capture_output=True,
        check=False,
    )
    if result.returncode != 0 or not result.stdout:
        fail(f"could not decode {track}")
    raw = result.stdout
    count = len(raw) // 2
    samples = [
        int.from_bytes(raw[i * 2 : i * 2 + 2], "little", signed=True) / 32768.0
        for i in range(count)
    ]
    energies = []
    for start in range(0, len(samples) - HOP, HOP):
        window = samples[start : start + HOP]
        energies.append(math.sqrt(sum(x * x for x in window) / HOP))
    return [max(0.0, b - a) for a, b in zip(energies, energies[1:], strict=False)]


def detect_grid(track: Path) -> dict:
    """Tempo by autocorrelation of the onset flux, then the phase that fits it."""
    flux = onset_flux(track)
    if len(flux) < 200:
        fail(f"{track} is too short to find a beat in")
    frames_per_second = SAMPLE_RATE / HOP
    mean = sum(flux) / len(flux)
    centred = [value - mean for value in flux]

    def prior(candidate: float) -> float:
        octaves = math.log2(candidate / 120.0)
        return math.exp(-0.5 * (octaves / 0.9) ** 2)

    best_bpm, best_score = 0.0, -1.0
    bpm = MIN_BPM
    while bpm <= MAX_BPM:
        lag = frames_per_second * 60.0 / bpm
        whole = int(round(lag))
        if whole >= len(centred) // 2:
            bpm += 0.5
            continue
        score = sum(
            centred[i] * centred[i + whole] for i in range(len(centred) - whole)
        )
        score = (score / (len(centred) - whole)) * prior(bpm)
        if score > best_score:
            best_bpm, best_score = bpm, score
        bpm += 0.5

    period = frames_per_second * 60.0 / best_bpm
    best_phase, best_energy = 0.0, -1.0
    steps = max(1, int(round(period)))
    for step in range(steps):
        hits = [
            flux[int(round(step + k * period))]
            for k in range(int((len(flux) - step) / period))
        ]
        energy = sum(hits) / max(1, len(hits))
        if energy > best_energy:
            best_phase, best_energy = step / frames_per_second, energy
    floor = sum(flux) / len(flux)
    confidence = round(best_energy / floor, 2) if floor > 0 else 0.0
    if floor < MIN_ONSET_STRENGTH:
        fail(
            f"{track.name} has no onsets to find a beat in (mean onset strength "
            f"{floor:.5f}, needs {MIN_ONSET_STRENGTH}). This is a held or ambient "
            "sound rather than a track with hits in it."
        )
    if confidence < MIN_CONFIDENCE:
        fail(
            f"{track.name} has no steady beat to cut to (confidence {confidence}, "
            f"needs {MIN_CONFIDENCE}). Orchestral score swells and rests instead of "
            "landing on a grid; synthesise a bed with "
            "tools/audio_synth/reel_score.py, or pass a track that has a pulse."
        )
    return {
        "bpm": round(best_bpm, 2),
        "first_beat": round(best_phase, 4),
        "beat_seconds": round(60.0 / best_bpm, 6),
        "confidence": confidence,
        "detected": True,
    }


def load_grid(track: Path) -> dict:
    sidecar = track.with_suffix(".grid.json")
    if sidecar.is_file():
        grid = json.loads(sidecar.read_text())
        grid.setdefault("beat_seconds", 60.0 / grid["bpm"])
        grid["detected"] = False
        return grid
    return detect_grid(track)


def quantise(spec: dict, grid: dict, min_beats: int, punch_every: int) -> list[str]:
    """Snap every shot's finished length to whole beats and mark the punches."""
    beat = grid["beat_seconds"]
    notes: list[str] = []
    for shot in spec.get("shots", []):
        if shot.get("flame_card"):
            continue
        slow = shot_slow_motion(shot)
        clip = float(shot["duration"]) * slow

        freeze = float(shot.get("freeze", 0.0) or 0.0)
        if freeze > 0.0:
            freeze = max(1, int(round(freeze / beat))) * beat
            shot["freeze"] = round(freeze, 4)
        beats = max(min_beats, int(round((clip + freeze) / beat)))
        wanted = beats * beat - freeze
        if wanted < beat:
            beats += int(math.ceil((beat - wanted) / beat))
            wanted = beats * beat - freeze
        shot["duration"] = round(wanted / slow, 4)
        held = f" + {freeze:.2f}s held" if freeze > 0.0 else ""
        notes.append(
            f"{shot.get('name', '?')}: {clip:.2f}s -> {wanted:.2f}s{held} "
            f"({beats} beats)"
        )
        marks = []
        step = max(1, punch_every)
        for index in range(0, beats, step):
            marks.append(
                {
                    "at": round(index * beat, 3),
                    "amount": 0.11 if index == 0 else 0.07,
                    "decay": round(beat * 0.55, 3),
                }
            )
        shot["punch"] = marks

        for key in shot.get("camera", []):
            if float(key.get("time", 0.0)) > wanted:
                key["time"] = round(wanted, 3)
    return notes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("spec", type=Path, nargs="?")
    parser.add_argument("--track", type=Path, required=True)
    parser.add_argument(
        "--detect",
        action="store_true",
        help="report the grid without touching the spec",
    )
    parser.add_argument("--min-beats", type=int, default=4)
    parser.add_argument("--punch-every", type=int, default=2)
    args = parser.parse_args()

    if not args.track.is_file():
        fail(f"track not found: {args.track}")

    if args.detect:
        found = detect_grid(args.track)
        print(json.dumps(found, indent=2))
        return 0

    if args.spec is None or not args.spec.is_file():
        fail("a promo spec is required unless --detect is given")

    grid = load_grid(args.track)
    spec = json.loads(args.spec.read_text())
    spec["music"] = str(args.track)
    spec["music_start"] = round(float(grid.get("drums_in", 0.0)), 3)
    spec["beat"] = {"bpm": grid["bpm"], "beat_seconds": grid["beat_seconds"]}

    notes = quantise(spec, grid, args.min_beats, args.punch_every)
    args.spec.write_text(json.dumps(spec, indent=2) + "\n")

    total = sum(shot["duration"] * shot_slow_motion(shot) for shot in spec["shots"])
    source = "detected" if grid.get("detected") else "from the track's grid sidecar"
    print(
        f"beat-align: {grid['bpm']} BPM {source}; "
        f"{len(notes)} shot(s) snapped, {total:.2f}s of picture"
    )
    for note in notes:
        print("   ", note)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
