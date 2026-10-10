"""Tactical overlays are drawn in world space and land where the renderer drew.

``scripts/tactical_overlay.py`` projects arrows, army blocks, frontage lines and
labels through the camera matrices the arena exported for each frame. These
tests pin the projection to known pixels (synthetic cameras and a real capture),
the arrow geometry and draw-on, terrain draping and style injection.
"""

import importlib.util
import json
import math
import sys
import tempfile
import unittest
from array import array
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = Path(__file__).resolve().parent / "fixtures" / "tactical_overlay"


def load(name: str, relative: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


overlay = load("tactical_overlay", "scripts/tactical_overlay.py")

try:
    import PIL  # noqa: F401

    HAVE_PIL = True
except ImportError:  # CI's bare interpreter has no Pillow
    HAVE_PIL = False


def synthetic_frame(eye, target, fov=40.0, aspect=16 / 9, near=0.5, far=500.0, t=0.0,
                    groups=None):
    view = overlay.look_at(eye, target)
    projection = overlay.perspective(fov, aspect, near, far)
    frame = {
        "frame": 0,
        "t": t,
        "view": overlay.flatten_column_major(view),
        "projection": overlay.flatten_column_major(projection),
        "eye": list(eye),
        "target": list(target),
        "fov_y": fov,
        "aspect": aspect,
        "near": near,
        "far": far,
    }
    if groups:
        frame["groups"] = groups
    return frame


def synthetic_track(frames, width=1920, height=1080, fps=30):
    header = {"type": "soi_camera_track", "version": 1, "fps": fps,
              "width": width, "height": height}
    for index, frame in enumerate(frames):
        frame["frame"] = index
        frame["t"] = index / fps
    return overlay.CameraTrack(header, frames)


class ProjectionTest(unittest.TestCase):
    def test_column_major_round_trip(self):
        matrix = [[float(r * 4 + c) for c in range(4)] for r in range(4)]
        flat = overlay.flatten_column_major(matrix)
        self.assertEqual(flat[:4], [0.0, 4.0, 8.0, 12.0])
        self.assertEqual(overlay.column_major(flat), matrix)

    def test_known_points_land_on_known_pixels(self):
        # A 90-degree square lens at the origin looking down -z: the frustum's
        # edges are the 45-degree diagonals, so these pixels are exact.
        frame = synthetic_frame((0, 0, 0), (0, 0, -1), fov=90.0, aspect=1.0, near=0.1)
        camera = overlay.Camera.from_frame(frame, 1000, 1000)
        cases = {
            (0.0, 0.0, -5.0): (500.0, 500.0),
            (5.0, 0.0, -5.0): (1000.0, 500.0),
            (-5.0, 0.0, -5.0): (0.0, 500.0),
            (0.0, 5.0, -5.0): (500.0, 0.0),
            (2.5, -2.5, -5.0): (750.0, 750.0),
        }
        for world, pixel in cases.items():
            projected = camera.project(world)
            self.assertIsNotNone(projected, world)
            self.assertAlmostEqual(projected[0], pixel[0], places=6)
            self.assertAlmostEqual(projected[1], pixel[1], places=6)

    def test_the_look_target_is_the_frame_centre_for_any_lens(self):
        frame = synthetic_frame((-40.0, 26.0, 55.0), (12.0, 1.5, -3.0), fov=23.0)
        camera = overlay.Camera.from_frame(frame, 1080, 1920)
        x, y = camera.project((12.0, 1.5, -3.0))
        self.assertAlmostEqual(x, 540.0, places=4)
        self.assertAlmostEqual(y, 960.0, places=4)

    def test_points_behind_the_lens_are_clipped_not_mirrored(self):
        frame = synthetic_frame((0, 10, 0), (0, 10, -1), fov=60.0, near=0.5)
        camera = overlay.Camera.from_frame(frame, 1920, 1080)
        self.assertIsNone(camera.project((0, 10, 5)))
        polygon = camera.project_polygon([(-1, 9, -10), (1, 9, -10), (1, 9, 10), (-1, 9, 10)])
        self.assertGreaterEqual(len(polygon), 4)
        for x, y in polygon:
            self.assertTrue(math.isfinite(x) and math.isfinite(y))
            self.assertGreater(y, 540.0, "a floor strip stays below the horizon")
        runs = camera.project_polyline([(0, 9, -10), (0, 9, 10)])
        self.assertEqual(len(runs), 1)


@unittest.skipUnless((FIXTURES / "real.camera.jsonl").is_file(), "no real capture fixture")
class RealCaptureTest(unittest.TestCase):
    """Frames 0, 120 and 239 of the track arena wrote for
    tools/arena/promos/tactical/cine_field_tactical.json (RTX 5060, Ultra),
    trimmed to two groups without per-unit rows. The shot aims an orbit camera
    at (-4, 0, 0)."""

    def setUp(self):
        self.track = overlay.CameraTrack.load(FIXTURES / "real.camera.jsonl")

    def test_matrices_agree_with_the_lens_they_describe(self):
        for frame in self.track.frames:
            if "projection" not in frame:
                continue
            expected = overlay.perspective(frame["fov_y"], frame["aspect"], frame["near"],
                                           frame["far"])
            actual = overlay.column_major(frame["projection"])
            for row in range(4):
                for column in range(4):
                    self.assertAlmostEqual(actual[row][column], expected[row][column], places=4)
            self.assertAlmostEqual(frame["aspect"], self.track.width / self.track.height,
                                   places=4)

    def test_the_exported_target_projects_to_the_frame_centre(self):
        for index, frame in enumerate(self.track.frames):
            camera = self.track.camera(index)
            x, y = camera.project(tuple(frame["target"]))
            self.assertAlmostEqual(x, self.track.width / 2, delta=0.05)
            self.assertAlmostEqual(y, self.track.height / 2, delta=0.05)
            view = overlay.column_major(frame["view"])
            eye = frame["eye"]
            local = [sum(view[r][k] * (eye + [1.0])[k] for k in range(4)) for r in range(3)]
            for component in local:
                self.assertAlmostEqual(component, 0.0, delta=1e-2)

    def test_the_authored_focus_point_lands_on_the_frame_centre(self):
        # The fixture's spec aims an orbit camera at the world point (-4, 0, 0):
        # that point, authored in the spec and never touched by the exporter,
        # must come out of the real matrices at the centre pixel.
        for index in range(len(self.track.frames)):
            camera = self.track.camera(index)
            x, y = camera.project((-4.0, 0.0, 0.0))
            self.assertAlmostEqual(x, self.track.width / 2, delta=0.05)
            self.assertAlmostEqual(y, self.track.height / 2, delta=0.05)

    def test_live_groups_project_inside_the_shot(self):
        for index, frame in enumerate(self.track.frames):
            camera = self.track.camera(index)
            for name, group in frame.get("groups", {}).items():
                x, y = camera.project(tuple(group["centroid"]))
                self.assertTrue(0 <= x <= self.track.width and 0 <= y <= self.track.height,
                                (index, name, x, y))


class ArrowGeometryTest(unittest.TestCase):
    def test_straight_arrow_and_draw_on(self):
        path = overlay.arrow_path([(0.0, 0.0), (40.0, 0.0)])
        self.assertAlmostEqual(overlay.polyline_length(path), 40.0)
        half = overlay.arrow_shape(path, 0.5, 3.0, 6.0, 8.0, taper=1.0)
        self.assertAlmostEqual(half.tip[0], 20.0, places=6)
        self.assertAlmostEqual(half.tip[1], 0.0, places=6)
        base = half.head[0]
        self.assertAlmostEqual(base[0][0], 14.0, places=6)
        self.assertAlmostEqual(abs(base[0][1] - base[1][1]), 8.0, places=6)
        self.assertIsNone(overlay.arrow_shape(path, 0.0, 3.0, 6.0, 8.0))

    def test_left_is_left_seen_from_above(self):
        # Facing +x, the game's right hand is +z (right = forward x up).
        self.assertEqual(overlay.left_of((1.0, 0.0)), (0.0, -1.0))
        shape = overlay.arrow_shape([(0.0, 0.0), (20.0, 0.0)], 1.0, 4.0, 4.0, 6.0, taper=1.0)
        left, right = shape.sections[0]
        self.assertLess(left[1], 0.0)
        self.assertGreater(right[1], 0.0)

    def test_bend_puts_the_apex_where_asked(self):
        path = overlay.arrow_path([(0.0, 0.0), (40.0, 0.0)], bend=0.25)
        apex = path[len(path) // 2]
        self.assertAlmostEqual(apex[0], 20.0, places=6)
        self.assertAlmostEqual(apex[1], -10.0, places=6, msg="0.25 chords to the left")
        self.assertEqual(path[0], (0.0, 0.0))
        self.assertEqual(path[-1], (40.0, 0.0))

    def test_spline_passes_through_every_waypoint(self):
        waypoints = [(0.0, 0.0), (10.0, 5.0), (20.0, -5.0), (30.0, 0.0)]
        path = overlay.arrow_path(waypoints)
        for point in waypoints:
            self.assertTrue(any(math.dist(point, p) < 1e-9 for p in path), point)

    def test_truncate_and_resample(self):
        path = [(0.0, 0.0), (10.0, 0.0), (10.0, 10.0)]
        cut = overlay.truncate(path, 15.0)
        self.assertEqual(cut[-1], (10.0, 5.0))
        samples = overlay.resample(path, 2.0)
        self.assertEqual(len(samples), 11)
        for a, b in zip(samples, samples[1:]):
            self.assertAlmostEqual(math.dist(a, b), 2.0, places=6)

    def test_taper_and_short_heads(self):
        shape = overlay.arrow_shape([(0.0, 0.0), (100.0, 0.0)], 1.0, 4.0, 8.0, 10.0, taper=0.5)
        first = shape.sections[0]
        last = shape.sections[-1]
        self.assertAlmostEqual(abs(first[0][1] - first[1][1]), 2.0, places=3)
        self.assertGreater(abs(last[0][1] - last[1][1]), 3.6)
        stub = overlay.arrow_shape([(0.0, 0.0), (100.0, 0.0)], 0.05, 4.0, 8.0, 10.0)
        head_length = math.dist(stub.head[0][0], stub.head[0][1])
        self.assertLess(head_length, 10.0, "a barely drawn arrow scales its head down")

    def test_ease_is_monotonic_and_clamped(self):
        for name in ("linear", "smooth", "in", "out"):
            values = [overlay.ease(name, t / 20) for t in range(-2, 23)]
            self.assertEqual(values[0], 0.0)
            self.assertEqual(values[-1], 1.0)
            self.assertEqual(values, sorted(values))


class TerrainTest(unittest.TestCase):
    def write_terrain(self, folder: Path, height):
        columns = rows = 21
        data = array("f", [height(-10 + c, -10 + r) for r in range(rows) for c in range(columns)])
        (folder / "terrain_probe_1.f32").write_bytes(data.tobytes())
        (folder / "terrain_probe_1.json").write_text(json.dumps({
            "type": "soi_terrain_heights", "data": "terrain_probe_1.f32",
            "origin": [-10, -10], "spacing": 1.0, "columns": columns, "rows": rows}))
        return overlay.Terrain.load(folder / "terrain_probe_1.json")

    def test_bilinear_sampling_reproduces_a_plane(self):
        with tempfile.TemporaryDirectory() as folder:
            terrain = self.write_terrain(Path(folder), lambda x, z: 0.5 * x - 0.25 * z + 3)
            for x, z in ((0.3, 0.7), (-9.5, 4.25), (7.75, -2.5)):
                self.assertAlmostEqual(terrain.height(x, z), 0.5 * x - 0.25 * z + 3, places=4)
            self.assertAlmostEqual(terrain.height(50.0, 0.0), 0.5 * 10 + 3, places=4,
                                   msg="outside the grid clamps to the edge")


class StyleTest(unittest.TestCase):
    def test_series_style_is_injected_over_the_defaults(self):
        series = {"palette": {"rome": "#102030"}, "font": "/series/Display.ttf",
                  "arrow": {"width_m": 5.0}}
        parsed = overlay.Overlay.parse({"elements": [
            {"type": "arrow", "points": [[0, 0], [10, 0]], "color": "rome"}]}, series)
        self.assertEqual(parsed.style["font"], "/series/Display.ttf")
        self.assertEqual(parsed.style["palette"]["carthage"],
                         overlay.DEFAULT_STYLE["palette"]["carthage"])
        self.assertEqual(parsed.elements[0].style["width_m"], 5.0)
        self.assertEqual(parsed.elements[0].style["head"], "triangle")
        self.assertEqual(overlay.parse_color("rome", parsed.style), (0x10, 0x20, 0x30))

    def test_precedence_default_series_overlay_class_inline(self):
        series = {"arrow": {"opacity": 0.5, "width_m": 5.0},
                  "classes": {"feint": {"opacity": 0.4, "head": "none"}}}
        description = {
            "style": {"arrow": {"opacity": 0.7}},
            "elements": [
                {"type": "arrow", "points": [[0, 0], [1, 0]]},
                {"type": "arrow", "points": [[0, 0], [1, 0]], "class": "feint"},
                {"type": "arrow", "points": [[0, 0], [1, 0]], "class": "feint",
                 "style": {"opacity": 0.2}},
            ],
        }
        parsed = overlay.Overlay.parse(description, series)
        self.assertEqual(parsed.elements[0].style["opacity"], 0.7)
        self.assertEqual(parsed.elements[0].style["width_m"], 5.0)
        self.assertEqual(parsed.elements[1].style["opacity"], 0.4)
        self.assertEqual(parsed.elements[1].style["head"], "none")
        self.assertEqual(parsed.elements[2].style["opacity"], 0.2)
        self.assertEqual(overlay.DEFAULT_STYLE["arrow"]["opacity"], 0.9,
                         "injection never mutates the defaults")

    def test_bad_descriptions_are_refused(self):
        for element in ({"type": "arc"}, {"type": "arrow", "points": [[0, 0]]},
                        {"type": "block"}, {"type": "label", "text": "x"},
                        {"type": "arrow", "points": [[0, 0], [1, 1]], "start": 2, "end": 1}):
            with self.assertRaises(ValueError, msg=element):
                overlay.Overlay.parse({"elements": [element]})
        with self.assertRaises(ValueError):
            overlay.Overlay.parse({"elements": [
                {"type": "arrow", "points": [[0, 0], [1, 0]], "class": "missing"}]})

    def test_series_tokens_are_translated(self):
        # The shape ``python3 scripts/documentary style --json`` writes (#1534).
        tokens = {
            "fonts": {"display": "assets/fonts/StandardIronDisplay-Bold.ttf",
                      "text": "assets/fonts/EBGaramond12-Bold.ttf"},
            "colors": {"ink": "#f4e7c8", "iron": "#120d09", "shadow": "#000000"},
            "sides": {
                "rome": {"name": "ROME", "color": "#c44034", "deep": "#78221c",
                         "contingents": {"legions": "#c44034"}},
                "carthage": {"name": "CARTHAGE", "color": "#487aba", "deep": "#244070",
                             "contingents": {"gauls": "#5f8a3c", "iberians": "#b07034"}},
            },
            "type_scale_px": {"label": 30},
            "tracking_em": {"label": 0.16},
            "stroke_px": {"arrow_outline": 2.5, "front_line": 5.0, "rule_heavy": 4.0,
                          "side_bar": 6.0},
            "opacity": {"arrow_fill": 0.88, "zone_fill": 0.22, "shadow": 0.72},
            "timing_s": {"fade_in": 0.5, "rule_draw": 0.6},
        }
        style = overlay.resolve_style(tokens)
        self.assertEqual(overlay.parse_color("carthage", style), (0x48, 0x7A, 0xBA))
        self.assertEqual(overlay.parse_color(overlay.owner_color(style, 3), style),
                         (0x5F, 0x8A, 0x3C), "owner 3 is the Gauls")
        self.assertTrue(style["font"].endswith("StandardIronDisplay-Bold.ttf"))
        self.assertTrue(Path(style["font"]).is_absolute())
        self.assertEqual(style["label"]["size_px"], 30)
        self.assertEqual(style["block"]["fill_opacity"], 0.22)
        self.assertEqual(style["arrow"]["outline"], "iron")
        self.assertEqual(style["fade_seconds"], 0.5)
        self.assertEqual(style["arrow"]["width_m"], overlay.DEFAULT_STYLE["arrow"]["width_m"])

    def test_fades(self):
        parsed = overlay.Overlay.parse({"elements": [
            {"type": "arrow", "points": [[0, 0], [1, 0]], "start": 1.0, "end": 3.0,
             "fade_in": 0.5, "fade_out": 0.5}]})
        element = parsed.elements[0]
        self.assertEqual(element.opacity(0.9), 0.0)
        self.assertAlmostEqual(element.opacity(1.25), 0.5)
        self.assertEqual(element.opacity(2.0), 1.0)
        self.assertAlmostEqual(element.opacity(2.75), 0.5)


def army(name, owner, xs, z, yaw):
    units = [[i + 1, x, 0.0, z, yaw] for i, x in enumerate(xs)]
    cx = sum(xs) / len(xs)
    forward = [math.sin(math.radians(yaw)), 0.0, math.cos(math.radians(yaw))]
    return name, {
        "owner": owner, "alive": len(units), "centroid": [cx, 0.0, z],
        "forward": forward, "front": [[min(xs), 0.0, z], [max(xs), 0.0, z]],
        "width": max(xs) - min(xs), "depth": 0.0, "units": units,
    }


class AnchorTest(unittest.TestCase):
    def test_group_anchors_follow_the_army(self):
        frames = []
        for step in range(3):
            groups = dict([army("rome", 1, [-8.0, 0.0, 8.0], -20.0 + step * 5, 0.0)])
            frames.append(synthetic_frame((0, 60, 60), (0, 0, 0), groups=groups))
        track = synthetic_track(frames)
        resolver = overlay.AnchorResolver(track)
        self.assertEqual(resolver.point({"group": "rome"}, 0), (0.0, -20.0))
        self.assertEqual(resolver.point({"group": "rome"}, 2), (0.0, -10.0))
        ahead = resolver.point({"group": "rome", "at": "front", "forward_m": 4.0}, 0)
        self.assertEqual(ahead, (0.0, -16.0))
        self.assertEqual(resolver.point({"group": "rome", "at": "front_right"}, 0), (8.0, -20.0))
        self.assertEqual(resolver.point({"group": "rome", "unit": 2}, 1), (8.0, -15.0))
        self.assertEqual(resolver.point([3, 4], 0), (3.0, 4.0))
        self.assertEqual(resolver.point([3, 9, 4], 0), (3.0, 4.0))


@unittest.skipUnless(HAVE_PIL, "Pillow is not installed")
class RenderTest(unittest.TestCase):
    def setUp(self):
        groups = dict([army("rome", 1, [-30.0, -26.0, -22.0], 0.0, 90.0),
                       army("carthage", 2, [22.0, 26.0, 30.0], 0.0, 270.0)])
        self.frames = [synthetic_frame((0.0, 80.0, 60.0), (0.0, 0.0, 0.0), fov=40.0,
                                       groups=groups) for _ in range(4)]

    def render(self, description, terrain=None, frame=3, style=None):
        track = synthetic_track(self.frames, 960, 540)
        renderer = overlay.Renderer(overlay.Overlay.parse(description, style), track,
                                    terrain or overlay.Terrain.flat())
        return renderer, renderer.render(frame)

    def test_arrow_paints_where_the_ground_projects(self):
        renderer, image = self.render({"elements": [
            {"type": "arrow", "points": [{"group": "rome"}, {"group": "carthage"}],
             "draw": 0, "fade_in": 0}]})
        camera = renderer.track.camera(3, 960, 540)
        middle = camera.project((0.0, 0.1, 0.0))
        self.assertGreater(image.getpixel((round(middle[0]), round(middle[1])))[3], 200)
        corner = image.getpixel((5, 5))
        self.assertEqual(corner[3], 0)
        colour = image.getpixel((round(middle[0]), round(middle[1])))[:3]
        self.assertEqual(colour, overlay.parse_color("rome", overlay.DEFAULT_STYLE),
                         "an arrow from a Roman group takes Rome's colour")

    def test_edges_are_anti_aliased(self):
        _, image = self.render({"elements": [
            {"type": "arrow", "points": [[-25, 0], [25, 0]], "bend": 0.3,
             "draw": 0, "fade_in": 0}]})
        alphas = {pixel[3] for pixel in image.getdata()}
        partial = [a for a in alphas if 0 < a < 200]
        self.assertGreater(len(partial), 10)

    def test_ground_overlays_ride_the_terrain(self):
        hill = overlay.Terrain(array("f", [8.0] * (201 * 201)), (-100.0, -100.0), 1.0, 201, 201)
        description = {"elements": [{"type": "frontage", "group": "carthage", "fade_in": 0,
                                     "draw": 0}]}
        renderer_flat, flat = self.render(description)
        renderer_hill, raised = self.render(description, hill)
        def rows(image):
            ys = [y for y in range(image.height) for x in range(0, image.width, 4)
                  if image.getpixel((x, y))[3] > 128]
            return sum(ys) / len(ys)
        camera = renderer_hill.track.camera(3, 960, 540)
        expected_shift = camera.project((26.0, 8.15, 0.0))[1] - camera.project((26.0, 0.15, 0.0))[1]
        self.assertAlmostEqual(rows(raised) - rows(flat), expected_shift, delta=1.5)

    def test_block_and_label_render(self):
        _, image = self.render({"elements": [
            {"type": "block", "group": "rome", "fade_in": 0},
            {"type": "label", "text": "Carthage", "at": {"group": "carthage"}, "fade_in": 0}]})
        self.assertGreater(sum(1 for p in image.getdata() if p[3] > 0), 500)

    def test_frames_without_a_camera_stay_clear(self):
        self.frames[3] = {"camera": False}
        _, image = self.render({"elements": [
            {"type": "arrow", "points": [[-25, 0], [25, 0]], "draw": 0, "fade_in": 0}]})
        self.assertEqual(image.getbbox(), None)


if __name__ == "__main__":
    unittest.main()
