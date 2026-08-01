#!/usr/bin/env python3
"""Cut, grade and caption the clips ``arena_app --promo-spec`` recorded.

The arena records one clip per authored shot plus a ``shots.json`` manifest.
This script turns that raw footage into a finished vertical short in a single
ffmpeg pass -- concatenate, grade, caption, title card, fade -- so the footage
is only re-encoded once.

Everything the edit needs lives in the same promo spec the arena consumed. The
arena ignores keys it does not know, so the editorial fields sit next to the
camera work they belong to::

    {
      "title": "THE LAST STAND",
      "subtitle": "STANDARD OF IRON",
      "grade": {"contrast": 1.12, "saturation": 1.14},
      "shots": [
        {"name": "collision", "caption": "HOLD THE LINE", ...}
      ]
    }

Typical use::

    build/bin/arena_app --promo-spec tools/arena/promos/last_stand.json \\
      --promo-out artifacts/promo
    scripts/promo-edit.py --spec tools/arena/promos/last_stand.json \\
      --clips artifacts/promo/last_stand

Exit status is non-zero when the footage is missing or ffmpeg fails, so this
can be chained straight after a capture run.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path


CAPTION_Y_FRACTION = 0.70
TITLE_Y_FRACTION = 0.42
CAPTION_FADE = 0.25
END_CARD_SECONDS = 2.2

FONT_CANDIDATES = (
    "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSerif-Bold.ttf",
)


def fail(message: str) -> None:
    print(f"promo-edit: {message}", file=sys.stderr)
    raise SystemExit(1)


def resolve_font(requested: str | None) -> str:
    candidates = (requested, *FONT_CANDIDATES) if requested else FONT_CANDIDATES
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return candidate
    fail("no usable bold font found; pass --font with a .ttf path")
    raise AssertionError("unreachable")


def fit_font_size(text: str, font: str, size: int, max_width: int) -> int:
    """Shrink a font size until the rendered string fits the frame width.

    drawtext has no auto-fit, and a title that overruns the frame is silently
    cropped rather than reported.
    """
    try:
        from PIL import ImageFont
    except ImportError:
        # Without a metrics library, fall back to a conservative estimate of
        # advance width for a bold serif face.
        estimated = int(max_width / max(len(text), 1) / 0.62)
        return max(18, min(size, estimated))

    for candidate in range(size, 17, -2):
        face = ImageFont.truetype(font, candidate)
        if face.getbbox(text)[2] <= max_width:
            return candidate
    return 18


def escape_text(text: str) -> str:
    """Escape a caption for ffmpeg's drawtext, which parses its own syntax."""
    out = text.replace("\\", r"\\\\")
    for char in (":", "'", "%", "[", "]", ","):
        out = out.replace(char, f"\\{char}")
    return out


def drawtext(
    *,
    text: str,
    font: str,
    size: int,
    y_expr: str,
    start: float,
    end: float,
    color: str = "white",
) -> str:
    """A centred, fading caption between two absolute timeline seconds."""
    fade_in = f"min(1,(t-{start:.3f})/{CAPTION_FADE})"
    fade_out = f"min(1,({end:.3f}-t)/{CAPTION_FADE})"
    alpha = f"if(between(t,{start:.3f},{end:.3f}),min({fade_in},{fade_out}),0)"
    return (
        f"drawtext=fontfile='{font}':text='{escape_text(text)}'"
        f":fontcolor={color}:fontsize={size}:x=(w-text_w)/2:y={y_expr}"
        f":borderw=4:bordercolor=black@0.85"
        f":shadowcolor=black@0.55:shadowx=0:shadowy=6"
        f":alpha='{alpha}'"
    )


def build_grade(grade: dict) -> str:
    """Contrast, colour and texture pass applied once to the whole cut."""
    contrast = float(grade.get("contrast", 1.12))
    saturation = float(grade.get("saturation", 1.14))
    brightness = float(grade.get("brightness", 0.03))
    gamma = float(grade.get("gamma", 1.08))
    vignette = float(grade.get("vignette", 0.10))
    grain = float(grade.get("grain", 5))
    sharpen = float(grade.get("sharpen", 0.6))

    stages = [
        f"eq=contrast={contrast}:saturation={saturation}"
        f":brightness={brightness}:gamma={gamma}",
        f"unsharp=5:5:{sharpen}:5:5:0.0",
    ]
    if vignette > 0:
        stages.append(f"vignette=angle={vignette:.3f}*PI")
    if grain > 0:
        stages.append(f"noise=alls={int(grain)}:allf=t+u")
    return ",".join(stages)


