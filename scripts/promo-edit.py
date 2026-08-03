#!/usr/bin/env python3
"""Cut, grade and caption the clips ``arena_app --promo-spec`` recorded.

The arena records one clip per authored shot plus a ``shots.json`` manifest.
This script turns that raw footage into a finished short in a single ffmpeg
pass -- join, grade, caption, title card, fade -- so the footage is only
re-encoded once. The frame is whatever the spec asked the arena to record;
vertical suits a battle, landscape suits a battle line.

Everything the edit needs lives in the same promo spec the arena consumed. The
arena ignores keys it does not know, so the editorial fields sit next to the
camera work they belong to::

    {
      "title": "THE LAST STAND",
      "subtitle": "STANDARD OF IRON",
      "grade": {"contrast": 1.12, "saturation": 1.14},
      "transition": {"type": "dissolve", "duration": 0.35},
      "shots": [
        {"name": "collision", "caption": "HOLD THE LINE",
         "transition": {"type": "dip", "duration": 0.5}, ...}
      ]
    }

A shot's ``transition`` describes how the cut *into* it is played, so the first
shot's is ignored. The spec-level ``transition`` is the default for every join;
``{"type": "cut"}`` on a shot restores a hard cut for that one join. See
``TRANSITIONS`` for the vocabulary. Joined shots overlap, so the finished cut is
shorter than the sum of its clips and caption timing is measured on the blended
timeline rather than on raw clip lengths.

Typical use::

    build/bin/arena_app --promo-spec tools/arena/promos/last_stand.json \\
      --promo-out artifacts/promo
    scripts/promo-edit.py --spec tools/arena/promos/last_stand.json \\
      --clips artifacts/promo/last_stand

Two delivery rules this script enforces, because both are invisible until the
short is already published:

* **Frame zero is never black.** Platforms use it as the thumbnail, so the cut
  opens hard rather than fading up from black; ``--opening-fade`` restores a
  fade deliberately. The finished file is read back and the run fails if frame
  zero is black anyway.
* **The score is mastered.** The music runs through ``audio_master_preview``,
  which links the same chain the game applies at decode, so the short is scored
  with the audio a player hears. Delivery then only adds headroom for the AAC
  encoder rather than a second loudness normaliser. See
  ``docs/AUDIO_MASTERING.md``.

Exit status is non-zero when the footage is missing or ffmpeg fails, so this
can be chained straight after a capture run.
"""

from __future__ import annotations

import argparse
import array
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

CAPTION_Y_FRACTION = 0.70
TITLE_Y_FRACTION = 0.42
CAPTION_FADE = 0.25
END_CARD_SECONDS = 2.2
OPENING_FADE = 0.0
FIRST_FRAME_MIN_PEAK = 8
MASTER_TOOL_CANDIDATES = (
    "build/bin/audio_master_preview",
    "build-release/bin/audio_master_preview",
    "build-debug/bin/audio_master_preview",
)
PLATFORM_CEILING = 0.631


def find_master_tool() -> Path | None:
    """Locate the binary that applies the game's decode-time mastering chain."""
    override = os.environ.get("SOI_AUDIO_MASTER")
    candidates = [Path(override)] if override else []
    candidates += [Path(candidate) for candidate in MASTER_TOOL_CANDIDATES]
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    return None


