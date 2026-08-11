#!/usr/bin/env python3
"""Validate that every .qrc entry points at a file that exists on disk.

Qt's rcc turns each ``<file>`` entry into a required build input. When an asset
is deleted but its entry is left behind, the failure surfaces as a ninja error
about a missing input in an unrelated target's autogen directory, and it takes
down every target that embeds the .qrc -- before a single object is compiled.
That is what happened when the audio field overhaul deleted 13 obsolete
``_vN`` combat sound effects and left their entries in assets.qrc.

This check therefore has to run *without* a build: a compiled validator cannot
guard the thing that stops it from being compiled. It is wired into
``make quality`` (and so into ``make format``, which ends with it) and into the
compiler-free quality workflow.

Entry paths are resolved relative to the directory holding the .qrc, matching
rcc, so tests/test_assets.qrc reaching up with ``../assets/...`` validates
correctly.

Only the qrc -> disk direction is checked. The reverse is deliberately not an
error: assets/audio/ambience and assets/audio/music are streamed from disk
rather than embedded, so files legitimately live outside the .qrc.

Usage:
    python3 scripts/validate_qrc_resources.py [QRC ...]

With no arguments every tracked .qrc in the repository is checked.
"""

from __future__ import annotations

import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


class Colors:
    RED = "\033[0;31m"
    GREEN = "\033[0;32m"
    BLUE = "\033[0;34m"
    NC = "\033[0m"
    BOLD = "\033[1m"


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def tracked_qrc_files(root: Path) -> list[Path]:
    try:
        output = subprocess.run(
            ["git", "ls-files", "*.qrc"],
            cwd=root,
            capture_output=True,
            text=True,
            check=True,
        ).stdout
        paths = [root / line for line in output.splitlines() if line.strip()]
    except (OSError, subprocess.CalledProcessError):
        paths = []

    if not paths:
        paths = [p for p in root.rglob("*.qrc") if "build" not in p.parts]

    return sorted(paths)


def display_path(qrc_path: Path, root: Path) -> str:
    try:
        return str(qrc_path.relative_to(root))
    except ValueError:
        return str(qrc_path)


def check_qrc(qrc_path: Path, root: Path) -> tuple[list[str], int]:
    """Return the problems found in one .qrc file, and how many entries it has."""
    rel_qrc = display_path(qrc_path, root)

    try:
        tree = ET.parse(qrc_path)
    except ET.ParseError as exc:
        return [f"{rel_qrc}: not well-formed XML: {exc}"], 0
    except OSError as exc:
        return [f"{rel_qrc}: cannot read: {exc}"], 0

    problems: list[str] = []
    seen: set[str] = set()
    entries = 0

    for resource in tree.getroot().iter("qresource"):
        prefix = (resource.get("prefix") or "/").strip()
        for entry in resource.iter("file"):
            entries += 1
            raw = (entry.text or "").strip()
            if not raw:
                problems.append(f"{rel_qrc}: empty <file> entry")
                continue

            target = (qrc_path.parent / raw).resolve()
            if not target.is_file():
                problems.append(f"{rel_qrc}: <file>{raw}</file> does not exist on disk")

            resource_path = (
                f"{prefix.rstrip('/')}/{(entry.get('alias') or raw).lstrip('/')}"
            )
            if resource_path in seen:
                problems.append(
                    f"{rel_qrc}: duplicate resource :{resource_path} "
                    f"(also from <file>{raw}</file>)"
                )
            seen.add(resource_path)

    return problems, entries


def main(argv: list[str]) -> int:
    root = repo_root()
    qrc_files = [Path(a).resolve() for a in argv] if argv else tracked_qrc_files(root)

    if not qrc_files:
        print(f"{Colors.RED}No .qrc files found{Colors.NC}")
        return 1

    print(f"{Colors.BOLD}{Colors.BLUE}Validating .qrc resources{Colors.NC}")

    problems: list[str] = []
    entry_count = 0
    for qrc_path in qrc_files:
        found, entries = check_qrc(qrc_path, root)
        problems.extend(found)
        entry_count += entries

    if problems:
        for problem in problems:
            print(f"  {Colors.RED}{problem}{Colors.NC}")
        print(
            f"{Colors.RED}✗ {len(problems)} problem(s) across "
            f"{len(qrc_files)} .qrc file(s){Colors.NC}"
        )
        return 1

    print(
        f"{Colors.GREEN}✓ {entry_count} resource entries across "
        f"{len(qrc_files)} .qrc file(s) resolve on disk{Colors.NC}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
