#!/usr/bin/env python3
"""Stage, verify and describe Steam depots from verified release packages.

Steam installs files, not archives. A player who buys the game must be able to
press Play without unpacking a ZIP, mounting a DMG or giving an AppImage FUSE.
Every depot is therefore the *unpacked* payload of a package the GitHub
release pipeline already built, self-tested and checksummed:

  windows  standard_of_iron-<v>-win-x64.zip             -> its contents
  linux    standard_of_iron-<v>-linux-x86_64.AppImage   -> its extracted AppDir
  macos    standard_of_iron-<v>-macos-<arch>.app.tar.gz -> the signed,
           notarized and stapled standard_of_iron.app

Staging from those packages rather than rebuilding is what ties a Steam build
to the exact bytes of a GitHub release candidate.

  stage    unpack one platform's package into a depot tree
  verify   fail on missing binaries, assets or licences; lost executable bits;
           symlinks that escape the depot; and anything that must never ship:
           keys, credentials, symbols, logs, build caches, save databases
  vdf      render SteamPipe app and depot build scripts from steam/steam.json
  manifest print a tree's files, symlinks, modes and hashes (to compare two)
  build-id pull the BuildID out of a SteamCMD log

usage:
  scripts/steam-release.py stage  --platform linux --package X.AppImage --out stage/linux
  scripts/steam-release.py verify --platform linux --stage stage/linux [--manifest m.json]
  scripts/steam-release.py vdf    --stage-root stage --platforms windows,linux,macos \\
                                  --desc "v0.1.0 (abc1234)" --out steam/out [--preview]
  scripts/steam-release.py manifest stage/macos/standard_of_iron.app
  scripts/steam-release.py build-id steamcmd.log
"""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
import plistlib
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import zipfile
from dataclasses import dataclass, field
from pathlib import Path
from string import Template

REPO_ROOT = Path(__file__).resolve().parents[1]
STEAM_DIR = REPO_ROOT / "steam"
PLATFORMS = ("windows", "linux", "macos")

# Branches a release candidate may never be set live on. Promotion to the
# public branch is a deliberate, manual step in Steamworks. It applies to a
# BuildID that has already been installed and tested from the beta branch.
PUBLIC_BRANCHES = {"default", "public"}

# The smallest a depot can plausibly be. A payload that lost its assets or its
# Qt runtime is tens of megabytes lighter. This is how a truncated extraction
# shows up before anyone installs it.
MINIMUM_DEPOT_BYTES = 40_000_000

# Paths are relative to the stage root; launch targets come from
# steam/steam.json. These are the files the game cannot start, render, or
# legally ship without.
REQUIRED = {
    "windows": [
        "standard_of_iron.exe",
        "qt.conf",
        "Qt6Core.dll",
        "Qt6Gui.dll",
        "Qt6Quick.dll",
        "Qt6Sql.dll",
        "platforms/qwindows.dll",
        "sqldrivers/qsqlite.dll",
        "opengl32sw.dll",
        "libgallium_wgl.dll",
        "d3dcompiler_47.dll",
        "LICENSE",
        "THIRD_PARTY_LICENSES.md",
        "assets/audio/audio_manifest.json",
        "assets/campaigns/second_punic_war.json",
        "assets/creatures/horse/horse.cmesh",
    ],
    "linux": [
        "usr/bin/standard_of_iron",
        "usr/bin/qt.conf",
        "usr/plugins/platforms/libqxcb.so",
        "usr/plugins/sqldrivers/libqsqlite.so",
        "usr/bin/LICENSE",
        "usr/bin/THIRD_PARTY_LICENSES.md",
        "usr/share/doc/standard_of_iron/LICENSE",
        "usr/share/doc/standard_of_iron/THIRD_PARTY_LICENSES.md",
        "usr/bin/assets/audio/audio_manifest.json",
        "usr/bin/assets/campaigns/second_punic_war.json",
        "usr/bin/assets/creatures/horse/horse.cmesh",
    ],
    "macos": [
        "standard_of_iron.app/Contents/Info.plist",
        "standard_of_iron.app/Contents/MacOS/standard_of_iron",
        "standard_of_iron.app/Contents/Frameworks/QtCore.framework",
        "standard_of_iron.app/Contents/PlugIns/platforms/libqcocoa.dylib",
        "standard_of_iron.app/Contents/PlugIns/sqldrivers/libqsqlite.dylib",
        "standard_of_iron.app/Contents/_CodeSignature/CodeResources",
        "standard_of_iron.app/Contents/Resources/LICENSE",
        "standard_of_iron.app/Contents/Resources/THIRD_PARTY_LICENSES.md",
        "standard_of_iron.app/Contents/Resources/assets/audio/audio_manifest.json",
        "standard_of_iron.app/Contents/Resources/assets/campaigns/second_punic_war.json",
        "standard_of_iron.app/Contents/Resources/assets/creatures/horse/horse.cmesh",
    ],
}

