"""The map generator: determinism, locks, provenance and the structural checks.

Generation runs the real map pipeline, about ten seconds a map at the sizes
used here, so the slow cases share a few generated maps. Set
SOI_MAPGEN_SOAK=1 to also sweep every preset at full size.
"""

from __future__ import annotations

import copy
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))

from mapgen import schema  # noqa: E402
from mapgen.orchestrator import generate  # noqa: E402
from mapgen.request import (  # noqa: E402
    FORCED_BY,
    STAGES,
    GenerationRequest,
    RequestError,
    derive_seed,
    resolve_locks,
    stage_seed,
)

SMALL = 380
EARLY_ARRAYS = ("terrain", "rivers", "lakes")
LOCKABLE_ARRAYS = ("terrain", "rivers", "lakes", "settlements", "roads", "bridges")


def _request(
    preset: str = "river_crossing", seed: int = 1, **parameters
) -> GenerationRequest:
    return GenerationRequest(
        seed=seed, preset=preset, width=SMALL, height=SMALL, parameters=dict(parameters)
    )


def _dump(definition: dict) -> str:
    return json.dumps(definition, indent=2, sort_keys=True)


class RequestModelTest(unittest.TestCase):
    def test_stage_seeds_follow_the_hash_rule(self) -> None:
        import hashlib

        digest = hashlib.sha256(b"73948291:water").digest()
        expected = int.from_bytes(digest[:8], "big") & ((1 << 53) - 1)
        self.assertEqual(stage_seed(73948291, "water"), expected)
        self.assertNotEqual(stage_seed(1, "water"), stage_seed(1, "roads"))
        self.assertNotEqual(stage_seed(1, "water"), stage_seed(1, "water", attempt=1))
        self.assertLess(derive_seed("x"), 1 << 53)

    def test_request_round_trips_through_json(self) -> None:
        request = GenerationRequest(
            seed=5,
            preset="siege_town",
            width=500,
            height=420,
            parameters={"water.rivers": 2},
            locks=["water", "terrain"],
        )
        again = GenerationRequest.from_json(json.loads(json.dumps(request.to_json())))
        self.assertEqual(again.to_json(), request.to_json())
        self.assertEqual(again.to_json()["locks"], ["terrain", "water"])

    def test_bad_requests_are_refused(self) -> None:
        with self.assertRaises(RequestError):
            GenerationRequest(seed=-1, preset="open_plains").validate()
        with self.assertRaises(RequestError):
            GenerationRequest(seed=1, preset="open_plains", width=50).validate()
        with self.assertRaises(RequestError):
            GenerationRequest(seed=1, preset="open_plains", locks=["sky"]).validate()
        with self.assertRaises(RequestError):
            schema.resolve_parameters("no_such_preset", {})
        with self.assertRaises(RequestError):
            schema.resolve_parameters("open_plains", {"water.rivers": 9})
        with self.assertRaises(RequestError):
            schema.resolve_parameters("open_plains", {"water.nonsense": 1})
        with self.assertRaises(RequestError):
            generate(GenerationRequest(seed=1, preset="open_plains", locks=["terrain"]))

    def test_forced_stages_follow_their_sources(self) -> None:
        locked, forced = resolve_locks(["roads", "terrain"])
        self.assertEqual(locked, {"terrain"})
        self.assertEqual(forced, {"roads": "settlements"})
        locked, forced = resolve_locks(["settlements", "roads", "dressing"])
        self.assertEqual(locked, {"settlements", "roads"})
        self.assertEqual(forced["dressing"], "terrain")
        locked, _ = resolve_locks(STAGES)
        self.assertEqual(locked, set(STAGES))
        for stage, sources in FORCED_BY.items():
            for source in sources:
                self.assertLess(STAGES.index(source), STAGES.index(stage))


class DescribeTest(unittest.TestCase):
    def test_schema_is_consistent(self) -> None:
        described = schema.describe()
        self.assertEqual([s["id"] for s in described["stages"]], list(STAGES))
        stage_ids = set(STAGES) | {"general", "tactical"}
        keys = set()
        for parameter in described["parameters"]:
            self.assertIn(parameter["stage"], stage_ids)
            self.assertNotIn(parameter["key"], keys)
            keys.add(parameter["key"])
            self.assertTrue(parameter["label"])
            if parameter["type"] in ("int", "float"):
                self.assertLessEqual(parameter["min"], parameter["default"])
                self.assertLessEqual(parameter["default"], parameter["max"])
            if parameter["type"] == "choice":
                self.assertIn(
                    parameter["default"], [c["id"] for c in parameter["choices"]]
                )
        self.assertEqual(len(described["presets"]), 10)
        for preset in described["presets"]:
            self.assertTrue(preset["label"] and preset["description"])
            resolved = schema.resolve_parameters(preset["id"], {})
            self.assertEqual(set(resolved), keys)

    def test_describe_command_prints_json(self) -> None:
        result = subprocess.run(
            [sys.executable, str(SCRIPTS / "soi-mapgen.py"), "--describe"],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["version"], 1)

    def test_command_rejects_a_lock_without_a_base(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPTS / "soi-mapgen.py"),
                    "--preset",
                    "open_plains",
                    "--seed",
                    "1",
                    "--lock",
                    "terrain",
                    "--output",
                    str(Path(tmp) / "m.json"),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
        self.assertEqual(result.returncode, 2)


