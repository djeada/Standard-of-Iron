#!/usr/bin/env python3
"""Fail the build when a render source file grows past the size limit.

Large translation units hide several jobs in one file and recompile slowly.
Every file under render/ stays at or under LIMIT lines. A file may go over
only when it is listed in ALLOWED with the reason it has to stay whole; an
entry whose file shrank back under the limit, or no longer exists, fails too,
so the list cannot go stale.

  usage: check-render-file-size.py [repo-root]
"""

from __future__ import annotations

import sys
from pathlib import Path

LIMIT = 1000

FOLDERS: tuple[str, ...] = ("render",)

SOURCE_SUFFIXES = (".h", ".cpp")

ALLOWED: dict[str, str] = {
    "render/creature/humanoid/asset/humanoid_clip_table.h": (
        "one authored constexpr table of every humanoid baker clip, in clip-id "
        "order; splitting it would scatter a list read and reviewed as a whole"
    ),
}


def check(root: Path) -> list[str]:
    problems: list[str] = []
    seen: set[str] = set()
    for folder in FOLDERS:
        base = root / folder
        if not base.is_dir():
            continue
        for source in sorted(base.rglob("*")):
            if source.suffix not in SOURCE_SUFFIXES:
                continue
            relative = source.relative_to(root).as_posix()
            lines = len(source.read_text(errors="replace").splitlines())
            if lines <= LIMIT:
                continue
            seen.add(relative)
            if relative not in ALLOWED:
                problems.append(
                    f"{relative}: {lines} lines, over the {LIMIT}-line limit; split "
                    "it along its stages, or list it in ALLOWED with the reason it "
                    "must stay whole"
                )
    for relative in sorted(ALLOWED):
        if relative not in seen:
            problems.append(
                f"{relative}: listed in ALLOWED but not over {LIMIT} lines (or gone); "
                "remove the entry"
            )
    return problems


def main(argv: list[str]) -> int:
    root = Path(argv[1]) if len(argv) > 1 else Path(__file__).resolve().parent.parent
    problems = check(root.resolve())
    if problems:
        print("render file size violations:")
        for problem in problems:
            print(f"  {problem}")
        print(f"{len(problems)} violation(s). See {Path(__file__).name}.")
        return 1
    print(
        f"render file size: every file in {', '.join(FOLDERS)} is within {LIMIT} lines"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
