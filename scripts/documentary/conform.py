"""Conform the picture: shots from the captures, overlays, graphics, at delivery size.

Each shot becomes one lossless segment (x264 ``-qp 0``, 4:4:4) at the profile's
size and the edit's frame rate:

1. the clip is read from its ``in`` point; when it was captured at a whole
   multiple of the delivery rate (the arena's high-rate captures), ``shutter``
   (default 0.5, a 180-degree shutter) of the sub-frames are averaged for real
   motion blur before decimating;
2. it is scaled with Lanczos to fill the frame -- a 2x supersampled capture is
   downsampled here, which is where the supersampling pays off;
3. the shot's RGBA overlay layers are composited over it: pre-rendered clips
   (``.mov``/``.webm``/``.mkv`` with alpha) or folders of PNG frames, looked
   up in the overlays folder, scaled to the frame, with ``opacity`` and an
   ``offset`` into the shot. This is how the world-registered tactical
   overlays of #1532 arrive;
4. ``fade_in``/``fade_out`` and ``join: "dip"`` (half a dip either side of
   the cut) are applied.

Shot boundaries are quantised to whole frames on the absolute timeline, so a
long episode never drifts against its sound. Segments are cached by their
inputs: a new VO take re-renders only the shots whose length changed. The
segments are joined losslessly, and graphics are laid over the joined picture
in the final encode (``delivery.py``) from RGBA renders cached the same way.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from . import EditError, graphics, media
from .episode import Episode, resolve_path
from .timeline import Span, Timeline


@dataclass(frozen=True)
class Profile:
    name: str
    width: int
    height: int
    fps: float | None = None
    preset: str = "slow"


PROFILES = {
    "2160p": Profile("2160p", 3840, 2160),
    "1080p": Profile("1080p", 1920, 1080),
    "preview": Profile("preview", 960, 540, preset="veryfast"),
    "tiny": Profile("tiny", 320, 180, 12.0, "ultrafast"),
}
REEL_PROFILES = {
    "1080p": Profile("1080p", 1080, 1920),
    "preview": Profile("preview", 540, 960, preset="veryfast"),
    "tiny": Profile("tiny", 180, 320, 12.0, "ultrafast"),
}
LOSSLESS = [
    "-c:v",
    "libx264",
    "-preset",
    "ultrafast",
    "-qp",
    "0",
    "-pix_fmt",
    "yuv444p",
]


def profile(name: str, reel: bool = False) -> Profile:
    table = REEL_PROFILES if reel else PROFILES
    if name not in table:
        raise EditError(f"unknown profile '{name}'; one of {sorted(table)}")
    return table[name]


def overlay_source(episode_overlays: Path, layer: dict, base: Path) -> Path:
    path = Path(layer["file"])
    for candidate in (
        path if path.is_absolute() else episode_overlays / path,
        resolve_path(path, base),
    ):
        if candidate.exists():
            return candidate
    raise EditError(
        f"overlay layer not found: {layer['file']} (looked in {episode_overlays})"
    )


def _overlay_input(path: Path, fps: float, layer: dict) -> list[str]:
    if path.is_dir():
        return [
            "-framerate",
            f"{fps}",
            "-pattern_type",
            "glob",
            "-i",
            str(path / "*.png"),
        ]
    return ["-ss", f"{float(layer.get('in', 0.0)):.4f}", "-i", str(path)]


def render_segment(
    out_dir: Path,
    name: str,
    source: dict,
    frames: int,
    prof: Profile,
    fps: float,
    fade_in: float = 0.0,
    fade_out: float = 0.0,
    overlays: list[tuple[Path, dict]] | None = None,
) -> Path:
    """One lossless segment of ``frames`` frames from a clip, graphic or black."""
    overlays = overlays or []
    w, h = prof.width, prof.height
    key_parts = [
        name,
        source.get("kind"),
        frames,
        w,
        h,
        fps,
        fade_in,
        fade_out,
        source.get("in"),
        source.get("shutter"),
        source.get("grade"),
    ]
    if source.get("path"):
        key_parts.append(media.file_key(source["path"]))
    if source.get("graphic"):
        key_parts.append(source["graphic"])
    for path, layer in overlays:
        key_parts.append(
            (
                media.file_key(path) if path.is_file() else str(path),
                sorted(layer.items()),
            )
        )
    out = out_dir / f"{name}.{media.data_key(*key_parts)}.mkv"
    if out.exists():
        return out
    for stale in out_dir.glob(f"{name}.*.mkv"):
        stale.unlink()
    out_dir.mkdir(parents=True, exist_ok=True)
    seconds = frames / fps
    args: list[str] = []
    chain: list[str] = []
    kind = source["kind"]
    if kind == "clip":
        info = media.probe(source["path"])
        cap = info.get("fps", fps) or fps
        args += [
            "-ss",
            f"{float(source.get('in', 0.0)):.4f}",
            "-i",
            str(source["path"]),
        ]
        ratio = cap / fps
        if ratio >= 1.9 and abs(ratio - round(ratio)) < 0.02:
            taps = max(
                1, int(round(round(ratio) * float(source.get("shutter") or 0.5)))
            )
            if taps > 1:
                chain.append(f"tmix=frames={taps}")
        chain += [
            f"fps={fps}",
            f"scale={w}:{h}:force_original_aspect_ratio=increase:flags=lanczos",
            f"crop={w}:{h}",
            "setsar=1",
            f"tpad=stop_mode=clone:stop_duration={seconds + 1:.3f}",
        ]
        grade = source.get("grade")
        if grade:
            chain += [
                "format=yuv444p",
                f"eq=contrast={grade.get('contrast', 1.0)}:saturation={grade.get('saturation', 1.0)}:"
                f"brightness={grade.get('brightness', 0.0)}:gamma={grade.get('gamma', 1.0)}",
            ]
    elif kind == "graphic":
        args += ["-i", str(source["path"])]
        chain += [f"fps={fps}", f"tpad=stop_mode=clone:stop_duration={seconds + 1:.3f}"]
    else:
        args += [
            "-f",
            "lavfi",
            "-i",
            f"color=c=black:s={w}x{h}:r={fps}:d={seconds + 1:.3f}",
        ]
    graph = [f"[0:v]{','.join(chain) if chain else 'null'},format=yuv444p[base0]"]
    current = "base0"
    for index, (path, layer) in enumerate(overlays, start=1):
        args += _overlay_input(path, fps, layer)
        opacity = float(layer.get("opacity", 1.0))
        offset = float(layer.get("offset", 0.0))
        graph.append(
            f"[{index}:v]fps={fps},scale={w}:{h}:flags=lanczos,format=rgba,"
            f"colorchannelmixer=aa={opacity:.3f},setpts=PTS-STARTPTS+{offset:.4f}/TB[ov{index}]"
        )
        graph.append(
            f"[{current}][ov{index}]overlay=eof_action=pass:format=auto[base{index}]"
        )
        current = f"base{index}"
    tail = []
    if fade_in > 0:
        tail.append(f"fade=t=in:st=0:d={fade_in:.3f}")
    if fade_out > 0:
        tail.append(
            f"fade=t=out:st={max(0.0, seconds - fade_out):.4f}:d={fade_out:.3f}"
        )
    tail.append("format=yuv444p")
    graph.append(f"[{current}]{','.join(tail)}[vout]")
    media.ffmpeg(
        [
            *args,
            "-filter_complex",
            ";".join(graph),
            "-map",
            "[vout]",
            "-frames:v",
            str(frames),
            "-r",
            f"{fps}",
            *LOSSLESS,
            "-an",
            str(out),
        ],
        f"segment {name}",
    )
    return out


def concat(segments: list[Path], out: Path) -> Path:
    listing = out.with_suffix(".txt")
    listing.write_text("".join(f"file '{p.resolve()}'\n" for p in segments))
    media.ffmpeg(
        ["-f", "concat", "-safe", "0", "-i", str(listing), "-c", "copy", str(out)],
        "joining segments",
    )
    return out


def section_numbers(episode: Episode) -> dict[str, int]:
    numbers, n = {}, 0
    for section in episode.sections:
        if section.get("chapter", True) is False:
            continue
        n += 1
        numbers[section["id"]] = n
    return numbers


def graphic_clip(
    episode: Episode,
    kind: str,
    data: dict,
    dur: float,
    prof: Profile,
    fps: float,
    work: Path,
) -> Path:
    key = media.data_key(
        kind,
        data,
        round(dur, 4),
        prof.width,
        prof.height,
        fps,
        media.file_key(Path(graphics.__file__)),
    )
    out = work / "graphics" / f"{kind}.{key}.mov"
    if not out.exists():
        graphics.render_clip(kind, data, prof.width, prof.height, fps, dur, out)
    return out


def build_picture(
    episode: Episode, timeline: Timeline, prof: Profile, work: Path
) -> Path:
    """The joined, overlaid picture of the whole episode (graphics come later)."""
    fps = prof.fps or episode.fps
    seg_dir = work / f"segments_{prof.name}"
    numbers = section_numbers(episode)
    segments = []
    shots = timeline.shots
    for i, shot in enumerate(shots):
        frames = int(round(shot.end * fps)) - int(round(shot.start * fps))
        data = shot.data
        fade_in = float(data.get("fade_in", 0.0))
        fade_out = float(data.get("fade_out", 0.0))
        if data.get("join") == "dip":
            fade_in = max(fade_in, float(data.get("join_dur", 0.5)) / 2)
        if i + 1 < len(shots) and shots[i + 1].data.get("join") == "dip":
            fade_out = max(fade_out, float(shots[i + 1].data.get("join_dur", 0.5)) / 2)
        overlays = [
            (overlay_source(episode.overlays, layer, episode.path.parent), layer)
            for layer in data.get("overlays", [])
        ]
        if data.get("clip"):
            source = {
                "kind": "clip",
                "path": episode.find_clip(data["clip"]),
                "in": shot.in_point,
                "shutter": data.get("shutter"),
                "grade": data.get("grade"),
            }
        elif data.get("graphic"):
            kind = data["graphic"]
            gdata = graphics.graphic_data(
                episode, {"type": kind, **data.get("graphic_data", {})}, numbers
            )
            path = graphic_clip(episode, kind, gdata, frames / fps, prof, fps, work)
            source = {"kind": "graphic", "path": path, "graphic": kind}
        else:
            source = {"kind": "black"}
        segments.append(
            render_segment(
                seg_dir, shot.id, source, frames, prof, fps, fade_in, fade_out, overlays
            )
        )
    return concat(segments, work / f"{episode.id}_{prof.name}_picture.mkv")


def graphics_layers(
    episode: Episode, timeline: Timeline, prof: Profile, work: Path
) -> list[tuple[Path, float, Span]]:
    """Every graphics event rendered as RGBA at the profile size: ``(clip, start, span)``."""
    fps = prof.fps or episode.fps
    numbers = section_numbers(episode)
    layers = []
    for span in timeline.graphics:
        data = graphics.graphic_data(episode, span.data, numbers)
        start = int(round(span.start * fps)) / fps
        dur = max(1, int(round(span.end * fps)) - int(round(span.start * fps))) / fps
        layers.append(
            (
                graphic_clip(episode, span.data["type"], data, dur, prof, fps, work),
                start,
                span,
            )
        )
    return layers


def overlay_graph(
    layers: list[tuple[Path, float]], first_input: int, base: str = "0:v"
) -> tuple[list[str], list[str], str]:
    """ffmpeg inputs and filters laying RGBA layers over ``base`` at their start times."""
    inputs: list[str] = []
    filters: list[str] = []
    current = base
    for n, (path, start) in enumerate(layers):
        index = first_input + n
        inputs += ["-i", str(path)]
        filters.append(
            f"[{index}:v]format=rgba,setpts=PTS-STARTPTS+{start:.4f}/TB[g{n}]"
        )
        filters.append(f"[{current}][g{n}]overlay=eof_action=pass:format=auto[o{n}]")
        current = f"o{n}"
    return inputs, filters, current
