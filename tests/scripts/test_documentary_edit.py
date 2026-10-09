"""Documentary tooling: timing, captions, rules, style and score plan (no ffmpeg needed)."""

import copy
import importlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

_doc = importlib.import_module("documentary")
EditError = _doc.EditError
captions, episode, mix, narration, rules, score, style, timeline = (
    importlib.import_module(f"documentary.{name}")
    for name in (
        "captions",
        "episode",
        "mix",
        "narration",
        "rules",
        "score",
        "style",
        "timeline",
    )
)

SAMPLE = ROOT / "scripts" / "documentary" / "samples" / "ep07_cannae.json"


def load_sample(data=None):
    if data is None:
        return episode.load(SAMPLE)
    folder = Path(tempfile.mkdtemp())
    (folder / "ep07_cannae_script.md").write_text(
        (SAMPLE.parent / "ep07_cannae_script.md").read_text()
    )
    path = folder / "ep07_cannae.json"
    path.write_text(json.dumps(data))
    return episode.load(path)


def resolve(ep, overrides=None):
    paragraphs = narration.parse_script(ep.script_path())
    takes = narration.gather_takes(paragraphs, ep.vo, None, ep.mix, analyse=False)
    for pid, seconds in (overrides or {}).items():
        takes[pid] = narration.Take(pid, seconds, [(0.0, seconds)])
    return paragraphs, takes, timeline.resolve(ep, paragraphs, takes)


class TimeExpressionTest(unittest.TestCase):
    def test_forms(self):
        self.assertEqual(timeline.parse_time(12.5), (None, None, False, 12.5))
        self.assertEqual(timeline.parse_time("3"), (None, None, False, 3.0))
        self.assertEqual(timeline.parse_time("vo:p03+0.4"), ("vo", "p03", False, 0.4))
        self.assertEqual(
            timeline.parse_time("shot:ring@end-1.5"), ("shot", "ring", True, -1.5)
        )
        self.assertEqual(
            timeline.parse_time("section:battle"), ("section", "battle", False, 0.0)
        )
        self.assertEqual(timeline.parse_time("end-6"), ("end", None, True, -6.0))

    def test_garbage_is_refused(self):
        with self.assertRaises(EditError):
            timeline.parse_time("p03 plus a bit")


class SampleEditTest(unittest.TestCase):
    def test_sample_resolves_to_template_length(self):
        _, _, tl = resolve(load_sample())
        self.assertTrue(300 <= tl.duration <= 480, tl.duration)
        self.assertEqual([s.id for s in tl.sections][0], "cold_open")
        chapters = tl.chapters()
        self.assertEqual(chapters[0][0], 0.0)
        self.assertNotIn("Series title", [title for _, title in chapters])

    def test_script_word_count_in_range(self):
        paragraphs = narration.parse_script(load_sample().script_path())
        report = narration.script_report(paragraphs, 150)
        self.assertTrue(550 <= report["words"] <= 750, report)
        self.assertEqual(report["reel_lines"], 5)

    def test_sample_passes_rules(self):
        ep = load_sample()
        paragraphs, _, tl = resolve(ep)
        found = rules.check(ep, tl, narration.script_report(paragraphs, 150)["words"])
        self.assertEqual([f for f in found if f.level == "error"], [])

    def test_new_take_retimes_everything_downstream(self):
        ep = load_sample()
        _, takes, before = resolve(ep)
        longer = takes["ep07_p03"].duration + 4.0
        _, _, after = resolve(ep, {"ep07_p03": longer})
        b = {v.id: v for v in before.vo}
        a = {v.id: v for v in after.vo}
        self.assertAlmostEqual(a["p03"].start, b["p03"].start)
        self.assertAlmostEqual(a["p04"].start - b["p04"].start, 4.0, places=3)
        self.assertAlmostEqual(after.duration - before.duration, 4.0, places=3)
        shots_b = {s.id: s for s in before.shots}
        shots_a = {s.id: s for s in after.shots}
        self.assertAlmostEqual(
            shots_a["road_map"].dur - shots_b["road_map"].dur, 4.0, places=3
        )
        self.assertAlmostEqual(shots_a["co_ring"].dur, shots_b["co_ring"].dur)
        gb = {g.id: g for g in before.graphics}
        ga = {g.id: g for g in after.graphics}
        self.assertAlmostEqual(ga["g_oob"].start - gb["g_oob"].start, 4.0, places=3)
        self.assertAlmostEqual(
            ga["g_counter_road"].end - gb["g_counter_road"].end, 4.0, places=3
        )

    def test_circular_timing_is_reported(self):
        data = json.loads(SAMPLE.read_text())
        data["vo"][0]["at"] = "shot:co_ring@end-1.0"
        with self.assertRaises(EditError):
            resolve(load_sample(data))

    def test_structural_errors_are_listed(self):
        data = json.loads(SAMPLE.read_text())
        data["sections"][0]["shots"][0].pop("until")
        data["music"][0]["role"] = "funky"
        with self.assertRaises(EditError) as ctx:
            episode.validate(data)
        self.assertIn("dur/until", str(ctx.exception))
        self.assertIn("role", str(ctx.exception))


