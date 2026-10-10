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
            {
                "saguntum",
                "rhone",
                "alps",
                "ticinus",
                "trebia",
                "trasimene",
                "cannae",
                "zama",
            },
        )

    def test_march_starts_at_carthago_nova_and_passes_cannae_before_zama(self) -> None:
        ids = [s["id"] for s in self.march["stops"]]
        self.assertEqual(ids[0], "carthago_nova")
        self.assertLess(ids.index("alps"), ids.index("ticinus"))
        self.assertLess(ids.index("trasimene"), ids.index("cannae"))
        self.assertLess(ids.index("cannae"), ids.index("zama"))

    def test_stop_points_match_their_coordinates(self) -> None:
        for stop in self.march["stops"]:
            u, v = hannibal_path.lon_lat_to_uv(
                stop["lonlat"][0], stop["lonlat"][1], BOUNDS
            )
            self.assertAlmostEqual(stop["uv"][0], u)
            self.assertAlmostEqual(stop["uv"][1], v)
            self.assertEqual(self.march["points"][stop["index"]], stop["uv"])

    def test_land_legs_stay_on_land(self) -> None:
        try:
            from PIL import Image
        except ImportError:
            self.skipTest("pillow is not installed")
        heightmap = ROOT / "assets" / "campaign_map" / "terrain_height.png"
        meta = json.loads(
            (ROOT / "assets" / "campaign_map" / "terrain_height.json").read_text()
        )
        image = Image.open(heightmap)
        width, height = image.size
        pixels = image.load()

        def is_land(u: float, v: float) -> bool:
            value = pixels[int(u * (width - 1)), int((1.0 - v) * (height - 1))]
            value = value[0] if isinstance(value, tuple) else value
            return meta["min_m"] + value / 65535 * (meta["max_m"] - meta["min_m"]) > 0

        points = self.march["points"]
        stops = self.march["stops"]
        for source, (start, end) in zip(
            hannibal_path.MARCH_STOPS, zip(stops, stops[1:], strict=False), strict=False
        ):
            if source.get("leg", "land") != "land":
                continue
            leg = points[start["index"] : end["index"] + 1]
            samples = [
                (a[0] + (b[0] - a[0]) * k / 10, a[1] + (b[1] - a[1]) * k / 10)
                for a, b in zip(leg, leg[1:], strict=False)
                for k in range(10)
            ]
            on_land = sum(is_land(u, v) for u, v in samples) / len(samples)
            self.assertGreaterEqual(
                on_land, 0.9, f"{start['id']} -> {end['id']} runs over the sea"
            )


if __name__ == "__main__":
    unittest.main()
