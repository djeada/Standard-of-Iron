"""The generation request: seed, preset, size, parameters and locks.

Everything that decides a generated map lives in one ``GenerationRequest``.
Stage seeds are derived from the global seed by hashing, never drawn in
sequence, so a parameter change in one stage cannot shift another stage's
random stream.
"""

from __future__ import annotations

import hashlib
from dataclasses import dataclass, field
from typing import Any

from . import GENERATOR_VERSION

STAGES: tuple[str, ...] = (
    "terrain",
    "water",
    "settlements",
    "roads",
    "forests",
    "landmarks",
    "dressing",
    "spawns",
)

STAGE_LABELS: dict[str, str] = {
    "terrain": "Terrain",
    "water": "Water",
    "settlements": "Settlements",
    "roads": "Roads",
    "forests": "Forests",
    "landmarks": "Landmarks",
    "dressing": "Dressing",
    "spawns": "Spawns",
}

# A stage whose intent is derived from another stage's geometry cannot stay
# locked while that stage is rerolled: roads run between settlement anchors,
# dressing is anchored to bridges, gates and ramps.
FORCED_BY: dict[str, tuple[str, ...]] = {
    "terrain": (),
    "water": (),
    "settlements": (),
    "roads": ("settlements",),
    "forests": ("settlements", "roads"),
    "landmarks": ("settlements", "roads"),
    "dressing": ("terrain", "water", "settlements", "roads", "landmarks"),
    "spawns": ("settlements",),
}

# Seeds stay inside the range a double holds exactly, so a seed survives a
# round trip through the editor's QJsonValue unchanged.
SEED_MASK = (1 << 53) - 1
MAX_SEED = (1 << 32) - 1
MIN_MAP_SIZE = 200
MAX_MAP_SIZE = 1000


class RequestError(ValueError):
    """A request that cannot be generated: bad preset, stage or value."""


def derive_seed(*parts: object) -> int:
    text = ":".join(str(part) for part in parts)
    digest = hashlib.sha256(text.encode("utf-8")).digest()
    return int.from_bytes(digest[:8], "big") & SEED_MASK


def stage_seed(seed: int, stage: str, attempt: int = 0) -> int:
    """The seed a stage draws from; retries derive a fresh one per attempt."""
    if attempt == 0:
        return derive_seed(seed, stage)
    return derive_seed(seed, stage, f"attempt{attempt}")


def layout_seed(seed: int, attempt: int = 0) -> int:
    return stage_seed(seed, "layout", attempt)


@dataclass
class GenerationRequest:
    seed: int
    preset: str
    width: int = 650
    height: int = 650
    parameters: dict[str, Any] = field(default_factory=dict)
    locks: list[str] = field(default_factory=list)
    version: int = GENERATOR_VERSION

    def validate(self) -> None:
        if self.version != GENERATOR_VERSION:
            raise RequestError(
                f"request version {self.version} is not supported "
                f"(this generator is version {GENERATOR_VERSION})"
            )
        if not 0 <= int(self.seed) <= MAX_SEED:
            raise RequestError(f"seed must be in 0..{MAX_SEED}")
        for name, value in (("width", self.width), ("height", self.height)):
            if not MIN_MAP_SIZE <= int(value) <= MAX_MAP_SIZE:
                raise RequestError(
                    f"{name} must be in {MIN_MAP_SIZE}..{MAX_MAP_SIZE}, got {value}"
                )
        unknown = [stage for stage in self.locks if stage not in STAGES]
        if unknown:
            raise RequestError(f"unknown stage(s) to lock: {', '.join(unknown)}")

    def to_json(self) -> dict[str, Any]:
        return {
            "version": self.version,
            "seed": int(self.seed),
            "preset": self.preset,
            "width": int(self.width),
            "height": int(self.height),
            "parameters": dict(sorted(self.parameters.items())),
            "locks": [stage for stage in STAGES if stage in self.locks],
        }

    @classmethod
    def from_json(cls, data: dict[str, Any]) -> "GenerationRequest":
        if not isinstance(data, dict):
            raise RequestError("a request must be a JSON object")
        try:
            return cls(
                seed=int(data.get("seed", 0)),
                preset=str(data.get("preset", "")),
                width=int(data.get("width", 650)),
                height=int(data.get("height", 650)),
                parameters=dict(data.get("parameters") or {}),
                locks=[str(stage) for stage in data.get("locks") or []],
                version=int(data.get("version", GENERATOR_VERSION)),
            )
        except (TypeError, ValueError) as error:
            raise RequestError(f"malformed request: {error}") from error


def resolve_locks(requested: list[str] | set[str]) -> tuple[set[str], dict[str, str]]:
    """Return the stages that really stay locked, and why the others regenerate.

    A requested lock is dropped when a stage it is derived from is rerolled;
    the second value maps each such stage to the first stage that forced it.
    Forcing is transitive because ``STAGES`` lists every forcer before the
    stages it forces.
    """
    locked: set[str] = set()
    forced: dict[str, str] = {}
    for stage in STAGES:
        if stage not in requested:
            continue
        forcer = next((dep for dep in FORCED_BY[stage] if dep not in locked), None)
        if forcer is None:
            locked.add(stage)
        else:
            forced[stage] = forcer
    return locked, forced