def build_drum_bed(seconds: float, tempo: float) -> str:
    """A procedural war-drum bed.

    Promo footage needs a pulse, and generating one from oscillators keeps the
    output free of any third-party audio licensing.
    """
    beat = 60.0 / tempo
    kick = (
        f"0.75*sin(2*PI*52*t)*exp(-7*mod(t,{beat:.4f}))"
        f"+0.35*sin(2*PI*98*t)*exp(-13*mod(t,{beat:.4f}))"
    )
    offbeat = f"+0.22*sin(2*PI*70*t)*exp(-16*mod(t+{beat / 2:.4f},{beat:.4f}))"
    drone = "+0.10*sin(2*PI*41*t)+0.05*sin(2*PI*61.5*t)"
    swell = f"*(0.55+0.45*min(1,t/{max(seconds * 0.6, 0.1):.3f}))"
    return f"aevalsrc='({kick}{offbeat}{drone}){swell}':s=48000:d={seconds:.3f}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--spec", required=True, type=Path, help="promo spec JSON")
    parser.add_argument(
        "--clips",
        required=True,
        type=Path,
        help="directory holding the recorded clips and shots.json",
    )
    parser.add_argument("--out", type=Path, help="output file (default: <clips>.mp4)")
    parser.add_argument("--font", help="path to a bold .ttf for captions")
    parser.add_argument(
        "--no-captions", action="store_true", help="skip all text overlays"
    )
    parser.add_argument(
        "--audio",
        choices=("music", "drums", "none"),
        default="music",
        help=(
            "score the cut with the game soundtrack the spec names, with a "
            "procedural drum bed as the fallback (default: music)"
        ),
    )
    parser.add_argument("--music", type=Path, help="override the spec's music track")
    parser.add_argument(
        "--music-start",
        type=float,
        help="seconds into the track to start (default: the spec's value)",
    )
    parser.add_argument("--tempo", type=float, default=104.0, help="drum bed tempo")
    args = parser.parse_args()

    if shutil.which("ffmpeg") is None:
        fail("ffmpeg is required but was not found on PATH")

    if not args.spec.is_file():
        fail(f"promo spec not found: {args.spec}")
    spec = json.loads(args.spec.read_text())

    manifest_path = args.clips / "shots.json"
    if not manifest_path.is_file():
        fail(f"no shots.json in {args.clips}; run arena_app --promo-spec first")
    manifest = json.loads(manifest_path.read_text())

    shots = manifest.get("shots", [])
    if not shots:
        fail("the capture manifest contains no shots")

    captions = {shot.get("name"): shot.get("caption") for shot in spec.get("shots", [])}
    font = resolve_font(args.font)
    output = args.out or args.clips.with_suffix(".mp4")
    output.parent.mkdir(parents=True, exist_ok=True)

    inputs: list[str] = []
    concat_labels = ""
    timeline: list[tuple[str, float, float]] = []
    cursor = 0.0
    for index, shot in enumerate(shots):
        clip = args.clips / shot["clip"]
        if not clip.is_file():
            fail(f"missing clip {clip}")
        inputs += ["-i", str(clip)]
        concat_labels += f"[{index}:v]"
        length = float(shot.get("clip_seconds") or 0.0)
        if length <= 0:
            fail(f"clip {clip.name} reports no duration")
        timeline.append((shot.get("name", ""), cursor, cursor + length))
        cursor += length

    total = cursor
    width = int(manifest.get("width", 1080))
    height = int(manifest.get("height", 1920))

    chain = [
        f"{concat_labels}concat=n={len(shots)}:v=1:a=0[cat]",
        f"[cat]{build_grade(spec.get('grade', {}))}[graded]",
    ]

    stage = "graded"
    if not args.no_captions:
        caption_size = max(46, int(width * 0.075))
        title_size = max(60, int(width * 0.105))
        subtitle_size = max(30, int(width * 0.040))
        caption_y = f"h*{CAPTION_Y_FRACTION}"
        safe_width = int(width * 0.88)
        step = 0
        for name, start, end in timeline:
            caption = captions.get(name)
            if not caption:
                continue

            visible_start = start + 0.15
            visible_end = max(visible_start + 0.6, end - 0.15)
            chain.append(
                f"[{stage}]"
                + drawtext(
                    text=caption,
                    font=font,
                    size=fit_font_size(caption, font, caption_size, safe_width),
                    y_expr=caption_y,
                    start=visible_start,
                    end=visible_end,
                )
                + f"[cap{step}]"
            )
            stage = f"cap{step}"
            step += 1

        card_start = max(0.0, total - END_CARD_SECONDS)
        title = spec.get("title")
        if title:
            chain.append(
                f"[{stage}]"
                + drawtext(
                    text=title,
                    font=font,
                    size=fit_font_size(title, font, title_size, safe_width),
                    y_expr=f"h*{TITLE_Y_FRACTION}",
                    start=card_start,
                    end=total,
                )
                + "[titled]"
            )
            stage = "titled"
        subtitle = spec.get("subtitle")
        if subtitle:
            chain.append(
                f"[{stage}]"
                + drawtext(
                    text=subtitle,
                    font=font,
                    size=fit_font_size(subtitle, font, subtitle_size, safe_width),
                    y_expr=f"h*{TITLE_Y_FRACTION}+{int(title_size * 1.35)}",
                    start=card_start + 0.35,
                    end=total,
                    color="#e6c98a",
                )
                + "[sub]"
            )
            stage = "sub"

    # No fade in: social platforms take the first frame as the cover image, and
    # a fade would hand them a black one. The cut opens on picture instead.
    chain.append(
        f"[{stage}]fade=t=out:st={max(0.0, total - 0.55):.3f}:d=0.55[vout]"
    )

    command = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", *inputs]
    filter_complex = ";".join(chain)

    music = args.music or (
        Path(spec["music"]) if args.audio == "music" and spec.get("music") else None
    )
    if music is not None and not music.is_file():
        fail(f"music track not found: {music}")
    mode = args.audio
    if mode == "music" and music is None:

        mode = "drums"

    audio_index = len(shots)
    if mode == "music":
        start = (
            args.music_start
            if args.music_start is not None
            else float(spec.get("music_start", 0.0))
        )
        command += ["-ss", f"{start:.3f}", "-i", str(music)]

        filter_complex += (
            f";[{audio_index}:a]aformat=channel_layouts=mono,aresample=48000,"
            "pan=stereo|c0=c0|c1=c0,loudnorm=I=-14:TP=-1.5:LRA=11,"
            f"afade=t=in:st=0:d=1.0,"
            f"afade=t=out:st={max(0.0, total - 1.8):.3f}:d=1.8[aout]"
        )
    elif mode == "drums":
        command += ["-f", "lavfi", "-i", build_drum_bed(total, args.tempo)]
        filter_complex += (
            f";[{audio_index}:a]afade=t=in:st=0:d=0.6,"
            f"afade=t=out:st={max(0.0, total - 1.2):.3f}:d=1.2,"
            "alimiter=limit=0.9[aout]"
        )

    command += ["-filter_complex", filter_complex, "-map", "[vout]"]
    if mode != "none":
        command += ["-map", "[aout]", "-c:a", "aac", "-b:a", "192k"]
    command += [
        "-c:v",
        "libx264",
        "-preset",
        "slow",
        "-crf",
        "19",
        "-pix_fmt",
        "yuv420p",
        "-movflags",
        "+faststart",
        "-r",
        str(manifest.get("fps", 60)),
        "-t",
        f"{total:.3f}",
        str(output),
    ]

    print(
        f"promo-edit: cutting {len(shots)} shot(s), {total:.2f}s, "
        f"{width}x{height} -> {output}"
    )
    result = subprocess.run(command, check=False)
    if result.returncode != 0:
        fail(f"ffmpeg failed with status {result.returncode}")

    print(f"promo-edit: wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
