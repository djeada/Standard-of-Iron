#!/usr/bin/env python3
"""Tactical overlays registered to the battlefield.

Arena promo capture writes, beside every clip, the camera that rendered each
frame (``NN_<shot>.camera.jsonl``) and the battlefield's height field
(``terrain_<scenario>_<seed>.json`` + ``.f32``). This module reads both and
draws documentary map graphics -- movement arrows, army blocks, frontage lines
and labels -- *in world space*: every element is described in arena metres,
draped over the terrain, and projected through the exact view and projection
matrices of each frame. Because the matrices are the ones the renderer used, an
arrow painted on a hillside stays on that hillside while the camera cranes,
orbits or dollies.

Usage::

    scripts/tactical_overlay.py \\
        --clip artifacts/promo/cannae/03_phase_one.mp4 \\
        --overlay tools/arena/promos/overlays/phase_one.json \\
        --out artifacts/promo/cannae/03_phase_one.tactical.mp4

``--alpha-out x.mov`` also (or instead) writes the overlay alone as ProRes 4444
with alpha, which is what the conform stage composites after its grade.
``--style series.json`` injects the series graphics package (#1534); every key
it sets overrides :data:`DEFAULT_STYLE`. See docs/PROMO_CAPTURE.md "Tactical
overlays" for the description format.

Only Pillow is required (no numpy): shapes are filled into supersampled
coverage masks and box-filtered down, which is what makes their edges
anti-aliased.
"""

from __future__ import annotations

import argparse
import array
import copy
import json
import math
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable, Sequence

REPO = Path(__file__).resolve().parents[1]
FONT_DIR = REPO / "assets" / "fonts"

Vec2 = tuple[float, float]
Vec3 = tuple[float, float, float]

# ---------------------------------------------------------------------------
# Style
# ---------------------------------------------------------------------------

#: Defaults for every element kind. Pixel sizes are authored for a 1080-pixel
#: short side and scale with the frame; world sizes are metres. The series
#: graphics package replaces this through ``--style`` / ``style=`` without
#: touching any overlay description.
DEFAULT_STYLE: dict[str, Any] = {
    "font": str(FONT_DIR / "StandardIronDisplay-Bold.ttf"),
    "font_fallback": str(FONT_DIR / "EBGaramond12-Bold.ttf"),
    "supersample": 3,
    "fade_seconds": 0.35,
    "palette": {
        "rome": "#c0392b",
        "carthage": "#e0a83a",
        "gaul": "#3f8f5a",
        "iberia": "#8e5bb5",
        "neutral": "#f3ecdc",
        "ink": "#15110b",
    },
    "owners": {"1": "rome", "2": "carthage", "3": "gaul", "4": "iberia"},
    "arrow": {
        "color": None,
        "opacity": 0.9,
        "width_m": 3.0,
        "taper": 0.55,
        "head": "triangle",
        "head_length_m": 7.0,
        "head_width_m": 8.0,
        "outline": "ink",
        "outline_px": 2.5,
        "outline_opacity": 0.55,
        "lift_m": 0.12,
        "draw_seconds": 1.2,
        "ease": "smooth",
        "step_m": 0.75,
    },
    "block": {
        "color": None,
        "fill_opacity": 0.32,
        "outline_px": 3.0,
        "outline_opacity": 0.95,
        "front_px": 7.0,
        "padding_m": 3.0,
        "lift_m": 0.1,
        "cell_m": 2.0,
    },
    "frontage": {
        "color": None,
        "opacity": 0.95,
        "width_px": 6.0,
        "outline": "ink",
        "outline_px": 2.0,
        "outline_opacity": 0.5,
        "extend_m": 2.0,
        "lift_m": 0.15,
        "draw_seconds": 0.6,
        "ease": "smooth",
        "step_m": 1.0,
    },
    "label": {
        "color": "neutral",
        "opacity": 1.0,
        "size_px": 34.0,
        "tracking": 0.06,
        "uppercase": True,
        "shadow": "ink",
        "shadow_px": 3.0,
        "shadow_opacity": 0.7,
        "offset_px": [0.0, -40.0],
        "height_m": 0.0,
    },
    "classes": {},
}

KINDS = ("arrow", "block", "frontage", "label")


def deep_merge(base: dict, *overrides: dict | None) -> dict:
    """Merge dictionaries recursively; later dictionaries win."""
    result = copy.deepcopy(base)
    for override in overrides:
        if not override:
            continue
        for key, value in override.items():
            if isinstance(value, dict) and isinstance(result.get(key), dict):
                result[key] = deep_merge(result[key], value)
            else:
                result[key] = copy.deepcopy(value)
    return result


def resolve_style(*overrides: dict | None) -> dict:
    """The default style with each override (series package, overlay) applied."""
    return deep_merge(DEFAULT_STYLE, *overrides)


def element_style(style: dict, element: dict) -> dict:
    """An element's resolved style: kind defaults, then classes, then inline."""
    kind = element["type"]
    resolved = copy.deepcopy(style.get(kind, {}))
    classes = element.get("class") or []
    if isinstance(classes, str):
        classes = [classes]
    for name in classes:
        if name not in style.get("classes", {}):
            raise ValueError(f"unknown style class '{name}'")
        resolved = deep_merge(resolved, style["classes"][name])
    resolved = deep_merge(resolved, element.get("style"))
    return resolved


def parse_color(value: Any, style: dict) -> tuple[int, int, int]:
    """A colour from a palette name, ``#rrggbb`` or ``[r, g, b]`` (0-255)."""
    palette = style.get("palette", {})
    seen = 0
    while isinstance(value, str) and value in palette and seen < 8:
        value = palette[value]
        seen += 1
    if isinstance(value, str):
        text = value.strip().lstrip("#")
        if len(text) == 6:
            return (int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16))
        raise ValueError(f"unknown colour '{value}'")
    if isinstance(value, (list, tuple)) and len(value) >= 3:
        return (int(value[0]), int(value[1]), int(value[2]))
    raise ValueError(f"unknown colour {value!r}")


def owner_color(style: dict, owner: int | None) -> Any:
    if owner is None:
        return "neutral"
    return style.get("owners", {}).get(str(owner), "neutral")


# ---------------------------------------------------------------------------
# Camera track and projection
# ---------------------------------------------------------------------------


def column_major(values: Sequence[float]) -> list[list[float]]:
    """A 4x4 row-indexed matrix from 16 column-major values (Qt/OpenGL)."""
    if len(values) != 16:
        raise ValueError("a matrix needs 16 values")
    return [[float(values[column * 4 + row]) for column in range(4)] for row in range(4)]


