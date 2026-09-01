#!/usr/bin/env python3
"""The ground a map actually builds, for the authored-placement audit.

A hill in a map JSON is a centre, a radius and maybe a shape name.  What the
engine raises from that is something else: `Landform::sample_hill` warps the
boundary with fbm, roughens it by up to +-34% of the radius, unions in an
off-centre lobe, widens and hash-rotates the whole footprint at campaign scale,
and then cuts ramp corridors that reach well outside the authored ellipse
entirely.  Two hills the same size on paper break the ground in different places.

None of that is recoverable from the JSON, and a second implementation of it in
Python would be wrong the first time the noise is retuned.  So this module asks
the engine: `tools/terrain_probe` builds the heightfield with the same calls
`TerrainService::initialize` makes and dumps three planes, and everything here
reads them back.

The probe binary is optional at import time.  A checkout with no build directory
still runs the rest of the audit; it just cannot see the ground.
"""

from __future__ import annotations

import array
import json
import math
import os
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

PROBE_SEARCH_DIRS = (
    "build/bin",
    "build-release/bin",
    "build-debug/bin",
    "bin",
)

SAMPLE_STEP = 0.45
"""Metres between footprint samples.

Half a tile: fine enough that the shoulder of a ramp cannot slip between two
samples of a tent, coarse enough that the biggest body on the map is a few
dozen lookups."""

TRUNK_SAMPLE_FRACTION = 0.30
"""How much of a canopy tree stands on the ground.

A pine is measured across its trunk, not its crown.  The crown is nine metres
across and hangs over whatever it likes; only the stem has to stand on ground
that does not break."""


class ProbeUnavailable(RuntimeError):
    """No terrain_probe to ask, so the ground cannot be measured."""


def find_probe(explicit: str | None = None) -> Path:
    if explicit:
        candidate = Path(explicit)
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
        raise ProbeUnavailable(f"{candidate} is not an executable terrain_probe")
    for directory in PROBE_SEARCH_DIRS:
        candidate = REPO_ROOT / directory / "terrain_probe"
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    raise ProbeUnavailable(
        "terrain_probe not found -- build it with "
        "`cmake --build build --target terrain_probe -j4`"
    )


def default_cache_dir() -> Path:
    return Path(tempfile.gettempdir()) / "soi-terrain-probe"


