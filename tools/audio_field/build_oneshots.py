#!/usr/bin/env python3
"""Cut the short combat and movement cues out of their source recordings.

Each take in `oneshots.py` is one CC0 performance -- a run of sword hits, a run
of footsteps. This finds the individual hits, keeps the best few as numbered
variants, and writes them into `assets/audio/sfx`.

Usage:
    python3 tools/audio_field/build_oneshots.py                # every take
    python3 tools/audio_field/build_oneshots.py footstep_*     # matching takes
    python3 tools/audio_field/build_oneshots.py --list         # sources
    python3 tools/audio_field/build_oneshots.py --dry-run      # detect only
    python3 tools/audio_field/build_oneshots.py --cache DIR    # keep downloads

ffmpeg with libvorbis is required.
"""

from __future__ import annotations

import argparse
import fnmatch
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import oneshots
import slice as slicer
from build_beds import download

REPO = Path(__file__).resolve().parent.parent.parent
OUT_DIR = REPO / "assets" / "audio"


def decode(path: Path, take: oneshots.Take) -> list[float]:
    """The source as mono float samples at the mixer's rate, pre-shaped.

    Filtering before detection as well as after keeps the gate honest: a source
    with rumble under 80 Hz reads as permanently open otherwise.
    """
    chain = [f"aresample={oneshots.RATE}", "aformat=channel_layouts=mono"]
    if take.highpass:
        chain.append(f"highpass=f={take.highpass:g}:poles=2")
    if take.lowpass:
        chain.append(f"lowpass=f={take.lowpass:g}:poles=2")

    raw = subprocess.run(
        [
            "ffmpeg",
            "-v",
            "error",
            "-i",
            str(path),
            "-af",
            ",".join(chain),
            "-f",
            "s16le",
            "-ac",
            "1",
            "-ar",
            str(oneshots.RATE),
            "-",
        ],
        capture_output=True,
        check=True,
    ).stdout
    count = len(raw) // 2
    return [s / 32768.0 for s in struct.unpack(f"<{count}h", raw)]


def chosen_hits(samples: list[float], take: oneshots.Take) -> list[slicer.Hit]:
    """The hits worth keeping from one source, in the order they occur in it.

    Ranking is by level, because a performance ramps and its quiet end is
    usually a miss or a settle rather than a take. Ordering the survivors back
    into source order keeps variant numbering stable when a threshold moves by
    a hair.
    """
    detect = take.detect
    hits = slicer.find_hits(
        samples,
        oneshots.RATE,
        open_db=detect.open_db,
        close_db=detect.close_db,
        tail_ms=detect.tail_ms,
        min_gap_ms=detect.min_gap_ms,
    )
    limit = int(oneshots.RATE * detect.max_length_ms / 1000.0)
    usable = [h for h in hits if h.end - h.start <= limit]
    ranked = sorted(usable, key=lambda h: -h.level)
    keep = ranked[take.rank_from : take.rank_from + take.count]
    return sorted(keep, key=lambda h: h.start)


def encode_once(samples: list[float], out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    clipped = [max(-1.0, min(1.0, s)) for s in samples]
    raw = struct.pack(f"<{len(clipped)}h", *[int(round(s * 32767.0)) for s in clipped])
    subprocess.run(
        [
            "ffmpeg",
            "-y",
            "-v",
            "error",
            "-f",
            "s16le",
            "-ar",
            str(oneshots.RATE),
            "-ac",
            "1",
            "-i",
            "-",
            "-c:a",
            "libvorbis",
            "-q:a",
            str(oneshots.QUALITY),
            str(out_path),
        ],
        input=raw,
        check=True,
    )


def decoded_peak_db(path: Path) -> float:
    result = subprocess.run(
        [
            "ffmpeg",
            "-hide_banner",
            "-nostats",
            "-i",
            str(path),
            "-af",
            "volumedetect",
            "-f",
            "null",
            "-",
        ],
        capture_output=True,
        text=True,
    )
    for line in result.stderr.splitlines():
        if "max_volume:" in line:
            return float(line.split("max_volume:")[1].split("dB")[0])
    raise RuntimeError(f"no peak reading for {path}")


def encode(samples: list[float], out_path: Path, ceiling_db: float = -1.0) -> None:
    """Encode, then check what actually comes back out, and back off if it clips.

    Vorbis does not preserve peaks. A hard metal transient normalised to
    -1.9 dBFS can decode above full scale, which the mixer then clips -- a
    click on the very cue that fires most often. Measuring the decoded file is
    the only honest check, so the encode repeats at lower gain until the
    decoder agrees.
    """
    trim = 1.0
    for _ in range(5):
        encode_once([s * trim for s in samples], out_path)
        peak_db = decoded_peak_db(out_path)
        if peak_db <= ceiling_db:
            return
        trim *= 10.0 ** ((ceiling_db - peak_db - 0.4) / 20.0)


def build(name: str, take: oneshots.Take, cache: Path, dry_run: bool) -> str:
    """Cut one take's variants, drawing across its sources in order.

    Each source contributes up to the number of variants still wanted, so a
    performance can supply all four on its own while three single-impact files
    supply one each.
    """
    cuts: list[list[float]] = []
    found: list[str] = []
    for url, _origin in take.sources:
        if len(cuts) >= take.count:
            break
        samples = decode(download(url, cache), take)
        wanted = take.count - len(cuts)
        for hit in chosen_hits(samples, take)[:wanted]:
            cut = slicer.cut(
                samples,
                hit,
                oneshots.RATE,
                lead_ms=take.lead_ms,
                length_ms=take.length_ms,
                fade_out_ms=take.fade_out_ms,
            )
            cuts.append(slicer.normalise(cut, take.peak))
            found.append(f"{hit.start / oneshots.RATE:.2f}s")

    if len(cuts) < take.count:
        return (
            f"{name}: only {len(cuts)} of {take.count} hits found -- "
            "loosen the gate, add a source, or lower count"
        )

    if not dry_run:
        for index, cut in enumerate(cuts, start=1):
            encode(cut, OUT_DIR / f"{take.prefix}_{index:02d}.ogg")

    longest = max(len(cut) for cut in cuts) / oneshots.RATE
    return f"{name}: {len(cuts)} × {longest * 1000:.0f} ms at {', '.join(found)}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("patterns", nargs="*", help="only takes matching these")
    parser.add_argument("--list", action="store_true", help="print sources and exit")
    parser.add_argument(
        "--dry-run", action="store_true", help="detect hits, write nothing"
    )
    parser.add_argument("--cache", type=Path, help="keep downloads here")
    args = parser.parse_args()

    names = sorted(oneshots.TAKES)
    if args.patterns:
        names = [n for n in names if any(fnmatch.fnmatch(n, p) for p in args.patterns)]
    if not names:
        print("no take matches", file=sys.stderr)
        return 1

    if args.list:
        for name in names:
            take = oneshots.TAKES[name]
            print(f"{name} -> {take.prefix}_01..{take.count:02d}.ogg")
            for _url, origin in take.sources:
                print(f"    {take.licence:<10} {origin}")
        return 0

    if shutil.which("ffmpeg") is None:
        print("ffmpeg is required", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory() as tmp:
        cache = args.cache or (Path(tmp) / "cache")
        for name in names:
            print(build(name, oneshots.TAKES[name], cache, args.dry_run))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