def mat_mul(a: list[list[float]], b: list[list[float]]) -> list[list[float]]:
    return [
        [sum(a[row][k] * b[k][column] for k in range(4)) for column in range(4)]
        for row in range(4)
    ]


def look_at(eye: Vec3, target: Vec3, up: Vec3 = (0.0, 1.0, 0.0)) -> list[list[float]]:
    """``QMatrix4x4::lookAt`` -- for tests and synthetic cameras."""
    f = _normalize(_sub(target, eye))
    s = _normalize(_cross(f, up))
    u = _cross(s, f)
    return [
        [s[0], s[1], s[2], -_dot(s, eye)],
        [u[0], u[1], u[2], -_dot(u, eye)],
        [-f[0], -f[1], -f[2], _dot(f, eye)],
        [0.0, 0.0, 0.0, 1.0],
    ]


def perspective(fov_y: float, aspect: float, near: float, far: float) -> list[list[float]]:
    """``QMatrix4x4::perspective`` -- for tests and synthetic cameras."""
    cotan = 1.0 / math.tan(math.radians(fov_y) / 2.0)
    clip = near - far
    return [
        [cotan / aspect, 0.0, 0.0, 0.0],
        [0.0, cotan, 0.0, 0.0],
        [0.0, 0.0, (near + far) / clip, (2.0 * near * far) / clip],
        [0.0, 0.0, -1.0, 0.0],
    ]


def flatten_column_major(matrix: list[list[float]]) -> list[float]:
    return [matrix[row][column] for column in range(4) for row in range(4)]


@dataclass
class Camera:
    """One frame's camera: ``view_projection`` maps world points to clip space."""

    view_projection: list[list[float]]
    width: int
    height: int

    @classmethod
    def from_frame(cls, frame: dict, width: int, height: int) -> "Camera | None":
        if "view" not in frame or "projection" not in frame:
            return None
        view = column_major(frame["view"])
        projection = column_major(frame["projection"])
        return cls(mat_mul(projection, view), width, height)

    def clip(self, point: Vec3) -> tuple[float, float, float, float]:
        m = self.view_projection
        x, y, z = point
        return (
            m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3],
            m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3],
            m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3],
            m[3][0] * x + m[3][1] * y + m[3][2] * z + m[3][3],
        )

    def to_pixels(self, clip: tuple[float, float, float, float]) -> Vec2:
        x, y, _, w = clip
        return (
            (x / w * 0.5 + 0.5) * self.width,
            (1.0 - (y / w * 0.5 + 0.5)) * self.height,
        )

    def project(self, point: Vec3) -> Vec2 | None:
        """Pixel position (origin top-left) or None behind the near plane."""
        clip = self.clip(point)
        if clip[2] < -clip[3] or clip[3] <= 1e-9:
            return None
        return self.to_pixels(clip)

    def project_polygon(self, points: Sequence[Vec3]) -> list[Vec2]:
        """Clip a world polygon against the near plane and project it."""
        clipped = clip_near([self.clip(p) for p in points], closed=True)
        return [self.to_pixels(c) for c in clipped]

    def project_polyline(self, points: Sequence[Vec3]) -> list[list[Vec2]]:
        """Project a world polyline, split into runs that stay before the lens."""
        runs: list[list[Vec2]] = []
        current: list[tuple[float, float, float, float]] = []
        clips = [self.clip(p) for p in points]
        for index, c in enumerate(clips):
            inside = c[2] + c[3] >= 0.0 and c[3] > 1e-9
            if index > 0:
                prev = clips[index - 1]
                prev_inside = prev[2] + prev[3] >= 0.0 and prev[3] > 1e-9
                if prev_inside != inside:
                    current.append(_near_intersection(prev, c))
                    if not inside:
                        runs.append(current)
                        current = []
            if inside:
                current.append(c)
        if current:
            runs.append(current)
        return [[self.to_pixels(c) for c in run] for run in runs if len(run) >= 2]


def _near_distance(c: tuple[float, float, float, float]) -> float:
    return c[2] + c[3]


def _near_intersection(a, b):
    da = _near_distance(a)
    db = _near_distance(b)
    t = da / (da - db) if da != db else 0.0
    point = tuple(a[i] + (b[i] - a[i]) * t for i in range(4))
    if point[3] <= 1e-9:
        point = (point[0], point[1], -1e-6, 1e-6)
    return point


def clip_near(clips, closed: bool = True):
    """Sutherland-Hodgman against the OpenGL near plane (z >= -w)."""
    if not clips:
        return []
    output = []
    count = len(clips)
    for index in range(count if closed else count - 1):
        current = clips[index]
        following = clips[(index + 1) % count]
        current_in = _near_distance(current) >= 0.0 and current[3] > 1e-9
        following_in = _near_distance(following) >= 0.0 and following[3] > 1e-9
        if current_in:
            output.append(current)
        if current_in != following_in:
            output.append(_near_intersection(current, following))
    return output


class CameraTrack:
    """A clip's ``.camera.jsonl``: a header line, then one line per frame."""

    def __init__(self, header: dict, frames: list[dict]):
        self.header = header
        self.frames = frames
        self.fps = float(header.get("fps", 30))
        self.width = int(header.get("width", 1920))
        self.height = int(header.get("height", 1080))
        self._unit_index: dict[int, dict] = {}

    @classmethod
    def load(cls, path: Path) -> "CameraTrack":
        lines = [line for line in Path(path).read_text().splitlines() if line.strip()]
        if not lines:
            raise ValueError(f"{path} is empty")
        header = json.loads(lines[0])
        if header.get("type") != "soi_camera_track":
            raise ValueError(f"{path} is not a camera track")
        frames = [json.loads(line) for line in lines[1:]]
        return cls(header, frames)

    def camera(self, index: int, width: int | None = None, height: int | None = None):
        frame = self.frames[index]
        return Camera.from_frame(frame, width or self.width, height or self.height)

    def frame_at(self, seconds: float) -> int:
        index = int(round(seconds * self.fps))
        return max(0, min(len(self.frames) - 1, index))

    def group(self, index: int, name: str) -> dict | None:
        groups = self.frames[index].get("groups") or {}
        group = groups.get(name)
        if not group or not group.get("alive"):
            return None
        return group

    def nearest_group(self, index: int, name: str) -> dict | None:
        """The group at a frame, or at the nearest frame that carried it."""
        for distance in range(len(self.frames)):
            for candidate in (index - distance, index + distance):
                if 0 <= candidate < len(self.frames):
                    group = self.group(candidate, name)
                    if group is not None:
                        return group
        return None


