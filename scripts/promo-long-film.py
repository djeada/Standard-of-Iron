#!/usr/bin/env python3
"""Finish a long-form ambience film: picture cut, chapter titles, soundtrack.

    scripts/promo-long-film.py --spec FILM.json --clips CAPTURE/<id> \
        --audio FILM_audio.json --out DIR --film-title "A Day in a Roman Town"

1. ``promo-edit.py`` cuts the picture (joins, grade, Steam end card) silently;
   an existing ``DIR/<id>_picture.mp4`` is reused, so a re-run only redoes the
   cheap steps.
2. The soundtrack is built chapter by chapter: each chapter's seam-sealed
   ambience beds loop for its length, one-shot calls land at seeded random
   gaps, chapters crossfade into each other, and short music "islands" open
   chapters with long stretches of pure ambience between.
3. The film title and chapter titles are burned in, the result is
   loudness-normalised to -14 LUFS, and ``DIR/<id>_youtube.txt`` gets the
   description, the Steam link and chapter timestamps.

The promo spec carries the chapters, which the arena ignores::

    "chapters": [{"title": "I. DAWN", "first_shot": "dawn_aerial"}, ...]

The audio file maps each chapter title to its layers (paths from the repo
root, gains linear, ``every`` a [min, max] gap in seconds, ``at`` seconds into
the chapter)::

    {"seed": 218, "crossfade": 5.0, "description": "...",
     "chapters": {"I. DAWN": {
         "beds":  [{"file": "assets/audio/ambience/mediterranean_plains.ogg", "gain": 0.55}],
         "calls": [{"file": "assets/audio/sfx/wildlife/bird_chirp.ogg", "gain": 0.25, "every": [8, 25]}],
         "music": [{"file": "assets/audio/music/base/base_sunlight_olive_groves.ogg", "at": 3, "gain": 0.35}]}}}

Titles are set in the display face, which has capitals only, so the film
title is upper-cased; it is drawn from a text file so apostrophes survive.
"""

from __future__ import annotations

import argparse
import json
import random
import subprocess
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
FONT = REPO / "assets/fonts/StandardIronDisplay-Bold.ttf"
STEAM_URL = "https://store.steampowered.com/app/5129960/Standard_of_Iron/"


def run(cmd: list[str]) -> str:
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit(f"failed: {' '.join(cmd[:6])}...\n{result.stderr[-2000:]}")
    return result.stdout + result.stderr


def duration(path: Path) -> float:
    out = run(
        [
            "ffprobe",
            "-v",
            "error",
            "-show_entries",
            "format=duration",
            "-of",
            "csv=p=0",
            str(path),
        ]
    )
    return float(out.strip())


def chapter_starts(spec: dict) -> list[tuple[str, float]]:
    join = float(spec.get("transition", {}).get("duration", 0.0))
    starts, t = {}, 0.0
    for index, shot in enumerate(spec["shots"]):
        starts[shot["name"]] = 0.0 if index == 0 else t
        t += float(shot["duration"]) - join
    return [(c["title"], starts[c["first_shot"]]) for c in spec["chapters"]]


def build_soundtrack(
    audio: dict, chapters: list[tuple[str, float]], total: float, work: Path
) -> Path:
    rng = random.Random(audio.get("seed", 1))
    xfade = float(audio.get("crossfade", 4.0))
    inputs: list[str] = []
    filters: list[str] = []
    mix: list[str] = []

    index = 0
    bounds = [s for _, s in chapters] + [total]
    for chapter_index, (title, start) in enumerate(chapters):
        end = bounds[chapter_index + 1]
        length = end - start + (xfade if chapter_index + 1 < len(chapters) else 0.0)
        plan = audio["chapters"][title]
        for bed in plan.get("beds", []):
            inputs.extend(["-stream_loop", "-1", "-i", str(REPO / bed["file"])])
            label = f"b{index}"
            filters.append(
                f"[{index}:a]aformat=sample_rates=48000:channel_layouts=stereo,"
                f"atrim=0:{length:.3f},volume={bed['gain']},"
                f"afade=t=in:d={xfade}:st=0,afade=t=out:d={xfade}:st={max(0.0, length - xfade):.3f},"
                f"adelay={int(start * 1000)}|{int(start * 1000)}[{label}]"
            )
            mix.append(f"[{label}]")
            index += 1
        for call in plan.get("calls", []):
            low, high = call["every"]
            t = start + rng.uniform(low, high) * 0.5
            while t < end - 3.0:
                inputs.extend(["-i", str(REPO / call["file"])])
                label = f"c{index}"
                filters.append(
                    f"[{index}:a]aformat=sample_rates=48000:channel_layouts=stereo,"
                    f"volume={call['gain'] * rng.uniform(0.6, 1.0):.3f},"
                    f"adelay={int(t * 1000)}|{int(t * 1000)}[{label}]"
                )
                mix.append(f"[{label}]")
                index += 1
                t += rng.uniform(low, high)
        for cue in plan.get("music", []):
            at = start + float(cue.get("at", 0.0))
            inputs.extend(["-i", str(REPO / cue["file"])])
            label = f"m{index}"
            filters.append(
                f"[{index}:a]aformat=sample_rates=48000:channel_layouts=stereo,"
                f"volume={cue['gain']},afade=t=in:d=3,areverse,afade=t=in:d=6,areverse,"
                f"adelay={int(at * 1000)}|{int(at * 1000)}[{label}]"
            )
            mix.append(f"[{label}]")
            index += 1

    filters.append(
        "".join(mix) + f"amix=inputs={len(mix)}:normalize=0:duration=longest,"
        f"atrim=0:{total:.3f},afade=t=out:d=4:st={total - 4:.3f},"
        "alimiter=limit=0.89:level=disabled[aout]"
    )
    script = work / "audio_graph.txt"
    script.write_text(";\n".join(filters))
    raw = work / "soundtrack.wav"
    run(
        [
            "ffmpeg",
            "-hide_banner",
            "-y",
            *inputs,
            "-filter_complex_script",
            str(script),
            "-map",
            "[aout]",
            "-ar",
            "48000",
            str(raw),
        ]
    )
    return raw


