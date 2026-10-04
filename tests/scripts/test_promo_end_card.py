"""Every promo ends by sending the viewer to the Steam store page."""

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


promo_edit = load("promo_edit", "scripts/promo-edit.py")
long_film = load("promo_long_film", "scripts/promo-long-film.py")


def refuses(spec: dict) -> bool:
    with contextlib.redirect_stderr(io.StringIO()):
        try:
            promo_edit.check_end_card(spec)
        except SystemExit:
            return True
    return False


class EndCardTest(unittest.TestCase):
    def test_preset_fills_the_card_and_the_spec_wins(self):
        spec = promo_edit.apply_end_card_preset(
            {"end_card": "steam_demo", "title": "SCIPIO VS HANNIBAL"}
        )
        self.assertEqual(spec["title"], "SCIPIO VS HANNIBAL")
        self.assertEqual(spec["end_card_destination"], promo_edit.STEAM_LINK)
        self.assertIn("STEAM", spec["subtitle"])

    def test_unknown_preset_is_refused(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            promo_edit.apply_end_card_preset({"end_card": "no_such_card"})

    def test_card_without_steam_link_is_refused(self):
        self.assertTrue(
            refuses(
                {
                    "title": "STANDARD OF IRON",
                    "end_card_destination": "GITHUB.COM/DJEADA/STANDARD-OF-IRON",
                }
            )
        )

    def test_steam_card_and_opt_out_pass(self):
        self.assertFalse(
            refuses(promo_edit.apply_end_card_preset({"end_card": "steam_demo"}))
        )
        self.assertFalse(refuses({"title": "INTERNAL REVIEW", "end_card_steam": False}))
        self.assertFalse(refuses({"shots": []}))

    def test_every_shipped_spec_ends_on_steam(self):
        import json

        for path in sorted((ROOT / "tools/arena/promos").rglob("*.json")):
            spec = json.loads(path.read_text())
            if not isinstance(spec, dict) or "shots" not in spec:
                continue
            with self.subTest(spec=str(path.relative_to(ROOT))):
                self.assertFalse(refuses(promo_edit.apply_end_card_preset(spec)))

    def test_store_url_falls_back_to_a_face_with_an_underscore(self):
        display = str(promo_edit.FONT_CANDIDATES[0])
        self.assertEqual(promo_edit.face_for("PLAY THE FREE DEMO", display), display)
        self.assertNotEqual(
            promo_edit.face_for(promo_edit.STEAM_LINK, display), display
        )


class ChapterNameTest(unittest.TestCase):
    def test_small_words_stay_lower_case(self):
        self.assertEqual(
            long_film.chapter_name("I. DAWN ON THE RIVER"), "Dawn on the River"
        )
        self.assertEqual(
            long_film.chapter_name("IV. STONE AND TIMBER"), "Stone and Timber"
        )


if __name__ == "__main__":
    unittest.main()
