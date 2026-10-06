"""Generation parameters, presets and the ``--describe`` schema.

Parameters are flat dotted keys (``water.rivers``). Each one carries the range
and wording the editor shows, so the Generator tab is built from this table
rather than from a second copy of it.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from . import GENERATOR_VERSION
from .request import FORCED_BY, STAGE_LABELS, STAGES, RequestError

DATA_DIR = Path(__file__).resolve().parent / "data"
PRESET_DIR = Path(__file__).resolve().parent / "presets"


def _float(key, stage, label, lo, hi, default, step, help_text):
    return {
        "key": key,
        "stage": stage,
        "label": label,
        "type": "float",
        "min": lo,
        "max": hi,
        "step": step,
        "default": default,
        "help": help_text,
    }


def _int(key, stage, label, lo, hi, default, help_text):
    return {
        "key": key,
        "stage": stage,
        "label": label,
        "type": "int",
        "min": lo,
        "max": hi,
        "default": default,
        "help": help_text,
    }


def _bool(key, stage, label, default, help_text):
    return {
        "key": key,
        "stage": stage,
        "label": label,
        "type": "bool",
        "default": default,
        "help": help_text,
    }


def _choice(key, stage, label, choices, default, help_text):
    return {
        "key": key,
        "stage": stage,
        "label": label,
        "type": "choice",
        "choices": [{"id": cid, "label": clabel} for cid, clabel in choices],
        "default": default,
        "help": help_text,
    }


def load_biomes() -> dict[str, dict]:
    return json.loads((DATA_DIR / "biomes.json").read_text(encoding="utf-8"))


def _parameters() -> list[dict[str, Any]]:
    biomes = load_biomes()
    return [
        _choice(
            "general.biome",
            "general",
            "Biome",
            [(key, value["label"]) for key, value in biomes.items()],
            "temperate",
            "Ground, vegetation, light and weather, taken from a shipped map.",
        ),
        _choice(
            "general.player_nation",
            "general",
            "Player nation",
            [("carthage", "Carthage"), ("roman_republic", "Rome")],
            "carthage",
            "The local player's nation. The opposing side is the other one.",
        ),
        _int(
            "tactical.sectors",
            "tactical",
            "Sectors",
            3,
            5,
            4,
            "Battle basins the field is cut into. Each boundary is a river, "
            "a ridge, a wood or open ground.",
        ),
        _float(
            "tactical.choke_intensity",
            "tactical",
            "Chokes",
            0.0,
            1.0,
            0.5,
            0.05,
            "How much of each sector boundary is closed. High values leave only "
            "the crossings open.",
        ),
        _float(
            "tactical.flank_bias",
            "tactical",
            "Flanking",
            0.0,
            1.0,
            0.5,
            0.05,
            "How far apart the two strategic routes run. Wide routes make "
            "independent flanks; narrow ones a single front.",
        ),
        _float(
            "tactical.formation_space",
            "tactical",
            "Formation space",
            0.0,
            1.0,
            0.5,
            0.05,
            "Open ground to keep for deploying formations. Raising it thins "
            "hills and woods.",
        ),
        _bool(
            "tactical.symmetric",
            "tactical",
            "Mirror layout",
            False,
            "Mirror hills, woods and lakes through the map centre so both sides "
            "face the same ground.",
        ),
        _float(
            "terrain.relief",
            "terrain",
            "Relief",
            0.0,
            1.0,
            0.5,
            0.05,
            "Size and height of hills.",
        ),
        _float(
            "terrain.hills",
            "terrain",
            "Hills",
            0.0,
            1.0,
            0.5,
            0.05,
            "How many defended hills stand beside the routes.",
        ),
        _float(
            "terrain.mountains",
            "terrain",
            "Mountains",
            0.0,
            1.0,
            0.0,
            0.05,
            "Mountain masses on the flanks. High values make a pass.",
        ),
        _float(
            "terrain.ridge_bias",
            "terrain",
            "Ridges",
            0.0,
            1.0,
            0.4,
            0.05,
            "Share of hills shaped as ridges, boomerangs, elbows and rings "
            "rather than mounds; also how often a sector boundary is a ridge.",
        ),
        _float(
            "terrain.roughness",
            "terrain",
            "Roughness",
            0.0,
            1.0,
            0.4,
            0.05,
            "Low-level unevenness of the open ground.",
        ),
        _int(
            "water.rivers",
            "water",
            "Rivers",
            0,
            3,
            1,
            "Rivers used as sector boundaries.",
        ),
        _int(
            "water.lakes",
            "water",
            "Lakes",
            0,
            6,
            1,
            "Lakes. The first ones sit at the map edge and close a flank.",
        ),
        _float(
            "water.river_width",
            "water",
            "River width",
            6.0,
            24.0,
            12.0,
            1.0,
            "Typical channel width; each river varies a little around it.",
        ),
        _int(
            "water.crossings",
            "water",
            "Crossings",
            1,
            4,
            2,
            "Bridges over each full river barrier. One makes a hold-the-bridge map.",
        ),
        _float(
            "water.edge_to_edge",
            "water",
            "Edge to edge",
            0.0,
            1.0,
            0.8,
            0.05,
            "Chance a river crosses the whole map rather than ending in a lake.",
        ),
        _bool(
            "water.coast",
            "water",
            "Coast",
            False,
            "A long lake along one flank, standing in for the sea.",
        ),
        _bool(
            "water.ring_moat",
            "water",
            "Moat",
            False,
            "Ring the main enemy settlement with water crossed by bridges.",
        ),
        _int(
            "roads.routes",
            "roads",
            "Main routes",
            1,
            3,
            2,
            "Edge-to-edge strategic routes.",
        ),
        _int(
            "roads.loops",
            "roads",
            "Cross links",
            1,
            3,
            1,
            "Roads between the routes (or round a single route). Each one closes "
            "a loop and opens a flank; a standard map needs at least one.",
        ),
        _float(
            "roads.secondary",
            "roads",
            "Side roads",
            0.0,
            1.0,
            0.4,
            0.05,
            "Spurs to defended hills; every one ends at a hill entrance.",
        ),
        _float(
            "roads.width",
            "roads",
            "Road width",
            3.0,
            6.0,
            3.6,
            0.2,
            "Main road width. Side roads are narrower.",
        ),
        _int(
            "settlements.towns",
            "settlements",
            "Towns",
            0,
            3,
            1,
            "Walled towns with a citadel.",
        ),
        _int(
            "settlements.forts",
            "settlements",
            "Forts",
            0,
            4,
            1,
            "Fortified camps.",
        ),
        _int(
            "settlements.camps",
            "settlements",
            "Camps",
            0,
            3,
            0,
            "Marching camps besides the player's own start camp.",
        ),
        _float(
            "settlements.plan_variety",
            "settlements",
            "Plan variety",
            0.0,
            1.0,
            0.6,
            0.05,
            "Share of settlements laid out as circles, stars, twins and steps "
            "instead of rectangles.",
        ),
        _float(
            "settlements.spacing",
            "settlements",
            "Spacing",
            80.0,
            260.0,
            150.0,
            10.0,
            "Minimum distance between settlements.",
        ),
        _float(
            "settlements.hilltop",
            "settlements",
            "Hilltop",
            0.0,
            1.0,
            0.2,
            0.05,
            "Chance a fort or camp is raised on its own hill.",
        ),
        _bool(
            "settlements.farmland",
            "settlements",
            "Farmland",
            True,
            "Worked fields outside towns and forts.",
        ),
        _bool(
            "settlements.walled_start",
            "settlements",
            "Walled start",
            True,
            "Give the player's start camp a rampart.",
        ),
        _float(
            "forests.density",
            "forests",
            "Density",
            0.0,
            1.0,
            0.5,
            0.05,
            "How many woods grow.",
        ),
        _float(
            "forests.size",
            "forests",
            "Size",
            0.0,
            1.0,
            0.5,
            0.05,
            "Typical wood radius.",
        ),
        _float(
            "forests.ambush",
            "forests",
            "Ambush woods",
            0.0,
            1.0,
            0.4,
            0.05,
            "Share of woods grown beside the roads rather than on the flanks.",
        ),
        _float(
            "forests.clearance",
            "forests",
            "Settlement clearance",
            10.0,
            80.0,
            35.0,
            5.0,
            "Ground kept clear of woods around settlements.",
        ),
        _int(
            "landmarks.count",
            "landmarks",
            "Landmarks",
            0,
            8,
            3,
            "Sanctuaries, shrines, hamlets and watch posts on the side routes.",
        ),
        _float(
            "landmarks.spacing",
            "landmarks",
            "Spacing",
            60.0,
            260.0,
            120.0,
            10.0,
            "Minimum distance between landmarks.",
        ),
        _float(
            "landmarks.guarded",
            "landmarks",
            "Guarded",
            0.0,
            1.0,
            0.4,
            0.05,
            "Share of landmarks held by an enemy picket - side objectives.",
        ),
        _float(
            "dressing.density",
            "dressing",
            "Density",
            0.0,
            1.0,
            0.5,
            0.05,
            "Bridgeheads, ramps, junctions and gate approaches dressed.",
        ),
        _float(
            "dressing.scatter",
            "dressing",
            "Scatter",
            0.0,
            1.0,
            0.5,
            0.05,
            "Copses, orchards and stones scattered over open ground.",
        ),
        _int(
            "spawns.army",
            "spawns",
            "Starting army",
            3,
            20,
            8,
            "Troops the player starts with beside the commander.",
        ),
        _float(
            "spawns.garrison",
            "spawns",
            "Garrisons",
            0.0,
            1.0,
            0.5,
            0.05,
            "How strongly enemy settlements are held at the start.",
        ),
    ]


PARAMETERS: list[dict[str, Any]] = _parameters()
PARAMETER_BY_KEY: dict[str, dict[str, Any]] = {p["key"]: p for p in PARAMETERS}


def load_presets() -> dict[str, dict[str, Any]]:
    presets: dict[str, dict[str, Any]] = {}
    for path in sorted(PRESET_DIR.glob("*.json")):
        preset = json.loads(path.read_text(encoding="utf-8"))
        preset_id = preset.get("id") or path.stem
        preset["id"] = preset_id
        presets[preset_id] = preset
    return dict(sorted(presets.items(), key=lambda item: item[1].get("order", 99)))


def coerce(key: str, value: Any) -> Any:
    """Validate one parameter value against its schema entry."""
    spec = PARAMETER_BY_KEY.get(key)
    if spec is None:
        raise RequestError(f"unknown parameter '{key}'")
    kind = spec["type"]
    try:
        if kind == "bool":
            if isinstance(value, str):
                lowered = value.strip().lower()
                if lowered not in {"true", "false", "1", "0", "yes", "no"}:
                    raise ValueError(value)
                return lowered in {"true", "1", "yes"}
            return bool(value)
        if kind == "choice":
            ids = [choice["id"] for choice in spec["choices"]]
            if str(value) not in ids:
                raise ValueError(f"expected one of {', '.join(ids)}")
            return str(value)
        if kind == "int":
            number = int(float(value))
            if float(value) != number:
                raise ValueError("expected an integer")
        else:
            number = round(float(value), 6)
    except (TypeError, ValueError) as error:
        raise RequestError(f"bad value for '{key}': {value!r} ({error})") from error
    if not spec["min"] <= number <= spec["max"]:
        raise RequestError(
            f"'{key}' must be in {spec['min']}..{spec['max']}, got {number}"
        )
    return number


def resolve_parameters(preset_id: str, overrides: dict[str, Any]) -> dict[str, Any]:
    """Schema defaults, then the preset, then the request's own values."""
    presets = load_presets()
    if preset_id not in presets:
        raise RequestError(f"unknown preset '{preset_id}'; known: {', '.join(presets)}")
    resolved = {spec["key"]: spec["default"] for spec in PARAMETERS}
    for key, value in presets[preset_id].get("parameters", {}).items():
        resolved[key] = coerce(key, value)
    for key, value in overrides.items():
        resolved[key] = coerce(key, value)
    return dict(sorted(resolved.items()))


def describe() -> dict[str, Any]:
    presets = load_presets()
    return {
        "version": GENERATOR_VERSION,
        "stages": [
            {
                "id": stage,
                "label": STAGE_LABELS[stage],
                "forced_by": list(FORCED_BY[stage]),
            }
            for stage in STAGES
        ],
        "parameters": PARAMETERS,
        "presets": [
            {
                "id": preset_id,
                "label": preset["label"],
                "description": preset.get("description", ""),
                "width": preset.get("width", 650),
                "height": preset.get("height", 650),
                "parameters": preset.get("parameters", {}),
            }
            for preset_id, preset in presets.items()
        ],
    }
