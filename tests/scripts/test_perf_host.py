"""The reference-host preflight must fail closed on anything it cannot prove."""

import importlib.util
import unittest
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    "perf_host", Path(__file__).resolve().parents[2] / "scripts/check-perf-host.py"
)
host = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(host)


def facts(**overrides):
    base = {
        "display": ":0",
        "wayland_display": None,
        "renderer": "NVIDIA GeForce RTX 5060/PCIe/SSE2",
        "glxinfo_ok": True,
        "xrandr_ok": True,
        "active_refresh_hz": [60.0],
        "load_average": [0.2, 0.3, 0.4],
    }
    base.update(overrides)
    return base


class PerfHostTest(unittest.TestCase):
    def test_a_quiet_hardware_host_passes(self):
        self.assertEqual(host.evaluate(facts(), [], [], 1.0), [])

    def test_software_rendering_is_rejected(self):
        failures = host.evaluate(
            facts(renderer="llvmpipe (LLVM 15.0.6, 256 bits)"), [], [], 1.0
        )
        self.assertEqual(len(failures), 1)
        self.assertIn("software rendering", failures[0])

    def test_a_headless_host_is_rejected(self):
        failures = host.evaluate(facts(display=None, wayland_display=None), [], [], 1.0)
        self.assertIn(
            "no graphical session: neither DISPLAY nor WAYLAND_DISPLAY set", failures
        )

    def test_a_non_sixty_hertz_mode_is_rejected(self):
        failures = host.evaluate(facts(active_refresh_hz=[144.0]), [], [], 1.0)
        self.assertEqual(len(failures), 1)
        self.assertIn("60 Hz", failures[0])

    def test_a_missing_mode_is_rejected(self):
        self.assertIn(
            "no active refresh rate was reported",
            host.evaluate(facts(active_refresh_hz=[]), [], [], 1.0),
        )

    def test_competitors_are_named(self):
        failures = host.evaluate(
            facts(), [{"pid": 3, "executable": "cc1plus"}], [], 1.0
        )
        self.assertEqual(len(failures), 1)
        self.assertIn("cc1plus", failures[0])

    def test_a_loaded_host_is_rejected(self):
        failures = host.evaluate(facts(load_average=[4.2, 1.0, 0.5]), [], [], 1.0)
        self.assertEqual(len(failures), 1)
        self.assertIn("4.20", failures[0])

    def test_missing_tools_are_listed(self):
        failures = host.evaluate(facts(), [], ["ninja", "glxinfo"], 1.0)
        self.assertIn("missing required tools: ninja, glxinfo", failures)

    def test_an_unreadable_renderer_is_rejected(self):
        self.assertIn(
            "the OpenGL renderer could not be identified",
            host.evaluate(facts(glxinfo_ok=False, renderer=""), [], [], 1.0),
        )
