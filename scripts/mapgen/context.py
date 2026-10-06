"""What a stage synthesiser is handed: the plan, the working map, a stream."""

from __future__ import annotations

import random
from dataclasses import dataclass, field
from typing import Any

from .geometry import RESERVED, Field, Point, build_field
from .layout import Layout
from .names import Namer
from .request import GenerationRequest

GENERATED_KEY = "generated"

COMMANDERS = {
    "carthage": (
        "carthage_sword_commander",
        "carthage_spear_commander",
        "carthage_bow_commander",
    ),
    "roman_republic": (
        "roman_veteran_consul",
        "roman_field_commander",
        "roman_legion_organizer",
    ),
}


@dataclass
class Context:
    request: GenerationRequest
    params: dict[str, Any]
    layout: Layout
    map: dict[str, Any]
    namer: Namer
    width: int
    height: int
    rng: random.Random = field(default_factory=random.Random)
    stage: str = ""
    notes: list[str] = field(default_factory=list)
    locked: set[str] = field(default_factory=set)
    reservations: list[tuple[Point, float]] = field(default_factory=list)
    reserved_lines: list[tuple[list[Point], float]] = field(default_factory=list)

    @property
    def player_nation(self) -> str:
        return str(self.params["general.player_nation"])

    @property
    def enemy_nation(self) -> str:
        return "roman_republic" if self.player_nation == "carthage" else "carthage"

    def nation_of(self, player_id: int) -> str:
        return self.player_nation if player_id == 1 else self.enemy_nation

    def field(self, reserved: bool = True) -> Field:
        result = build_field(self.map, self.width, self.height)
        if reserved:
            for (x, z), r in self.reservations:
                result.mark_disc(x, z, r, RESERVED)
            for points, r in self.reserved_lines:
                result.mark_polyline(points, r, RESERVED)
        return result

    def tag(self, entry: dict[str, Any]) -> dict[str, Any]:
        entry[GENERATED_KEY] = self.stage
        return entry

    def add(self, key: str, entry: dict[str, Any]) -> dict[str, Any]:
        self.map.setdefault(key, []).append(self.tag(entry))
        return entry

    def world(self, u: float, v: float) -> Point:
        return self.layout.frame.to_world(u, v)

    def mirror(self, p: Point) -> Point:
        return (self.width - 1.0 - p[0], self.height - 1.0 - p[1])

    def note(self, text: str) -> None:
        self.notes.append(text)