@dataclass
class SurfaceField:
    """The built heightfield of one map, indexed in authored grid coordinates.

    Grid coordinate ``(x, z)`` is cell ``(x, z)`` of the heightfield: the engine
    inverts its own world conversion with ``grid_half = size * 0.5 - 0.5``, so a
    map coordinate lands on a cell *centre* and no half-cell correction belongs
    here.  See `docs/MAP_OBJECT_PLACEMENT.md`.
    """

    width: int
    height: int
    tile_size: float
    heights: array.array
    entrances: bytes

    def height_at(self, x: float, z: float) -> float:
        clamped_x = min(max(x, 0.0), self.width - 1.001)
        clamped_z = min(max(z, 0.0), self.height - 1.001)
        cell_x = int(clamped_x)
        cell_z = int(clamped_z)
        frac_x = clamped_x - cell_x
        frac_z = clamped_z - cell_z
        row = cell_z * self.width + cell_x
        next_row = row + self.width
        top = self.heights[row] * (1.0 - frac_x) + self.heights[row + 1] * frac_x
        bottom = (
            self.heights[next_row] * (1.0 - frac_x)
            + self.heights[next_row + 1] * frac_x
        )
        return top * (1.0 - frac_z) + bottom * frac_z

    def is_hill_entrance(self, x: float, z: float) -> bool:
        cell_x = min(max(int(round(x)), 0), self.width - 1)
        cell_z = min(max(int(round(z)), 0), self.height - 1)
        return self.entrances[cell_z * self.width + cell_x] != 0

    def footprint_samples(
        self,
        x: float,
        z: float,
        half_x: float,
        half_z: float,
        rotation: float,
        is_disc: bool,
    ) -> list[tuple[float, float]]:
        """The ground a body covers, as points in grid coordinates."""
        steps_x = max(1, int(math.ceil(2.0 * half_x / SAMPLE_STEP)))
        steps_z = max(1, int(math.ceil(2.0 * half_z / SAMPLE_STEP)))
        cosine = math.cos(rotation)
        sine = math.sin(rotation)
        points: list[tuple[float, float]] = []
        for index_z in range(steps_z + 1):
            local_z = -half_z + 2.0 * half_z * index_z / steps_z
            for index_x in range(steps_x + 1):
                local_x = -half_x + 2.0 * half_x * index_x / steps_x
                if (
                    is_disc
                    and math.hypot(
                        local_x / max(half_x, 1e-6), local_z / max(half_z, 1e-6)
                    )
                    > 1.0
                ):
                    continue
                points.append(
                    (
                        x + local_x * cosine - local_z * sine,
                        z + local_x * sine + local_z * cosine,
                    )
                )
        return points

    def relief(self, points: list[tuple[float, float]]) -> float:
        """How far the ground breaks across a footprint, in metres.

        A model is settled on the lowest ground its footprint spans, so this is
        also how far the high corner of the body floats above the surface."""
        if not points:
            return 0.0
        lowest = math.inf
        highest = -math.inf
        for x, z in points:
            value = self.height_at(x, z)
            lowest = min(lowest, value)
            highest = max(highest, value)
        return highest - lowest

    def entrance_coverage(self, points: list[tuple[float, float]]) -> float:
        """The share of a footprint standing in a hill entrance ramp."""
        if not points:
            return 0.0
        on_ramp = sum(1 for x, z in points if self.is_hill_entrance(x, z))
        return on_ramp / len(points)

    def settled_bearing(
        self, x: float, z: float, reach: float, prefer_off_ramp: bool = False
    ) -> tuple[float, float]:
        """The way towards ground a body can stand on.

        Scored over a ring rather than differenced cell by cell: the engine's
        surface carries noise at the cell scale, so the gradient of one cell
        points at a bump and not at the foot of the hill.  Flattest wins, and
        leaving the ramp wins first when it is a ramp being escaped -- a body
        pushed along a gateway is still in the gateway."""
        best_score = None
        best = (1.0, 0.0)
        for step in range(24):
            angle = 2.0 * math.pi * step / 24.0
            unit_x = math.cos(angle)
            unit_z = math.sin(angle)
            target_x = x + unit_x * reach
            target_z = z + unit_z * reach
            relief = self.relief(
                [
                    (target_x, target_z),
                    (target_x + reach * 0.35, target_z),
                    (target_x - reach * 0.35, target_z),
                    (target_x, target_z + reach * 0.35),
                    (target_x, target_z - reach * 0.35),
                ]
            )
            on_ramp = 1.0 if self.is_hill_entrance(target_x, target_z) else 0.0
            score = relief + (on_ramp * 100.0 if prefer_off_ramp else on_ramp)
            if best_score is None or score < best_score:
                best_score = score
                best = (unit_x, unit_z)
        return best

    def downhill(self, x: float, z: float, reach: float) -> tuple[float, float]:
        """The way off a slope: the bearing the ground falls away fastest.

        Sampled over a ring rather than differenced cell by cell, because the
        engine's surface carries noise at the cell scale and the gradient of one
        cell points at a bump rather than at the foot of the hill."""
        best_drop = 0.0
        best = (0.0, 0.0)
        here = self.height_at(x, z)
        for step in range(16):
            angle = 2.0 * math.pi * step / 16.0
            unit_x = math.cos(angle)
            unit_z = math.sin(angle)
            drop = here - self.height_at(x + unit_x * reach, z + unit_z * reach)
            if drop > best_drop:
                best_drop = drop
                best = (unit_x, unit_z)
        return best


def _probe_output(map_path: Path, probe: Path, cache_dir: Path) -> Path:
    cache_dir.mkdir(parents=True, exist_ok=True)
    cached = cache_dir / (map_path.stem + ".terrain")
    if cached.exists():
        stamp = cached.stat().st_mtime
        if stamp >= map_path.stat().st_mtime and stamp >= probe.stat().st_mtime:
            return cached
    result = subprocess.run(
        [str(probe), str(map_path), str(cached)],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise ProbeUnavailable(
            f"terrain_probe failed on {map_path.name}: {result.stderr.strip()}"
        )
    return cached


def load_surface(
    map_path: Path, probe: Path, cache_dir: Path | None = None
) -> SurfaceField:
    """Build (or reuse) the probe dump for one map and read it back."""
    dump = _probe_output(map_path, probe, cache_dir or default_cache_dir())
    raw = dump.read_bytes()
    break_index = raw.index(b"\n")
    header = json.loads(raw[:break_index])
    width = int(header["width"])
    height = int(header["height"])
    cells = width * height
    offset = break_index + 1
    heights = array.array("f")
    heights.frombytes(raw[offset : offset + cells * 4])
    offset += cells * 4
    entrances = raw[offset : offset + cells]
    if len(heights) != cells or len(entrances) != cells:
        raise ProbeUnavailable(f"{dump} is truncated")
    return SurfaceField(
        width=width,
        height=height,
        tile_size=float(header["tile_size"]),
        heights=heights,
        entrances=entrances,
    )
