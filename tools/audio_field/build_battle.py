#!/usr/bin/env python3
"""Build the composed battle cues from their CC0 sources.

Each cue in `battle.py` is a stack of layers summed together, enveloped and
normalised. This is the third builder in the directory and the only one that
composes rather than extracts: `build_beds.py` cuts a window out of one place
and loop-seals it, `build_oneshots.py` finds transients in a performance, and
this one adds several recordings together to make a sound nobody recorded.

Usage:
    python3 tools/audio_field/build_battle.py                # every cue
    python3 tools/audio_field/build_battle.py 'elephant_*'   # matching cues
    python3 tools/audio_field/build_battle.py --list         # sources
    python3 tools/audio_field/build_battle.py --cache DIR    # keep downloads

ffmpeg with libvorbis is required, and the first run needs network access.
"""

from __future__ import annotations

import argparse
import fnmatch
import math
import re
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import battle
import sources
from build_beds import cut_layer, download, write_wav

REPO = Path(__file__).resolve().parent.parent.parent
OUT_DIR = REPO / "assets" / "audio"
RATE = sources.RATE


def layer_samples(layer: sources.Source, seconds: float, work: Path) -> list[float]:
    """One layer and all of its ranks, summed, at the cue's length.

    A rank is the same recording offset in time, quieter and darker. Summing a
    performance against itself is the only way to get a body of men out of a
    library that holds one man walking -- and the offsets must share no common
    factor, or the copies line up periodically and the ear hears an echo
    instead of a crowd.
    """
    total = int(seconds * RATE)
    out = [0.0] * total

    def add(samples: list[float], offset: int, gain: float) -> None:
        limit = min(len(samples), total - offset)
        for i in range(max(0, limit)):
            out[offset + i] += samples[i] * gain

    base = cut_layer(layer, download(layer.url, CACHE), seconds, work)
    add(base, 0, 1.0)

    gain = 1.0
    cutoff = layer.lowpass or 0.0
    for rank_index, offset_seconds in enumerate(layer.ranks, start=1):
        gain *= layer.rank_falloff
        ranked = layer
        if cutoff:
            cutoff *= layer.rank_lowpass
            ranked = sources.Source(
                **{**layer.__dict__, "lowpass": cutoff, "ranks": ()}
            )
        else:
            ranked = sources.Source(**{**layer.__dict__, "ranks": ()})
        rank_dir = work / f"rank{rank_index}"
        rank_dir.mkdir(parents=True, exist_ok=True)
        samples = cut_layer(ranked, download(layer.url, CACHE), seconds, rank_dir)
        add(samples, int(offset_seconds * RATE), gain)

    return out


def envelope(buf: list[float], attack: float, release: float) -> list[float]:
    """Fade in and out, so a cue neither clicks on nor stops dead."""
    attack_n = min(int(attack * RATE), len(buf))
    release_n = min(int(release * RATE), len(buf) - attack_n)
    for i in range(attack_n):
        buf[i] *= i / attack_n
    for i in range(release_n):
        buf[len(buf) - 1 - i] *= i / release_n
    return buf


def rms_of(buf: list[float]) -> float:
    return (sum(s * s for s in buf) / len(buf)) ** 0.5


def to_peak(buf: list[float], ceiling: float) -> list[float]:
    loudest = max((abs(s) for s in buf), default=0.0)
    if loudest <= 0.0:
        raise RuntimeError("layer stack summed to silence")
    return [s * (ceiling / loudest) for s in buf]


def softclip(buf: list[float], drive: float) -> list[float]:
    """The tanh shaper `tools/audio_synth/dsp.py` uses, restated here.

    Deliberately not imported: the synthesised and recorded pipelines are kept
    separate on purpose, and a one-line waveshaper is not worth coupling them
    over.
    """
    return [math.tanh(x * drive) / math.tanh(drive) for x in buf]


MAX_DRIVE = 6.0


def fit_level(
    buf: list[float], rms_db: float, ceiling: float
) -> tuple[list[float], float, float]:
    """Reach the target RMS under a fixed peak ceiling, by softening peaks.

    A composed stack is peakier than the dense generated file it replaces, so
    scaling it to the same RMS would put its transients through the roof, and
    scaling it to the same peak leaves it up to 15 dB quiet. Neither is a
    drop-in. Reducing the crest factor first is what lets both hold at once,
    and tanh is the gentlest way to do that -- it compresses the peaks
    continuously rather than shearing them.

    Returns the buffer, the drive used (0.0 if none was needed) and the RMS
    actually achieved, so the caller can report a cue that could not get there.
    """
    target = 10.0 ** (rms_db / 20.0)

    scaled = to_peak(buf, ceiling)
    if rms_of(scaled) >= target:

        return [s * (target / rms_of(scaled)) for s in scaled], 0.0, rms_db

    best = scaled
    low, high = 0.0, MAX_DRIVE
    for _ in range(24):
        drive = (low + high) / 2.0
        if drive <= 0.01:
            break
        candidate = to_peak(softclip(scaled, drive), ceiling)
        if rms_of(candidate) < target:
            low = drive
        else:
            high = drive
            best = candidate
    achieved = rms_of(best)
    achieved_db = 20.0 * math.log10(achieved) if achieved > 0 else -120.0
    return best, high, achieved_db