# ---------------------------------------------------------------------------
# Terrain
# ---------------------------------------------------------------------------


class Terrain:
    """The exported height field, sampled bilinearly (metres)."""

    def __init__(
        self,
        heights: Sequence[float] | None,
        origin: Vec2 = (0.0, 0.0),
        spacing: float = 1.0,
        columns: int = 0,
        rows: int = 0,
    ):
        self.heights = heights
        self.origin = origin
        self.spacing = spacing
        self.columns = columns
        self.rows = rows

    @classmethod
    def flat(cls) -> "Terrain":
        return cls(None)

    @classmethod
    def load(cls, path: Path) -> "Terrain":
        path = Path(path)
        header = json.loads(path.read_text())
        data = array.array("f")
        with open(path.parent / header["data"], "rb") as handle:
            data.frombytes(handle.read())
        if sys.byteorder != "little":
            data.byteswap()
        columns = int(header["columns"])
        rows = int(header["rows"])
        if len(data) != columns * rows:
            raise ValueError(f"{path}: expected {columns * rows} heights, got {len(data)}")
        origin = (float(header["origin"][0]), float(header["origin"][1]))
        return cls(data, origin, float(header["spacing"]), columns, rows)

    def height(self, x: float, z: float) -> float:
        if self.heights is None:
            return 0.0
        fx = (x - self.origin[0]) / self.spacing
        fz = (z - self.origin[1]) / self.spacing
        fx = min(max(fx, 0.0), self.columns - 1.000001)
        fz = min(max(fz, 0.0), self.rows - 1.000001)
        ix = int(fx)
        iz = int(fz)
        tx = fx - ix
        tz = fz - iz
        row = iz * self.columns
        h00 = self.heights[row + ix]
        h10 = self.heights[row + ix + 1]
        h01 = self.heights[row + self.columns + ix]
        h11 = self.heights[row + self.columns + ix + 1]
        return (h00 * (1 - tx) + h10 * tx) * (1 - tz) + (h01 * (1 - tx) + h11 * tx) * tz

    def drape(self, x: float, z: float, lift: float = 0.0) -> Vec3:
        return (x, self.height(x, z) + lift, z)


# ---------------------------------------------------------------------------
# Geometry
# ---------------------------------------------------------------------------


def _sub(a, b):
    return tuple(a[i] - b[i] for i in range(len(a)))


def _dot(a, b):
    return sum(a[i] * b[i] for i in range(len(a)))


def _cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def _normalize(v):
    length = math.sqrt(_dot(v, v))
    if length < 1e-12:
        return v
    return tuple(c / length for c in v)


def ease(name: str, t: float) -> float:
    t = min(max(t, 0.0), 1.0)
    if name == "linear":
        return t
    if name == "in":
        return t * t
    if name == "out":
        return 1.0 - (1.0 - t) * (1.0 - t)
    return t * t * (3.0 - 2.0 * t)


def quadratic_bezier(a: Vec2, control: Vec2, b: Vec2, segments: int) -> list[Vec2]:
    points = []
    for index in range(segments + 1):
        t = index / segments
        u = 1.0 - t
        points.append(
            (
                u * u * a[0] + 2 * u * t * control[0] + t * t * b[0],
                u * u * a[1] + 2 * u * t * control[1] + t * t * b[1],
            )
        )
    return points