# This file exists only in a Developer-ID-signed, notarized and stapled
# bundle, because `xcrun stapler staple` writes the ticket here. An ad-hoc CI
# build has none.
MACOS_STAPLED_TICKET = "standard_of_iron.app/Contents/CodeResources"

# Libraries the Linux payload must carry itself rather than borrow from the
# host, or from the Steam Linux Runtime, which does not ship Qt.
LINUX_REQUIRED_GLOBS = ["usr/lib/libQt6Core.so*", "usr/lib/libQt6Quick.so*"]

# The only executable a Windows depot may carry. Anything else (bpat_baker.exe,
# test binaries) is a build tool that leaked out of build/bin.
WINDOWS_EXECUTABLES = {"standard_of_iron.exe"}

# In the GitHub ZIP, left out of the Steam depot. Steam installs the MSVC
# runtime itself (Steamworks, Installation > Redistributables), so the 18 MB
# installer windeployqt adds would only sit unused in every player's install.
WINDOWS_STEAM_OMITTED = {"vc_redist.x64.exe"}

# Never in a customer depot. Matched against every path component's name.
FORBIDDEN_NAMES = [
    # signing material and credentials
    "*.p12",
    "*.pfx",
    "*.pem",
    "*.key",
    "*.cer",
    "*.mobileprovision",
    "*.keychain",
    "*.keychain-db",
    "*.provisionprofile",
    "config.vdf",
    "ssfn*",
    ".env",
    "*.env",
    # Qt's Direct3D 12 shader compiler: the game renders only through OpenGL
    "dxcompiler.dll",
    "dxil.dll",
    # debug symbols -- kept as private CI artifacts instead
    "*.pdb",
    "*.dSYM",
    "*.debug",
    "*.ilk",
    # build trees and developer detritus. Not *.obj: the campaign map ships
    # Wavefront meshes under that extension.
    "CMakeCache.txt",
    "CMakeFiles",
    "cmake_install.cmake",
    ".ninja_log",
    ".ninja_deps",
    "*.o",
    "*.log",
    "runlog.txt",
    ".git",
    ".gitignore",
    "__pycache__",
    "*.pyc",
    ".DS_Store",
    "__MACOSX",
    "._*",
    # Player data. A save database in the install directory would be served
    # to every customer and flagged by Steam's file verification.
    "*.sqlite",
    "*.sqlite-wal",
    "*.sqlite-shm",
]


@dataclass
class Report:
    errors: list[str] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)

    def error(self, message: str) -> None:
        self.errors.append(message)

    def note(self, message: str) -> None:
        self.notes.append(message)


def load_config(path: Path | None = None) -> dict:
    with open(path or STEAM_DIR / "steam.json", encoding="utf-8") as handle:
        return json.load(handle)


# --------------------------------------------------------------------- stage


def stage(platform: str, package: Path, out: Path) -> None:
    if not package.is_file():
        raise SystemExit(f"error: package not found: {package}")
    if out.exists() and any(out.iterdir()):
        raise SystemExit(f"error: {out} is not empty; stage into a fresh directory")
    out.mkdir(parents=True, exist_ok=True)

    if platform == "windows":
        stage_windows(package, out)
    elif platform == "linux":
        stage_linux(package, out)
    elif platform == "macos":
        stage_macos(package, out)
    else:
        raise SystemExit(f"error: unknown platform {platform}")
    print(f"staged {platform}: {package.name} -> {out}")


def stage_windows(package: Path, out: Path) -> None:
    with zipfile.ZipFile(package) as archive:
        for member in archive.infolist():
            target = (out / member.filename).resolve()
            if not target.is_relative_to(out.resolve()):
                raise SystemExit(f"error: {member.filename} escapes the stage")
        members = [
            member
            for member in archive.infolist()
            if member.filename.rstrip("/") not in WINDOWS_STEAM_OMITTED
        ]
        archive.extractall(out, members)