def master_music(track: Path, workdir: Path) -> Path | None:
    """Run the music through the same chain the game applies at decode.

    Promo audio that skipped it carried the raw generated master, clipping and
    all, which is the one thing viewers hear before they see anything.
    """
    tool = find_master_tool()
    if tool is None:
        print(
            "promo-edit: warning: audio_master_preview not built, scoring with the "
            "raw track. Run 'cmake --build build --target audio_master_preview' to "
            "get the same audio the game plays.",
            file=sys.stderr,
        )
        return None
    rendered = workdir / f"{track.stem}.mastered.wav"
    result = subprocess.run(
        [str(tool), "--render", str(rendered), str(track)],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0 or not rendered.is_file():
        print(
            f"promo-edit: warning: mastering {track} failed, scoring with the raw "
            f"track ({result.stderr.strip()})",
            file=sys.stderr,
        )
        return None
    print(f"promo-edit: scored with the mastered render of {track.name}")
    return rendered


def first_frame_peak(video: Path) -> int:
    """Brightest sample in frame zero, 0-255, or -1 when it cannot be read."""
    result = subprocess.run(
        [
            "ffmpeg",
            "-hide_banner",
            "-loglevel",
            "error",
            "-i",
            str(video),
            "-frames:v",
            "1",
            "-f",
            "rawvideo",
            "-pix_fmt",
            "gray",
            "-",
        ],
        check=False,
        capture_output=True,
    )
    if result.returncode != 0 or not result.stdout:
        return -1
    return max(array.array("B", result.stdout))


TRANSITIONS = {
    "cut": None,
    "dissolve": "fade",
    "dip": "fadeblack",
    "flash": "fadewhite",
    "whip": "hblur",
    "smear": "smoothleft",
    "smear_right": "smoothright",
    "push": "slideleft",
    "push_right": "slideright",
    "zoom": "zoomin",
    "radial": "radial",
    "grain": "pixelize",
    "iris": "circleopen",
    "wipe": "wipeleft",
    "bleach": "fadegrays",
}

DEFAULT_TRANSITION = "dissolve"
DEFAULT_TRANSITION_SECONDS = 0.35

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


class Join:
    """How one shot is played into the next."""

    __slots__ = ("kind", "transition", "seconds")

    def __init__(self, kind: str, transition: str | None, seconds: float) -> None:
        self.kind = kind
        self.transition = transition
        self.seconds = seconds if transition else 0.0

    @property
    def blended(self) -> bool:
        return self.transition is not None and self.seconds > 0.0


HARD_CUT = Join("cut", None, 0.0)


def read_transition(config, kind: str, seconds: float) -> tuple[str, float]:
    """Read a spec's ``transition`` value over a kind/duration pair.

    Accepts a bare name (``"dip"``), a full object, or nothing at all, so a reel
    that wants one dissolve everywhere only writes the spec-level default. Kind
    and duration stay separate all the way down: a spec whose default is a hard
    cut must still hand its authored duration to the one shot that asks for a
    dissolve.
    """
    if isinstance(config, str):
        kind = config
    elif isinstance(config, dict):
        kind = str(config.get("type", kind))
        seconds = float(config.get("duration", seconds))
    elif config is not None:
        fail(f"a transition must be a name or an object, not {type(config).__name__}")

    kind = kind.strip().lower()
    if kind not in TRANSITIONS:
        known = ", ".join(sorted(TRANSITIONS))
        fail(f"unknown transition '{kind}'; known transitions are {known}")
    if seconds < 0:
        fail(f"transition '{kind}' has a negative duration")
    return kind, seconds


def resolve_join(config, default_kind: str, default_seconds: float) -> Join:
    kind, seconds = read_transition(config, default_kind, default_seconds)
    return Join(kind, TRANSITIONS[kind], seconds)


def plan_timeline(
    lengths: list[float], joins: list[Join]
) -> tuple[list[tuple[float, float]], float]:
    """Place every shot on the finished timeline.

    A blended join overlaps its two shots, so each transition pulls the rest of
    the reel earlier by its own duration. An overlap is clamped against both
    neighbours' *unshared* footage: two dissolves either side of a short shot
    must not consume the same frames twice, or the second xfade would be asked
    for an offset that lies before the first one finished.
    """
    spans: list[tuple[float, float]] = []
    cursor = 0.0
    previous_overlap = 0.0
    for index, length in enumerate(lengths):
        join = joins[index]
        overlap = 0.0
        if index > 0 and join.blended:
            headroom = min(lengths[index - 1] - previous_overlap, length)
            overlap = min(join.seconds, max(0.0, headroom * 0.9))
            if overlap <= 0.0:
                join.seconds = 0.0
                join.transition = None
            else:
                join.seconds = overlap
        cursor = max(0.0, cursor - overlap)
        spans.append((cursor, cursor + length))
        cursor += length
        previous_overlap = overlap
    return spans, cursor


def build_join_graph(
    lengths: list[float], joins: list[Join], spans: list[tuple[float, float]]
) -> tuple[list[str], str]:
    """Join the clips into one stream, blending where a transition asks for it.

    xfade has no zero-length form, so runs of hard cuts are concatenated into a
    single segment first and only the blended joins become xfades.
    """
    segments: list[list[int]] = [[0]]
    for index in range(1, len(lengths)):
        if joins[index].blended:
            segments.append([index])
        else:
            segments[-1].append(index)

    chain: list[str] = []
    labels: list[str] = []
    for position, members in enumerate(segments):
        if len(members) == 1:
            labels.append(f"{members[0]}:v")
            continue
        sources = "".join(f"[{index}:v]" for index in members)
        chain.append(f"{sources}concat=n={len(members)}:v=1:a=0[seg{position}]")
        labels.append(f"seg{position}")

    stage = labels[0]
    for position in range(1, len(segments)):
        join = joins[segments[position][0]]
        offset = spans[segments[position][0]][0]
        chain.append(
            f"[{stage}][{labels[position]}]"
            f"xfade=transition={join.transition}:duration={join.seconds:.3f}"
            f":offset={offset:.3f}[join{position}]"
        )
        stage = f"join{position}"
    return chain, stage


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
    parser.add_argument(
        "--opening-fade",
        type=float,
        default=OPENING_FADE,
        help=(
            "seconds of fade-in on the opening frame (default: 0, because a fade "
            "makes frame zero black and platforms use it as the thumbnail)"
        ),
    )
    parser.add_argument(
        "--transition",
        choices=tuple(sorted(TRANSITIONS)),
        help="override every join the spec asks for (use 'cut' for hard cuts)",
    )
    parser.add_argument(
        "--transition-duration",
        type=float,
        help="override every transition's length in seconds",
    )
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
    authored = {shot.get("name"): shot for shot in spec.get("shots", [])}
    font = resolve_font(args.font)
    output = args.out or args.clips.with_suffix(".mp4")
    output.parent.mkdir(parents=True, exist_ok=True)

    default_kind, default_seconds = read_transition(
        spec.get("transition"), DEFAULT_TRANSITION, DEFAULT_TRANSITION_SECONDS
    )

    inputs: list[str] = []
    names: list[str] = []
    lengths: list[float] = []
    joins: list[Join] = []
    for index, shot in enumerate(shots):
        clip = args.clips / shot["clip"]
        if not clip.is_file():
            fail(f"missing clip {clip}")
        inputs += ["-i", str(clip)]
        length = float(shot.get("clip_seconds") or 0.0)
        if length <= 0:
            fail(f"clip {clip.name} reports no duration")
        name = shot.get("name", "")
        names.append(name)
        lengths.append(length)
        if index == 0:
            joins.append(HARD_CUT)
            continue
        authored_join = (
            args.transition
            if args.transition
            else authored.get(name, {}).get("transition")
        )
        join = resolve_join(authored_join, default_kind, default_seconds)
        if args.transition_duration is not None:
            join.seconds = args.transition_duration
        joins.append(join)

    spans, total = plan_timeline(lengths, joins)
    width = int(manifest.get("width", 1080))
    height = int(manifest.get("height", 1920))

    chain, joined = build_join_graph(lengths, joins, spans)
    chain.append(f"[{joined}]{build_grade(spec.get('grade', {}))}[graded]")

    timeline = [
        (names[index], spans[index][0], spans[index][1]) for index in range(len(names))
    ]
    stage = "graded"
    if not args.no_captions:

        type_base = min(width, height)
        caption_size = max(46, int(type_base * 0.075))
        title_size = max(60, int(type_base * 0.105))
        subtitle_size = max(30, int(type_base * 0.040))
        caption_y = f"h*{CAPTION_Y_FRACTION}"
        safe_width = int(width * 0.88)
        card_start = max(0.0, total - END_CARD_SECONDS)
        has_card = bool(spec.get("title") or spec.get("subtitle"))
        step = 0
        for index, (name, start, end) in enumerate(timeline):
            caption = captions.get(name)
            if not caption:
                continue

            leading = max(0.15, joins[index].seconds)
            trailing = 0.15
            if index + 1 < len(joins):
                trailing = max(trailing, joins[index + 1].seconds)
            visible_start = start + leading
            visible_end = max(visible_start + 0.6, end - trailing)

            if has_card and visible_start < card_start:
                visible_end = min(visible_end, card_start - CAPTION_FADE)
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

    opening = max(0.0, args.opening_fade)
    fades = []
    if opening > 0.0:
        fades.append(f"fade=t=in:st=0:d={opening:.2f}")
    fades.append(f"fade=t=out:st={max(0.0, total - 0.55):.3f}:d=0.55")
    chain.append(f"[{stage}]" + ",".join(fades) + "[vout]")

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
    workdir: tempfile.TemporaryDirectory | None = None
    if mode == "music":
        start = (
            args.music_start
            if args.music_start is not None
            else float(spec.get("music_start", 0.0))
        )
        workdir = tempfile.TemporaryDirectory(prefix="promo-edit-")
        mastered = master_music(music, Path(workdir.name))
        command += ["-ss", f"{start:.3f}", "-i", str(mastered or music)]

        if mastered is not None:

            level = f"alimiter=limit={PLATFORM_CEILING:.3f}:level=disabled"
        else:
            level = "loudnorm=I=-14:TP=-1.5:LRA=11"
        filter_complex += (
            f";[{audio_index}:a]aformat=channel_layouts=stereo,aresample=48000,"
            f"{level},"
            f"afade=t=in:st=0:d=1.0,"
            f"afade=t=out:st={max(0.0, total - 1.8):.3f}:d=1.8[aout]"
        )
    elif mode == "drums":
        command += ["-f", "lavfi", "-i", build_drum_bed(total, args.tempo)]
        filter_complex += (
            f";[{audio_index}:a]afade=t=in:st=0:d=0.6,"
            f"afade=t=out:st={max(0.0, total - 1.2):.3f}:d=1.2,"
            f"alimiter=limit={PLATFORM_CEILING:.3f}:level=disabled[aout]"
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

    blended = [join for join in joins[1:] if join.blended]
    joined_note = (
        f"{len(blended)} blended join(s): "
        + ", ".join(f"{join.kind} {join.seconds:.2f}s" for join in blended)
        if blended
        else "hard cuts throughout"
    )
    print(
        f"promo-edit: cutting {len(shots)} shot(s), {total:.2f}s, "
        f"{width}x{height} -> {output}\npromo-edit: {joined_note}"
    )
    result = subprocess.run(command, check=False)
    if workdir is not None:
        workdir.cleanup()
    if result.returncode != 0:
        fail(f"ffmpeg failed with status {result.returncode}")

    peak = first_frame_peak(output)
    if peak < 0:
        print("promo-edit: warning: could not read the first frame back")
    elif peak < FIRST_FRAME_MIN_PEAK:
        fail(
            f"the first frame of {output} is black (peak luma {peak}); social "
            "platforms use it as the thumbnail. Check --opening-fade and the "
            "first shot's framing."
        )

    print(f"promo-edit: wrote {output} (first frame peak luma {peak})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