def catmull_rom(points: Sequence[Vec2], segments: int) -> list[Vec2]:
    """A centripetal-free (uniform) Catmull-Rom curve through every point."""
    if len(points) < 3:
        return list(points)
    padded = [points[0], *points, points[-1]]
    result: list[Vec2] = [points[0]]
    for i in range(1, len(padded) - 2):
        p0, p1, p2, p3 = padded[i - 1], padded[i], padded[i + 1], padded[i + 2]
        for step in range(1, segments + 1):
            t = step / segments
            t2 = t * t
            t3 = t2 * t
            result.append(
                tuple(
                    0.5
                    * (
                        2 * p1[k]
                        + (-p0[k] + p2[k]) * t
                        + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2
                        + (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3
                    )
                    for k in range(2)
                )
            )
    return result


def arrow_path(points: Sequence[Vec2], bend: float = 0.0, smooth: bool = True) -> list[Vec2]:
    """The centreline of an arrow in the ground plane (x, z).

    Two points with ``bend`` make a quadratic curve whose apex sits ``bend``
    chord-lengths to the left of the direction of travel (negative bends right);
    three or more points are joined by a Catmull-Rom spline (``smooth``) or
    straight segments.
    """
    if len(points) < 2:
        raise ValueError("an arrow needs at least two points")
    if len(points) == 2:
        a, b = points
        if abs(bend) < 1e-9:
            return [tuple(a), tuple(b)]
        chord = (b[0] - a[0], b[1] - a[1])
        left = (chord[1], -chord[0])
        middle = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
        # A quadratic's apex sits halfway to its control point.
        control = (middle[0] + left[0] * bend * 2, middle[1] + left[1] * bend * 2)
        return quadratic_bezier(tuple(a), control, tuple(b), 48)
    if smooth:
        return catmull_rom([tuple(p) for p in points], 16)
    return [tuple(p) for p in points]


def polyline_length(points: Sequence[Vec2]) -> float:
    return sum(math.dist(points[i], points[i + 1]) for i in range(len(points) - 1))


def truncate(points: Sequence[Vec2], length: float) -> list[Vec2]:
    """The first ``length`` metres of a polyline."""
    if length <= 0.0 or not points:
        return [points[0]] if points else []
    result = [points[0]]
    travelled = 0.0
    for index in range(len(points) - 1):
        a, b = points[index], points[index + 1]
        segment = math.dist(a, b)
        if travelled + segment >= length:
            t = (length - travelled) / segment if segment > 0 else 0.0
            result.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
            return result
        result.append(b)
        travelled += segment
    return result


def resample(points: Sequence[Vec2], step: float) -> list[Vec2]:
    """Points every ``step`` metres along a polyline, keeping both ends."""
    total = polyline_length(points)
    if total <= 1e-9:
        return [points[0], points[-1]]
    count = max(1, int(math.ceil(total / step)))
    result = []
    for index in range(count + 1):
        target = total * index / count
        result.append(truncate(points, target)[-1])
    return result


def tangents(points: Sequence[Vec2]) -> list[Vec2]:
    result = []
    for index in range(len(points)):
        a = points[max(0, index - 1)]
        b = points[min(len(points) - 1, index + 1)]
        d = (b[0] - a[0], b[1] - a[1])
        length = math.hypot(*d) or 1.0
        result.append((d[0] / length, d[1] / length))
    return result


def left_of(tangent: Vec2) -> Vec2:
    """The left-hand normal in the ground plane, as a viewer above sees it.

    The world is +y up and x/z span the ground; facing +x, +z is to the right
    (``right = forward x up``), so left is ``(t.z, -t.x)``.
    """
    return (tangent[1], -tangent[0])


@dataclass
class ArrowShape:
    """An arrow's ground-plane outline as ribbon cross-sections plus head."""

    sections: list[tuple[Vec2, Vec2]]  # (left, right) pairs along the shaft
    head: list[tuple[Vec2, Vec2]]  # (left, right) pairs from base to tip
    tip: Vec2


def arrow_shape(
    path: Sequence[Vec2],
    progress: float,
    width: float,
    head_length: float,
    head_width: float,
    taper: float = 1.0,
    step: float = 0.75,
    head: str = "triangle",
) -> ArrowShape | None:
    """Build the drawn part of an arrow at ``progress`` (0..1) of its draw-on."""
    total = polyline_length(path)
    visible = total * min(max(progress, 0.0), 1.0)
    if visible <= 1e-3:
        return None
    drawn = truncate(path, visible)
    head_length = 0.0 if head == "none" else head_length
    scale = 1.0
    if head_length > 0 and visible < head_length * 1.6:
        scale = visible / (head_length * 1.6)
    head_length *= scale
    head_width *= scale
    shaft_length = max(0.0, visible - head_length)
    shaft = resample(truncate(drawn, shaft_length), step) if shaft_length > 1e-3 else [drawn[0]]
    if len(shaft) < 2:
        shaft = [drawn[0], drawn[0]]
    tip = drawn[-1]
    directions = tangents(shaft)
    end_direction = tangents(drawn)[-1]
    # Taper by distance along the whole path, so drawing on never fattens it.
    sections = []
    travelled = 0.0
    for index, (point, direction) in enumerate(zip(shaft, directions)):
        if index > 0:
            travelled += math.dist(shaft[index - 1], point)
        if index == len(shaft) - 1:
            direction = end_direction
        reached = min(1.0, travelled / total) if total > 0 else 1.0
        half = 0.5 * width * (taper + (1.0 - taper) * reached)
        normal = left_of(direction)
        sections.append(
            (
                (point[0] + normal[0] * half, point[1] + normal[1] * half),
                (point[0] - normal[0] * half, point[1] - normal[1] * half),
            )
        )
    head_sections = []
    if head_length > 0:
        base = shaft[-1]
        normal = left_of(end_direction)
        rows = max(2, int(math.ceil(head_length / step)) + 1)
        for row in range(rows):
            t = row / (rows - 1)
            centre = (base[0] + (tip[0] - base[0]) * t, base[1] + (tip[1] - base[1]) * t)
            half = 0.5 * head_width * (1.0 - t)
            head_sections.append(
                (
                    (centre[0] + normal[0] * half, centre[1] + normal[1] * half),
                    (centre[0] - normal[0] * half, centre[1] - normal[1] * half),
                )
            )
    return ArrowShape(sections, head_sections, tip)


# ---------------------------------------------------------------------------
# Overlay description
# ---------------------------------------------------------------------------


@dataclass
class Element:
    data: dict
    style: dict
    start: float
    end: float
    fade_in: float
    fade_out: float

    @property
    def kind(self) -> str:
        return self.data["type"]

    def opacity(self, t: float) -> float:
        if t < self.start or t > self.end:
            return 0.0
        alpha = 1.0
        if self.fade_in > 0:
            alpha = min(alpha, (t - self.start) / self.fade_in)
        if self.fade_out > 0 and math.isfinite(self.end):
            alpha = min(alpha, (self.end - t) / self.fade_out)
        return max(0.0, min(1.0, alpha))


@dataclass
class Overlay:
    elements: list[Element]
    style: dict
    description: dict = field(default_factory=dict)

    @classmethod
    def parse(cls, description: dict, style: dict | None = None) -> "Overlay":
        """Read an overlay description; ``style`` is the injected series style.

        Precedence, lowest first: :data:`DEFAULT_STYLE`, the injected style,
        the description's own ``style`` block, an element's ``class`` entries,
        then its inline ``style``.
        """
        resolved = resolve_style(style, description.get("style"))
        elements = []
        for index, raw in enumerate(description.get("elements", [])):
            kind = raw.get("type")
            if kind not in KINDS:
                raise ValueError(f"element {index}: unknown type {kind!r} (one of {KINDS})")
            element_styles = element_style(resolved, raw)
            fade = float(resolved.get("fade_seconds", 0.35))
            start = float(raw.get("start", 0.0))
            end = float(raw.get("end", math.inf))
            if end <= start:
                raise ValueError(f"element {index}: end {end} is not after start {start}")
            _validate(kind, raw, index)
            elements.append(
                Element(
                    raw,
                    element_styles,
                    start,
                    end,
                    float(raw.get("fade_in", fade)),
                    float(raw.get("fade_out", fade)),
                )
            )
        return cls(elements, resolved, description)


def _validate(kind: str, raw: dict, index: int) -> None:
    if kind == "arrow" and len(raw.get("points", [])) < 2:
        raise ValueError(f"element {index}: an arrow needs two or more points")
    if kind in ("block", "frontage") and not raw.get("group"):
        raise ValueError(f"element {index}: a {kind} follows a live 'group'")
    if kind == "label" and "text" not in raw:
        raise ValueError(f"element {index}: a label needs 'text'")
    if kind == "label" and "at" not in raw:
        raise ValueError(f"element {index}: a label needs an 'at' anchor")


# ---------------------------------------------------------------------------
# Anchors
# ---------------------------------------------------------------------------


class AnchorResolver:
    """Turns element anchors into ground-plane points for a frame.

    An anchor is ``[x, z]`` / ``[x, y, z]`` in arena metres, or a group
    reference ``{"group": name, "at": "centroid"|"front"|"front_left"|
    "front_right"|"rear", "unit": k, "forward_m": d, "right_m": d,
    "offset": [dx, dz]}``. ``unit`` follows the k-th living unit of the group
    as first exported (by entity id, so it stays the same maniple).
    """

    def __init__(self, track: CameraTrack):
        self.track = track
        self._unit_ids: dict[tuple[str, int], int] = {}

    def unit_id(self, name: str, ordinal: int) -> int | None:
        key = (name, ordinal)
        if key not in self._unit_ids:
            for frame in range(len(self.track.frames)):
                group = self.track.group(frame, name)
                if group and len(group.get("units", [])) > ordinal:
                    self._unit_ids[key] = int(group["units"][ordinal][0])
                    break
            else:
                return None
        return self._unit_ids[key]

    def point(self, anchor: Any, frame: int) -> Vec2 | None:
        if isinstance(anchor, (list, tuple)):
            if len(anchor) == 2:
                return (float(anchor[0]), float(anchor[1]))
            if len(anchor) == 3:
                return (float(anchor[0]), float(anchor[2]))
            raise ValueError(f"anchor {anchor!r} needs [x, z] or [x, y, z]")
        if not isinstance(anchor, dict) or "group" not in anchor:
            raise ValueError(f"anchor {anchor!r} is neither a point nor a group")
        name = anchor["group"]
        group = self.track.nearest_group(frame, name)
        if group is None:
            return None
        forward = group.get("forward", [0.0, 0.0, 1.0])
        forward2 = (float(forward[0]), float(forward[2]))
        right2 = (-forward2[1], forward2[0])
        at = anchor.get("at", "centroid")
        if "unit" in anchor:
            wanted = self.unit_id(name, int(anchor["unit"]))
            unit = next((u for u in group.get("units", []) if int(u[0]) == wanted), None)
            if unit is None:
                return None
            base = (float(unit[1]), float(unit[3]))
        elif at == "centroid":
            c = group["centroid"]
            base = (float(c[0]), float(c[2]))
        elif at in ("front", "front_left", "front_right", "rear"):
            left, right = group["front"]
            if at == "front_left":
                base = (float(left[0]), float(left[2]))
            elif at == "front_right":
                base = (float(right[0]), float(right[2]))
            else:
                base = ((left[0] + right[0]) / 2.0, (left[2] + right[2]) / 2.0)
                if at == "rear":
                    depth = float(group.get("depth", 0.0))
                    base = (base[0] - forward2[0] * depth, base[1] - forward2[1] * depth)
        else:
            raise ValueError(f"unknown group anchor '{at}'")
        ahead = float(anchor.get("forward_m", 0.0))
        aside = float(anchor.get("right_m", 0.0))
        offset = anchor.get("offset", [0.0, 0.0])
        return (
            base[0] + forward2[0] * ahead + right2[0] * aside + float(offset[0]),
            base[1] + forward2[1] * ahead + right2[1] * aside + float(offset[-1]),
        )


# ---------------------------------------------------------------------------
# Rendering
# ---------------------------------------------------------------------------


class Layer:
    """One element's supersampled coverage masks inside a pixel window."""

    def __init__(self, width: int, height: int, scale: int):
        self.width = width
        self.height = height
        self.scale = scale
        self.polygons: list[list[Vec2]] = []
        self.fill_lines: list[tuple[list[Vec2], float]] = []
        self.outline_lines: list[tuple[list[Vec2], float]] = []
        self.outline_polygons: list[tuple[list[Vec2], float]] = []

    def bounds(self, pad: float) -> tuple[int, int, int, int] | None:
        xs: list[float] = []
        ys: list[float] = []
        for polygon in self.polygons:
            xs += [p[0] for p in polygon]
            ys += [p[1] for p in polygon]
        for line, width in self.fill_lines + self.outline_lines:
            xs += [p[0] for p in line]
            ys += [p[1] for p in line]
        for polygon, _ in self.outline_polygons:
            xs += [p[0] for p in polygon]
            ys += [p[1] for p in polygon]
        if not xs:
            return None
        left = max(0, int(math.floor(min(xs) - pad)))
        top = max(0, int(math.floor(min(ys) - pad)))
        right = min(self.width, int(math.ceil(max(xs) + pad)))
        bottom = min(self.height, int(math.ceil(max(ys) + pad)))
        if right <= left or bottom <= top:
            return None
        return left, top, right, bottom

    def masks(self, pad: float):
        from PIL import Image, ImageDraw

        box = self.bounds(pad)
        if box is None:
            return None
        left, top, right, bottom = box
        s = self.scale
        size = ((right - left) * s, (bottom - top) * s)

        def local(points):
            return [((x - left) * s, (y - top) * s) for x, y in points]

        fill = Image.new("L", size, 0)
        draw = ImageDraw.Draw(fill)
        for polygon in self.polygons:
            if len(polygon) >= 3:
                draw.polygon(local(polygon), fill=255)
        for line, width in self.fill_lines:
            _thick_line(draw, local(line), width * s)
        outline = None
        if self.outline_lines or self.outline_polygons:
            outline = Image.new("L", size, 0)
            odraw = ImageDraw.Draw(outline)
            for line, width in self.outline_lines:
                _thick_line(odraw, local(line), width * s)
            for polygon, width in self.outline_polygons:
                if len(polygon) >= 2:
                    _thick_line(odraw, local(polygon + [polygon[0]]), width * s)
        small = (right - left, bottom - top)
        fill = fill.reduce(s) if s > 1 else fill
        if outline is not None and s > 1:
            outline = outline.reduce(s)
        if fill.size != small:
            fill = fill.resize(small)
        if outline is not None and outline.size != small:
            outline = outline.resize(small)
        return box, fill, outline


def _thick_line(draw, points: list[Vec2], width: float) -> None:
    """A polyline with round joins and caps (Pillow's own joins leave notches)."""
    if len(points) < 2 or width <= 0:
        return
    radius = width / 2.0
    draw.line(points, fill=255, width=max(1, int(round(width))))
    for x, y in points:
        draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=255)


