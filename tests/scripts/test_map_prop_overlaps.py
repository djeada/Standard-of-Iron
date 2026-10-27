"""Repair bounds and collision checks must agree with authored map coordinates."""

import importlib.util
import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "soi_map_prop_repair", ROOT / "scripts/fix-map-prop-overlaps.py"
)
REPAIR = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = REPAIR
SPEC.loader.exec_module(REPAIR)


class MapPropRepairTest(unittest.TestCase):
    def body(self, x=0.0, z=0.0, half_x=1.0, half_z=1.0, rotation=0.0):
        return REPAIR.Placeable(
            key="prop",
            kind="world_prop",
            name="tent",
            x=x,
            z=z,
            half_x=half_x,
            half_z=half_z,
            priority=1,
            rotation=rotation,
        )

    def spot(self, terrain, x, z):
        return REPAIR.is_placeable_spot(self.body(), x, z, terrain, 0.0, 0.0, 1.0)

    def test_world_map_repairs_accept_both_sides_of_the_origin(self):
        terrain = REPAIR.read_terrain(
            {
                "coord_system": "world",
                "grid": {"width": 40, "height": 30, "tile_size": 2.0},
            }
        )
        self.assertTrue(self.spot(terrain, -20.0, -10.0))
        self.assertTrue(self.spot(terrain, 20.0, 10.0))
        for x, z in [(-40.0, 0.0), (40.0, 0.0), (0.0, -30.0), (0.0, 30.0)]:
            self.assertFalse(self.spot(terrain, x, z))

    def test_grid_map_repairs_keep_positive_bounds(self):
        terrain = REPAIR.read_terrain({"grid": {"width": 40, "height": 30}})
        self.assertTrue(self.spot(terrain, 20.0, 10.0))
        self.assertFalse(self.spot(terrain, -20.0, 10.0))
        self.assertFalse(self.spot(terrain, 40.0, 10.0))

    def test_collision_culling_preserves_rotated_corners(self):
        timber = self.body(half_x=10.0, rotation=math.pi / 4)
        corner = self.body(7.4, 7.4)
        self.assertGreater(timber.overlap_with(corner, 0.15), 0.0)
        self.assertGreater(
            REPAIR.Segment(7.0, 7.0, 8.0, 7.0, 0.25).depth_into(timber, 0.0), 0.0
        )
        self.assertLessEqual(timber.overlap_with(self.body(100.0, 100.0), 0.15), 0.0)
        self.assertLessEqual(
            REPAIR.Segment(100.0, 100.0, 110.0, 100.0, 1.0).depth_into(timber, 0.0), 0.0
        )

    def test_wall_defenders_may_overlap_their_supporting_wall(self):
        wall = self.body()
        wall.kind = "structure"
        wall.body_type = "wall_segment"
        defender = REPAIR.collect(
            {"spawns": [{"id": "archers", "type": "archer", "on_wall": True}]},
            "map.json",
        )[0]
        self.assertTrue(wall.exempt_from(defender))
        self.assertTrue(defender.exempt_from(wall))
        defender.payload["on_wall"] = False
        self.assertFalse(wall.exempt_from(defender))
        defender.payload["on_wall"] = True
        wall.body_type = "barracks"
        self.assertFalse(wall.exempt_from(defender))

    def test_cleanup_preserves_capturable_landmarks(self):
        data = {
            "world_props": [
                {"type": kind, "x": 0, "z": 0}
                for kind in ["magic_shrine", "cursed_gold_vein", "tent"]
            ]
        }
        items = REPAIR.collect(data, "map.json")
        violations = [REPAIR.Violation("slope", item, 1.0) for item in items]
        self.assertEqual(REPAIR.drop_unplaceable(data, items, violations), 1)
        self.assertEqual(
            [prop["type"] for prop in data["world_props"]],
            ["magic_shrine", "cursed_gold_vein"],
        )
        self.assertEqual(len(items), 2)

    def test_escape_search_keeps_obstacles_along_the_whole_travel_budget(self):
        mover = self.body()
        anchor = self.body(0.5, 0.0)
        obstacle = self.body(-3.0, 0.0)
        anchor.key = "anchor"
        obstacle.key = "obstacle"
        anchor.priority = obstacle.priority = 2
        items = [mover, anchor, obstacle]
        self.assertTrue(
            REPAIR.try_move(
                mover,
                items,
                REPAIR.Terrain(),
                0.15,
                4.0,
                1.5,
                -1.0,
                0.0,
                0.0,
                0.0,
                anchor,
            )
        )
        self.assertLessEqual(mover.overlap_with(anchor, 0.15), 0.0)
        self.assertLessEqual(mover.overlap_with(obstacle, 0.15), 0.0)
