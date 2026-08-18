#!/usr/bin/env python3
"""Build the sound bed for an ambience capture and mux it onto the footage.

The ambience lane wants something ``promo-edit.py`` deliberately does not do.
A promo is scored: one music track, laid once, plus timed one-shots. An
ambience piece is *layered and endless*: two or three looping beds running the
whole length, and a handful of distant animal calls scattered thinly enough
that the viewer notices them only now and then. A three-hour sleep video needs
those beds looped to three hours, which a single ``-i track.ogg`` cannot do.

The layer list lives in the same promo spec the arena consumed, under an
``ambience`` key the arena ignores::

    "ambience": {
      "seed": 5501,
      "beds": [
        {"file": "assets/audio/ambience/storm.ogg",             "gain": 0.85},
        {"file": "assets/audio/ambience/camp_fire_night.ogg",   "gain": 0.60},
        {"file": "assets/audio/ambience/mountain_camp_night.ogg", "gain": 0.16}
      ],
      "calls": [
        {"file": "assets/audio/sfx/wildlife/wolf_howl_distant.ogg",
         "gain": 0.14, "every": [25.0, 70.0]}
      ]
    }

A **bed** loops for the whole duration. Every bed is seam-sealed when it is
built -- both builders fold each tail back over its head -- so ``-stream_loop``
repeats one without a click and no crossfade is needed here.

A **call** is a one-shot placed at random with a gap drawn uniformly from
``every`` seconds. ``seed`` fixes those draws, so re-running this produces the
same bed for the same spec -- an ambience video that is re-cut should not come
back with the animals in different places.

Gains are linear and are *not* normalised per input: ``amix`` divides by its
input count by default, which would drop the rain by 5 dB the moment a wolf
calls three fields away. ``normalize=0`` keeps the mix the author wrote, and a
limiter at the end catches the sum.

The finished bed goes through ``audio_master_preview`` -- the same decode-time
chain the game applies -- for the reason ``promo-edit.py`` does it: an ambience
video is mostly its sound, so it should be the sound a player hears.

Typical use::

    build/bin/arena_app --promo-spec tools/arena/promos/night_watch.json \\
      --promo-out artifacts/promo
    scripts/ambience-audio.py --spec tools/arena/promos/night_watch.json \\
      --clip artifacts/promo/night_watch/01_the_watch.mp4

``--loop-to`` repeats the *video* as well, which is how the long-form cuts are
made: capture 30 s once, then loop the picture and build a bed of the full
length so the audio never repeats on the same period as the image. A camera
track that returns to its start (see ``tools/arena/README.md``) makes the
picture seam invisible; the audio seam is avoided by construction, because the
beds are looped at their own lengths rather than at the video's.
"""

from __future__ import annotations

import argparse
import json
import os
import random
import subprocess
import sys
import tempfile
from pathlib import Path

MASTER_TOOL_CANDIDATES = (
    "build/bin/audio_master_preview",
    "build-release/bin/audio_master_preview",
    "build-debug/bin/audio_master_preview",
)
OUTPUT_CEILING = 0.79
FADE_SECONDS = 2.0


def fail(message: str) -> None:
    print(f"ambience-audio: {message}", file=sys.stderr)
    raise SystemExit(1)


def find_master_tool() -> Path | None:
    override = os.environ.get("SOI_AUDIO_MASTER")
    candidates = [Path(override)] if override else []
    candidates += [Path(candidate) for candidate in MASTER_TOOL_CANDIDATES]
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    return None