class Renderer:
    """Renders an :class:`Overlay` for frames of one :class:`CameraTrack`."""

    def __init__(self, overlay: Overlay, track: CameraTrack, terrain: Terrain,
                 width: int | None = None, height: int | None = None):
        self.overlay = overlay
        self.track = track
        self.terrain = terrain
        self.width = width or track.width
        self.height = height or track.height
        self.ui = min(self.width, self.height) / 1080.0
        self.anchors = AnchorResolver(track)
        self._fonts: dict[tuple[str, int], Any] = {}
        self._fixed: dict[tuple[int, int], list[Vec2] | None] = {}

    # -- time
    def time_of(self, frame: int) -> float:
        record = self.track.frames[frame]
        return float(record.get("t", frame / self.track.fps))

    def active(self, frame: int) -> list[tuple[Element, float]]:
        t = self.time_of(frame)
        return [(e, e.opacity(t)) for e in self.overlay.elements if e.opacity(t) > 0.0]

    # -- anchors
    def anchor_frame(self, element: Element, frame: int) -> int:
        """Live elements read the current frame; fixed ones their start."""
        track = element.data.get("track", "live" if element.kind != "arrow" else "fixed")
        if track == "live":
            return frame
        if isinstance(track, (int, float)) and not isinstance(track, bool):
            return self.track.frame_at(float(track))
        return self.track.frame_at(element.start)

    def resolve_points(self, element: Element, frame: int) -> list[Vec2] | None:
        at = self.anchor_frame(element, frame)
        key = (id(element), at)
        if key not in self._fixed:
            points = []
            for anchor in element.data.get("points", []):
                point = self.anchors.point(anchor, at)
                if point is None:
                    points = None
                    break
                points.append(point)
            self._fixed[key] = points
        return self._fixed[key]

    # -- primitives
    def px(self, value: float) -> float:
        return float(value) * self.ui

    def colour(self, value, owner: int | None = None):
        if value is None:
            value = owner_color(self.overlay.style, owner)
        return parse_color(value, self.overlay.style)

    def font(self, size: int):
        from PIL import ImageFont

        style = self.overlay.style
        key = (style["font"], size)
        if key not in self._fonts:
            try:
                self._fonts[key] = ImageFont.truetype(style["font"], size)
            except OSError:
                self._fonts[key] = ImageFont.truetype(style["font_fallback"], size)
        return self._fonts[key]

    # -- elements
    def arrow_layer(self, element: Element, camera: Camera, t: float) -> Layer | None:
        style = element.style
        points = self.resolve_points(element, self._frame)
        if not points:
            return None
        path = arrow_path(points, float(element.data.get("bend", 0.0)),
                          bool(element.data.get("smooth", True)))
        draw = float(element.data.get("draw", style.get("draw_seconds", 1.2)))
        progress = 1.0 if draw <= 0 else ease(style.get("ease", "smooth"),
                                              (t - element.start) / draw)
        shape = arrow_shape(
            path,
            progress,
            float(style["width_m"]),
            float(style["head_length_m"]),
            float(style["head_width_m"]),
            float(style.get("taper", 1.0)),
            float(style.get("step_m", 0.75)),
            style.get("head", "triangle"),
        )
        if shape is None:
            return None
        lift = float(style.get("lift_m", 0.1))
        layer = Layer(self.width, self.height, int(self.overlay.style["supersample"]))
        drape = self.terrain.drape

        def quad_strip(sections):
            for (l0, r0), (l1, r1) in zip(sections, sections[1:]):
                polygon = camera.project_polygon([
                    drape(*l0, lift), drape(*l1, lift), drape(*r1, lift), drape(*r0, lift)
                ])
                if len(polygon) >= 3:
                    layer.polygons.append(polygon)

        quad_strip(shape.sections)
        if shape.head:
            quad_strip(shape.head)
        outline_px = self.px(style.get("outline_px", 0.0))
        if outline_px > 0 and style.get("outline"):
            boundary = [s[0] for s in shape.sections]
            if shape.head:
                boundary += [h[0] for h in shape.head] + [shape.tip]
                boundary += [h[1] for h in reversed(shape.head)]
            boundary += [s[1] for s in reversed(shape.sections)]
            for run in camera.project_polyline([drape(x, z, lift) for x, z in boundary]
                                               + [drape(*boundary[0], lift)]):
                layer.outline_lines.append((run, outline_px * 2.0))
        return layer

    def block_geometry(self, element: Element, frame: int):
        group = self.track.nearest_group(self.anchor_frame(element, frame),
                                         element.data["group"])
        if group is None:
            return None
        style = element.style
        pad = float(element.data.get("padding_m", style.get("padding_m", 3.0)))
        forward = (float(group["forward"][0]), float(group["forward"][2]))
        right = (-forward[1], forward[0])
        centre = (float(group["centroid"][0]), float(group["centroid"][2]))
        units = group.get("units") or []
        lat = [((u[1] - centre[0]) * right[0] + (u[3] - centre[1]) * right[1]) for u in units]
        lon = [((u[1] - centre[0]) * forward[0] + (u[3] - centre[1]) * forward[1]) for u in units]
        if not lat:
            lat = lon = [0.0]
        return centre, forward, right, (min(lat) - pad, max(lat) + pad,
                                        min(lon) - pad, max(lon) + pad), group

    def block_layer(self, element: Element, camera: Camera, t: float):
        geometry = self.block_geometry(element, self._frame)
        if geometry is None:
            return None
        centre, forward, right, (l0, l1, d0, d1), group = geometry
        style = element.style
        lift = float(style.get("lift_m", 0.1))
        cell = max(0.5, float(style.get("cell_m", 2.0)))

        def at(lat, lon):
            return self.terrain.drape(centre[0] + right[0] * lat + forward[0] * lon,
                                      centre[1] + right[1] * lat + forward[1] * lon, lift)

        columns = max(1, int(math.ceil((l1 - l0) / cell)))
        rows = max(1, int(math.ceil((d1 - d0) / cell)))
        layer = Layer(self.width, self.height, int(self.overlay.style["supersample"]))
        grid = [[at(l0 + (l1 - l0) * c / columns, d0 + (d1 - d0) * r / rows)
                 for c in range(columns + 1)] for r in range(rows + 1)]
        for r in range(rows):
            for c in range(columns):
                polygon = camera.project_polygon(
                    [grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c]])
                if len(polygon) >= 3:
                    layer.polygons.append(polygon)
        ring = ([grid[0][c] for c in range(columns + 1)]
                + [grid[r][columns] for r in range(1, rows + 1)]
                + [grid[rows][c] for c in range(columns - 1, -1, -1)]
                + [grid[r][0] for r in range(rows - 1, -1, -1)])
        outline_px = self.px(style.get("outline_px", 0.0))
        front_px = self.px(style.get("front_px", 0.0))
        lines = []
        if outline_px > 0:
            lines += [(run, outline_px) for run in camera.project_polyline(ring)]
        if front_px > 0:
            lines += [(run, front_px) for run in camera.project_polyline(grid[rows])]
        return layer, lines, group

    def frontage_layer(self, element: Element, camera: Camera, t: float):
        group = self.track.nearest_group(self.anchor_frame(element, self._frame),
                                         element.data["group"])
        if group is None:
            return None
        style = element.style
        left, right = group["front"]
        a = (float(left[0]), float(left[2]))
        b = (float(right[0]), float(right[2]))
        length = math.dist(a, b)
        extend = float(style.get("extend_m", 0.0))
        if length > 1e-6:
            d = ((b[0] - a[0]) / length, (b[1] - a[1]) / length)
        else:
            forward = group.get("forward", [0, 0, 1])
            d = (-float(forward[2]), float(forward[0]))
        a = (a[0] - d[0] * extend, a[1] - d[1] * extend)
        b = (b[0] + d[0] * extend, b[1] + d[1] * extend)
        draw = float(element.data.get("draw", style.get("draw_seconds", 0.6)))
        progress = 1.0 if draw <= 0 else ease(style.get("ease", "smooth"),
                                              (t - element.start) / draw)
        middle = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
        a = (middle[0] + (a[0] - middle[0]) * progress, middle[1] + (a[1] - middle[1]) * progress)
        b = (middle[0] + (b[0] - middle[0]) * progress, middle[1] + (b[1] - middle[1]) * progress)
        lift = float(style.get("lift_m", 0.15))
        line = [self.terrain.drape(x, z, lift)
                for x, z in resample([a, b], float(style.get("step_m", 1.0)))]
        layer = Layer(self.width, self.height, int(self.overlay.style["supersample"]))
        width = self.px(style.get("width_px", 6.0))
        for run in camera.project_polyline(line):
            layer.fill_lines.append((run, width))
            if style.get("outline") and style.get("outline_px", 0) > 0:
                layer.outline_lines.append((run, width + 2 * self.px(style["outline_px"])))
        return layer, group

    # -- composition
    def render(self, frame: int):
        """The overlay for one clip frame as an RGBA ``PIL.Image``."""
        from PIL import Image

        canvas = Image.new("RGBA", (self.width, self.height), (0, 0, 0, 0))
        active = self.active(frame)
        if not active:
            return canvas
        camera = self.track.camera(frame, self.width, self.height)
        if camera is None:
            return canvas
        self._frame = frame
        t = self.time_of(frame)
        for element, alpha in active:
            style = element.style
            if element.kind == "arrow":
                layer = self.arrow_layer(element, camera, t)
                if layer is not None:
                    owner = self._owner_of(element)
                    self._composite(canvas, layer,
                                    self.colour(style.get("color"), owner),
                                    alpha * float(style.get("opacity", 1.0)),
                                    style.get("outline"),
                                    alpha * float(style.get("outline_opacity", 1.0)))
            elif element.kind == "block":
                built = self.block_layer(element, camera, t)
                if built is not None:
                    layer, lines, group = built
                    colour = self.colour(style.get("color"), group.get("owner"))
                    self._composite(canvas, layer, colour,
                                    alpha * float(style.get("fill_opacity", 0.3)), None, 0.0)
                    edge = Layer(self.width, self.height, layer.scale)
                    edge.fill_lines = lines
                    self._composite(canvas, edge, colour,
                                    alpha * float(style.get("outline_opacity", 1.0)), None, 0.0)
            elif element.kind == "frontage":
                built = self.frontage_layer(element, camera, t)
                if built is not None:
                    layer, group = built
                    self._composite(canvas, layer,
                                    self.colour(style.get("color"), group.get("owner")),
                                    alpha * float(style.get("opacity", 1.0)),
                                    style.get("outline"),
                                    alpha * float(style.get("outline_opacity", 1.0)))
            elif element.kind == "label":
                self._label(canvas, element, camera, alpha)
        return canvas

    def _owner_of(self, element: Element) -> int | None:
        for anchor in element.data.get("points", []):
            if isinstance(anchor, dict) and "group" in anchor:
                group = self.track.nearest_group(self._frame, anchor["group"])
                if group is not None:
                    return group.get("owner")
        return element.data.get("owner")

    def _composite(self, canvas, layer: Layer, colour, opacity: float,
                   outline_colour, outline_opacity: float) -> None:
        from PIL import Image

        pad = max([w for _, w in layer.fill_lines + layer.outline_lines] + [0.0]) + 4.0
        built = layer.masks(pad)
        if built is None:
            return
        box, fill, outline = built
        size = fill.size
        piece = Image.new("RGBA", size, (0, 0, 0, 0))
        if outline is not None and outline_colour:
            ink = parse_color(outline_colour, self.overlay.style)
            plate = Image.new("RGBA", size, (*ink, 0))
            plate.putalpha(outline.point(lambda v: int(v * outline_opacity)))
            piece = Image.alpha_composite(piece, plate)
        plate = Image.new("RGBA", size, (*colour, 0))
        plate.putalpha(fill.point(lambda v: int(v * opacity)))
        piece = Image.alpha_composite(piece, plate)
        region = canvas.crop(box)
        canvas.paste(Image.alpha_composite(region, piece), box[:2])

    def _label(self, canvas, element: Element, camera: Camera, alpha: float) -> None:
        from PIL import Image, ImageDraw

        style = element.style
        point = self.anchors.point(element.data["at"], self.anchor_frame(element, self._frame))
        if point is None:
            return
        height = float(element.data.get("height_m", style.get("height_m", 0.0)))
        world = self.terrain.drape(point[0], point[1], height)
        anchor = camera.project(world)
        if anchor is None:
            return
        text = str(element.data["text"])
        if style.get("uppercase", True):
            text = text.upper()
        size = max(6, int(round(self.px(style.get("size_px", 34.0)))))
        font = self.font(size)
        offset = style.get("offset_px", [0.0, -40.0])
        x = anchor[0] + self.px(offset[0])
        y = anchor[1] + self.px(offset[1])
        tracking = float(style.get("tracking", 0.0)) * size
        # Prefix widths keep the face's kerning pairs; tracking adds to them.
        starts = [font.getlength(text[:index]) + tracking * index for index in range(len(text))]
        total = font.getlength(text) + tracking * max(0, len(text) - 1)
        ascent, descent = font.getmetrics()
        left = x - total / 2.0
        top = y - (ascent + descent) / 2.0
        shadow_px = self.px(style.get("shadow_px", 0.0))
        margin = int(shadow_px * 3 + 4)
        box = (int(left) - margin, int(top) - margin,
               int(left + total) + margin + 1, int(top + ascent + descent) + margin + 1)
        layer = Image.new("RGBA", (box[2] - box[0], box[3] - box[1]), (0, 0, 0, 0))
        mask = Image.new("L", layer.size, 0)
        draw = ImageDraw.Draw(mask)
        for ch, start in zip(text, starts):
            draw.text((left - box[0] + start, top - box[1]), ch, font=font, fill=255)
        if shadow_px > 0 and style.get("shadow"):
            from PIL import ImageFilter

            ink = parse_color(style["shadow"], self.overlay.style)
            halo = mask.filter(ImageFilter.MaxFilter(3)).filter(
                ImageFilter.GaussianBlur(shadow_px))
            plate = Image.new("RGBA", layer.size, (*ink, 0))
            shadow_alpha = alpha * float(style.get("shadow_opacity", 0.7))
            plate.putalpha(halo.point(lambda v: int(min(255, v * 1.6) * shadow_alpha)))
            layer = Image.alpha_composite(layer, plate)
        colour = self.colour(style.get("color"))
        plate = Image.new("RGBA", layer.size, (*colour, 0))
        opacity = alpha * float(style.get("opacity", 1.0))
        plate.putalpha(mask.point(lambda v: int(v * opacity)))
        layer = Image.alpha_composite(layer, plate)
        # Paste with clipping: the label may sit partly outside the frame.
        dest = (max(0, box[0]), max(0, box[1]),
                min(self.width, box[2]), min(self.height, box[3]))
        if dest[2] <= dest[0] or dest[3] <= dest[1]:
            return
        crop = layer.crop((dest[0] - box[0], dest[1] - box[1],
                           dest[2] - box[0], dest[3] - box[1]))
        region = canvas.crop(dest)
        canvas.paste(Image.alpha_composite(region, crop), dest[:2])


