"""Configure-time metadata must not invalidate unrelated build outputs."""

import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("cmake"), "CMake is required")
class BuildIncrementalityTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.source = Path(self.temp.name)
        self.build = self.source / "build"

    def run_command(self, *args, env=None):
        result = subprocess.run(
            args, cwd=self.source, env=env, text=True, capture_output=True, check=False
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout

    def configure(self):
        generator = "Ninja" if shutil.which("ninja") else "Unix Makefiles"
        self.run_command(
            "cmake", "-S", str(self.source), "-B", str(self.build), "-G", generator
        )

    def resource_fixture(self):
        assets = self.source / "assets/campaign_map"
        assets.mkdir(parents=True)
        for name in (
            "campaign_base_color.png",
            "campaign_water.png",
            "coastlines_uv.json",
            "rivers_uv.json",
            "land_mesh.bin",
            "provinces.json",
            "terrain_height.png",
            "terrain_height.json",
        ):
            (assets / name).write_text("fixture\n")
        # Manifest generation needs the LinguistTools target, but the test does
        # not compile catalogues or require a Qt installation.
        (self.source / "Qt6Config.cmake").write_text(
            "set(Qt6LinguistTools_FOUND TRUE)\n"
            "add_executable(Qt6::lrelease IMPORTED)\n"
            'set_property(TARGET Qt6::lrelease PROPERTY IMPORTED_LOCATION "${CMAKE_COMMAND}")\n'
        )
        (self.source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.21)\n"
            "project(ResourceIncrementality NONE)\n"
            "set(QT_VERSION_MAJOR 6)\n"
            'set(Qt6_DIR "${CMAKE_CURRENT_SOURCE_DIR}")\n'
            f'include("{ROOT.as_posix()}/cmake/CampaignMapAssets.cmake")\n'
            f'include("{ROOT.as_posix()}/cmake/Translations.cmake")\n'
            "foreach(manifest IN ITEMS campaign_map_generated translations_generated)\n"
            '  add_custom_command(OUTPUT "${manifest}.copied"\n'
            '    COMMAND "${CMAKE_COMMAND}" -E copy "${manifest}.qrc" "${manifest}.copied"\n'
            '    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${manifest}.qrc")\n'
            '  add_custom_target(${manifest} ALL DEPENDS "${manifest}.copied")\n'
            "endforeach()\n"
        )

    def test_unchanged_configure_preserves_resources_and_their_consumers(self):
        self.resource_fixture()
        self.configure()
        self.run_command("cmake", "--build", str(self.build))
        manifests = list(self.build.glob("*.qrc"))
        self.assertEqual(len(manifests), 2)
        old_time = 1_600_000_000_000_000_000
        for manifest in manifests:
            os.utime(manifest, ns=(old_time, old_time))
        consumers = {p: p.stat().st_mtime_ns for p in self.build.glob("*.copied")}
        self.configure()
        self.run_command("cmake", "--build", str(self.build))
        for manifest in manifests:
            with self.subTest(manifest=manifest.name):
                self.assertEqual(manifest.stat().st_mtime_ns, old_time)
        for consumer, timestamp in consumers.items():
            with self.subTest(consumer=consumer.name):
                self.assertEqual(consumer.stat().st_mtime_ns, timestamp)

    def test_changed_resource_manifests_are_regenerated(self):
        self.resource_fixture()
        self.configure()
        manifests = {p: p.read_text() for p in self.build.glob("*.qrc")}
        for manifest in manifests:
            manifest.write_text("<stale/>\n")
        self.configure()
        self.run_command("cmake", "--build", str(self.build))
        for manifest, expected in manifests.items():
            with self.subTest(manifest=manifest.name):
                self.assertEqual(manifest.read_text(), expected)
                self.assertEqual(manifest.with_suffix(".copied").read_text(), expected)

    @unittest.skipUnless(shutil.which("make"), "GNU Make is required")
    def test_make_reuses_the_incremental_bake_with_each_generator(self):
        shutil.copy(ROOT / "Makefile", self.source)
        shutil.copytree(ROOT / "make", self.source / "make")
        (self.source / "scripts").mkdir()
        shutil.copy(ROOT / "scripts/run-map-pipeline.sh", self.source / "scripts")
        recipe = self.source / "assets/creatures/horse/horse.cmesh"
        nation = self.source / "assets/data/nations/rome.json"
        loadouts = self.source / "assets/visuals/unit_equipment_loadouts.json"
        for path in (recipe, nation, loadouts):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("first recipe\n")
        (self.source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.21)\n"
            "project(MakeIncrementality NONE)\n"
            f'include("{ROOT.as_posix()}/cmake/CreatureBakeInputs.cmake")\n'
            "add_custom_command(OUTPUT baked.txt\n"
            '  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/assets/creatures/horse/horse.cmesh" baked.txt\n'
            "  DEPENDS ${SOI_CREATURE_BAKE_INPUTS})\n"
            "add_custom_target(bake_creature_assets DEPENDS baked.txt)\n"
            "add_custom_target(standard_of_iron ALL)\n"
            "add_dependencies(standard_of_iron bake_creature_assets)\n"
        )
        generators = ["Unix Makefiles"]
        if shutil.which("ninja"):
            generators.append("Ninja")
        for index, generator in enumerate(generators):
            with self.subTest(generator=generator):
                build = self.source / f"build-{index}"
                env = {
                    **os.environ,
                    "CMAKE_GENERATOR": generator,
                    "SKIP_MAP_PIPELINE": "1",
                }

                def make(target, build=build, env=env):
                    self.run_command(
                        "make", target, f"BUILD_DIR={build}", "BUILD_JOBS=2", env=env
                    )

                make("build-app")
                baked = build / "baked.txt"
                timestamp = baked.stat().st_mtime_ns
                for target in ("build-app", "build", "bake-bpat"):
                    make(target)
                    self.assertEqual(baked.stat().st_mtime_ns, timestamp, target)
                recipe.write_text("changed recipe\n")
                os.utime(baked, ns=(0, 0))
                make("bake-bpat")
                self.assertEqual(baked.read_text(), "changed recipe\n")
                for content in (nation, loadouts):
                    timestamp = baked.stat().st_mtime_ns
                    content.write_text("changed content\n")
                    os.utime(content, ns=(timestamp + 1_000_000_000,) * 2)
                    make("bake-bpat")
                    self.assertNotEqual(baked.stat().st_mtime_ns, timestamp)
                    os.utime(content, ns=(0, 0))
                # Adding an old package and removing it must both rebake.
                extra = recipe.with_name("extra.cmesh")
                for action in ("add", "remove"):
                    timestamp = baked.stat().st_mtime_ns
                    if action == "add":
                        extra.write_text("extra package\n")
                        os.utime(extra, ns=(0, 0))
                    else:
                        extra.unlink()
                    make("bake-bpat")
                    self.assertNotEqual(baked.stat().st_mtime_ns, timestamp, action)
                # Generated outputs and unrelated textures must not rebake.
                timestamp = baked.stat().st_mtime_ns
                for suffix in ("bpat", "png"):
                    recipe.with_suffix(f".{suffix}").write_text("unrelated\n")
                make("bake-bpat")
                self.assertEqual(baked.stat().st_mtime_ns, timestamp)

    @unittest.skipUnless(
        shutil.which("git") and shutil.which("c++"),
        "Git and a C++ compiler are required",
    )
    def test_commit_change_only_invalidates_replay_source(self):
        self.run_command("git", "init", "--quiet")

        def commit(message):
            self.run_command(
                "git",
                "-c",
                "user.name=Build Test",
                "-c",
                "user.email=build-test@example.invalid",
                "-c",
                "commit.gpgsign=false",
                "commit",
                "--allow-empty",
                "--quiet",
                "-m",
                message,
            )

        (self.source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.21)\n"
            "project(ReplayIncrementality LANGUAGES CXX)\n"
            "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n"
            "set(QT_VERSION_MAJOR 6)\n"
            "foreach(component IN ITEMS Core Gui Sql)\n"
            "  add_library(Qt6::${component} INTERFACE IMPORTED)\n"
            "endforeach()\n"
            f'add_subdirectory("{ROOT.as_posix()}/game" game)\n'
            "set_target_properties(soi_missions soi_mission_runtime PROPERTIES AUTOMOC OFF)\n"
        )

        def commands():
            return {
                entry["file"]: entry["command"]
                for entry in json.loads(
                    (self.build / "compile_commands.json").read_text()
                )
            }

        commit("first")
        self.configure()
        before = commands()
        commit("second")
        self.configure()
        after = commands()
        changed = {path for path in before if before[path] != after[path]}
        self.assertEqual(changed, {str(ROOT / "game/command/replay.cpp")})


if __name__ == "__main__":
    unittest.main()
