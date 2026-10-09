"""Documentary chain end to end on synthetic inputs (needs ffmpeg and Pillow).

The sample Cannae edit is built at the ``tiny`` profile from synthetic VO and
placeholder captures: upload package, reel, then a longer retake of one
paragraph, which must re-time the cues and subtitles on the next build. Also
covers the measured depth of the VO ducking, stem normalisation and stills of
every graphic in both aspects.
"""

import importlib
import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

HAVE_PIL = importlib.util.find_spec("PIL") is not None
HAVE_FFMPEG = shutil.which("ffmpeg") is not None and shutil.which("ffprobe") is not None
SAMPLE = ROOT / "scripts" / "documentary" / "samples" / "ep07_cannae.json"


def mod(name):
    return importlib.import_module(f"documentary.{name}")


@unittest.skipUnless(HAVE_PIL and HAVE_FFMPEG, "needs ffmpeg and Pillow")
class EpisodeChainTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = Path(tempfile.mkdtemp(prefix="soi_doc_"))
        cls.dirs = {k: cls.tmp / k for k in ("clips", "vo", "overlays", "work")}
        cls.cli = mod("cli")
        cls.opts = []
        for key, path in cls.dirs.items():
            cls.opts += [f"--{key}", str(path)]
        assert cls.cli.main(["synth-vo", str(SAMPLE), "--vo", str(cls.dirs["vo"])]) == 0
        assert (
            cls.cli.main(
                [
                    "placeholders",
                    str(SAMPLE),
                    *cls.opts,
                    "--size",
                    "320x180",
                    "--fps",
                    "24",
                ]
            )
            == 0
        )
        cls.package = cls.tmp / "package"
        cls.reel = cls.tmp / "reel"
        cls.package_rc = cls.cli.main(
            [
                "package",
                str(SAMPLE),
                *cls.opts,
                "--profile",
                "tiny",
                "--out",
                str(cls.package),
                "--allow-scratch",
            ]
        )
        cls.reel_rc = cls.cli.main(
            [
                "reel",
                str(SAMPLE),
                *cls.opts,
                "--profile",
                "tiny",
                "--out",
                str(cls.reel),
            ]
        )

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _timeline(self):
        args = type(
            "A",
            (),
            {
                "edit": SAMPLE,
                "clips": self.dirs["clips"],
                "vertical_clips": None,
                "vo": self.dirs["vo"],
                "overlays": self.dirs["overlays"],
                "work": self.dirs["work"],
                "analyse": True,
            },
        )()
        return self.cli._load(args)

    def test_package_contents(self):
        self.assertEqual(self.package_rc, 0)
        manifest = json.loads((self.package / "upload.json").read_text())
        self.assertTrue(manifest["accepted"], manifest["qc"])
        for name in (
            "ep07_cannae_tiny.mp4",
            "ep07_cannae.en.srt",
            "ep07_cannae.en.vtt",
            "chapters.txt",
            "thumbnail.jpg",
            "description.txt",
        ):
            self.assertTrue((self.package / name).stat().st_size > 0, name)
        self.assertAlmostEqual(manifest["qc"]["integrated_lufs"], -14.0, delta=1.0)
        self.assertLessEqual(manifest["qc"]["true_peak_dbtp"], -1.0)
        self.assertTrue(manifest["narration"]["synthetic"])
        chapters = (self.package / "chapters.txt").read_text().splitlines()
        self.assertTrue(chapters[0].startswith("0:00 "))
        self.assertGreaterEqual(len(chapters), 3)
        description = (self.package / "description.txt").read_text()
        for needle in (
            "Polybius",
            "Livy",
            "ElevenLabs",
            "store.steampowered.com/app/5129960",
        ):
            self.assertIn(needle, description)
        from PIL import Image

        self.assertEqual(Image.open(self.package / "thumbnail.jpg").size, (1280, 720))
        mixed = manifest["mix"]["vo"]
        self.assertEqual(mixed["recorded"], 14)
        self.assertGreater(mixed["vo_over_music_lu"], 8.0)

    def test_reel(self):
        self.assertEqual(self.reel_rc, 0)
        manifest = json.loads((self.reel / "ep07_cannae_reel.json").read_text())
        self.assertLess(manifest["seconds"], 60.0)
        self.assertEqual(
            [p["role"] for p in manifest["parts"]],
            ["hook", "beat", "beat", "beat", "consequence", "tag"],
        )
        self.assertEqual(
            (manifest["qc"]["width"], manifest["qc"]["height"]), (180, 320)
        )
        self.assertGreater(manifest["captions"], 3)

    def test_scratch_narration_cannot_ship(self):
        out = self.tmp / "refused"
        rc = self.cli.main(
            ["package", str(SAMPLE), *self.opts, "--profile", "tiny", "--out", str(out)]
        )
        self.assertEqual(rc, 1)
        self.assertFalse(out.exists() and any(out.glob("*.mp4")))

    def test_retake_retimes_cues_and_subtitles(self):
        captions = mod("captions")
        _, _, _, before = self._timeline()
        srt_before = captions.parse_srt(
            (self.package / "ep07_cannae.en.srt").read_text()
        )
        paragraphs = mod("narration").parse_script(
            SAMPLE.parent / "ep07_cannae_script.md"
        )
        mod("narration").synthesise(
            paragraphs, self.dirs["vo"], take=2, only=["p03"], stretch=1.6
        )
        try:
            _, _, takes, after = self._timeline()
            self.assertEqual(takes["ep07_p03"].take, 2)
            grow = {v.id: v for v in after.vo}["p03"].dur - {
                v.id: v for v in before.vo
            }["p03"].dur
            self.assertGreater(grow, 2.0)
            b = {v.id: v for v in before.vo}
            a = {v.id: v for v in after.vo}
            self.assertAlmostEqual(a["p04"].start - b["p04"].start, grow, places=2)
            gb = {g.id: g for g in before.graphics}
            ga = {g.id: g for g in after.graphics}
            self.assertAlmostEqual(
                ga["g_oob"].start - gb["g_oob"].start, grow, places=2
            )
            out = self.tmp / "subs"
            self.assertEqual(
                self.cli.main(
                    ["subtitles", str(SAMPLE), *self.opts, "--out", str(out)]
                ),
                0,
            )
            srt_after = captions.parse_srt((out / "ep07_cannae.en.srt").read_text())
            first_p04 = next(c for c in srt_after if c.text.startswith("It was led"))
            old_p04 = next(c for c in srt_before if c.text.startswith("It was led"))
            self.assertAlmostEqual(first_p04.start - old_p04.start, grow, delta=0.3)
        finally:
            (self.dirs["vo"] / "ep07_p03_t2.wav").unlink(missing_ok=True)


