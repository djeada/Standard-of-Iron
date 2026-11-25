#!/usr/bin/env python3
"""Proves that targets sharing a precompiled header can really use it.

GCC ignores a .gch whose macros, -fPIC or floating point flags differ from the
translation unit's, and it does so silently unless -Winvalid-pch is on. The
build sets PCH_WARN_INVALID off for shared PCHs, so a flag drifting apart would
only show up as a slower build. This compiles one translation unit of every
target that reuses a soi_pch_* owner with -fsyntax-only -Winvalid-pch and fails
on any warning about the header.

Usage: scripts/check-shared-pch.py [--build-dir build] [--jobs 4]
The owners must be built first: cmake --build <dir> --target soi_pch_plain
soi_pch_qt soi_pch_qt_test
"""

import argparse
import concurrent.futures
import json
import re
import shlex
import subprocess
import sys
from pathlib import Path

DROPPED = {"-MD", "-MMD", "-c"}
DROPPED_WITH_VALUE = {"-MT", "-MF", "-o"}


def target_of(entry):
    match = re.search(
        r"CMakeFiles/([^/]+)\.dir/", entry.get("output", entry["command"])
    )
    return match.group(1) if match else None


def check_command(entry):
    args = shlex.split(entry["command"])
    kept = []
    skip = False
    for arg in args:
        if skip:
            skip = False
        elif arg in DROPPED_WITH_VALUE:
            skip = True
        elif arg not in DROPPED:
            kept.append(arg)
    kept += ["-fsyntax-only", "-Winvalid-pch"]
    result = subprocess.run(
        kept, cwd=entry["directory"], capture_output=True, text=True, check=False
    )
    return [
        line
        for line in result.stderr.splitlines()
        if "Winvalid-pch" in line or "cmake_pch" in line and "warning" in line
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    build = Path(args.build_dir).resolve()
    commands = json.loads((build / "compile_commands.json").read_text())
    source_root = build.parent
    chosen = {}
    for entry in commands:
        command = entry["command"]
        match = re.search(r"-include (\S*soi_pch_\w+\.dir/cmake_pch\.hxx)", command)
        if not match or not entry["file"].startswith(str(source_root)):
            continue
        if str(build) in entry["file"]:
            continue
        chosen.setdefault(target_of(entry), entry)
    if not chosen:
        print("no target reuses a shared PCH; was the build configured with GCC?")
        return 1
    failures = 0
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        futures = {pool.submit(check_command, e): t for t, e in chosen.items()}
        for future in concurrent.futures.as_completed(futures):
            target = futures[future]
            problems = future.result()
            if problems:
                failures += 1
                print(f"FAIL {target}: {problems[0]}")
            else:
                print(f"ok   {target}")
    print(f"{len(chosen) - failures}/{len(chosen)} targets use their shared PCH")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