def encode(wav_path: Path, out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
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
            "4",
            str(out_path),
        ],
        check=True,
    )


def decoded_peak_db(path: Path) -> float:
    """Peak of the file as the game will actually decode it."""
    result = subprocess.run(
        ["ffmpeg", "-hide_banner", "-i", str(path), "-af", "astats", "-f", "null", "-"],
        capture_output=True,
        text=True,
        check=True,
    )
    match = re.search(r"Peak level dB: (-?[\d.]+)", result.stderr)
    if match is None:
        raise RuntimeError(f"could not measure the decoded peak of {path}")
    return float(match.group(1))


DECODE_CEILING_DB = -0.5


def encode_below_ceiling(
    samples: list[float], out_path: Path, work: Path
) -> tuple[float, float]:
    """Encode, then check what comes back out, and back off until it fits.

    Headroom in the WAV is not a guarantee about the .ogg. Vorbis is lossy, and
    a shaped signal can decode above where it went in -- which is how 28 of the
    32 files being replaced ended up over full scale despite presumably having
    looked fine before encoding. Measuring the decoded file is the only check
    that actually answers the question.
    """
    trim = 1.0
    for _ in range(6):
        wav_path = work / "mix.wav"
        write_wav(wav_path, [s * trim for s in samples])
        encode(wav_path, out_path)
        peak = decoded_peak_db(out_path)
        if peak <= DECODE_CEILING_DB:
            return peak, trim

        trim *= 10.0 ** ((DECODE_CEILING_DB - peak - 0.3) / 20.0)
    raise RuntimeError(f"{out_path} still decodes above {DECODE_CEILING_DB} dBFS")


def build(name: str, cue: battle.Cue) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)

        mixed = [0.0] * int(cue.seconds * RATE)
        for index, layer in enumerate(cue.layers):
            layer_dir = work / f"layer{index}"
            layer_dir.mkdir(parents=True, exist_ok=True)
            samples = layer_samples(layer, cue.seconds, layer_dir)
            for i, sample in enumerate(samples):
                mixed[i] += sample

        target_rms = battle.TARGET_RMS_DB[name]
        mixed, drive, achieved = fit_level(
            envelope(mixed, cue.attack, cue.release), target_rms, battle.CEILING
        )
        out_path = OUT_DIR / f"{cue.path}.ogg"
        peak, trim = encode_below_ceiling(mixed, out_path, work)
        size = out_path.stat().st_size
        miss = achieved + 20.0 * math.log10(trim) - target_rms
        note = f"  drive {drive:.2f}" if drive else ""
        if trim < 1.0:
            note += f"  trimmed {20.0 * math.log10(trim):+.1f} dB for the codec"
        if abs(miss) > 1.0:
            note += f"  MISSED by {miss:+.1f} dB"
        print(
            f"{name}: {cue.seconds:g}s, {len(cue.layers)} layer(s), "
            f"{achieved:.1f}/{target_rms:.1f} dB RMS, peak {peak:.2f} dBFS "
            f"-> {cue.path}.ogg ({size / 1024:.0f} KiB){note}"
        )


CACHE = Path(tempfile.gettempdir()) / "soi-battle-sources"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("patterns", nargs="*", default=[])
    parser.add_argument(
        "--list",
        action="store_true",
        help="print every source recording and its licence",
    )
    parser.add_argument(
        "--cache",
        type=Path,
        default=CACHE,
        help="where downloads are kept between runs",
    )
    args = parser.parse_args()

    globals()["CACHE"] = args.cache

    if args.list:
        seen: dict[str, str] = {}
        for cue in battle.CUES.values():
            for layer in cue.layers:
                seen[layer.origin] = layer.licence
        for origin, licence in sorted(seen.items()):
            print(f"{licence:10s} {origin}")
        return 0

    selected = {
        name: cue
        for name, cue in battle.CUES.items()
        if not args.patterns or any(fnmatch.fnmatch(name, p) for p in args.patterns)
    }
    if not selected:
        print("no cues matched", file=sys.stderr)
        return 1

    for name, cue in selected.items():
        build(name, cue)
    print(f"\n{len(selected)} cue(s) written under {OUT_DIR}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