class RulesTest(unittest.TestCase):
    def _found(self, mutate):
        data = json.loads(SAMPLE.read_text())
        mutate(data)
        ep = load_sample(data)
        _, _, tl = resolve(ep)
        return [f.message for f in rules.check(ep, tl) if f.level == "error"]

    def test_banned_track(self):
        def mutate(d):
            d["music"][2][
                "file"
            ] = "assets/audio/music/combat/combat_last_defensive_wall.ogg"

        self.assertTrue(any("banned" in m for m in self._found(mutate)))

    def test_duck_range(self):
        self.assertTrue(
            any(
                "duck_db" in m
                for m in self._found(lambda d: d["mix"].update(duck_db=-4))
            )
        )

    def test_battle_music_under_words(self):
        def mutate(d):
            cue = next(c for c in d["music"] if c["id"] == "road_bed")
            cue["role"] = "battle"

        self.assertTrue(any("under" in m for m in self._found(mutate)))

    def test_hit_on_a_line(self):
        def mutate(d):
            d["sfx"].append(
                {
                    "file": "assets/audio/sfx/combat/charge_roar.ogg",
                    "at": "vo:p05+1.0",
                    "hit": True,
                }
            )

        self.assertTrue(any("lands on a line" in m for m in self._found(mutate)))

    def test_climax_needs_silence_first(self):
        def mutate(d):
            cue = next(c for c in d["music"] if c["id"] == "battle_trap")
            cue["until"] = "shot:cl_ring+2.0"

        self.assertTrue(any("silence before climax" in m for m in self._found(mutate)))

    def test_frame_zero_and_end_card(self):
        def mutate(d):
            d["sections"][0]["shots"][0]["fade_in"] = 1.0
            d["graphics"] = [g for g in d["graphics"] if g["type"] != "end_card"]

        found = self._found(mutate)
        self.assertTrue(any("frame zero" in m for m in found))
        self.assertTrue(any("Steam" in m for m in found))

    def test_reel_flags_slow_music(self):
        found = rules.check_music_choice(
            ["assets/audio/music/base/base_legion_at_dusk.ogg"], marketing=True
        )
        self.assertEqual(found[0].level, "warning")


class CaptionTest(unittest.TestCase):
    TEXT = (
        "Varro packed his legions deeper than usual, to break the enemy centre by sheer weight. "
        "Hannibal did the opposite. He pushed his Gauls and Iberians forward in a thin, bulging crescent, "
        "and held his African veterans back on either wing."
    )

    def test_sentences(self):
        self.assertEqual(len(captions.split_sentences(self.TEXT)), 3)
        self.assertEqual(
            captions.split_sentences("The second of August, 216 BC. Rome waited."),
            ["The second of August, 216 BC.", "Rome waited."],
        )

    def test_lines_fit(self):
        segments = [(0.0, 5.2), (5.7, 7.1), (7.6, 15.0)]
        caps = captions.finalise(
            captions.paragraph_captions("p07", self.TEXT, 100.0, segments, 15.0)
        )
        for cap in caps:
            lines = cap.lines()
            self.assertLessEqual(len(lines), 2, lines)
            self.assertTrue(all(len(line) <= 42 for line in lines), lines)
            self.assertGreaterEqual(cap.end - cap.start, 0.99)
            self.assertLessEqual(cap.end - cap.start, 7.0)
        for a, b in zip(caps, caps[1:], strict=False):
            self.assertLessEqual(a.end, b.start)
        self.assertEqual(" ".join(c.text for c in caps), self.TEXT)

    def test_sentence_boundaries_snap_to_pauses(self):
        sentences = captions.split_sentences(self.TEXT)
        spans = captions.sentence_times(
            sentences, [(0.0, 5.2), (5.7, 7.1), (7.6, 15.0)], 15.0
        )
        self.assertAlmostEqual(spans[1][0], 5.7)
        self.assertAlmostEqual(spans[2][0], 7.6)

    def test_srt_and_vtt(self):
        caps = [captions.Caption(1.0, 3.5, "The ring was closed.")]
        srt = captions.to_srt(caps)
        self.assertIn("00:00:01,000 --> 00:00:03,500", srt)
        self.assertTrue(captions.to_vtt(caps).startswith("WEBVTT"))
        self.assertEqual(captions.parse_srt(srt)[0].text, "The ring was closed.")

    def test_reel_style_is_short_capitals(self):
        caps = captions.paragraph_captions(
            "r02",
            "Hannibal put his weakest troops in the centre, and let them fall back.",
            0.0,
            [(0.0, 4.0)],
            4.0,
            captions.REEL,
        )
        self.assertTrue(all(c.text.isupper() for c in caps))
        self.assertTrue(
            all(len(line) <= 18 for c in caps for line in c.lines(captions.REEL))
        )


