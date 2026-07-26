#!/usr/bin/env python3
"""Reject source files that still carry "do not commit" markers.

This is deliberately narrow: it only flags artefacts that are never meant to
reach main - unresolved merge conflicts, interactive debugger hooks and
explicit NOCOMMIT annotations.  Ordinary TODO/FIXME comments are fine.

Used by the pre-commit hook and by `make quality`.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

PATTERNS: tuple[tuple[str, re.Pattern[str]], ...] = (
    ("unresolved merge conflict", re.compile(r"^(<{7}|={7}|>{7})(\s|$)")),
    (
        "explicit do-not-commit marker",
        re.compile(r"\bNOCOMMIT\b|\bDO\s*NOT\s*COMMIT\b", re.I),
    ),
    (
        "interactive debugger left in source",
        re.compile(r"^\s*(import\s+pdb\b|pdb\.set_trace\(|breakpoint\(\))"),
    ),
)

SKIP_PREFIXES = ("third_party/", "build/", "dist/")


def check(path: Path) -> list[str]:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return [f"{path}: cannot read: {exc}"]

    problems: list[str] = []
    for number, line in enumerate(text.splitlines(), start=1):
        for label, pattern in PATTERNS:
            if pattern.search(line):
                problems.append(f"{path}:{number}: {label}: {line.strip()[:80]}")
    return problems


def main(argv: list[str]) -> int:
    problems: list[str] = []
    for raw in argv:
        rel = raw.replace("\\", "/")
        if any(rel.startswith(prefix) for prefix in SKIP_PREFIXES):
            continue
        path = Path(raw)
        if path.is_file():
            problems.extend(check(path))

    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print(
            f"\n{len(problems)} quality marker(s) must be removed before committing.",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
