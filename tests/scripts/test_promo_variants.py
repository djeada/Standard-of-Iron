"""promo-edit.py cuts a framing variant recorded in the same capture pass."""

import contextlib
import importlib.util
import io
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def load(name: str, relative: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


promo_edit = load("promo_edit_variants", "scripts/promo-edit.py")


def shot(name, variants):
    return {
        "name": name,
        "clip": f"01_{name}.mp4",
        "clip_seconds": 4.0,
        "frames": 120,
        "camera": f"01_{name}.camera.jsonl",
        "variants": variants,
    }


def vertical(name, width=1080, height=1920):
    return {
        "name": "vertical",
        "width": width,
        "height": height,
        "clip": f"01_{name}.vertical.mp4",
        "camera": f"01_{name}.vertical.camera.jsonl",
        "poster": f"01_{name}.vertical.png",
        "frames": 120,
        "clip_seconds": 4.0,
    }


class SelectVariantTest(unittest.TestCase):
    def test_swaps_clips_and_frame_size(self):
        manifest = {"width": 1920, "height": 1080, "fps": 30}
        shots = [shot("a", [vertical("a")]), shot("b", [vertical("b")])]
        sized, swapped = promo_edit.select_variant(manifest, shots, "vertical")
        self.assertEqual((sized["width"], sized["height"]), (1080, 1920))
        self.assertEqual(manifest["width"], 1920, "the manifest itself is untouched")
        self.assertEqual(
            [s["clip"] for s in swapped], ["01_a.vertical.mp4", "01_b.vertical.mp4"]
        )
        self.assertEqual(swapped[0]["name"], "a")
        self.assertEqual(swapped[0]["camera"], "01_a.vertical.camera.jsonl")

    def test_refuses_a_shot_without_the_variant(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            promo_edit.select_variant(
                {}, [shot("a", [vertical("a")]), shot("b", [])], "vertical"
            )

    def test_refuses_mixed_sizes(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            promo_edit.select_variant(
                {},
                [shot("a", [vertical("a")]), shot("b", [vertical("b", 720, 1280)])],
                "vertical",
            )


if __name__ == "__main__":
    unittest.main()