SMALL_WORDS = {"a", "an", "and", "at", "by", "for", "in", "of", "on", "the", "to"}


def chapter_name(title: str) -> str:
    words = title.split(". ", 1)[-1].lower().split()
    return " ".join(
        w if i and w in SMALL_WORDS else w.capitalize() for i, w in enumerate(words)
    )


def stamp(seconds: float) -> str:
    s = int(round(seconds))
    return (
        f"{s // 60}:{s % 60:02d}"
        if s < 3600
        else f"{s // 3600}:{s % 3600 // 60:02d}:{s % 60:02d}"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--spec", type=Path, required=True)
    parser.add_argument("--clips", type=Path, required=True)
    parser.add_argument("--audio", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--film-title", required=True)
    args = parser.parse_args()

    spec = json.loads(args.spec.read_text())
    audio = json.loads(args.audio.read_text())
    args.out.mkdir(parents=True, exist_ok=True)
    picture = args.out / f"{spec['id']}_picture.mp4"
    if not picture.exists():
        print("cutting picture with promo-edit ...", flush=True)
        log = run(
            [
                "python3",
                str(REPO / "scripts/promo-edit.py"),
                "--spec",
                str(args.spec),
                "--clips",
                str(args.clips),
                "--out",
                str(picture),
                "--audio",
                "none",
            ]
        )
        print(log.strip().splitlines()[-1])
    total = duration(picture)
    chapters = chapter_starts(spec)
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        print("building soundtrack ...", flush=True)
        soundtrack = build_soundtrack(audio, chapters, total, work)
        draw = []
        font = str(FONT).replace(":", r"\:")
        title_file = work / "film_title.txt"
        title_file.write_text(args.film_title.upper())
        draw.append(
            f"drawtext=fontfile='{font}':textfile='{title_file}':fontsize=64:fontcolor=white:"
            "borderw=2:bordercolor=black@0.55:x=(w-text_w)/2:y=h*0.80:"
            "alpha='if(lt(t,1.0),0,if(lt(t,2.5),(t-1.0)/1.5,if(lt(t,8),1,if(lt(t,9.5),(9.5-t)/1.5,0))))'"
        )
        for title, start in chapters[1:]:
            a, b = start + 1.0, start + 7.0
            draw.append(
                f"drawtext=fontfile='{font}':text='{title}':fontsize=46:fontcolor=white:"
                "borderw=2:bordercolor=black@0.55:x=(w-text_w)/2:y=h*0.84:"
                f"alpha='if(lt(t,{a}),0,if(lt(t,{a + 1.2}),(t-{a})/1.2,if(lt(t,{b - 1.2}),1,"
                f"if(lt(t,{b}),({b}-t)/1.2,0))))'"
            )
        mixed = work / "mixed.mp4"
        print("encoding titles + soundtrack ...", flush=True)
        run(
            [
                "ffmpeg",
                "-hide_banner",
                "-y",
                "-i",
                str(picture),
                "-i",
                str(soundtrack),
                "-filter_complex",
                f"[0:v]{','.join(draw)}[v]",
                "-map",
                "[v]",
                "-map",
                "1:a",
                "-c:v",
                "libx264",
                "-preset",
                "medium",
                "-crf",
                "18",
                "-pix_fmt",
                "yuv420p",
                "-c:a",
                "pcm_s16le",
                "-shortest",
                str(mixed.with_suffix(".mkv")),
            ]
        )
        final = args.out / f"{spec['id']}.mp4"
        measured = run(
            [
                "ffmpeg",
                "-hide_banner",
                "-i",
                str(mixed.with_suffix(".mkv")),
                "-af",
                "loudnorm=I=-14:TP=-1.5:LRA=11:print_format=json",
                "-f",
                "null",
                "-",
            ]
        )
        stats = json.loads(measured[measured.rindex("{") : measured.rindex("}") + 1])
        norm = (
            f"loudnorm=I=-14:TP=-1.5:LRA=11:measured_I={stats['input_i']}:measured_TP={stats['input_tp']}:"
            f"measured_LRA={stats['input_lra']}:measured_thresh={stats['input_thresh']}:"
            f"offset={stats['target_offset']}:linear=true,aresample=48000"
        )
        run(
            [
                "ffmpeg",
                "-hide_banner",
                "-y",
                "-i",
                str(mixed.with_suffix(".mkv")),
                "-c:v",
                "copy",
                "-af",
                norm,
                "-c:a",
                "aac",
                "-b:a",
                "256k",
                "-movflags",
                "+faststart",
                str(final),
            ]
        )
    lines = [
        args.film_title,
        "",
        audio.get("description", ""),
        "",
        f"Play the free demo of Standard of Iron on Steam: {STEAM_URL}",
        "Open source: https://github.com/djeada/standard-of-iron",
        "",
        "Chapters:",
    ]
    lines += [f"{stamp(start)} {chapter_name(title)}" for title, start in chapters]
    (args.out / f"{spec['id']}_youtube.txt").write_text("\n".join(lines) + "\n")
    print(f"wrote {final} ({total / 60:.1f} min)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
