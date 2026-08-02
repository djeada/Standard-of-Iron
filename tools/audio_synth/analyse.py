#!/usr/bin/env python3
"""Measure generated cue sounds so they can be judged without listening.

Reports duration, peak, RMS, spectral centroid and how the centroid and pitch
move across the clip. That is enough to catch the failures that matter: a
silent recipe, a filter that blew up, a "dull" sound that came out bright, or a
rising sweep that actually falls.

Usage:
    python3 tools/audio_synth/analyse.py assets/audio/sfx/ui/*.ogg
"""

from __future__ import annotations

import argparse
import array
import cmath
import math
import subprocess
from pathlib import Path

SR = 16000


def decode(path: Path) -> list[float]:
    raw = subprocess.run(
        [
            "ffmpeg",
            "-v",
            "quiet",
            "-i",
            str(path),
            "-f",
            "s16le",
            "-ac",
            "1",
            "-ar",
            str(SR),
            "-",
        ],
        capture_output=True,
        check=True,
    ).stdout
    samples = array.array("h")
    samples.frombytes(raw)
    return [s / 32768.0 for s in samples]


def _dft_magnitudes(frame: list[float]) -> list[float]:
    """Naive DFT over a short frame. Slow but dependency free."""
    size = len(frame)
    bins = size // 2
    out = []
    for k in range(1, bins):
        acc = 0j
        step = -2j * math.pi * k / size
        for n, value in enumerate(frame):
            acc += value * cmath.exp(step * n)
        out.append(abs(acc))
    return out


def centroid(samples: list[float], start: int, length: int) -> float:
    frame = samples[start : start + length]
    if len(frame) < 32:
        return 0.0

    windowed = [
        value * (0.5 - 0.5 * math.cos(2 * math.pi * i / (len(frame) - 1)))
        for i, value in enumerate(frame)
    ]
    mags = _dft_magnitudes(windowed)
    total = sum(mags)
    if total <= 1e-9:
        return 0.0
    weighted = sum(mag * (k + 1) for k, mag in enumerate(mags))
    return (weighted / total) * SR / len(frame)


def measure(path: Path) -> dict:
    samples = decode(path)
    if not samples:
        return {"path": path, "error": "no audio"}
    peak = max(abs(s) for s in samples)
    rms = math.sqrt(sum(s * s for s in samples) / len(samples))
    dc = sum(samples) / len(samples)

    frame = 512
    head = centroid(samples, 0, frame)
    mid = centroid(samples, max(0, len(samples) // 2 - frame // 2), frame)
    late = centroid(samples, max(0, len(samples) - frame), frame)
    whole = centroid(samples, 0, min(len(samples), 4096))

    half = sum(s * s for s in samples[: len(samples) // 2])
    energy = sum(s * s for s in samples) or 1e-12

    return {
        "path": path,
        "seconds": len(samples) / SR,
        "peak_db": 20 * math.log10(peak) if peak > 0 else -99,
        "rms_db": 20 * math.log10(rms) if rms > 0 else -99,
        "dc": dc,
        "centroid": whole,
        "centroid_head": head,
        "centroid_mid": mid,
        "centroid_late": late,
        "front_energy": half / energy,
        "clipped": sum(1 for s in samples if abs(s) > 0.999),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="+", type=Path)
    args = parser.parse_args()

    print(
        f"{'file':44s} {'sec':>5s} {'peak':>6s} {'rms':>7s} "
        f"{'cent':>6s} {'head':>6s} {'late':>6s} {'front':>6s} {'clip':>4s}"
    )
    for path in args.files:
        info = measure(path)
        if "error" in info:
            print(f"{path.name:44s} {info['error']}")
            continue
        print(
            f"{path.name:44s} {info['seconds']:5.2f} {info['peak_db']:6.1f} "
            f"{info['rms_db']:7.1f} {info['centroid']:6.0f} {info['centroid_head']:6.0f} "
            f"{info['centroid_late']:6.0f} {info['front_energy']:6.2f} "
            f"{info['clipped']:4d}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
