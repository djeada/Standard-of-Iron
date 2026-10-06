"""Runs a generation request through the stages and the map pipeline.

The order is fixed (``request.STAGES``). Each unlocked stage synthesises its
intent from its own seed, then the pipeline tool that materialises it runs:

    terrain      (intent only)
    water        -> generate-map-water.py
    settlements  (intent only; laid out after the roads exist)
    roads        -> generate-map-roads.py, then generate-map-settlements.py
    forests      (intent only)
    landmarks    -> generate-map-landmarks.py
    dressing     -> generate-map-dressing.py
    spawns       (intent only)
    placement    -> fix-map-prop-overlaps.py
    validation

A tool only runs when something it reads changed, so a stage whose inputs are
all locked keeps its output verbatim. A candidate that fails a hard check is
regenerated with fresh stage seeds, a bounded number of times.
"""

from __future__ import annotations

import copy
import random
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable

from . import GENERATOR, GENERATOR_VERSION
from .context import Context
from .geometry import dist
from .layout import build_layout
from .materialize import run_tool
from .names import Namer
from .provenance import (
    OUTPUT_ARRAYS,
    STAGE_ARRAYS,
    authored_ends,
    owning_stage,
    remove_stage,
    restore_structures,
    stash_hand_placed_structures,
    tag_outputs,
    tag_roads,
)
from .request import (
    STAGES,
    GenerationRequest,
    RequestError,
    derive_seed,
    layout_seed,
    resolve_locks,
    stage_seed,
)
from .root import fresh_map
from .schema import resolve_parameters
from .stages import (
    dressing,
    forests,
    landmarks,
    roads,
    settlements,
    spawns,
    terrain,
    water,
)
from .validate import validate

MAX_ATTEMPTS = 4

SYNTHESISERS: dict[str, Callable[[Context], None]] = {
    "terrain": terrain.synthesise,
    "water": water.synthesise,
    "settlements": settlements.synthesise,
    "roads": roads.synthesise,
    "forests": forests.synthesise,
    "landmarks": landmarks.synthesise,
    "dressing": dressing.synthesise,
    "spawns": spawns.synthesise,
}

Progress = Callable[[dict[str, Any]], None]


@dataclass
class Result:
    definition: dict[str, Any] | None
    report: dict[str, Any]

    @property
    def ok(self) -> bool:
        return bool(self.report.get("ok"))


@dataclass
class _StageRecord:
    id: str
    status: str
    seed: int | None = None
    seconds: float = 0.0
    forced_by: str | None = None
    notes: list[str] = field(default_factory=list)

    def to_json(self) -> dict[str, Any]:
        return {
            "id": self.id,
            "status": self.status,
            "seed": self.seed,
            "seconds": round(self.seconds, 2),
            "forced_by": self.forced_by,
            "notes": self.notes,
        }


def _emit(progress: Progress | None, event: dict[str, Any]) -> None:
    if progress is not None:
        progress(event)


def _existing_ids(definition: dict[str, Any]) -> set[str]:
    ids = set()
    for key in ("settlements", "landmarks", "dressing", "forests", "lakes"):
        for entry in definition.get(key) or []:
            if entry.get("id"):
                ids.add(str(entry["id"]))
    return ids


def _protected_hills(definition: dict[str, Any], locked: set[str]) -> list[dict]:
    """Generated hills a locked hilltop settlement stands on survive a terrain reroll."""
    if "settlements" not in locked:
        return []
    hills = []
    for s in definition.get("settlements") or []:
        if not s.get("on_hill"):
            continue
        for hill in definition.get("terrain") or []:
            if (
                hill.get("type") == "hill"
                and dist((hill["x"], hill["z"]), (s["x"], s["z"])) < 45.0
            ):
                hills.append(hill)
    return hills


def _prepare_base(base: dict[str, Any], locked: set[str]) -> dict[str, Any]:
    definition = copy.deepcopy(base)
    keep = _protected_hills(definition, locked)
    for stage in STAGES:
        if stage not in locked:
            remove_stage(definition, stage, keep=keep)
    return definition


def _layout_seed(
    request: GenerationRequest, base: dict | None, locked: set[str], attempt: int
) -> int:
    if base and locked:
        recorded = (base.get("generation") or {}).get("layout_seed")
        if isinstance(recorded, int):
            return recorded
    return layout_seed(request.seed, attempt)


def _reserve(ctx: Context) -> None:
    layout = ctx.layout
    for lane in range(len(layout.lanes)):
        ctx.reserved_lines.append((layout.lane_world(lane), 18.0))
    ctx.reservations.append((ctx.world(*layout.start), 45.0))
    main_radius = 70.0 if ctx.params["settlements.towns"] else 50.0
    if ctx.params["water.ring_moat"]:
        main_radius = 96.0
    ctx.reservations.append((ctx.world(*layout.enemy_main), main_radius))
    for link in layout.links:
        for lane in (link.lane_a, link.lane_b):
            ctx.reservations.append(
                (ctx.world(link.u, layout.lane_v(lane, link.u)), 40.0)
            )
        a = ctx.world(link.u, layout.lane_v(link.lane_a, link.u))
        b = ctx.world(link.u, layout.lane_v(link.lane_b, link.u))
        ctx.reserved_lines.append(([a, b], 14.0))