@unittest.skipUnless(HAVE_PIL and HAVE_FFMPEG, "needs ffmpeg and Pillow")
class DuckingDepthTest(unittest.TestCase):
    def test_music_sits_ten_db_under_words(self):
        media, mix, timeline, narration = (
            mod("media"),
            mod("mix"),
            mod("timeline"),
            mod("narration"),
        )
        tmp = Path(tempfile.mkdtemp(prefix="soi_duck_"))
        try:
            tone = tmp / "tone.wav"
            media.ffmpeg(
                ["-f", "lavfi", "-i", "sine=f=220:r=48000:d=12", "-ac", "2", str(tone)]
            )
            voice = tmp / "voice.wav"
            media.ffmpeg(
                [
                    "-f",
                    "lavfi",
                    "-i",
                    "sine=f=500:r=48000:d=4",
                    "-c:a",
                    "pcm_s24le",
                    str(voice),
                ]
            )
            take = narration.Take("ep01_p01", 4.0, [(0.0, 4.0)], voice, voice, 1)
            vo = [timeline.VoSpan("p01", 5.0, 9.0, {}, "", take)]
            music = [
                timeline.Span(
                    "bed",
                    0.0,
                    12.0,
                    {"role": "bed", "file": str(tone), "fade_in": 0, "fade_out": 0},
                )
            ]
            params = {**mod("episode").DEFAULT_MIX, "duck_db": -10.0}
            report = mix.build(mix.MixPlan("duck", 12.0, vo, music, params=params), tmp)
            music_bus = Path(report["stems"]["music"])
            clear = media.loudness(_cut(media, music_bus, 1.0, 3.0, tmp / "a.wav"))[
                "integrated_lufs"
            ]
            under = media.loudness(_cut(media, music_bus, 6.0, 8.5, tmp / "b.wav"))[
                "integrated_lufs"
            ]
            self.assertAlmostEqual(clear - under, 10.0, delta=0.6)
            self.assertAlmostEqual(report["integrated_lufs"], -14.0, delta=0.5)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


