"""Steam depots must be complete, launchable, and carry nothing private."""

import importlib.util
import os
import plistlib
import stat
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    "steam_release", Path(__file__).resolve().parents[2] / "scripts/steam-release.py"
)
steam = importlib.util.module_from_spec(SPEC)
# dataclasses resolve the module's annotations through sys.modules.
sys.modules[SPEC.name] = steam
SPEC.loader.exec_module(steam)


def config(app_id=0, depot_id=0):
    loaded = steam.load_config()
    loaded["app_id"] = app_id
    for depot in loaded["depots"].values():
        depot["depot_id"] = depot_id
    return loaded


def touch(root: Path, relative: str, data: bytes = b"x", executable=False) -> Path:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    if executable:
        path.chmod(path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    return path


def windows_tree(root: Path) -> None:
    for relative in steam.REQUIRED["windows"]:
        touch(root, relative)
    touch(root, "standard_of_iron.exe", b"MZ" + b"\0" * 64)


def linux_tree(root: Path) -> None:
    for relative in steam.REQUIRED["linux"]:
        touch(root, relative)
    touch(root, "usr/bin/standard_of_iron", b"\x7fELF" + b"\0" * 64, executable=True)
    touch(root, "usr/lib/libQt6Core.so.6")
    touch(root, "usr/lib/libQt6Quick.so.6")
    os.symlink("usr/bin/standard_of_iron", root / "AppRun.wrapped")


def macos_tree(root: Path, identifier="io.github.djeada.standardofiron") -> None:
    for relative in steam.REQUIRED["macos"]:
        if relative.endswith(".framework"):
            continue
        touch(root, relative)
    app = root / "standard_of_iron.app"
    touch(app, "Contents/MacOS/standard_of_iron", b"\xcf\xfa\xed\xfe", executable=True)
    with open(app / "Contents/Info.plist", "wb") as handle:
        plistlib.dump(
            {
                "CFBundleExecutable": "standard_of_iron",
                "CFBundleIdentifier": identifier,
            },
            handle,
        )
    versions = app / "Contents/Frameworks/QtCore.framework/Versions"
    touch(versions, "A/QtCore")
    os.symlink("A", versions / "Current")
    os.symlink("Versions/Current/QtCore", versions.parent / "QtCore")
    touch(root, steam.MACOS_STAPLED_TICKET)


class SteamStageTest(unittest.TestCase):
    def setUp(self):
        self._minimum = steam.MINIMUM_DEPOT_BYTES
        steam.MINIMUM_DEPOT_BYTES = 0
        self._scratch = tempfile.TemporaryDirectory()
        self.root = Path(self._scratch.name)

    def tearDown(self):
        steam.MINIMUM_DEPOT_BYTES = self._minimum
        self._scratch.cleanup()

    def verify(self, platform, **kwargs):
        return steam.verify(platform, self.root, config(), **kwargs)

    def test_complete_trees_pass(self):
        for platform, build in (
            ("windows", windows_tree),
            ("linux", linux_tree),
            ("macos", macos_tree),
        ):
            with self.subTest(platform=platform):
                self._scratch.cleanup()
                self._scratch = tempfile.TemporaryDirectory()
                self.root = Path(self._scratch.name)
                build(self.root)
                self.assertEqual(self.verify(platform).errors, [])

    def test_private_and_developer_files_are_rejected(self):
        windows_tree(self.root)
        for relative in (
            "standard_of_iron.pdb",
            "renderer_self_test_out.log",
            "signing/cert.p12",
            "config/config.vdf",
            "saves/saves.sqlite",
            "bpat_baker.exe",
            "CMakeCache.txt",
        ):
            touch(self.root, relative)
        errors = "\n".join(self.verify("windows").errors)
        for name in (
            "standard_of_iron.pdb",
            "renderer_self_test_out.log",
            "cert.p12",
            "config.vdf",
            "saves.sqlite",
            "bpat_baker.exe",
            "CMakeCache.txt",
        ):
            self.assertIn(name, errors)

    def test_wavefront_assets_are_not_mistaken_for_objects(self):
        windows_tree(self.root)
        touch(self.root, "assets/campaign_map/land_mesh.obj")
        self.assertEqual(self.verify("windows").errors, [])

    def test_missing_licence_is_rejected(self):
        windows_tree(self.root)
        (self.root / "THIRD_PARTY_LICENSES.md").unlink()
        self.assertIn(
            "missing required file: THIRD_PARTY_LICENSES.md",
            self.verify("windows").errors,
        )

    def test_linux_launch_target_must_stay_executable(self):
        linux_tree(self.root)
        binary = self.root / "usr/bin/standard_of_iron"
        binary.chmod(0o644)
        self.assertTrue(any("not executable" in e for e in self.verify("linux").errors))

    def test_symlinks_may_not_leave_the_depot(self):
        linux_tree(self.root)
        os.symlink("/etc/passwd", self.root / "usr/bin/leak")
        os.symlink("../../../outside", self.root / "usr/bin/escape")
        errors = "\n".join(self.verify("linux").errors)
        self.assertIn("usr/bin/leak", errors)
        self.assertIn("usr/bin/escape", errors)

    def test_ad_hoc_mac_bundle_is_rejected_unless_allowed(self):
        macos_tree(self.root)
        (self.root / steam.MACOS_STAPLED_TICKET).unlink()
        self.assertTrue(any("notarization" in e for e in self.verify("macos").errors))
        self.assertEqual(self.verify("macos", allow_unsigned=True).errors, [])

    def test_placeholder_bundle_identifier_is_rejected(self):
        macos_tree(self.root, identifier="com.yourcompany.standard_of_iron")
        self.assertTrue(
            any("CFBundleIdentifier" in e for e in self.verify("macos").errors)
        )

    def test_dereferenced_framework_symlinks_are_rejected(self):
        macos_tree(self.root)
        framework = (
            self.root / "standard_of_iron.app/Contents/Frameworks/QtCore.framework"
        )
        current = framework / "Versions/Current"
        current.unlink()
        touch(current, "QtCore")
        self.assertTrue(any("dereferenced" in e for e in self.verify("macos").errors))

    def test_manifest_records_every_file(self):
        linux_tree(self.root)
        manifest = self.root.parent / f"{self.root.name}-manifest.json"
        try:
            self.verify("linux", manifest=manifest)
            text = manifest.read_text()
            self.assertIn('"usr/bin/standard_of_iron"', text)
            self.assertIn('"symlink": "usr/bin/standard_of_iron"', text)
        finally:
            manifest.unlink(missing_ok=True)


class SteamWindowsStageTest(unittest.TestCase):
    def test_the_msvc_installer_stays_out_of_the_depot(self):
        with tempfile.TemporaryDirectory() as scratch:
            root = Path(scratch)
            package = root / "game.zip"
            with zipfile.ZipFile(package, "w") as archive:
                archive.writestr("standard_of_iron.exe", b"MZ")
                archive.writestr("vc_redist.x64.exe", b"MZ")
                archive.writestr("assets/audio/audio_manifest.json", b"{}")
            steam.stage("windows", package, root / "stage")
            self.assertTrue((root / "stage" / "standard_of_iron.exe").is_file())
            self.assertTrue(
                (root / "stage" / "assets/audio/audio_manifest.json").is_file()
            )
            self.assertFalse((root / "stage" / "vc_redist.x64.exe").exists())


class SteamVdfTest(unittest.TestCase):
    def setUp(self):
        self._scratch = tempfile.TemporaryDirectory()
        self.root = Path(self._scratch.name)
        for platform in steam.PLATFORMS:
            (self.root / "stage" / platform).mkdir(parents=True)

    def tearDown(self):
        self._scratch.cleanup()

    def render(self, cfg, branch="rc", platforms=("windows", "linux", "macos")):
        return steam.render_vdf(
            cfg,
            stage_root=self.root / "stage",
            platforms=list(platforms),
            desc='v0.1.0 "rc" (abc1234)',
            branch=branch,
            preview=True,
            out=self.root / "out",
        )

    def test_never_sets_a_build_live_on_the_public_branch(self):
        for branch in ("default", "public", "Default"):
            with self.subTest(branch=branch), self.assertRaises(SystemExit):
                self.render(config(app_id=10, depot_id=11), branch=branch)

    def test_refuses_unassigned_ids(self):
        with self.assertRaises(SystemExit):
            self.render(config(app_id=0, depot_id=11))
        with self.assertRaises(SystemExit):
            self.render(config(app_id=10, depot_id=0))

    def test_renders_one_depot_script_per_platform(self):
        cfg = config(app_id=10)
        for index, depot in enumerate(cfg["depots"].values()):
            depot["depot_id"] = 11 + index
        script = self.render(cfg, platforms=("windows", "macos"))
        text = script.read_text()
        self.assertIn('"AppID" "10"', text)
        self.assertIn('"SetLive" "rc"', text)
        self.assertIn('"Preview" "1"', text)
        self.assertIn('v0.1.0 \\"rc\\" (abc1234)', text)
        self.assertIn('"11"', text)
        self.assertIn('"13"', text)
        self.assertNotIn('"12"', text)
        depot = (self.root / "out" / "depot_build_13.vdf").read_text()
        self.assertIn(str((self.root / "stage" / "macos").resolve()), depot)


class SteamBuildIdTest(unittest.TestCase):
    def test_reads_the_build_id_from_steamcmd_output(self):
        log = "Uploading content...\nSuccessfully finished AppID 480 build (BuildID 12345678).\n"
        self.assertEqual(steam.parse_build_id(log), "12345678")
        self.assertIsNone(steam.parse_build_id("ERROR! Failed to commit build"))


if __name__ == "__main__":
    unittest.main()
