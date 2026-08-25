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
      "sfx": [
        {"file": "assets/audio/sfx/combat/roman_cavalry_charge.ogg",
         "at": 12.4, "gain": 0.9}
      ],
      "shots": [
        {"name": "collision", "caption": "HOLD THE LINE",
         "transition": {"type": "dip", "duration": 0.5}, ...}
      ]
    }

``sfx`` cues are timed one-shots laid over the score. ``at`` is a time on the
finished, blended timeline -- the same one caption timing uses -- so a cue is
placed against the cut the viewer sees rather than against raw clip lengths.

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

Three delivery rules this script enforces, because all three are invisible until
the short is already published:

* **Frame zero is never black.** Platforms use it as the thumbnail, so the cut
  opens hard rather than fading up from black; ``--opening-fade`` restores a
  fade deliberately, and a card at timeline zero carries its text at full
  opacity on its first frame. The finished file is read back and measured for
  how much of it is bright enough to see, because a peak alone passes a black
  frame with grain on it. See ``docs/PROMO_CAPTURE.md``.
* **The cut does not flash.** The finished file is measured for mean-luminance
  jumps between frames and refused if it trips a miniature WCAG 2.3.1: any jump
  over ``FLASH_HARD_DELTA``, or more than ``FLASHES_PER_SECOND`` jumps over
  ``FLASH_DELTA`` inside a one-second window. A ``flash`` join drives the whole
  frame to white and measured 102-123/255 in a single frame on this reel, which
  is a seizure risk rather than a stylistic choice; ``dip`` goes through black
  and lands around 20. ``--allow-flashes`` publishes anyway when content really
  needs it.
* **The score is mastered.** The music runs through ``audio_master_preview``,
  which links the same chain the game applies at decode, so the short is scored
  with the audio a player hears. Delivery then only adds headroom for the AAC
  encoder rather than a second loudness normaliser. See
  ``docs/AUDIO_MASTERING.md``.

Exit status is non-zero when the footage is missing or ffmpeg fails, so this
can be chained straight after a capture run. A cut that fails a delivery check
is never left at the output path -- it is quarantined as ``<name>.rejected.mp4``
and any earlier publish is removed, so a failed run cannot leave an uploadable
file behind for a later step to pick up.
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
FREEZE_TEXT_Y_FRACTION = 0.44
TITLE_Y_FRACTION = 0.42
CAPTION_FADE = 0.25
END_CARD_SECONDS = 2.2
OPENING_FADE = 0.0
FIRST_FRAME_MIN_PEAK = 8
FIRST_FRAME_VISIBLE_LUMA = 32
FIRST_FRAME_MIN_VISIBLE = 0.001

PUNCH_MAX = 0.22
FLASH_DELTA = 25
FLASH_HARD_DELTA = 60
FLASHES_PER_SECOND = 3


MOTION_LIMITS = {
    "yaw": 12.0,
    "pitch": 6.0,
    "fov": 6.0,
    "roll_rate": 4.0,
    "roll": 5.0,
    "shake": 0.03,
    "min_clip": 1.5,
    "mean_clip": 2.0,
}


MOTION_P90_LIMIT = 6.0
MASTER_TOOL_CANDIDATES = (
    "build/bin/audio_master_preview",
    "build-release/bin/audio_master_preview",
    "build-debug/bin/audio_master_preview",
)
PLATFORM_CEILING = 0.631
SCORE_UNDER_SFX = 1.0


def find_master_tool() -> Path | None:
    """Locate the binary that applies the game's decode-time mastering chain."""
    override = os.environ.get("SOI_AUDIO_MASTER")
    candidates = [Path(override)] if override else []
    candidates += [Path(candidate) for candidate in MASTER_TOOL_CANDIDATES]
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    return None