def probe_duration(path: Path) -> float:
    result = subprocess.run(
        [
            "ffprobe",
            "-v",
            "error",
            "-show_entries",
            "format=duration",
            "-of",
            "default=nw=1:nk=1",
            str(path),
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        fail(f"could not probe {path}: {result.stderr.strip()}")
    return float(result.stdout.strip())


def scatter(call: dict, total: float, rng: random.Random) -> list[float]:
    """Times for one call layer, spaced by a gap drawn from ``every``."""
    low, high = call.get("every", [30.0, 90.0])
    at = rng.uniform(low * 0.3, high)
    times = []
    while at < total - 1.0:
        times.append(at)
        at += rng.uniform(low, high)
    return times


def build_bed(spec: dict, total: float, out_wav: Path) -> None:
    beds = spec.get("beds", [])
    calls = spec.get("calls", [])
    if not beds and not calls:
        fail("ambience spec has neither beds nor calls")
    rng = random.Random(spec.get("seed", 0))

    inputs: list[str] = []
    chain: list[str] = []
    labels: list[str] = []
    index = 0

    for bed in beds:
        path = Path(bed["file"])
        if not path.is_file():
            fail(f"bed not found: {path}")

        inputs += ["-stream_loop", "-1", "-i", str(path)]
        label = f"b{index}"
        chain.append(
            f"[{index}:a]aformat=channel_layouts=stereo,aresample=48000,"
            f"atrim=0:{total:.3f},asetpts=N/SR/TB,"
            f"volume={float(bed.get('gain', 1.0)):.4f}[{label}]"
        )
        labels.append(label)
        index += 1

    for call in calls:
        path = Path(call["file"])
        if not path.is_file():
            fail(f"call not found: {path}")
        times = scatter(call, total, rng)
        if not times:
            continue
        inputs += ["-i", str(path)]
        gain = float(call.get("gain", 1.0))

        parts = []
        for shot, at in enumerate(times):
            src = f"[{index}:a]" if shot == 0 else f"[c{index}s{shot}]"
            if shot == 0:
                chain.append(
                    f"[{index}:a]aformat=channel_layouts=stereo,aresample=48000,"
                    f"asplit={len(times)}"
                    + "".join(f"[c{index}s{n}]" for n in range(len(times)))
                )
                src = f"[c{index}s0]"
            ms = int(at * 1000.0)
            label = f"k{index}_{shot}"
            chain.append(f"{src}adelay={ms}|{ms},volume={gain:.4f}[{label}]")
            parts.append(label)
        labels.extend(parts)
        print(f"ambience-audio: {path.name} x{len(times)}")
        index += 1

    mix_in = "".join(f"[{label}]" for label in labels)
    fade_at = max(0.0, total - FADE_SECONDS)
    chain.append(
        f"{mix_in}amix=inputs={len(labels)}:normalize=0:dropout_transition=0,"
        f"volume={float(spec.get('gain', 1.0)):.4f},"
        f"afade=t=in:st=0:d={FADE_SECONDS:.2f},"
        f"afade=t=out:st={fade_at:.3f}:d={FADE_SECONDS:.2f},"
        f"alimiter=limit={OUTPUT_CEILING:.3f}:level=disabled[aout]"
    )

    command = [
        "ffmpeg",
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        *inputs,
        "-filter_complex",
        ";".join(chain),
        "-map",
        "[aout]",
        "-t",
        f"{total:.3f}",
        "-c:a",
        "pcm_s16le",
        str(out_wav),
    ]
    result = subprocess.run(command, check=False, capture_output=True, text=True)
    if result.returncode != 0:
        fail(f"ffmpeg failed building the bed: {result.stderr.strip()}")


def master(bed: Path, workdir: Path) -> Path:
    tool = find_master_tool()
    if tool is None:
        print(
            "ambience-audio: warning: audio_master_preview not built, using the "
            "raw mix. Run 'cmake --build build --target audio_master_preview'.",
            file=sys.stderr,
        )
        return bed
    rendered = workdir / "bed.mastered.wav"
    result = subprocess.run(
        [str(tool), "--render", str(rendered), str(bed)],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0 or not rendered.is_file():
        print(
            f"ambience-audio: warning: mastering failed, using the raw mix "
            f"({result.stderr.strip()})",
            file=sys.stderr,
        )
        return bed
    print("ambience-audio: bed run through the game's decode-time chain")
    return rendered


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument(
        "--spec",
        type=Path,
        required=True,
        help="promo spec carrying the 'ambience' block",
    )
    parser.add_argument(
        "--clip", type=Path, required=True, help="silent video the arena captured"
    )
    parser.add_argument(
        "--out", type=Path, help="output path (default: <clip>.scored.mp4)"
    )
    parser.add_argument(
        "--loop-to",
        type=float,
        default=0.0,
        help="repeat the picture out to this many seconds",
    )
    parser.add_argument(
        "--bed-only", action="store_true", help="write the mixed bed as .wav and stop"
    )
    args = parser.parse_args()

    if not args.spec.is_file():
        fail(f"spec not found: {args.spec}")
    if not args.clip.is_file():
        fail(f"clip not found: {args.clip}")
    spec = json.loads(args.spec.read_text()).get("ambience")
    if not spec:
        fail(f"{args.spec} has no 'ambience' block")

    clip_seconds = probe_duration(args.clip)
    total = args.loop_to if args.loop_to > 0.0 else clip_seconds
    output = args.out or args.clip.with_suffix(".scored.mp4")

    with tempfile.TemporaryDirectory(prefix="ambience-audio-") as tmp:
        workdir = Path(tmp)
        raw = workdir / "bed.wav"
        build_bed(spec, total, raw)
        bed = master(raw, workdir)

        if args.bed_only:
            final = output.with_suffix(".wav")
            final.parent.mkdir(parents=True, exist_ok=True)
            final.write_bytes(bed.read_bytes())
            print(f"ambience-audio: wrote {final} ({total:.1f}s)")
            return 0

        loops = max(1, int(total / clip_seconds + 0.999)) - 1
        command = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y"]
        if loops > 0:
            command += ["-stream_loop", str(loops)]
        command += [
            "-i",
            str(args.clip),
            "-i",
            str(bed),
            "-map",
            "0:v:0",
            "-map",
            "1:a:0",
            "-c:v",
            "copy",
            "-c:a",
            "aac",
            "-b:a",
            "192k",
            "-t",
            f"{total:.3f}",
            "-movflags",
            "+faststart",
            str(output),
        ]
        output.parent.mkdir(parents=True, exist_ok=True)
        result = subprocess.run(command, check=False, capture_output=True, text=True)
        if result.returncode != 0:
            fail(f"ffmpeg failed muxing: {result.stderr.strip()}")

    print(f"ambience-audio: wrote {output} ({total:.1f}s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
