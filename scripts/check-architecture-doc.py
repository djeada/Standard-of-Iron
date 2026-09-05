#!/usr/bin/env python3
"""Fail when docs/ARCHITECTURE.md states a number the repository disagrees with.

The architecture document carried a count of ambient `instance()` call sites for
months after the number changed, and claimed the renderer links `game_systems`
after the root CMake had been changed to link `game_sim`. Both were read as
current by anyone planning work off the document.

A count in prose is only trustworthy if something recomputes it. This checks the
claims that have a machine-readable source:

  - the ambient call-site total and its per-directory breakdown against
    scripts/ambient_instance_budget.json
  - the full-world scan total and the loop-nested count against
    scripts/world_scan_budget.json and scripts/world_scan_nested_allow.json
  - the target the document says `render_gl` links against the root CMakeLists

Claims with no such source -- how a subsystem is meant to be used, why a split
exists -- are out of scope here and stay the reader's job.

  usage: check-architecture-doc.py [repo-root]
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

DOC = "docs/ARCHITECTURE.md"
AMBIENT_BUDGET = "scripts/ambient_instance_budget.json"
SCAN_BUDGET = "scripts/world_scan_budget.json"
NESTED_ALLOW = "scripts/world_scan_nested_allow.json"
ROOT_CMAKE = "CMakeLists.txt"

AMBIENT_TOTAL = re.compile(
    r"(\d+) call sites still reach per-match state through the ambient"
)
AMBIENT_ENTRY = re.compile(r"`([a-z_]+/[a-z_]+)`\s+(\d+)")
SCAN_TOTAL = re.compile(r"(\d+) full-world entity scans remain, of which (\d+)")
RENDER_LINK = re.compile(r"`render_gl` links `([a-z_]+)`")
CMAKE_RENDER_LINK = re.compile(
    r"target_link_libraries\(\s*render_gl\s+PUBLIC\s+([a-z_]+)\s*\)"
)


def load(root: Path, name: str):
    return json.loads((root / name).read_text())


def check(root: Path) -> list[str]:
    problems: list[str] = []
    doc = (root / DOC).read_text()

    ambient = load(root, AMBIENT_BUDGET)
    match = AMBIENT_TOTAL.search(doc)
    if match is None:
        problems.append(
            f"{DOC}: no sentence states the ambient call-site total; "
            f"{AMBIENT_BUDGET} records {sum(ambient.values())}"
        )
    elif int(match.group(1)) != sum(ambient.values()):
        problems.append(
            f"{DOC}: says {match.group(1)} ambient call sites, "
            f"{AMBIENT_BUDGET} records {sum(ambient.values())}"
        )
    else:
        stop = doc.find("\n- ", match.start())
        sentence = doc[match.start() : stop if stop != -1 else len(doc)]
        stated = {k: int(v) for k, v in AMBIENT_ENTRY.findall(sentence)}
        for directory, count in sorted(ambient.items()):
            if stated.get(directory) != count:
                problems.append(
                    f"{DOC}: says {directory} has {stated.get(directory)} "
                    f"ambient call sites, the budget records {count}"
                )
        for directory in sorted(set(stated) - set(ambient)):
            problems.append(
                f"{DOC}: lists {directory} in the ambient breakdown, "
                f"{AMBIENT_BUDGET} does not"
            )

    scans = load(root, SCAN_BUDGET)
    nested = load(root, NESTED_ALLOW)
    match = SCAN_TOTAL.search(doc)
    if match is None:
        problems.append(f"{DOC}: no sentence states the full-world scan total")
    else:
        if int(match.group(1)) != sum(scans.values()):
            problems.append(
                f"{DOC}: says {match.group(1)} full-world scans, "
                f"{SCAN_BUDGET} records {sum(scans.values())}"
            )
        if int(match.group(2)) != len(nested):
            problems.append(
                f"{DOC}: says {match.group(2)} loop-nested scans, "
                f"{NESTED_ALLOW} records {len(nested)}"
            )

    cmake = (root / ROOT_CMAKE).read_text()
    linked = CMAKE_RENDER_LINK.search(cmake)
    stated_link = RENDER_LINK.search(doc)
    if linked is None:
        problems.append(f"{ROOT_CMAKE}: no PUBLIC link line found for render_gl")
    elif stated_link is None:
        problems.append(
            f"{DOC}: does not say what render_gl links; "
            f"{ROOT_CMAKE} links {linked.group(1)}"
        )
    elif stated_link.group(1) != linked.group(1):
        problems.append(
            f"{DOC}: says render_gl links {stated_link.group(1)}, "
            f"{ROOT_CMAKE} links {linked.group(1)}"
        )

    return problems


def main(argv: list[str]) -> int:
    root = (
        Path(argv[1]).resolve()
        if len(argv) > 1
        else Path(__file__).resolve().parents[1]
    )
    problems = check(root)
    if problems:
        print("check-architecture-doc: the document disagrees with the repository:")
        for problem in problems:
            print(f"  {problem}")
        print(
            "\nUpdate the sentence in the same change that moved the number, or "
            "rerun the\nguard with --write and copy the new counts across."
        )
        return 1
    print("check-architecture-doc: ok (counts and link edges match)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