def build_sfx_layer(
    filter_complex: str,
    cues: list[dict],
    first_input: int,
    bed_label: str,
) -> tuple[str, list[str]]:
    """Lay timed one-shots over the score and mix them into a single bus.

    A cue is ``{"file": ..., "at": <seconds on the finished timeline>}`` plus an
    optional linear ``gain``. Times are measured on the *blended* timeline, the
    same one captions use, because that is the cut the viewer hears.

    ``amix`` defaults to dividing by the number of inputs, which would drop the
    score by 10 dB the moment a single wolf barks over it; ``normalize=0`` keeps
    every input at the level it was given.

    **The score is not ducked.** Sidechaining the music under the effects was
    tried and removed: a reel with twenty-odd cues spends most of its length
    inside one cue or another, so the score was pulled down for 9% of its
    windows and the result read as quiet and discontinuous — the music pumping
    against the action rather than playing under it.

    A static balance does the same job without moving: the score sits at
    ``SCORE_UNDER_SFX`` for the whole cut and the effects are mixed on top with
    their own gains. That leaves the score continuous and lets any number of
    cues overlap, which is what a mixer is for. The limiter at the end catches
    the sum.
    """
    inputs: list[str] = []
    labels: list[str] = []
    for index, cue in enumerate(cues):
        path = Path(cue["file"])
        if not path.is_file():
            fail(f"sfx file not found: {path}")
        at_ms = int(round(max(0.0, float(cue.get("at", 0.0))) * 1000.0))
        gain = float(cue.get("gain", 1.0))
        label = f"sfx{index}"
        inputs += ["-i", str(path)]
        filter_complex += (
            f";[{first_input + index}:a]aformat=channel_layouts=stereo,"
            f"aresample=48000,volume={gain:.3f},"
            f"adelay={at_ms}|{at_ms}[{label}]"
        )
        labels.append(f"[{label}]")

    filter_complex += (
        f";{''.join(labels)}amix=inputs={len(labels)}:duration=longest:"
        f"dropout_transition=0:normalize=0[sfxmix]"
        f";[{bed_label}]volume={SCORE_UNDER_SFX:.3f}[bedlow]"
        f";[bedlow][sfxmix]amix=inputs=2:duration=first:"
        f"dropout_transition=0:normalize=0,"
        f"alimiter=limit={PLATFORM_CEILING:.3f}:level=disabled[aout]"
    )
    return filter_complex, inputs


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