class GeneratedMapTest(unittest.TestCase):
    """Shares one generated map between the slow assertions."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.result = generate(_request())
        cls.definition = cls.result.definition

    def test_generation_is_deterministic(self) -> None:
        again = generate(_request())
        self.assertEqual(_dump(again.definition), _dump(self.definition))

    def test_candidate_passes_every_hard_check(self) -> None:
        failures = [c for c in self.result.report["checks"] if c["status"] == "fail"]
        self.assertTrue(self.result.ok, failures)
        self.assertEqual(self.result.report["metrics"]["out_of_bounds"], 0)
        self.assertGreaterEqual(self.result.report["metrics"]["route_loops"], 1)

    def test_nothing_lies_outside_the_map(self) -> None:
        limit = SMALL - 1 + 0.6
        for key in (
            "terrain",
            "lakes",
            "forests",
            "settlements",
            "structures",
            "spawns",
            "world_props",
        ):
            for entry in self.definition.get(key) or []:
                points = [
                    (entry[k][0], entry[k][1]) for k in ("start", "end") if k in entry
                ]
                if "x" in entry:
                    points.append((entry["x"], entry["z"]))
                self.assertTrue(points, (key, entry))
                for x, z in points:
                    self.assertTrue(
                        -0.6 <= x <= limit and -0.6 <= z <= limit, (key, entry)
                    )

    def test_metadata_reproduces_the_request(self) -> None:
        generation = self.definition["generation"]
        self.assertEqual(generation["seed"], 1)
        self.assertEqual(generation["preset"], "river_crossing")
        self.assertEqual(generation["generator"], "soi_mapgen")
        self.assertEqual(
            generation["parameters"], schema.resolve_parameters("river_crossing", {})
        )
        request = GenerationRequest.from_json(generation)
        self.assertEqual(request.width, SMALL)

    def test_save_and_reload_is_stable(self) -> None:
        text = _dump(self.definition)
        self.assertEqual(_dump(json.loads(text)), text)

    def test_generated_elements_carry_provenance(self) -> None:
        for key, stage in (
            ("terrain", "terrain"),
            ("rivers", "water"),
            ("settlements", "settlements"),
            ("roads", "roads"),
            ("bridges", "roads"),
            ("forests", "forests"),
        ):
            for entry in self.definition.get(key) or []:
                self.assertEqual(entry.get("generated"), stage, (key, entry))
        built = [s for s in self.definition["structures"] if s.get("settlement")]
        self.assertTrue(built)
        self.assertTrue(all(s.get("generated") == "settlements" for s in built))

    def test_a_later_stage_change_leaves_earlier_stages_alone(self) -> None:
        changed = generate(
            _request(**{"dressing.scatter": 0.9, "forests.density": 0.9})
        )
        for key in EARLY_ARRAYS + ("settlements",):
            self.assertEqual(changed.definition[key], self.definition[key], key)

    def test_locked_stages_survive_a_reroll(self) -> None:
        request = _request(seed=2)
        request.locks = ["terrain", "water", "settlements", "roads"]
        rerolled = generate(request, base=copy.deepcopy(self.definition))
        self.assertTrue(
            rerolled.ok, [c for c in rerolled.report["checks"] if c["status"] == "fail"]
        )
        for key in LOCKABLE_ARRAYS:
            self.assertEqual(rerolled.definition[key], self.definition[key], key)
        built_before = [s for s in self.definition["structures"] if s.get("settlement")]
        built_after = [
            s for s in rerolled.definition["structures"] if s.get("settlement")
        ]
        self.assertEqual(built_after, built_before)
        statuses = {s["id"]: s["status"] for s in rerolled.report["stages"]}
        self.assertEqual(statuses["terrain"], "locked")
        self.assertEqual(statuses["forests"], "done")

    def test_reroll_keeps_authored_work_and_reports_forcing(self) -> None:
        base = copy.deepcopy(self.definition)
        authored_hill = {
            "type": "hill",
            "x": 30.0,
            "z": 30.0,
            "width": 24.0,
            "depth": 18.0,
            "height": 2.0,
        }
        authored_prop = {
            "type": "statue",
            "x": 25.0,
            "z": 350.0,
            "scale": 1.0,
            "rotation": 0.0,
        }
        authored_building = {"type": "home", "x": 350.0, "z": 25.0, "player_id": 0}
        base["terrain"].append(authored_hill)
        base["world_props"].append(authored_prop)
        base["structures"].append(authored_building)
        request = _request(seed=3)
        request.locks = ["water", "roads"]
        rerolled = generate(request, base=base)
        definition = rerolled.definition
        self.assertIn(authored_hill, definition["terrain"])
        self.assertIn(authored_prop, definition["world_props"])
        self.assertIn(authored_building, definition["structures"])
        self.assertEqual(definition["rivers"], self.definition["rivers"])
        roads = next(s for s in rerolled.report["stages"] if s["id"] == "roads")
        self.assertEqual(roads["forced_by"], "settlements")
        self.assertEqual(roads["status"], "done")
        self.assertNotEqual(
            [h for h in definition["terrain"] if h.get("generated")],
            [h for h in self.definition["terrain"] if h.get("generated")],
        )


class PresetTest(unittest.TestCase):
    """Every preset produces a valid battlefield for a fixed seed."""

    PRESETS = ("open_plains", "mountain_pass", "siege_town")

    def test_fixed_seeds_pass_every_hard_check(self) -> None:
        soak = os.environ.get("SOI_MAPGEN_SOAK") == "1"
        presets = (
            [p["id"] for p in schema.describe()["presets"]] if soak else self.PRESETS
        )
        for index, preset in enumerate(presets):
            with self.subTest(preset=preset):
                request = GenerationRequest(seed=400 + index, preset=preset)
                if not soak:
                    request.width = request.height = 460
                result = generate(request)
                failures = [c for c in result.report["checks"] if c["status"] == "fail"]
                self.assertTrue(result.ok, failures)


if __name__ == "__main__":
    unittest.main()
