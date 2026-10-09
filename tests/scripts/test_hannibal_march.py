"""The historical march in hannibal_path.json that the campaign map film draws.

The generator owns two things: the eight campaign mission lines the game
draws, which must not change, and the named march the film reveals stop by
stop. These checks hold the committed asset to the generator.
"""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "map_pipeline"))

import hannibal_path  # noqa: E402

ASSET = ROOT / "assets" / "campaign_map" / "hannibal_path.json"
BOUNDS = json.loads((ROOT / "tools" / "map_pipeline" / "map_bounds.json").read_text())


class HannibalMarchTest(unittest.TestCase):
    def setUp(self) -> None:
        self.asset = json.loads(ASSET.read_text())
        self.march = hannibal_path.build_march(BOUNDS)

    def test_committed_asset_matches_generator(self) -> None:
        self.assertEqual(self.asset["march"], json.loads(json.dumps(self.march)))
        self.assertEqual(
            self.asset["lines"],
            json.loads(json.dumps(hannibal_path.build_hannibal_path(BOUNDS))),
        )

    def test_march_validates(self) -> None:
        self.assertTrue(hannibal_path.validate_march(self.march))

    def test_battle_sites_are_on_the_march(self) -> None:
        battles = {s["id"] for s in self.march["stops"] if s["kind"] == "battle"}
        self.assertEqual(
            battles,
            {"saguntum", "rhone", "alps", "ticinus", "trebia", "trasimene", "cannae", "zama"},
        )

    def test_march_starts_at_carthago_nova_and_passes_cannae_before_zama(self) -> None:
        ids = [s["id"] for s in self.march["stops"]]
        self.assertEqual(ids[0], "carthago_nova")
        self.assertLess(ids.index("alps"), ids.index("ticinus"))
        self.assertLess(ids.index("trasimene"), ids.index("cannae"))
        self.assertLess(ids.index("cannae"), ids.index("zama"))

    def test_stop_points_match_their_coordinates(self) -> None:
        for stop in self.march["stops"]:
            u, v = hannibal_path.lon_lat_to_uv(stop["lonlat"][0], stop["lonlat"][1], BOUNDS)
            self.assertAlmostEqual(stop["uv"][0], u)
            self.assertAlmostEqual(stop["uv"][1], v)
            self.assertEqual(self.march["points"][stop["index"]], stop["uv"])


if __name__ == "__main__":
    unittest.main()