def _placement(
    definition: dict[str, Any], scratch: Path, regenerated: set[str], reroll: bool
):
    """Overlap correction that never moves what this run did not generate."""
    snapshots: dict[str, dict] = {}
    for array in (*OUTPUT_ARRAYS, "roads", "bridges"):
        for index, entry in enumerate(definition.get(array) or []):
            key = f"{array}:{index}"
            entry["_mapgen"] = key
            if reroll and owning_stage(entry) not in regenerated:
                snapshots[key] = copy.deepcopy(entry)
    updated, run = run_tool("placement", definition, scratch)
    for array in (*OUTPUT_ARRAYS, "roads", "bridges"):
        rebuilt = []
        for entry in updated.get(array) or []:
            key = entry.pop("_mapgen", None)
            if key in snapshots:
                entry = snapshots[key]
                entry.pop("_mapgen", None)
            rebuilt.append(entry)
        updated[array] = rebuilt
    return updated, run


def _attempt(
    request: GenerationRequest,
    params: dict[str, Any],
    base: dict[str, Any] | None,
    locked: set[str],
    forced: dict[str, str],
    attempt: int,
    scratch: Path,
    progress: Progress | None,
):
    width, height = int(request.width), int(request.height)
    if base is not None:
        definition = _prepare_base(base, locked)
        grid = definition.get("grid") or {}
        width = int(grid.get("width", width))
        height = int(grid.get("height", height))
    else:
        definition = fresh_map(width, height, params)

    lseed = _layout_seed(request, base, locked, attempt)
    layout = build_layout(random.Random(lseed), width, height, params)
    ctx = Context(
        request=request,
        params=params,
        layout=layout,
        map=definition,
        namer=Namer(_existing_ids(definition)),
        width=width,
        height=height,
        locked=set(locked),
    )
    _reserve(ctx)
    regenerated = {stage for stage in STAGES if stage not in locked}
    records: list[_StageRecord] = []
    stage_seeds: dict[str, int | None] = {}
    base_seeds = ((base or {}).get("generation") or {}).get("stage_seeds") or {}

    def materialise(tool: str, record: _StageRecord) -> None:
        nonlocal definition
        subject = definition
        rings: list[dict] = []
        if tool == "water":
            # The water tool validates every river as a polyline, and a ring
            # has no ends to put on an edge. Rings are exempt by design, so
            # they sit the tool out.
            rings = [
                r for r in definition.get("rivers") or [] if r.get("shape") == "ring"
            ]
            subject = {
                **definition,
                "rivers": [r for r in definition["rivers"] if r.get("shape") != "ring"],
            }
            if not subject["rivers"]:
                return
        stashed = stash_hand_placed_structures(subject) if tool == "settlements" else []
        authored_roads = authored_ends(subject.get("roads") or [])
        authored_bridges = authored_ends(subject.get("bridges") or [])
        updated, run = run_tool(tool, subject, scratch)
        record.seconds += run.seconds
        if not run.ok:
            record.status = "failed"
            record.notes.append(f"{tool} tool failed: " + " | ".join(run.lines(4)))
            return
        if rings:
            updated["rivers"] = list(updated.get("rivers") or []) + rings
        # Locked means verbatim. The settlement tool may grow or reshape a
        # hill to carry a settlement, or move a ramp out of a new ring; a
        # locked stage's arrays are put back, the change is reported, and the
        # validation gates decide whether the candidate still holds.
        for stage in locked:
            for array in STAGE_ARRAYS[stage]:
                before = subject.get(array) or []
                if (updated.get(array) or []) != before:
                    updated[array] = copy.deepcopy(before)
                    record.notes.append(
                        f"{tool} tool wanted to change locked {array}; kept the locked version"
                    )
        if tool == "settlements":
            restore_structures(updated, stashed)
        if tool == "roads":
            tag_roads(updated, authored_roads, authored_bridges)
        tag_outputs(updated, tool)
        definition = updated
        ctx.map = definition

    for stage in STAGES:
        record = _StageRecord(stage, "locked", forced_by=forced.get(stage))
        if stage in locked:
            stage_seeds[stage] = base_seeds.get(stage)
            records.append(record)
            _emit(
                progress,
                {
                    "event": "stage",
                    "stage": stage,
                    "status": "locked",
                    "attempt": attempt,
                },
            )
            # Locked intent is still laid out again when what it stands on changed.
            if stage == "roads" and regenerated & {"terrain", "water"}:
                materialise("settlements", record)
            if stage == "landmarks" and regenerated & {"terrain", "water", "forests"}:
                materialise("landmarks", record)
            continue
        _emit(
            progress,
            {"event": "stage", "stage": stage, "status": "running", "attempt": attempt},
        )
        started = time.monotonic()
        seed = stage_seed(request.seed, stage, attempt)
        stage_seeds[stage] = seed
        record.status = "done"
        record.seed = seed
        ctx.stage = stage
        ctx.rng = random.Random(seed)
        ctx.notes = []
        try:
            SYNTHESISERS[stage](ctx)
        except Exception as error:  # a synthesiser bug must not lose the run
            record.status = "failed"
            ctx.notes.append(f"synthesis failed: {type(error).__name__}: {error}")
        definition = ctx.map
        record.notes.extend(ctx.notes)
        synth_seconds = time.monotonic() - started
        if record.status == "done":
            if stage == "water" and definition.get("rivers"):
                materialise("water", record)
            elif stage == "roads":
                if definition.get("roads"):
                    materialise("roads", record)
                if definition.get("settlements") and record.status == "done":
                    materialise("settlements", record)
            elif stage == "landmarks" and definition.get("landmarks"):
                materialise("landmarks", record)
            elif stage == "dressing" and definition.get("dressing"):
                materialise("dressing", record)
        record.seconds += synth_seconds
        records.append(record)
        _emit(
            progress,
            {
                "event": "stage",
                "stage": stage,
                "status": "done" if record.status == "done" else "failed",
                "seconds": round(record.seconds, 2),
                "attempt": attempt,
            },
        )

    _emit(
        progress,
        {
            "event": "stage",
            "stage": "placement",
            "status": "running",
            "attempt": attempt,
        },
    )
    definition, placement = _placement(
        definition, scratch, regenerated, base is not None
    )
    _emit(
        progress,
        {
            "event": "stage",
            "stage": "placement",
            "status": "done" if placement.ok else "failed",
            "seconds": round(placement.seconds, 2),
            "attempt": attempt,
        },
    )
    placement_record = _StageRecord(
        "placement", "done" if placement.ok else "failed", seconds=placement.seconds
    )
    placement_record.notes = [line for line in placement.lines(3)]

    if base is None:
        river = next(
            (r.get("name") for r in definition.get("rivers") or [] if r.get("name")),
            None,
        )
        main = next(
            (
                s.get("name")
                for s in definition.get("settlements") or []
                if s.get("player_id") == 2
            ),
            None,
        )
        definition["name"] = Namer.map_name(
            random.Random(derive_seed(request.seed, "name")), river, main
        )

    definition["generation"] = {
        "version": GENERATOR_VERSION,
        "generator": GENERATOR,
        "seed": int(request.seed),
        "preset": request.preset,
        "width": width,
        "height": height,
        "parameters": params,
        "stage_seeds": stage_seeds,
        "locks": [stage for stage in STAGES if stage in locked],
        "layout_seed": lseed,
        "attempt": attempt,
    }
    return definition, records + [placement_record], layout