def stage_linux(package: Path, out: Path) -> None:
    # --appimage-extract runs the AppImage's own runtime. It needs neither FUSE
    # nor root, and reproduces the AppDir byte for byte, including symlinks
    # and executable bits.
    package = package.resolve()
    package.chmod(package.stat().st_mode | stat.S_IXUSR)
    with tempfile.TemporaryDirectory() as scratch:
        subprocess.run(
            [str(package), "--appimage-extract"],
            cwd=scratch,
            check=True,
            stdout=subprocess.DEVNULL,
        )
        root = Path(scratch) / "squashfs-root"
        for entry in root.iterdir():
            shutil.move(str(entry), out / entry.name)


def stage_macos(package: Path, out: Path) -> None:
    # tar, not Python's tarfile. Framework bundles are full of symlinks
    # (Versions/Current, Headers, Resources), and codesign treats a
    # dereferenced one as a modified bundle.
    subprocess.run(["tar", "-xzf", str(package.resolve()), "-C", str(out)], check=True)


# -------------------------------------------------------------------- verify


def forbidden(name: str) -> str | None:
    for pattern in FORBIDDEN_NAMES:
        if fnmatch.fnmatchcase(name, pattern):
            return pattern
    return None


def is_executable(path: Path) -> bool:
    return bool(path.stat().st_mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def walk(root: Path):
    for directory, dirnames, filenames in os.walk(root, followlinks=False):
        base = Path(directory)
        for name in sorted(dirnames):
            yield base / name
        for name in sorted(filenames):
            yield base / name


def tree_manifest(root: Path) -> tuple[list[dict], int]:
    """Every file and symlink under root, with content hashes and modes."""
    entries = []
    total = 0
    for path in walk(root):
        relative = path.relative_to(root).as_posix()
        if path.is_symlink():
            entries.append({"path": relative, "symlink": os.readlink(path)})
            continue
        if path.is_dir():
            continue
        info = path.stat()
        total += info.st_size
        entries.append(
            {
                "path": relative,
                "size": info.st_size,
                "mode": oct(stat.S_IMODE(info.st_mode)),
                "sha256": sha256(path),
            }
        )
    return entries, total


def verify(
    platform: str,
    stage_dir: Path,
    config: dict,
    *,
    allow_unsigned: bool = False,
    manifest: Path | None = None,
) -> Report:
    report = Report()
    if not stage_dir.is_dir():
        report.error(f"stage directory {stage_dir} does not exist")
        return report

    depot = config["depots"][platform]
    root = stage_dir.resolve()

    for relative in REQUIRED[platform]:
        if not (stage_dir / relative).exists():
            report.error(f"missing required file: {relative}")
    if platform == "linux":
        for pattern in LINUX_REQUIRED_GLOBS:
            if not list(stage_dir.glob(pattern)):
                report.error(f"missing bundled library: {pattern}")

    verify_launch(platform, stage_dir, depot, report)

    if platform == "macos":
        verify_macos_bundle(stage_dir, report, allow_unsigned=allow_unsigned)

    for path in walk(stage_dir):
        relative = path.relative_to(stage_dir).as_posix()
        pattern = forbidden(path.name)
        if pattern is not None:
            report.error(f"must not ship: {relative} (matches {pattern})")
        if (
            platform == "windows"
            and path.suffix.lower() == ".exe"
            and relative not in WINDOWS_EXECUTABLES
        ):
            report.error(f"must not ship: {relative} (build tool, not the game)")

        if path.is_symlink():
            target = os.readlink(path)
            resolved = (path.parent / target).resolve()
            if os.path.isabs(target) or not resolved.is_relative_to(root):
                report.error(f"symlink escapes the depot: {relative} -> {target}")
            elif not resolved.exists():
                report.error(f"dangling symlink: {relative} -> {target}")

    entries, total = tree_manifest(stage_dir)
    if total < MINIMUM_DEPOT_BYTES:
        report.error(
            f"depot is implausibly small ({total} bytes < {MINIMUM_DEPOT_BYTES}); "
            "the package was probably truncated or lost its assets"
        )
    report.note(f"{len(entries)} entries, {total} bytes")

    if manifest is not None:
        manifest.parent.mkdir(parents=True, exist_ok=True)
        manifest.write_text(
            json.dumps(
                {"platform": platform, "total_bytes": total, "files": entries},
                indent=1,
            )
            + "\n",
            encoding="utf-8",
        )
    return report


def verify_launch(platform: str, stage_dir: Path, depot: dict, report: Report) -> None:
    launch = stage_dir / depot["launch"]
    if not launch.exists():
        report.error(f"launch target does not exist: {depot['launch']}")
        return

    if depot.get("working_dir"):
        if not (stage_dir / depot["working_dir"]).is_dir():
            report.error(f"working directory does not exist: {depot['working_dir']}")

    if platform == "linux":
        # SteamPipe carries the executable bit through from a Linux upload,
        # but it cannot restore one the stage already lost.
        if launch.is_symlink() or not launch.is_file():
            report.error(
                f"Linux launch target is not a regular file: {depot['launch']}"
            )
        elif not is_executable(launch):
            report.error(f"Linux launch target is not executable: {depot['launch']}")
        with open(launch, "rb") as handle:
            if handle.read(4) != b"\x7fELF":
                report.error(
                    f"Linux launch target is not an ELF binary: {depot['launch']}"
                )
    elif platform == "windows":
        with open(launch, "rb") as handle:
            if handle.read(2) != b"MZ":
                report.error(
                    f"Windows launch target is not a PE binary: {depot['launch']}"
                )
    elif platform == "macos":
        # Steam must launch the bundle, so LaunchServices applies its
        # Info.plist. Launching the inner Mach-O directly loses that.
        if not depot["launch"].endswith(".app") or not launch.is_dir():
            report.error(
                f"macOS launch target must be an .app bundle: {depot['launch']}"
            )


def verify_macos_bundle(
    stage_dir: Path, report: Report, *, allow_unsigned: bool
) -> None:
    app = stage_dir / "standard_of_iron.app"
    plist_path = app / "Contents" / "Info.plist"
    if plist_path.is_file():
        with open(plist_path, "rb") as handle:
            info = plistlib.load(handle)
        executable = info.get("CFBundleExecutable", "")
        binary = app / "Contents" / "MacOS" / executable
        if not executable or not binary.is_file():
            report.error(f"CFBundleExecutable '{executable}' is not in Contents/MacOS")
        elif not is_executable(binary):
            report.error(f"Contents/MacOS/{executable} is not executable")
        identifier = info.get("CFBundleIdentifier", "")
        if not identifier or identifier.startswith("com.yourcompany."):
            report.error(
                f"CFBundleIdentifier '{identifier}' is Qt's placeholder or empty; "
                "set MACOSX_BUNDLE_GUI_IDENTIFIER"
            )

    framework = app / "Contents" / "Frameworks" / "QtCore.framework"
    if framework.is_dir() and not (framework / "Versions" / "Current").is_symlink():
        report.error(
            "QtCore.framework/Versions/Current is not a symlink. The bundle was "
            "copied with symlinks dereferenced, and its signature no longer holds"
        )

    if not (stage_dir / MACOS_STAPLED_TICKET).is_file():
        message = (
            "no stapled notarization ticket (Contents/CodeResources). This is an "
            "ad-hoc CI build, not a Developer-ID-signed and notarized app"
        )
        if allow_unsigned:
            report.note(f"allowed: {message}")
        else:
            report.error(message)


# ----------------------------------------------------------------------- vdf


def vdf_escape(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def render_vdf(
    config: dict,
    *,
    stage_root: Path,
    platforms: list[str],
    desc: str,
    branch: str,
    preview: bool,
    out: Path,
) -> Path:
    app_id = int(config.get("app_id") or 0)
    if app_id <= 0:
        raise SystemExit("error: steam/steam.json has no app_id yet")
    if branch.strip().lower() in PUBLIC_BRANCHES:
        raise SystemExit(
            f"error: refusing to set a build live on '{branch}'. Upload to a "
            "beta branch and promote the tested BuildID in Steamworks."
        )
    if not platforms:
        raise SystemExit("error: no platforms selected")

    out.mkdir(parents=True, exist_ok=True)
    depot_template = Template(
        (STEAM_DIR / "depot_build.vdf.in").read_text(encoding="utf-8")
    )
    app_template = Template(
        (STEAM_DIR / "app_build.vdf.in").read_text(encoding="utf-8")
    )

    depot_lines = []
    for platform in platforms:
        depot_id = int(config["depots"][platform].get("depot_id") or 0)
        if depot_id <= 0:
            raise SystemExit(f"error: steam/steam.json has no depot_id for {platform}")
        content = (stage_root / platform).resolve()
        if not content.is_dir():
            raise SystemExit(f"error: no staged {platform} depot at {content}")
        script = out / f"depot_build_{depot_id}.vdf"
        script.write_text(
            depot_template.substitute(
                depot_id=depot_id, content_root=vdf_escape(str(content))
            ),
            encoding="utf-8",
        )
        depot_lines.append(f'\t\t"{depot_id}" "{vdf_escape(str(script.resolve()))}"')

    build_output = (out / "output").resolve()
    build_output.mkdir(parents=True, exist_ok=True)
    app_script = out / f"app_build_{app_id}.vdf"
    app_script.write_text(
        app_template.substitute(
            app_id=app_id,
            desc=vdf_escape(desc),
            preview="1" if preview else "0",
            set_live=vdf_escape(branch),
            content_root=vdf_escape(str(stage_root.resolve())),
            build_output=vdf_escape(str(build_output)),
            depots="\n".join(depot_lines),
        ),
        encoding="utf-8",
    )
    return app_script


# ------------------------------------------------------------------ build-id

BUILD_ID_PATTERN = re.compile(
    r"Successfully finished AppID \d+ build \(BuildID (\d+)\)"
)


def parse_build_id(log_text: str) -> str | None:
    matches = BUILD_ID_PATTERN.findall(log_text)
    return matches[-1] if matches else None


# ----------------------------------------------------------------------- cli


def parse_platforms(value: str) -> list[str]:
    platforms = [item.strip() for item in value.split(",") if item.strip()]
    for platform in platforms:
        if platform not in PLATFORMS:
            raise SystemExit(f"error: unknown platform '{platform}'")
    return platforms


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--config", type=Path, default=None)
    commands = parser.add_subparsers(dest="command", required=True)

    stage_cmd = commands.add_parser("stage")
    stage_cmd.add_argument("--platform", choices=PLATFORMS, required=True)
    stage_cmd.add_argument("--package", type=Path, required=True)
    stage_cmd.add_argument("--out", type=Path, required=True)

    verify_cmd = commands.add_parser("verify")
    verify_cmd.add_argument("--platform", choices=PLATFORMS, required=True)
    verify_cmd.add_argument("--stage", type=Path, required=True)
    verify_cmd.add_argument("--manifest", type=Path)
    verify_cmd.add_argument(
        "--allow-unsigned",
        action="store_true",
        help="accept an ad-hoc macOS bundle (local dry runs only; never for upload)",
    )

    vdf_cmd = commands.add_parser("vdf")
    vdf_cmd.add_argument("--stage-root", type=Path, required=True)
    vdf_cmd.add_argument("--platforms", default=",".join(PLATFORMS))
    vdf_cmd.add_argument("--desc", required=True)
    vdf_cmd.add_argument("--branch", default=None)
    vdf_cmd.add_argument("--preview", action="store_true")
    vdf_cmd.add_argument("--out", type=Path, required=True)

    manifest_cmd = commands.add_parser(
        "manifest", help="print a tree's files, symlinks, modes and hashes as JSON"
    )
    manifest_cmd.add_argument("root", type=Path)

    build_id_cmd = commands.add_parser("build-id")
    build_id_cmd.add_argument("log", type=Path)

    args = parser.parse_args(argv)
    config = load_config(args.config)

    if args.command == "stage":
        stage(args.platform, args.package, args.out)
        return 0

    if args.command == "verify":
        report = verify(
            args.platform,
            args.stage,
            config,
            allow_unsigned=args.allow_unsigned,
            manifest=args.manifest,
        )
        for note in report.notes:
            print(f"note: {note}")
        for error in report.errors:
            print(f"error: {error}", file=sys.stderr)
        status = "FAIL" if report.errors else "PASS"
        print(f"STEAM_STAGE_VERIFY {args.platform}: {status}")
        return 1 if report.errors else 0

    if args.command == "vdf":
        branch = args.branch if args.branch is not None else config.get("rc_branch", "")
        script = render_vdf(
            config,
            stage_root=args.stage_root,
            platforms=parse_platforms(args.platforms),
            desc=args.desc,
            branch=branch,
            preview=args.preview,
            out=args.out,
        )
        print(script)
        return 0

    if args.command == "manifest":
        entries, total = tree_manifest(args.root)
        print(json.dumps({"total_bytes": total, "files": entries}, indent=1))
        return 0

    if args.command == "build-id":
        build_id = parse_build_id(
            args.log.read_text(encoding="utf-8", errors="replace")
        )
        if build_id is None:
            print("error: no BuildID in the SteamCMD log", file=sys.stderr)
            return 1
        print(build_id)
        return 0

    return 2


if __name__ == "__main__":
    sys.exit(main())