class NarrationTest(unittest.TestCase):
    def test_script_parsing_and_takes(self):
        folder = Path(tempfile.mkdtemp())
        (folder / "s.md").write_text(
            "# T\n<!-- note -->\n[ep07_p01] One line {slowly}\ncontinues here.\n\n[ep07_r01] Reel line.\n"
        )
        paragraphs = narration.parse_script(folder / "s.md")
        self.assertEqual([p.id for p in paragraphs], ["ep07_p01", "ep07_r01"])
        self.assertEqual(paragraphs[0].clean, "One line continues here.")
        vo = folder / "vo"
        (vo / "scratch").mkdir(parents=True)
        for name in ("ep07_p01.wav", "ep07_p01_t3.wav", "ep07_p01_t2.wav"):
            (vo / name).write_bytes(b"")
        (vo / "scratch" / "ep07_r01.wav").write_bytes(b"")
        takes = narration.find_takes(vo)
        self.assertEqual(narration.choose_take("ep07_p01", takes)[0], 3)
        self.assertEqual(
            narration.choose_take("ep07_p01", takes, 2)[1].name, "ep07_p01_t2.wav"
        )
        self.assertTrue(narration.choose_take("ep07_r01", takes)[2])
        self.assertEqual(narration.recording_name("ep07_p04", 2), "ep07_p04_t2.wav")


class DuckEnvelopeTest(unittest.TestCase):
    def test_depth_attack_and_release(self):
        env = mix.duck_envelope(
            [(2.0, 4.0)],
            8.0,
            -10.0,
            attack=0.3,
            hold=0.2,
            release=1.0,
            merge_gap=0.9,
            rate=100,
        )
        self.assertAlmostEqual(env[100], 1.0)
        self.assertAlmostEqual(env[300], 10 ** (-10 / 20), places=4)
        self.assertLess(env[195], 1.0)
        self.assertAlmostEqual(env[165], 1.0)
        self.assertAlmostEqual(env[415], 10 ** (-10 / 20), places=4)
        self.assertGreater(env[500], env[430])
        self.assertAlmostEqual(env[530], 1.0)

    def test_short_pauses_stay_ducked(self):
        env = mix.duck_envelope(
            [(1.0, 2.0), (2.5, 3.0)], 5.0, -10.0, 0.3, 0.2, 1.0, 0.9, rate=100
        )
        self.assertAlmostEqual(env[225], 10 ** (-10 / 20), places=4)

    def test_fill_envelope_is_zero_under_takes(self):
        fill = mix.fill_envelope([(1.0, 2.0)], 3.0, ramp=0.05, rate=100)
        self.assertEqual(fill[150], 0.0)
        self.assertEqual(fill[50], 1.0)