def generate(
    request: GenerationRequest,
    base: dict[str, Any] | None = None,
    progress: Progress | None = None,
    max_attempts: int = MAX_ATTEMPTS,
) -> Result:
    request.validate()
    params = resolve_parameters(request.preset, request.parameters)
    if base is None and request.locks:
        raise RequestError("--lock needs --base: there is nothing to keep")
    locked, forced = resolve_locks(request.locks) if base is not None else (set(), {})
    started = time.monotonic()
    attempts: list[dict[str, Any]] = []
    definition = None
    records: list[_StageRecord] = []
    report_checks = None
    with tempfile.TemporaryDirectory(prefix="soi-mapgen-") as tmp:
        scratch = Path(tmp)
        for attempt in range(max_attempts):
            definition, records, layout = _attempt(
                request, params, base, locked, forced, attempt, scratch, progress
            )
            _emit(
                progress,
                {
                    "event": "stage",
                    "stage": "validation",
                    "status": "running",
                    "attempt": attempt,
                },
            )
            report_checks = validate(
                definition,
                int(definition["grid"]["width"]),
                int(definition["grid"]["height"]),
                params,
                layout.sectors,
                scratch,
            )
            _emit(
                progress,
                {
                    "event": "stage",
                    "stage": "validation",
                    "status": "done" if report_checks.ok else "failed",
                    "attempt": attempt,
                },
            )
            attempts.append({"attempt": attempt, "failures": report_checks.failures()})
            if report_checks.ok:
                break
    assert report_checks is not None and definition is not None
    report_checks.metrics["seconds"] = round(time.monotonic() - started, 2)
    report_checks.metrics["attempts"] = len(attempts)
    request_json = request.to_json()
    request_json["parameters"] = params
    request_json["locks"] = [stage for stage in STAGES if stage in locked]
    report = {
        "version": GENERATOR_VERSION,
        "ok": report_checks.ok,
        "request": request_json,
        "stages": [record.to_json() for record in records],
        "attempts": attempts,
        "checks": report_checks.checks,
        "metrics": report_checks.metrics,
    }
    return Result(definition, report)