def _cut(media, path, start, end, out):
    media.ffmpeg(["-ss", str(start), "-to", str(end), "-i", str(path), str(out)])
    return out


@unittest.skipUnless(HAVE_PIL and HAVE_FFMPEG, "needs ffmpeg and Pillow")
class StemToolsTest(unittest.TestCase):
    def test_normalise_then_validate(self):
        media, score = mod("media"), mod("score")
        plan = score.load_plan()
        cues = {c["id"]: c for c in score.expand(plan)}
        tmp = Path(tempfile.mkdtemp(prefix="soi_stems_"))
        try:
            folder = tmp / "raw" / "ep07_elegy"
            folder.mkdir(parents=True)
            stems = {}
            for i, name in enumerate(cues["ep07_elegy"]["stems"]):
                path = folder / f"ep07_elegy.{name}.wav"
                media.ffmpeg(
                    [
                        "-f",
                        "lavfi",
                        "-i",
                        f"sine=f={220 * (i + 1)}:r=48000:d=6",
                        "-ac",
                        "2",
                        "-af",
                        "volume=-30dB",
                        "-c:a",
                        "pcm_s24le",
                        str(path),
                    ]
                )
                stems[name] = {"file": path.name}
            meta = {
                "schema": "soi-documentary-stems/1",
                "cue": "ep07_elegy",
                "tempo_bpm": 96,
                "key": "D minor",
                "duration_s": 6.0,
                "sample_rate": 48000,
                "stems": stems,
                "source": {
                    "service": "test",
                    "render": "synthetic",
                    "date": "2026-10-09",
                    "licence": "test",
                },
            }
            (folder / "stems.json").write_text(json.dumps(meta))
            self.assertTrue(
                any("normalize-stems" in p for p in score.validate(folder, plan, cues))
            )
            result = score.normalize(folder, tmp / "norm", plan)
            self.assertAlmostEqual(
                result["mix_lufs"], plan["loudness"]["cue_lufs"], delta=0.5
            )
            self.assertEqual(
                score.validate(tmp / "norm" / "ep07_elegy", plan, cues), []
            )
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


@unittest.skipUnless(HAVE_PIL, "needs Pillow")
class GraphicsStillsTest(unittest.TestCase):
    def test_every_graphic_renders_in_both_aspects(self):
        episode, graphics, conform = mod("episode"), mod("graphics"), mod("conform")
        ep = episode.load(SAMPLE)
        numbers = conform.section_numbers(ep)
        kinds = set()
        for g in ep.data["graphics"]:
            data = graphics.graphic_data(ep, g, numbers)
            for w, h in ((320, 180), (180, 320)):
                image = graphics.frame(g["type"], data, w, h, 3.0, 6.0)
                self.assertEqual(image.size, (w, h))
                self.assertIsNotNone(image.getchannel("A").getbbox(), g["id"])
            kinds.add(g["type"])
        self.assertEqual(kinds, set(episode.GRAPHIC_TYPES))

    def test_end_card_carries_the_steam_link(self):
        fields = mod("graphics").end_card_fields("steam_demo")
        self.assertIn(
            "STORE.STEAMPOWERED.COM/APP/5129960", fields["end_card_destination"]
        )


if __name__ == "__main__":
    unittest.main()
