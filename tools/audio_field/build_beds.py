#!/usr/bin/env python3
"""Rebuild the recorded ambience beds from their public-domain sources.

Every bed in `sources.py` is cut from a freely licensed field recording rather
than generated, so this script is the only record of how a shipped `.ogg` was
made. Give it a working directory and it downloads the originals once, cuts the
declared window, shapes it, seals the loop and encodes at the mixer's rate.

Usage:
    python3 tools/audio_field/build_beds.py                 # every bed
    python3 tools/audio_field/build_beds.py weather_*       # matching beds
    python3 tools/audio_field/build_beds.py --list          # names and sources
    python3 tools/audio_field/build_beds.py --cache DIR     # keep downloads

Downloads land in a cache directory (default: a temporary one) and are reused,
because the originals run to tens of megabytes each.

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
import time
import urllib.request
import wave
from dataclasses import replace
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import sources

REPO = Path(__file__).resolve().parent.parent.parent
OUT_DIR = REPO / "assets" / "audio" / "ambience"

USER_AGENT = "Standard-of-Iron-ambience-build/1.0"


def download(url: str, cache: Path, attempts: int = 5) -> Path:
    """Fetch `url` into `cache`, reusing what is already there.

    Retries on any error. Archive.org answers a burst of requests with a 500
    rather than a 429, so a failure here says nothing about whether the URL is
    right -- without the retry, a correct build looks like a broken one.
    """
    cache.mkdir(parents=True, exist_ok=True)
    target = cache / url.rsplit("/", 1)[-1].replace("%2F", "_")
    if target.exists() and target.stat().st_size > 0:
        return target

    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    for attempt in range(attempts):
        try:
            with urllib.request.urlopen(request, timeout=300) as response:
                partial = target.with_suffix(target.suffix + ".part")
                partial.write_bytes(response.read())
            partial.rename(target)
            return target
        except Exception as error:
            if attempt == attempts - 1:
                raise RuntimeError(f"could not fetch {url}: {error}") from error
            time.sleep(4.0 * (attempt + 1))
    raise AssertionError("unreachable")


def cut_layer(
    source: sources.Source, local: Path, seconds: float, work: Path
) -> list[float]:
    """One layer, cut and shaped, as mono float samples at the mixer's rate."""
    chain = [f"aresample={sources.RATE}", "aformat=channel_layouts=mono"]
    if source.speed != 1.0:
        chain.append(f"asetrate={sources.RATE * source.speed:g}")
        chain.append(f"aresample={sources.RATE}")
    if source.highpass:
        chain.append(f"highpass=f={source.highpass:g}:poles=2")
    if source.lowpass:
        chain.append(f"lowpass=f={source.lowpass:g}:poles=2")
    if source.shelf_db:
        chain.append(
            f"treble=g={source.shelf_db:g}:f={source.shelf_hz:g}:width_type=q:width=0.7"
        )
    if source.gain != 1.0:
        chain.append(f"volume={source.gain:g}")

    wav_path = work / "layer.wav"
    loop = ["-stream_loop", "-1"] if source.loop_source else []
    subprocess.run(
        [
            "ffmpeg",
            "-y",
            "-v",
            "error",
            *loop,
            "-ss",
            f"{source.start:g}",
            "-i",
            str(local),
            "-t",
            f"{seconds:g}",
            "-af",
            ",".join(chain),
            "-c:a",
            "pcm_s16le",
            str(wav_path),
        ],
        check=True,
    )
    return read_wav(wav_path)


def read_wav(path: Path) -> list[float]:
    with wave.open(str(path), "rb") as handle:
        if handle.getsampwidth() != 2 or handle.getnchannels() != 1:
            raise RuntimeError(f"{path} is not 16-bit mono")
        raw = handle.readframes(handle.getnframes())
    count = len(raw) // 2
    return [s / 32768.0 for s in struct.unpack(f"<{count}h", raw)]


def write_wav(path: Path, samples: list[float]) -> None:
    clipped = [max(-1.0, min(1.0, s)) for s in samples]
    raw = struct.pack(f"<{len(clipped)}h", *[int(round(s * 32767.0)) for s in clipped])
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(sources.RATE)
        handle.writeframes(raw)


