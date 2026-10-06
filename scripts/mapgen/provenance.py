"""Who made each element, and what a reroll may remove.

The generator marks everything it authors with ``"generated": "<stage>"``.
An element without the mark is authored: rerolls leave it alone and the
synthesisers treat it as an obstacle. The materialisers keep their own
ownership keys (``settlement``, ``landmark``, ``dressing``); output that
belongs to a generated intent entry gains the ``generated`` mark too.
"""

from __future__ import annotations

from typing import Any, Iterable

from .context import GENERATED_KEY
from .geometry import dist

# Arrays a stage owns outright.
STAGE_ARRAYS: dict[str, tuple[str, ...]] = {
    "terrain": ("terrain",),
    "water": ("rivers", "lakes"),
    "settlements": ("settlements",),
    "roads": ("roads", "bridges"),
    "forests": ("forests",),
    "landmarks": ("landmarks",),
    "dressing": ("dressing",),
    "spawns": ("spawns",),
}
# Materialiser output, found by the key the tool writes on it.
OUTPUT_KEYS: dict[str, tuple[str, tuple[str, ...]]] = {
    "settlements": ("settlement", ("structures",)),
    "landmarks": ("landmark", ("structures", "world_props", "spawns")),
    "dressing": ("dressing", ("world_props",)),
}
OUTPUT_ARRAYS = ("structures", "world_props", "spawns")
OWNERSHIP_KEYS = ("settlement", "landmark", "dressing")


def is_generated(entry: dict[str, Any], stage: str | None = None) -> bool:
    mark = entry.get(GENERATED_KEY)
    return mark is not None and (stage is None or mark == stage)


def owning_stage(entry: dict[str, Any]) -> str | None:
    mark = entry.get(GENERATED_KEY)
    if isinstance(mark, str):
        return mark
    return None


def remove_stage(
    definition: dict[str, Any], stage: str, keep: Iterable[dict] = ()
) -> int:
    """Drop the elements a stage generated; authored ones stay."""
    keep_ids = {id(entry) for entry in keep}
    removed = 0
    arrays = set(STAGE_ARRAYS[stage])
    if stage in OUTPUT_KEYS:
        arrays.update(OUTPUT_KEYS[stage][1])
    for key in arrays:
        entries = definition.get(key) or []
        kept = [e for e in entries if not is_generated(e, stage) or id(e) in keep_ids]
        removed += len(entries) - len(kept)
        definition[key] = kept
    return removed


def tag_outputs(definition: dict[str, Any], stage: str) -> None:
    """Mark materialiser output whose intent entry the generator authored."""
    if stage not in OUTPUT_KEYS:
        return
    key, arrays = OUTPUT_KEYS[stage]
    intent_key = {
        "settlements": "settlements",
        "landmarks": "landmarks",
        "dressing": "dressing",
    }[stage]
    generated_ids = {
        entry.get("id")
        for entry in definition.get(intent_key) or []
        if is_generated(entry, stage)
    }
    for array in arrays:
        for entry in definition.get(array) or []:
            if entry.get(key) in generated_ids:
                entry[GENERATED_KEY] = stage


def _endpoints(
    entry: dict[str, Any],
) -> tuple[tuple[float, float], tuple[float, float]] | None:
    start, end = entry.get("start"), entry.get("end")
    if not start or not end:
        return None
    return (float(start[0]), float(start[1])), (float(end[0]), float(end[1]))


def _matches(a, b, tolerance: float) -> bool:
    return (dist(a[0], b[0]) <= tolerance and dist(a[1], b[1]) <= tolerance) or (
        dist(a[0], b[1]) <= tolerance and dist(a[1], b[0]) <= tolerance
    )


def tag_roads(
    definition: dict[str, Any], authored_roads: list, authored_bridges: list
) -> None:
    """The road tool rewrites both arrays; keep authored entries unmarked.

    Authored roads are recognised by their endpoints, which the tool keeps
    (it only nudges a map-edge endpoint one cell inside the map). Everything
    else - the generator's routes and the tool's own connectors and bridges -
    is generated.
    """
    for entry in definition.get("roads") or []:
        if entry.get(GENERATED_KEY) is not None:
            continue
        ends = _endpoints(entry)
        if ends and any(_matches(ends, a, 4.0) for a in authored_roads):
            continue
        entry[GENERATED_KEY] = "roads"
    for entry in definition.get("bridges") or []:
        ends = _endpoints(entry)
        if ends and any(_matches(ends, a, 4.0) for a in authored_bridges):
            entry.pop(GENERATED_KEY, None)
            continue
        entry[GENERATED_KEY] = "roads"


def authored_ends(entries: list[dict[str, Any]]) -> list:
    return [ends for e in entries if not is_generated(e) and (ends := _endpoints(e))]


def stash_hand_placed_structures(definition: dict[str, Any]) -> list[dict[str, Any]]:
    """Structures the settlement tool would delete: placed by hand, unowned."""
    stashed = [
        entry
        for entry in definition.get("structures") or []
        if not any(entry.get(k) is not None for k in OWNERSHIP_KEYS)
        and not entry.get("authored")
    ]
    return stashed


def restore_structures(
    definition: dict[str, Any], stashed: list[dict[str, Any]]
) -> None:
    """Put back what the settlement tool dropped; it never keeps these."""
    definition.setdefault("structures", []).extend(stashed)