class StyleTest(unittest.TestCase):
    def test_tokens_and_spellings(self):
        tokens = style.as_dict()
        self.assertEqual(tokens["colors"]["ink"], "#f4e7c8")
        self.assertIn("rome", tokens["sides"])
        self.assertEqual(style.qml_argb((255, 0, 0), 0.5), "#80ff0000")
        json.loads(style.to_json())

    def test_layout_scales_by_short_side(self):
        self.assertEqual(style.unit(3840, 2160), 2.0)
        self.assertEqual(style.unit(1080, 1920), 1.0)
        self.assertEqual(style.aspect(1080, 1920), "9:16")
        left, top, right, bottom = style.safe_box(1080, 1920)
        self.assertGreater(1920 - bottom, 400)

    def test_stamp_text(self):
        self.assertEqual(
            style.stamp_text("Cannae", {"day": 2, "month": "August", "year": 216}),
            ("CANNAE", "2 AUGUST 216 BC"),
        )
        self.assertEqual(
            style.stamp_text("New Carthage", {"season": "Spring", "year": 218})[1],
            "SPRING 218 BC",
        )
        self.assertEqual(style.roman(7), "VII")


class ScorePlanTest(unittest.TestCase):
    def setUp(self):
        self.plan = score.load_plan()
        self.cues = score.expand(self.plan)

    def test_every_episode_has_every_role(self):
        episodes = {c["episode"] for c in self.cues} - {"series"}
        self.assertEqual(len(episodes), 9)
        for ep in episodes:
            roles = {c["role"] for c in self.cues if c["episode"] == ep}
            self.assertTrue(
                {"bed", "tension", "battle", "climax", "elegy"} <= roles, ep
            )

    def test_counts(self):
        summary = score.summary(self.cues)
        self.assertEqual(summary["cues"], 111)
        self.assertEqual(summary["by_role"]["climax"], 9)

    def test_manifest_needs_no_service(self):
        data = score.manifest(self.cues, self.plan)
        self.assertEqual(data["count"], len(self.cues))
        first = data["requests"][0]
        self.assertTrue(first["cue"].startswith("ep07_"))
        self.assertIn("Instrumental only", first["prompt"])
        for request in data["requests"]:
            self.assertNotIn("last_defensive_wall", json.dumps(request))

    def test_stem_metadata_schema(self):
        cue = next(c for c in self.cues if c["id"] == "ep07_battle_a")
        meta = {
            "schema": "soi-documentary-stems/1",
            "cue": "ep07_battle_a",
            "tempo_bpm": 96,
            "key": "D minor",
            "duration_s": 25.0,
            "sample_rate": 48000,
            "stems": {s: {"file": f"ep07_battle_a.{s}.wav"} for s in cue["stems"]},
            "source": {
                "service": "ElevenLabs Music",
                "render": "x",
                "date": "2026-10-09",
                "licence": "commercial",
            },
        }
        self.assertEqual(score.validate_meta(meta, cue), [])
        bad = copy.deepcopy(meta)
        bad["key"] = "H minor"
        bad["stems"].pop("brass")
        bad["stems"]["kazoo"] = {"file": "k.wav"}
        problems = score.validate_meta(bad, cue)
        self.assertTrue(any("kazoo" in p for p in problems))
        self.assertTrue(any("brass" in p for p in problems))

    def test_reuse_check(self):
        a = {
            "file": "x.ogg",
            "src": (0.0, 40.0),
            "episode": "ep07",
            "cue": "a",
            "plan_cue": "ep07_climax",
        }
        b = {
            "file": "x.ogg",
            "src": (20.0, 60.0),
            "episode": "ep09",
            "cue": "b",
            "plan_cue": "ep09_climax",
        }
        c = {
            "file": "x.ogg",
            "src": (0.0, 10.0),
            "episode": "ep09",
            "cue": "t",
            "plan_cue": "series_title_theme",
        }
        d = {
            "file": "x.ogg",
            "src": (0.0, 10.0),
            "episode": "ep07",
            "cue": "t",
            "plan_cue": "series_title_theme",
        }
        self.assertEqual(len(score.reuse([a, b], set())), 1)
        self.assertEqual(score.reuse([c, d], {"series_title_theme"}), [])

    def test_sample_edit_reuses_nothing_long(self):
        ep = load_sample()
        _, _, tl = resolve(ep)
        signatures = {c["id"] for c in self.cues if c.get("signature")}
        self.assertEqual(score.reuse(score.passages(ep, tl), signatures), [])
        planned = {c["id"] for c in self.cues}
        for span in tl.music:
            self.assertIn(span.data["plan_cue"], planned)


if __name__ == "__main__":
    unittest.main()