def seal_loop(buf: list[float], fade_seconds: float) -> list[float]:
    """Fold the tail back over the head so the file loops without a step.

    The same equal-power fold the synthesised beds use, so both sets of beds
    behave identically at the seam.
    """
    fade = min(int(fade_seconds * sources.RATE), len(buf) // 4)
    if fade < 2:
        return buf
    loop = len(buf) - fade
    out = list(buf)
    for k in range(fade):
        t = (k / fade) * 0.5 * math.pi
        out[k] = buf[k] * math.sin(t) + buf[loop + k] * math.cos(t)
    return out[:loop]


def rotate(samples: list[float], offset: int) -> list[float]:
    """Shift a layer in time, wrapping what falls off the front back onto the end.

    Ranks are rotations rather than later windows of the source, because the
    footstep recordings run ten to sixteen seconds and a bed runs twenty-two:
    seeking further in for each rank walks off the end of the file, and a mix
    is only as long as its shortest layer.
    """
    if not samples:
        return samples
    shift = offset % len(samples)
    return samples[-shift:] + samples[:-shift] if shift else list(samples)


def mix(layers: list[list[float]]) -> list[float]:
    length = min(len(layer) for layer in layers)
    return [sum(layer[i] for layer in layers) for i in range(length)]


def measured_lufs(path: Path) -> float:
    result = subprocess.run(
        [
            "ffmpeg",
            "-hide_banner",
            "-nostats",
            "-i",
            str(path),
            "-af",
            "ebur128=framelog=quiet",
            "-f",
            "null",
            "-",
        ],
        capture_output=True,
        text=True,
    )
    for line in result.stderr.splitlines():
        if "I:" in line and "LUFS" in line:
            return float(line.split("I:")[1].split("LUFS")[0])
    raise RuntimeError(f"no loudness reading for {path}")


def level_chain(gain_db: float) -> str:
    """Lift to the loudness target, then hold the peaks down to the ceiling.

    A field recording of gusts or crickets has a far higher crest factor than
    the filtered noise these beds replaced, so raising it to the target
    loudness by gain alone would blow through the headroom the decode-time
    mastering needs. The limiter trades a little of that crest -- inaudible on
    a bed, which nobody is listening to for its transients -- for a bed that
    plays at the same level as every other one.
    """
    ceiling = sources.PEAK_CEILING
    return (
        f"volume={gain_db:.2f}dB,"
        f"alimiter=level_in=1:level_out=1:limit={ceiling:g}:"
        "attack=5:release=250:level=disabled"
    )


def apply_level(wav_path: Path, out_path: Path, gain_db: float) -> None:
    subprocess.run(
        [
            "ffmpeg",
            "-y",
            "-v",
            "error",
            "-i",
            str(wav_path),
            "-af",
            level_chain(gain_db),
            "-c:a",
            "pcm_s16le",
            str(out_path),
        ],
        check=True,
    )


def encode(wav_path: Path, ogg_path: Path, gain_db: float) -> None:
    ogg_path.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            "ffmpeg",
            "-y",
            "-v",
            "error",
            "-i",
            str(wav_path),
            "-af",
            level_chain(gain_db),
            "-c:a",
            "libvorbis",
            "-q:a",
            str(sources.QUALITY),
            "-ac",
            "1",
            "-ar",
            str(sources.RATE),
            str(ogg_path),
        ],
        check=True,
    )


def build(name: str, bed: sources.Bed, cache: Path, work: Path) -> str:
    """Produce one shipped bed.

    The cut runs `fade` seconds longer than the bed: sealing the loop consumes
    that tail, so asking for exactly `seconds` would deliver a short bed.

    Levelling then settles gain and limiting against each other, because
    limiting changes the loudness the gain was calculated to reach.
    """
    span = bed.seconds + bed.fade
    layers = []
    for index, layer in enumerate(bed.layers):
        local = download(layer.url, cache)
        layer_work = work / f"{name}.{index}"
        layer_work.mkdir(parents=True, exist_ok=True)
        layers.append(cut_layer(layer, local, span, layer_work))

        gain = layer.gain
        lowpass = layer.lowpass or 12000.0
        for rank, offset in enumerate(layer.ranks, start=1):
            gain *= layer.rank_falloff
            lowpass *= layer.rank_lowpass
            copy = replace(layer, gain=gain, lowpass=lowpass)
            rank_work = work / f"{name}.{index}.rank{rank}"
            rank_work.mkdir(parents=True, exist_ok=True)
            shifted = cut_layer(copy, local, span, rank_work)
            layers.append(rotate(shifted, int(offset * sources.RATE)))

    samples = seal_loop(mix(layers), bed.fade)

    wav_path = work / f"{name}.wav"
    write_wav(wav_path, samples)

    gain_db = sources.TARGET_LUFS - measured_lufs(wav_path)
    levelled = work / f"{name}.levelled.wav"
    for _ in range(4):
        apply_level(wav_path, levelled, gain_db)
        error_db = sources.TARGET_LUFS - measured_lufs(levelled)
        if abs(error_db) < 0.3:
            break
        gain_db += error_db

    encode(wav_path, OUT_DIR / f"{name}.ogg", gain_db)
    return (
        f"{name}: {len(samples) / sources.RATE:.1f}s, "
        f"{gain_db:+.1f} dB applied, {error_db:+.1f} dB off target"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("patterns", nargs="*", help="only beds matching these")
    parser.add_argument("--list", action="store_true", help="print sources and exit")
    parser.add_argument("--cache", type=Path, help="keep downloads here")
    args = parser.parse_args()

    names = sorted(sources.BEDS)
    if args.patterns:
        names = [n for n in names if any(fnmatch.fnmatch(n, p) for p in args.patterns)]
    if not names:
        print("no bed matches", file=sys.stderr)
        return 1

    if args.list:
        for name in names:
            bed = sources.BEDS[name]
            print(f"{name} ({bed.seconds:g}s)")
            for layer in bed.layers:
                print(f"    {layer.licence:<26} {layer.origin}")
                print(f"    {'':<26} from {layer.start:g}s of {layer.url}")
        return 0

    if shutil.which("ffmpeg") is None:
        print("ffmpeg is required", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory() as tmp:
        cache = args.cache or (Path(tmp) / "cache")
        work = Path(tmp) / "work"
        work.mkdir(parents=True, exist_ok=True)
        for name in names:
            print(build(name, sources.BEDS[name], cache, work))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
