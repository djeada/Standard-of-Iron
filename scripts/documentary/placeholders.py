"""Stand-in captures so an edit builds before anything is filmed.

For every clip the edit names (episode shots and reel parts) this writes an
arena-shaped capture: ``<clips>/<capture>/NN_<shot>.mp4`` with a moving test
pattern, the shot name burned in, a quiet tone as its game audio, at twice the
delivery frame rate (so the motion-blur path is exercised) and, by default,
2x the tiny profile's size (the supersampled-capture path). Each capture
folder gets a ``shots.json`` and a ``timeline.json`` like the arena writes.
Every overlay layer named by a shot becomes an RGBA ``qtrle`` clip with a
moving arrow, standing in for #1532's tactical overlays.

Placeholders are for testing the chain and for animatics; never deliver them.
"""

from __future__ import annotations

import json
import math
import subprocess
from pathlib import Path

from . import EditError, media, style
from .episode import Episode


def needed(episode: Episode, timeline=None) -> dict[str, tuple[bool, float]]:
    """``{clip ref: (vertical, seconds needed)}`` across the episode and its reel."""
    out: dict[str, tuple[bool, float]] = {}
    spans = {s.id: s for s in timeline.shots} if timeline else {}
    for shot in episode.shots():
        ref = shot.get("clip")
        if not ref:
            continue
        span = spans.get(shot["id"])
        length = (span.dur if span else 30.0) + float(
            shot.get("in", 0.0) if isinstance(shot.get("in"), (int, float)) else 0.0
        )
        prev = out.get(ref, (False, 0.0))[1]
        out[ref] = (False, max(prev, length + 2.0))
    for part in episode.data.get("reel", {}).get("parts", []):
        ref = part.get("clip")
        if ref:
            prev = out.get(ref, (True, 0.0))[1]
            out[ref] = (True, max(prev, float(part.get("in", 0.0)) + 15.0))
    return out


def make_clip(
    path: Path, label: str, size: tuple[int, int], fps: float, seconds: float, seed: int
) -> None:
    w, h = size
    font = str(style.FONTS["display"]).replace(":", r"\:")
    hue = (seed * 47) % 360
    media.ffmpeg(
        [
            "-f",
            "lavfi",
            "-i",
            f"testsrc2=s={w}x{h}:r={fps}:d={seconds:.2f}",
            "-f",
            "lavfi",
            "-i",
            f"sine=f={140 + seed % 9 * 20}:r=48000:d={seconds:.2f}",
            "-vf",
            f"hue=h={hue}:s=0.55,eq=brightness=0.04,drawtext=fontfile='{font}':text='{label.upper()}':"
            f"fontsize={max(10, min(w, h) // 12)}:fontcolor=white:borderw=2:bordercolor=black:"
            "x=(w-text_w)/2:y=h*0.80",
            "-af",
            "volume=0.08",
            "-c:v",
            "libx264",
            "-preset",
            "ultrafast",
            "-crf",
            "30",
            "-pix_fmt",
            "yuv420p",
            "-c:a",
            "aac",
            "-b:a",
            "96k",
            "-shortest",
            str(path),
        ],
        f"placeholder {label}",
    )


def make_overlay(
    path: Path, size: tuple[int, int], fps: float, seconds: float, color
) -> None:
    """An RGBA arrow sweeping across the frame (stand-in for a tactical overlay)."""
    from PIL import Image, ImageDraw

    w, h = size
    frames = int(seconds * fps)
    proc = subprocess.Popen(
        [
            "ffmpeg",
            "-hide_banner",
            "-v",
            "error",
            "-y",
            "-f",
            "rawvideo",
            "-pix_fmt",
            "rgba",
            "-s",
            f"{w}x{h}",
            "-r",
            f"{fps}",
            "-i",
            "-",
            "-c:v",
            "qtrle",
            "-pix_fmt",
            "argb",
            str(path),
        ],
        stdin=subprocess.PIPE,
    )
    stroke = max(2, int(style.STROKE["arrow"] * style.unit(w, h)))
    for n in range(frames):
        t = n / fps
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        grow = min(1.0, t / max(1.0, seconds * 0.6))
        x0, y0 = w * 0.2, h * 0.65
        x1 = x0 + w * 0.55 * grow
        y1 = y0 - h * 0.25 * math.sin(grow * math.pi * 0.5)
        d.line([(x0, y0), (x1, y1)], fill=tuple(color) + (225,), width=stroke)
        d.polygon(
            [
                (x1 + stroke * 2.5, y1),
                (x1 - stroke, y1 - stroke * 2),
                (x1 - stroke, y1 + stroke * 2),
            ],
            fill=tuple(color) + (225,),
        )
        proc.stdin.write(img.tobytes())
    proc.stdin.close()
    if proc.wait() != 0:
        raise EditError(f"overlay placeholder {path.name} failed")


def generate(
    episode: Episode,
    clips: Path,
    overlays: Path,
    vertical: Path | None = None,
    size: tuple[int, int] = (640, 360),
    fps: float | None = None,
    timeline=None,
) -> list[Path]:
    fps = fps or episode.fps * 2
    vertical = vertical or clips
    written = []
    captures: dict[Path, list[dict]] = {}
    for seed, (ref, (is_vertical, seconds)) in enumerate(
        sorted(needed(episode, timeline).items())
    ):
        root = vertical if is_vertical else clips
        folder, _, name = ref.partition("/")
        target_dir = root / folder
        target_dir.mkdir(parents=True, exist_ok=True)
        index = len(captures.setdefault(target_dir, []))
        path = target_dir / f"{index:02d}_{name}.mp4"
        clip_size = (size[1], size[0]) if is_vertical else size
        make_clip(path, name.replace("_", " "), clip_size, fps, seconds, seed)
        captures[target_dir].append(
            {
                "name": name,
                "clip": path.name,
                "clip_seconds": seconds,
                "scene_start": 0.0,
            }
        )
        written.append(path)
    for folder, shots in captures.items():
        (folder / "shots.json").write_text(
            json.dumps(
                {
                    "id": folder.name,
                    "title": folder.name,
                    "width": size[0],
                    "height": size[1],
                    "fps": fps,
                    "placeholder": True,
                    "shots": shots,
                },
                indent=1,
            )
        )
        (folder / "timeline.json").write_text(
            json.dumps(
                {
                    "id": folder.name,
                    "matches": [
                        {
                            "scenario": folder.name,
                            "decided": True,
                            "events": [
                                {"event": "first_contact", "at": 4.0},
                                {"event": "decision", "at": 30.0},
                            ],
                        }
                    ],
                },
                indent=1,
            )
        )
    overlays.mkdir(parents=True, exist_ok=True)
    spans = {s.id: s for s in timeline.shots} if timeline else {}
    for shot in episode.shots():
        for layer in shot.get("overlays", []):
            path = overlays / layer["file"]
            if path.suffix == "":
                continue
            seconds = (spans[shot["id"]].dur if shot["id"] in spans else 20.0) + 1.0
            side = "carthage" if "rome" not in layer["file"] else "rome"
            make_overlay(path, size, episode.fps, seconds, style.side_color(side))
            written.append(path)
    return written
