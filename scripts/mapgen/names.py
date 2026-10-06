"""Deterministic place names from small data-driven pools.

Names are drawn from the stage's own random stream, so the same seed names the
same places, and they are stored on the intent entries as plain data: nothing
at runtime depends on a random name.
"""

from __future__ import annotations

import json
import random
import re
from pathlib import Path

DATA = json.loads(
    (Path(__file__).resolve().parent / "data" / "names.json").read_text(
        encoding="utf-8"
    )
)


def slug(name: str) -> str:
    text = re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")
    return text or "place"


class Namer:
    """Hands out unique names and ids for one map."""

    def __init__(self, used_ids: set[str] | None = None) -> None:
        self.used_ids = set(used_ids or ())
        self.used_names: set[str] = set()

    def _unique(self, rng: random.Random, pool: list[str]) -> str:
        fresh = [name for name in pool if name not in self.used_names]
        name = rng.choice(fresh or pool)
        if name in self.used_names:
            suffix = 2
            while f"{name} {suffix}" in self.used_names:
                suffix += 1
            name = f"{name} {suffix}"
        self.used_names.add(name)
        return name

    def make_id(self, prefix: str, name: str) -> str:
        base = f"{prefix}_{slug(name)}"
        candidate, n = base, 2
        while candidate in self.used_ids:
            candidate = f"{base}_{n}"
            n += 1
        self.used_ids.add(candidate)
        return candidate

    def settlement(self, rng: random.Random, nation: str) -> str:
        pool = DATA["settlements"].get(nation) or DATA["settlements"]["roman_republic"]
        return self._unique(rng, pool)

    def river(self, rng: random.Random) -> str:
        return self._unique(rng, DATA["rivers"])

    def forest(self, rng: random.Random) -> str:
        pool = [
            f"{p} {r}"
            for p in DATA["forests"]["prefix"]
            for r in DATA["forests"]["root"]
        ]
        return self._unique(rng, pool)

    def landmark(self, rng: random.Random, kind: str, nation: str) -> str:
        deities = DATA["deities"].get(nation) or DATA["deities"]["roman_republic"]
        pool = [
            template.format(deity=deity, place=place)
            for template in DATA["landmarks"].get(kind, ["{place}"])
            for deity in deities
            for place in DATA["places"]
        ]
        return self._unique(rng, sorted(set(pool)))

    @staticmethod
    def map_name(rng: random.Random, river: str | None, settlement: str | None) -> str:
        first = river or settlement or "Nameless"
        return f"{first} {rng.choice(DATA['maps']['second'])}"