# ---------------------------------------------------------------------------
# Files and ffmpeg
# ---------------------------------------------------------------------------


def companions(clip: Path) -> tuple[Path, Path | None]:
    """The camera track and terrain written beside an arena clip."""
    clip = Path(clip)
    track = clip.with_suffix(".camera.jsonl")
    if not track.is_file():
        raise SystemExit(f"no camera track beside {clip} (expected {track.name})")
    header = json.loads(track.read_text().split("\n", 1)[0])
    terrain = clip.parent / header["terrain"] if header.get("terrain") else None
    if terrain is not None and not terrain.is_file():
        terrain = None
    return track, terrain


def load_json(path: Path | str | None) -> dict | None:
    if path is None:
        return None
    return json.loads(Path(path).read_text())


def iter_frames(renderer: Renderer, first: int, last: int) -> Iterable[bytes]:
    blank = None
    for frame in range(first, last + 1):
        if not renderer.active(frame):
            if blank is None:
                blank = bytes(renderer.width * renderer.height * 4)
            yield blank
            continue
        yield renderer.render(frame).tobytes()


def encode_alpha(renderer: Renderer, out: Path, first: int, last: int) -> None:
    """Write the overlay alone as ProRes 4444 with alpha."""
    cmd = [
        "ffmpeg", "-y", "-v", "error",
        "-f", "rawvideo", "-pix_fmt", "rgba",
        "-s", f"{renderer.width}x{renderer.height}",
        "-r", f"{renderer.track.fps:g}", "-i", "-",
        "-c:v", "prores_ks", "-profile:v", "4444", "-pix_fmt", "yuva444p10le",
        str(out),
    ]
    _pipe(cmd, iter_frames(renderer, first, last))


