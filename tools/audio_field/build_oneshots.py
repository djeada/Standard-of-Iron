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
import math
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
    if take.speed != 1.0:
        chain.append(f"asetrate={oneshots.RATE * take.speed:g}")
        chain.append(f"aresample={oneshots.RATE}")
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


def apply_stone_room_echo(samples: list[float], target_len: int) -> list[float]:
    """Convolve a dry stone footstep with vaulted masonry reflections and decay."""
    taps = (
        (0.011, 0.34),
        (0.019, -0.26),
        (0.031, 0.21),
        (0.047, -0.16),
        (0.071, 0.13),
        (0.103, -0.10),
        (0.149, 0.075),
        (0.211, -0.052),
        (0.289, 0.034),
        (0.373, -0.020),
    )
    out = [0.0] * target_len
    for i in range(min(len(samples), target_len)):
        out[i] += samples[i]
    lp = 0.0
    for delay_s, gain in taps:
        offset = int(round(delay_s * oneshots.RATE))
        lp = 0.0
        for i, x in enumerate(samples):
            dst = i + offset
            if dst >= target_len:
                break
            lp = lp + 0.28 * (x - lp)
            out[dst] += lp * gain
    fade_out = min(int(0.11 * oneshots.RATE), target_len // 3)
    for k in range(fade_out):
        t = 0.5 * (1.0 + math.cos(math.pi * (k + 1) / fade_out))
        out[target_len - fade_out + k] *= t
    return out


def window_slice(
    samples: list[float], start_s: float, take: oneshots.Take
) -> list[float]:
    length = int(oneshots.RATE * take.length_ms / 1000.0)
    if not samples or length <= 0:
        return []
    begin = int(round(start_s * oneshots.RATE)) % max(1, len(samples))
    out: list[float] = []
    while len(out) < length:
        chunk = samples[begin : begin + (length - len(out))]
        if not chunk:
            begin = 0
            continue
        out.extend(chunk)
        begin = 0
    fade_in = min(int(oneshots.RATE * take.fade_in_ms / 1000.0), len(out) // 4)
    fade_out = min(int(oneshots.RATE * take.fade_out_ms / 1000.0), len(out) // 3)
    for i in range(fade_in):
        out[i] *= 0.5 * (1.0 - math.cos(math.pi * i / max(1, fade_in)))
    for i in range(fade_out):
        out[len(out) - fade_out + i] *= 0.5 * (
            1.0 + math.cos(math.pi * (i + 1) / max(1, fade_out))
        )
    return out


def trim_leading_quiet(samples: list[float], fade_in_ms: float) -> list[float]:
    if not samples:
        return samples
    peak = max(abs(s) for s in samples)
    if peak <= 1e-6:
        return samples
    thresh = max(0.0055, peak * 0.04)
    first = 0
    for idx, val in enumerate(samples):
        if abs(val) > thresh:
            first = idx
            break
    preroll = int(0.008 * oneshots.RATE)
    start = max(0, first - preroll)
    if start == 0:
        return samples
    out = list(samples[start:])
    fade_in = min(int(oneshots.RATE * max(3.0, fade_in_ms) / 1000.0), len(out) // 4)
    for i in range(fade_in):
        out[i] *= 0.5 * (1.0 - math.cos(math.pi * i / max(1, fade_in)))
    return out


def trim_trailing_quiet(samples: list[float], fade_out_ms: float) -> list[float]:
    if not samples:
        return samples
    peak = max(abs(s) for s in samples)
    if peak <= 1e-6:
        return samples
    floor = peak * (10.0 ** (-54.0 / 20.0))
    last = len(samples) - 1
    while last > 0 and abs(samples[last]) <= floor:
        last -= 1
    tail_pad = int(0.11 * oneshots.RATE)
    end = min(len(samples), last + 1 + tail_pad)
    if end >= len(samples):
        return samples
    out = list(samples[: max(end, int(0.08 * oneshots.RATE))])
    fade_out = min(int(oneshots.RATE * max(25.0, fade_out_ms) / 1000.0), len(out) // 3)
    for i in range(fade_out):
        out[len(out) - fade_out + i] *= 0.5 * (
            1.0 + math.cos(math.pi * (i + 1) / max(1, fade_out))
        )
    return out


def build(name: str, take: oneshots.Take, cache: Path, dry_run: bool) -> str:
    """Cut one take's variants, drawing across its sources in order.

    Each source contributes up to the number of variants still wanted, so a
    performance can supply all four on its own while three single-impact files
    supply one each.
    """
    cuts: list[list[float]] = []
    found: list[str] = []
    decoded_sources = [
        decode(download(url, cache), take) for url, _origin in take.sources
    ]

    if take.windows:
        for index, win_start in enumerate(take.windows[: take.count]):
            src_samples = decoded_sources[index % len(decoded_sources)]
            cut = trim_leading_quiet(
                window_slice(src_samples, win_start, take), take.fade_in_ms
            )
            if take.room_echo:
                cut = apply_stone_room_echo(
                    cut, int(oneshots.RATE * take.length_ms / 1000.0)
                )
            else:
                cut = trim_trailing_quiet(cut, take.fade_out_ms)
            cuts.append(slicer.normalise(cut, take.peak))
            found.append(f"{win_start:.2f}s")
    else:
        for samples in decoded_sources:
            if len(cuts) >= take.count:
                break
            wanted = take.count - len(cuts)
            for hit in chosen_hits(samples, take)[:wanted]:
                cut = slicer.cut(
                    samples,
                    hit,
                    oneshots.RATE,
                    lead_ms=take.lead_ms,
                    length_ms=take.length_ms,
                    fade_out_ms=take.fade_out_ms,
                    fade_in_ms=take.fade_in_ms,
                )
                cut = trim_leading_quiet(cut, take.fade_in_ms)
                if take.room_echo:
                    cut = apply_stone_room_echo(
                        cut, int(oneshots.RATE * take.length_ms / 1000.0)
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
