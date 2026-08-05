#!/usr/bin/env python3
"""Render every looping ambience bed into assets/audio/ambience.

The beds are generated for the same reasons the cue sounds are: nothing is
sampled or licensed, a bed can be retuned by editing a recipe, and they can be
produced at the rate the mixer actually runs at.

Usage:
    python3 tools/audio_synth/synthesize_ambience.py             # every bed
    python3 tools/audio_synth/synthesize_ambience.py rainy       # matching beds
    python3 tools/audio_synth/synthesize_ambience.py --wav-only  # skip encoding
    python3 tools/audio_synth/synthesize_ambience.py --list      # names only

ffmpeg with libvorbis is required for encoding.
"""

from __future__ import annotations

import argparse
import fnmatch
import multiprocessing
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import ambience
import dsp

REPO = Path(__file__).resolve().parent.parent.parent
OUT_DIR = REPO / "assets" / "audio" / "ambience"

QUALITY = 4


def encode(wav_path: Path, ogg_path: Path) -> None:
    ogg_path.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            "ffmpeg",
            "-y",
            "-v",
            "error",
            "-i",
            str(wav_path),
            "-c:a",
            "libvorbis",
            "-q:a",
            str(QUALITY),
            "-ac",
            "1",
            "-ar",
            str(ambience.RATE),
            str(ogg_path),
        ],
        check=True,
    )


def render_one(args: tuple[str, bool, Path]) -> tuple[str, int, float]:
    name, wav_only, out_dir = args
    buf = ambience.render(name)
    peak = max(abs(s) for s in buf) if buf else 0.0
    with tempfile.TemporaryDirectory() as tmp:
        wav_path = Path(tmp) / f"{name}.wav"
        dsp.write_wav(wav_path, buf)
        if wav_only:
            target = out_dir / f"{name}.wav"
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(wav_path.read_bytes())
        else:
            encode(wav_path, out_dir / f"{name}.ogg")
    return name, len(buf), peak


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("patterns", nargs="*", help="only beds matching these")
    parser.add_argument("--wav-only", action="store_true")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--jobs", type=int, default=0)
    parser.add_argument(
        "--out",
        type=Path,
        default=None,
        help="write somewhere other than assets/audio/ambience",
    )
    args = parser.parse_args()

    names = sorted(ambience.BEDS)
    if args.patterns:
        names = [
            n for n in names if any(fnmatch.fnmatch(n, f"*{p}*") for p in args.patterns)
        ]
    if not names:
        print("no beds matched", file=sys.stderr)
        return 1

    if args.list:
        for name in names:
            print(name)
        return 0

    if args.out is not None:
        global OUT_DIR
        OUT_DIR = args.out

    jobs = args.jobs or min(len(names), multiprocessing.cpu_count())
    work = [(name, args.wav_only, OUT_DIR) for name in names]
    with multiprocessing.Pool(jobs) as pool:
        for name, frames, peak in pool.imap_unordered(render_one, work):
            seconds = frames / ambience.RATE
            print(f"  {name:<44} {seconds:5.1f}s  peak {peak:.3f}")

    print(f"rendered {len(names)} ambience bed(s) at {ambience.RATE} Hz")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
