#!/usr/bin/env python3
"""Symbolize the playable GL creation sites recorded in a frame-pacing report.

The runtime records raw return addresses so that tracing costs nothing at
capture time. Run this against the same executable that produced the report.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

FRAME = re.compile(
    r"^(?P<object>[^(]+)\((?:(?P<symbol>[^+)]*))?\+(?P<offset>0x[0-9a-f]+)\)"
)


def symbolize(binary: Path, offsets: list[str]) -> dict[str, str]:
    if not offsets:
        return {}
    try:
        result = subprocess.run(
            ["addr2line", "-f", "-C", "-p", "-e", str(binary), *offsets],
            capture_output=True,
            text=True,
            timeout=60,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        return {offset: f"<addr2line failed: {error}>" for offset in offsets}
    lines = result.stdout.splitlines()
    return {
        offset: (lines[index].strip() if index < len(lines) else "<no symbol>")
        for index, offset in enumerate(offsets)
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=6)
    args = parser.parse_args()

    try:
        report = json.loads(args.report.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        print(f"cannot read report: {error}", file=sys.stderr)
        return 2
    sites = report.get("playable_gl_creation_sites")
    if not isinstance(sites, list) or not sites:
        print(
            "the report has no playable_gl_creation_sites; rerun with "
            "SOI_TRACE_PLAYABLE_GL=1",
            file=sys.stderr,
        )
        return 1

    binary_name = args.binary.name
    offsets = []
    for site in sites:
        for frame in site.get("stack", [])[: args.frames]:
            match = FRAME.match(frame)
            if match and Path(match.group("object")).name == binary_name:
                offsets.append(match.group("offset"))
    resolved = symbolize(args.binary, sorted(set(offsets)))

    total = 0
    for site in sites:
        count = int(site.get("count", 0))
        total += count
        print(f"\n{site.get('kind', '?')} x{count}")
        for frame in site.get("stack", [])[: args.frames]:
            match = FRAME.match(frame)
            if match and Path(match.group("object")).name == binary_name:
                print(f"    {resolved.get(match.group('offset'), frame)}")
            else:
                print(f"    {frame}")
    print(f"\n{len(sites)} sites, {total} recorded creations")
    return 0


if __name__ == "__main__":
    sys.exit(main())