def composite(renderer: Renderer, clip: Path, out: Path, crf: int = 16) -> None:
    """Burn the overlay into a copy of the clip (audio passes through)."""
    cmd = [
        "ffmpeg", "-y", "-v", "error",
        "-i", str(clip),
        "-f", "rawvideo", "-pix_fmt", "rgba",
        "-s", f"{renderer.width}x{renderer.height}",
        "-r", f"{renderer.track.fps:g}", "-i", "-",
        "-filter_complex", "[0:v][1:v]overlay=0:0:format=auto:eof_action=pass,format=yuv420p[v]",
        "-map", "[v]", "-map", "0:a?",
        "-c:v", "libx264", "-preset", "medium", "-crf", str(crf),
        "-c:a", "copy",
        str(out),
    ]
    _pipe(cmd, iter_frames(renderer, 0, len(renderer.track.frames) - 1))


def _pipe(cmd: list[str], frames: Iterable[bytes]) -> None:
    process = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    assert process.stdin is not None
    try:
        for data in frames:
            process.stdin.write(data)
    finally:
        process.stdin.close()
    if process.wait() != 0:
        raise SystemExit(f"ffmpeg failed: {' '.join(cmd)}")


def build_renderer(clip: Path, overlay: dict, style: dict | None = None,
                   width: int | None = None, height: int | None = None) -> Renderer:
    track_path, terrain_path = companions(clip)
    track = CameraTrack.load(track_path)
    terrain = Terrain.load(terrain_path) if terrain_path else Terrain.flat()
    return Renderer(Overlay.parse(overlay, style), track, terrain, width, height)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--clip", type=Path, required=True,
                        help="an arena clip with its .camera.jsonl beside it")
    parser.add_argument("--overlay", type=Path, required=True,
                        help="the overlay description (JSON)")
    parser.add_argument("--style", type=Path,
                        help="a style JSON (the series graphics package) to inject")
    parser.add_argument("--out", type=Path, help="the clip with the overlay burned in")
    parser.add_argument("--alpha-out", type=Path, help="the overlay alone, ProRes 4444")
    parser.add_argument("--frames", metavar="DIR", type=Path,
                        help="also write RGBA PNGs of every --stills frame here")
    parser.add_argument("--stills", type=str, default="",
                        help="comma-separated frame numbers for --frames")
    args = parser.parse_args(argv)

    renderer = build_renderer(args.clip, load_json(args.overlay), load_json(args.style))
    if args.frames and args.stills:
        args.frames.mkdir(parents=True, exist_ok=True)
        for token in args.stills.split(","):
            frame = int(token)
            renderer.render(frame).save(args.frames / f"overlay_{frame:05d}.png")
    if args.alpha_out:
        encode_alpha(renderer, args.alpha_out, 0, len(renderer.track.frames) - 1)
    if args.out:
        composite(renderer, args.clip, args.out)
    if not (args.out or args.alpha_out or args.frames):
        parser.error("nothing to write: pass --out, --alpha-out or --frames")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