def frame_metrics(video: Path) -> tuple[list[float], list[float]]:
    """Per-frame luminance jump and motion energy over the finished cut.

    Both are read from one downscaled greyscale pass. The first is the mean
    luminance change, which is what flashing looks like. The second is the mean
    *absolute pixel* change, which is what a moving camera looks like -- a whip
    pan barely moves mean luminance while replacing most of the frame.
    """
    width, height = 64, 36
    result = subprocess.run(
        [
            "ffmpeg",
            "-hide_banner",
            "-loglevel",
            "error",
            "-i",
            str(video),
            "-vf",
            f"scale={width}:{height},format=gray",
            "-f",
            "rawvideo",
            "-",
        ],
        capture_output=True,
        check=False,
    )
    if result.returncode != 0 or not result.stdout:
        return [], []
    size = width * height
    raw = result.stdout
    frames = [raw[i * size : (i + 1) * size] for i in range(len(raw) // size)]
    means = [sum(frame) / size for frame in frames]
    jumps = [abs(b - a) for a, b in zip(means, means[1:], strict=False)]
    motion = [
        sum(abs(x - y) for x, y in zip(a, b, strict=False)) / size
        for a, b in zip(frames, frames[1:], strict=False)
    ]
    return jumps, motion


def motion_offence(motion: list[float]) -> str | None:
    """Refuse a cut whose frame is in constant violent movement."""
    if not motion:
        return None
    ordered = sorted(motion)
    p90 = ordered[int(0.9 * (len(ordered) - 1))]
    if p90 > MOTION_P90_LIMIT:
        return (
            f"nine frames in ten change by {p90:.1f}/255 or more "
            f"(limit {MOTION_P90_LIMIT}); the camera is moving through the whole cut"
        )
    return None


def shorter_arc(from_degrees: float, to_degrees: float) -> float:
    delta = (to_degrees - from_degrees) % 360.0
    return delta - 360.0 if delta > 180.0 else delta


def camera_offences(spec: dict) -> list[str]:
    """Author-time camera limits, mirrored from `Arena::Promo::motion_violations`."""
    offences: list[str] = []
    clips: list[float] = []
    for shot in spec.get("shots", []):
        if shot.get("flame_card"):
            continue
        name = shot.get("name", "?")
        clip = float(shot.get("duration", 0.0)) * float(shot.get("slow_motion", 1.0))
        clips.append(clip)
        if clip + 1e-3 < MOTION_LIMITS["min_clip"]:
            offences.append(
                f"{name}: is on screen for {clip:.2f}s, under the "
                f"{MOTION_LIMITS['min_clip']:.2f}s a viewer needs to read a frame"
            )
        if float(shot.get("shake", 0.0)) > MOTION_LIMITS["shake"] + 1e-4:
            offences.append(
                f"{name}: shakes at {float(shot['shake']):.3f}, over the "
                f"{MOTION_LIMITS['shake']:.3f} ceiling"
            )
        if shot.get("gameplay_camera"):
            continue
        keys = shot.get("camera", [])
        for key in keys:
            if abs(float(key.get("roll", 0.0))) > MOTION_LIMITS["roll"] + 1e-4:
                offences.append(
                    f"{name}: rolls the horizon {abs(float(key['roll'])):.1f} degrees, "
                    f"over the {MOTION_LIMITS['roll']:.1f} ceiling"
                )
                break
        for before, after in zip(keys, keys[1:], strict=False):
            span = max(
                0.001, float(after.get("time", 0.0)) - float(before.get("time", 0.0))
            )
            for axis, limit, delta in (
                (
                    "yaw",
                    MOTION_LIMITS["yaw"],
                    shorter_arc(
                        float(before.get("yaw", 0.0)), float(after.get("yaw", 0.0))
                    ),
                ),
                (
                    "pitch",
                    MOTION_LIMITS["pitch"],
                    float(after.get("pitch", 0.0)) - float(before.get("pitch", 0.0)),
                ),
                (
                    "fov",
                    MOTION_LIMITS["fov"],
                    float(after.get("fov", 40.0)) - float(before.get("fov", 40.0)),
                ),
                (
                    "roll",
                    MOTION_LIMITS["roll_rate"],
                    float(after.get("roll", 0.0)) - float(before.get("roll", 0.0)),
                ),
            ):
                rate = abs(delta) / span
                if rate > limit + 1e-3:
                    offences.append(
                        f"{name}: swings {axis} at {rate:.1f} deg/s, over the "
                        f"{limit:.1f} deg/s ceiling"
                    )
    if clips:
        mean = sum(clips) / len(clips)
        if mean + 1e-3 < MOTION_LIMITS["mean_clip"]:
            offences.append(
                f"the cut averages {mean:.2f}s a shot, under the "
                f"{MOTION_LIMITS['mean_clip']:.2f}s that keeps a reel from reading "
                "as strobing"
            )
    return offences


def photosensitivity_offence(jumps: list[float], fps: float) -> str | None:
    """WCAG 2.3.1 in miniature: no more than three big luminance jumps a second."""
    if not jumps:
        return None
    window = max(1, int(round(fps)))
    for index, jump in enumerate(jumps):
        if jump > FLASH_HARD_DELTA:
            return (
                f"a {jump:.0f}/255 luminance jump at {index / fps:.2f}s "
                f"(limit {FLASH_HARD_DELTA})"
            )
    for start in range(0, max(1, len(jumps) - window + 1)):
        flashes = sum(1 for j in jumps[start : start + window] if j > FLASH_DELTA)
        if flashes > FLASHES_PER_SECOND:
            return (
                f"{flashes} luminance jumps over {FLASH_DELTA}/255 within one second "
                f"at {start / fps:.2f}s (limit {FLASHES_PER_SECOND})"
            )
    return None


class FirstFrame:
    """What frame zero looks like to a thumbnail crawler.

    ``peak`` alone cannot answer the question. Film grain over a black card puts
    a handful of samples above any low threshold, and a title card is legible
    while being 99% black, so a mean would reject it. What separates "an image"
    from "black with something in it" is how much of the frame is bright enough
    to see at all, which is what ``visible_fraction`` counts.
    """

    __slots__ = ("peak", "visible_fraction")

    def __init__(self, peak: int, visible_fraction: float) -> None:
        self.peak = peak
        self.visible_fraction = visible_fraction

    @property
    def readable(self) -> bool:
        return (
            self.peak >= FIRST_FRAME_MIN_PEAK
            and self.visible_fraction >= FIRST_FRAME_MIN_VISIBLE
        )

    def describe(self) -> str:
        return f"peak luma {self.peak}, {self.visible_fraction * 100.0:.3f}% visible"


def measure_first_frame(video: Path) -> FirstFrame | None:
    """Read frame zero back off disk, or None when it cannot be decoded."""
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
        return None
    samples = array.array("B", result.stdout)
    visible = sum(1 for sample in samples if sample >= FIRST_FRAME_VISIBLE_LUMA)
    return FirstFrame(max(samples), visible / len(samples))


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

REPO_ROOT = Path(__file__).resolve().parent.parent
BUNDLED_FONT_DIR = REPO_ROOT / "assets" / "fonts"

FONT_CANDIDATES = (
    BUNDLED_FONT_DIR / "StandardIronDisplay-Bold.ttf",
    BUNDLED_FONT_DIR / "EBGaramond12-Bold.ttf",
)
"""Preferred caption faces, most brand-specific first.

These are the faces the repository ships, not the ones the host happens to
have installed, and that is the whole point. This used to be an eight-deep
walk down /usr/share/fonts that started at EB Garamond and ended at Liberation
Serif, which meant the same promo spec rendered in a different typeface
depending on which machine cut it -- a build box, a laptop and CI could each
publish a differently-lettered reel from identical inputs and nothing in the
output said so. Reels are the game's most-seen surface; they cannot be a
function of the packages installed.

The order is also a compromise between two things the cards have to do at
once. An ancient-looking card wants inscriptional Roman capitals -- high
stroke contrast, fine bracketed serifs, the shapes cut on a Trajanic base.
But captions here are drawn over moving footage with a 4 px black border, and
hairline serifs disappear under it. So the display face leads, and EB
Garamond's bold -- old-style enough to read as carved rather than typed, with
stem weight the border cannot swallow -- backs it up and covers everything the
display face has no glyph for."""


def fail(message: str) -> None:
    print(f"promo-edit: {message}", file=sys.stderr)
    raise SystemExit(1)


def reject(staged: Path, output: Path, message: str) -> None:
    """Refuse a finished encode without leaving it where it would be published.

    The delivery checks can only run on a real encode, so by the time one fails
    the whole file exists. Publishing it anyway and reporting a non-zero status
    is not enough: a reel is cut at the end of a long capture pipeline, the
    message scrolls away, and what is left on disk is a plausible-looking .mp4
    under exactly the name the upload step expects. So the encode lands beside
    the output and is only moved into place once every check has passed. A
    rejected cut is kept for inspection under a name nobody will upload.
    """
    quarantine = output.with_suffix(f".rejected{output.suffix}")
    if staged.is_file():
        staged.replace(quarantine)
        kept = f"the rejected cut is at {quarantine} for inspection"
    else:
        kept = "no encode survived to inspect"
    removed = ""
    if output.is_file():
        output.unlink()
        removed = f"\npromo-edit: removed the earlier {output.name}, which is stale"
    print(
        f"promo-edit: refusing to publish: {message}\n"
        f"promo-edit: {kept}; {output.name} was not written{removed}",
        file=sys.stderr,
    )
    raise SystemExit(1)


def resolve_font(requested: str | None) -> str:
    if requested:
        if Path(requested).is_file():
            return requested
        fail(f"--font {requested} is not a file")
    for candidate in FONT_CANDIDATES:
        if candidate.is_file():
            return str(candidate)
    fail(
        "no bundled caption face found under "
        f"{BUNDLED_FONT_DIR}; the repository ships one so that every machine "
        "cuts the same lettering. Restore it, or pass --font explicitly and "
        "accept that the reel will not match the others."
    )
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


THIN_SPACE = "\u2009"


def tracked(text: str, spaces: int = 1) -> str:
    """Insert tracking between characters.

    Roman display lettering is set wide: inscriptional capitals are cut with a
    lot of air between them, and a title set solid reads like body copy blown
    up rather than like something carved. ffmpeg's drawtext has no
    letter-spacing control, so the spacing goes into the string itself.

    The separator is a thin space rather than a word space. A word space is
    about a quarter of an em, which pulls the letters so far apart that the
    words stop reading as words; a thin space is nearer a fifth and lands where
    a typographer would set display capitals.
    """
    return (THIN_SPACE * spaces).join(text)


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
    fade: float = CAPTION_FADE,
) -> str:
    """A centred, fading caption between two absolute timeline seconds.

    ``fade`` of zero makes the text land at full opacity on its first frame,
    which is what a card sitting at timeline zero needs: a fade-in there is a
    black frame zero, and frame zero is the thumbnail.
    """
    fade_out = f"min(1,({end:.3f}-t)/{CAPTION_FADE})"
    if fade <= 0.0:
        visible = fade_out
    else:
        fade_in = f"min(1,(t-{start:.3f})/{fade:.3f})"
        visible = f"min({fade_in},{fade_out})"
    alpha = f"if(between(t,{start:.3f},{end:.3f}),{visible},0)"
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


def punch_expression(punches: list[dict]) -> str:
    """A zoom that snaps in on a beat and settles back before the next one.

    Written as one expression of `t` so the whole reel still encodes in a single
    pass: each punch adds `amount * exp(-(t - at) / decay)` once its time has
    passed, and the crop is scaled back up to the frame afterwards.
    """
    terms = ["1"]
    for punch in punches:
        at = float(punch.get("at", 0.0))
        amount = max(0.0, min(PUNCH_MAX, float(punch.get("amount", 0.08))))
        decay = max(0.05, float(punch.get("decay", 0.25)))
        if amount <= 0.0:
            continue
        terms.append(
            f"{amount:.4f}*if(gte(t,{at:.3f}),exp(-(t-{at:.3f})/{decay:.3f}),0)"
        )
    return "+".join(terms)


def shot_filter(shot: dict, width: int, height: int) -> str:
    """Per-clip effects: hold the last frame, and punch in on the beat."""
    stages: list[str] = []
    freeze = float(shot.get("freeze", 0.0) or 0.0)
    if freeze > 0.0:
        stages.append(f"tpad=stop_mode=clone:stop_duration={freeze:.3f}")
    punches = shot.get("punch") or []
    if punches:
        zoom = punch_expression(punches)

        stages.append(
            f"crop=w='trunc(iw/({zoom})/2)*2':h='trunc(ih/({zoom})/2)*2'"
            ":x='(iw-ow)/2':y='(ih-oh)/2'"
        )
        stages.append(f"scale={width}:{height}:flags=bicubic")
        stages.append("setsar=1")
    return ",".join(stages)


def build_join_graph(
    lengths: list[float],
    joins: list[Join],
    spans: list[tuple[float, float]],
    effects: list[str] | None = None,
) -> tuple[list[str], str]:
    """Join the clips into one stream, blending where a transition asks for it.

    xfade has no zero-length form, so runs of hard cuts are concatenated into a
    single segment first and only the blended joins become xfades.

    Every input is stamped onto one timebase on the way in. ``concat`` hands its
    output back on the timebase of its first input while an untouched decoded
    stream keeps the demuxer's, and ``xfade`` refuses a pair that disagrees:
    mixing hard cuts and dissolves in the same reel produced ``First input link
    main timebase (1/15360) do not match ... (1/1000000)`` and wrote no file at
    all. Normalising up front costs nothing and cannot drift.
    """
    segments: list[list[int]] = [[0]]
    for index in range(1, len(lengths)):
        if joins[index].blended:
            segments.append([index])
        else:
            segments[-1].append(index)

    chain: list[str] = []
    for index in range(len(lengths)):
        extra = (effects[index] if effects and index < len(effects) else "") or ""
        prefix = f"{extra}," if extra else ""
        chain.append(f"[{index}:v]{prefix}settb=AVTB,setpts=PTS-STARTPTS[n{index}]")

    labels: list[str] = []
    for position, members in enumerate(segments):
        if len(members) == 1:
            labels.append(f"n{members[0]}")
            continue
        sources = "".join(f"[n{index}]" for index in members)
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
        "--no-sfx",
        action="store_true",
        help="drop the spec's sound-effect cues and score with music alone",
    )
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
    parser.add_argument(
        "--allow-flashes",
        action="store_true",
        help="publish even when the cut trips the photosensitivity check",
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
    staged = output.with_suffix(f".staging{output.suffix}")

    offences = camera_offences(spec)
    if offences:
        fail(
            "this spec's camera work is unwatchable on a phone:\n  "
            + "\n  ".join(offences)
        )

    default_kind, default_seconds = read_transition(
        spec.get("transition"), DEFAULT_TRANSITION, DEFAULT_TRANSITION_SECONDS
    )

    inputs: list[str] = []
    names: list[str] = []
    lengths: list[float] = []
    freezes: list[float] = []
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
        freeze = float(authored.get(name, {}).get("freeze", 0.0) or 0.0)
        names.append(name)
        lengths.append(length + freeze)
        freezes.append(freeze)
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

    effects = [shot_filter(authored.get(name, {}), width, height) for name in names]
    chain, joined = build_join_graph(lengths, joins, spans, effects)
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

        title_safe_width = int(width * 0.80)

        advisory = spec.get("advisory")
        advisory_end = 0.0
        if advisory:

            advisory_seconds = max(1.2, float(spec.get("advisory_seconds", 3.0)))
            advisory_end = advisory_seconds
            hold = advisory_seconds - 0.45
            advisory_size = max(24, int(type_base * 0.034))

            chain.append(
                f"[{stage}]"
                f"drawbox=x=0:y=0:w=iw:h=ih:color=black@1.0:t=fill"
                f":enable='lt(t,{advisory_seconds:.3f})'"
                f"[carded]"
            )
            stage = "carded"

            for line_index, line in enumerate(advisory.split("|")):
                offset = int(advisory_size * 1.45 * line_index)
                chain.append(
                    f"[{stage}]"
                    + drawtext(
                        text=tracked(line.strip()),
                        font=font,
                        size=fit_font_size(
                            tracked(line.strip()), font, advisory_size, safe_width
                        ),
                        y_expr=f"h*0.5-{int(advisory_size * 0.6)}+{offset}",
                        start=0.0,
                        end=hold,
                        color="white",
                        fade=0.0,
                    )
                    + f"[advised{line_index}]"
                )
                stage = f"advised{line_index}"

        card_seconds = float(spec.get("end_card_seconds", END_CARD_SECONDS))
        card_start = max(0.0, total - card_seconds)
        has_card = bool(spec.get("title") or spec.get("subtitle"))
        step = 0
        for index, (name, start, end) in enumerate(timeline):

            freeze = freezes[index] if index < len(freezes) else 0.0
            punchline = authored.get(name, {}).get("freeze_text")
            if punchline and freeze > 0.0:
                chain.append(
                    f"[{stage}]"
                    + drawtext(
                        text=tracked(punchline),
                        font=font,
                        size=fit_font_size(
                            tracked(punchline), font, title_size, title_safe_width
                        ),
                        y_expr=f"h*{FREEZE_TEXT_Y_FRACTION}",
                        start=max(start, end - freeze + 0.05),
                        end=end - 0.03,
                        fade=0.08,
                    )
                    + f"[punch{step}]"
                )
                stage = f"punch{step}"
                step += 1

            caption = captions.get(name)
            if not caption:
                continue

            leading = max(0.15, joins[index].seconds)
            trailing = 0.15
            if index + 1 < len(joins):
                trailing = max(trailing, joins[index + 1].seconds)
            visible_start = max(start + leading, advisory_end + 0.25)
            visible_end = max(visible_start + 0.6, end - trailing)

            if has_card and visible_start < card_start:
                visible_end = min(visible_end, card_start - CAPTION_FADE)
            chain.append(
                f"[{stage}]"
                + drawtext(
                    text=tracked(caption),
                    font=font,
                    size=fit_font_size(
                        tracked(caption), font, caption_size, safe_width
                    ),
                    y_expr=caption_y,
                    start=visible_start,
                    end=visible_end,
                )
                + f"[cap{step}]"
            )
            stage = f"cap{step}"
            step += 1

        for index, (name, start, end) in enumerate(timeline):
            act_title = authored.get(name, {}).get("act_title")
            if not act_title:
                continue
            leading = max(0.15, joins[index].seconds)
            trailing = 0.15
            if index + 1 < len(joins):
                trailing = max(trailing, joins[index + 1].seconds)

            visible_start = max(start + leading, advisory_end + 0.3)
            visible_end = max(visible_start + 0.6, end - trailing)
            if visible_end <= visible_start + 0.3:
                continue
            act_text = tracked(act_title)
            act_size = fit_font_size(act_text, font, title_size, title_safe_width)
            chain.append(
                f"[{stage}]"
                + drawtext(
                    text=act_text,
                    font=font,
                    size=act_size,
                    y_expr="(h-text_h)/2",
                    start=visible_start,
                    end=visible_end,
                )
                + f"[act{step}]"
            )
            stage = f"act{step}"
            step += 1
            kicker = authored.get(name, {}).get("act_kicker")
            if kicker:
                chain.append(
                    f"[{stage}]"
                    + drawtext(
                        text=tracked(kicker),
                        font=font,
                        size=fit_font_size(
                            tracked(kicker), font, subtitle_size, safe_width
                        ),
                        y_expr=f"(h-text_h)/2-{int(act_size * 0.95)}",
                        start=visible_start + 0.2,
                        end=visible_end,
                        color="#e6c98a",
                    )
                    + f"[act{step}]"
                )
                stage = f"act{step}"
                step += 1

        title = spec.get("title")
        if title:
            chain.append(
                f"[{stage}]"
                + drawtext(
                    text=tracked(title),
                    font=font,
                    size=fit_font_size(
                        tracked(title), font, title_size, title_safe_width
                    ),
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
                    text=tracked(subtitle),
                    font=font,
                    size=fit_font_size(
                        tracked(subtitle), font, subtitle_size, safe_width
                    ),
                    y_expr=f"h*{TITLE_Y_FRACTION}+{int(title_size * 1.35)}",
                    start=card_start + 0.35,
                    end=total,
                    color="#e6c98a",
                )
                + "[sub]"
            )
            stage = "sub"

        credit_lines = list(spec.get("end_card_lines", []))
        if credit_lines:

            credit_size = max(24, int(type_base * 0.032))
            credit_top = int(title_size * 1.35) + int(subtitle_size * 2.4)
            for line_index, line in enumerate(credit_lines):
                offset = credit_top + int(credit_size * 1.7 * line_index)
                chain.append(
                    f"[{stage}]"
                    + drawtext(
                        text=tracked(line),
                        font=font,
                        size=fit_font_size(
                            tracked(line), font, credit_size, safe_width
                        ),
                        y_expr=f"h*{TITLE_Y_FRACTION}+{offset}",
                        start=card_start + 0.8 + (0.25 * line_index),
                        end=total,
                        color="#cfc6b4",
                    )
                    + f"[credit{line_index}]"
                )
                stage = f"credit{line_index}"

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

    sfx_cues: list[dict] = [] if args.no_sfx else list(spec.get("sfx", []))
    bed_label = "abed" if sfx_cues and mode != "none" else "aout"

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
            f"afade=t=out:st={max(0.0, total - 1.8):.3f}:d=1.8[{bed_label}]"
        )
    elif mode == "drums":
        command += ["-f", "lavfi", "-i", build_drum_bed(total, args.tempo)]
        filter_complex += (
            f";[{audio_index}:a]afade=t=in:st=0:d=0.6,"
            f"afade=t=out:st={max(0.0, total - 1.2):.3f}:d=1.2,"
            f"alimiter=limit={PLATFORM_CEILING:.3f}:level=disabled[{bed_label}]"
        )

    if sfx_cues and mode != "none":
        filter_complex, sfx_inputs = build_sfx_layer(
            filter_complex, sfx_cues, len(shots) + 1, bed_label
        )
        command += sfx_inputs
        print(f"promo-edit: mixing {len(sfx_cues)} sound effect cue(s) over the score")

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
        str(staged),
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
        reject(staged, output, f"ffmpeg failed with status {result.returncode}")

    first = measure_first_frame(staged)
    if first is None:
        reject(
            staged,
            output,
            "the first frame could not be read back, so it cannot be shown to be "
            "anything but black.",
        )
    elif not first.readable:
        reject(
            staged,
            output,
            f"frame zero is black ({first.describe()}); social platforms use it as "
            "the thumbnail. An opening card must carry its text at full opacity on "
            "its first frame rather than fading up, and a shot opening the cut must "
            "already be lit. Check --opening-fade, the advisory card and the first "
            "shot's framing.",
        )

    fps = float(manifest.get("fps", 60))
    jumps, motion = frame_metrics(staged)
    offence = photosensitivity_offence(jumps, fps)
    if not jumps:
        print("promo-edit: warning: could not measure the cut for flashing")
    elif offence is not None and not args.allow_flashes:
        reject(
            staged,
            output,
            f"the cut flashes: {offence}. A full-frame flash is a seizure risk, so "
            "this is refused by default. Replace `flash` joins with `dip` (through "
            "black) or `dissolve`, lengthen the join, or pass --allow-flashes if the "
            "content genuinely needs it.",
        )
    elif offence is not None:
        print(f"promo-edit: warning: --allow-flashes set; {offence}")

    moving = motion_offence(motion)
    if moving is not None:
        reject(
            staged,
            output,
            f"the cut never holds still: {moving}. Hold shots longer, drop `shake`, "
            "and let the camera push or drift rather than orbit -- a frame the eye "
            "cannot settle on reads as chaos however good the footage is.",
        )

    staged.replace(output)
    worst = max(jumps) if jumps else 0.0
    print(
        f"promo-edit: wrote {output} (frame zero {first.describe()}, "
        f"worst luminance jump {worst:.0f}/255)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
