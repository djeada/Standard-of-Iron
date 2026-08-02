#!/usr/bin/env python3
"""Render every procedurally generated cue sound into assets/audio.

The game's UI, order, build, alert and commander-combat sounds are synthesised
rather than recorded. That keeps them tiny, keeps them ours with no licence
attached, and means a sound can be retuned by editing a recipe instead of
booking a session.

Usage:
    python3 tools/audio_synth/synthesize_cues.py            # write every cue
    python3 tools/audio_synth/synthesize_cues.py ui.click    # write some cues
    python3 tools/audio_synth/synthesize_cues.py --wav-only  # skip encoding
    python3 tools/audio_synth/synthesize_cues.py --list      # names only

ffmpeg with libvorbis is required for encoding.
"""

from __future__ import annotations

import argparse
import fnmatch
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import dsp
from cues import RECIPES

REPO = Path(__file__).resolve().parent.parent.parent
AUDIO_DIR = REPO / "assets" / "audio"


def encode(wav_path: Path, ogg_path: Path, quality: int) -> None:
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
            str(quality),
            "-ac",
            "1",
            "-ar",
            str(dsp.SR),
            str(ogg_path),
        ],
        check=True,
    )


def render_take(
    cue_id: str, take: int, out_dir: Path, wav_only: bool
) -> tuple[Path, float, int]:
    recipe = RECIPES[cue_id]
    dsp.set_variant(take)
    try:
        buffer = dsp.finish(recipe.render(), recipe.peak_dbfs)
    finally:
        dsp.set_variant(0)
    duration = len(buffer) / dsp.SR

    target = out_dir / recipe.take_path(take)
    if wav_only:
        target = target.with_suffix(".wav")
        dsp.write_wav(target, buffer)
        return target, duration, target.stat().st_size

    with tempfile.TemporaryDirectory() as tmp:
        wav_path = Path(tmp) / "render.wav"
        dsp.write_wav(wav_path, buffer)
        encode(wav_path, target, recipe.quality)
    return target, duration, target.stat().st_size


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("patterns", nargs="*", help="cue ids or globs; default all")
    parser.add_argument("--wav-only", action="store_true", help="skip Vorbis encoding")
    parser.add_argument("--list", action="store_true", help="print cue ids and exit")
    parser.add_argument("--out", type=Path, default=AUDIO_DIR, help="output root")
    args = parser.parse_args()

    if args.list:
        for cue_id in sorted(RECIPES):
            print(cue_id)
        return 0

    if not args.wav_only and shutil.which("ffmpeg") is None:
        print(
            "ffmpeg is required to encode Vorbis; pass --wav-only to skip",
            file=sys.stderr,
        )
        return 1

    selected = sorted(RECIPES)
    if args.patterns:
        selected = [
            cue_id
            for cue_id in selected
            if any(fnmatch.fnmatch(cue_id, pattern) for pattern in args.patterns)
        ]
        if not selected:
            print("no cue matched", *args.patterns, file=sys.stderr)
            return 1

    total_bytes = 0
    total_seconds = 0.0
    total_files = 0
    for cue_id in selected:
        for take in range(RECIPES[cue_id].takes):
            path, duration, size = render_take(cue_id, take, args.out, args.wav_only)
            total_bytes += size
            total_seconds += duration
            total_files += 1
            label = cue_id if take == 0 else f"{cue_id} (take {take + 1})"
            print(
                f"{label:36s} {duration:5.2f}s {size / 1024:7.1f} KiB  "
                f"{path.relative_to(args.out)}"
            )

    print(
        f"\n{len(selected)} cues, {total_files} files, "
        f"{total_seconds:.1f}s of audio, {total_bytes / 1024:.1f} KiB total"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
