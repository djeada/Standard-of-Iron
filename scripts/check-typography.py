#!/usr/bin/env python3
"""Reject hard-coded font sizes in QML.

Gameplay text has to follow the interface scale a player sets in Settings, and
it has to stay above a legibility floor at the smallest scale. Neither happens
when a screen writes ``font.pixelSize: 9``: the literal is frozen at whatever
the author's monitor made look right. Every font size therefore has to come
from ``Design.Typography`` -- a named rung, or ``Typography.scaled()`` /
``Typography.display()`` for the few sizes that are genuinely computed.

Point sizes are rejected outright. Qt resolves them against the screen's
reported DPI, so the same declaration renders at a different size than the
pixel-based design tokens do, and the two ladders drift apart.

Used by the pre-commit hook and by ``make quality``.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

DECLARATION = re.compile(r"\bfont\.(pixelSize|pointSize)\s*:\s*(.+?)\s*(?://.*)?$")


BARE_NUMBER = re.compile(r"(?<![\w.])\d+(?![\w.])")

TOKEN_SOURCE = re.compile(r"\bTypography\.\w+")

GUIDANCE = (
    "\nFont sizes come from Design.Typography, never from a literal.\n"
    "  font.pixelSize: Design.Typography.body        -- a named rung\n"
    "  font.pixelSize: Design.Typography.scaled(15)  -- computed, with the "
    "legibility floor\n"
    "  font.pixelSize: Design.Typography.display(54) -- computed display text, "
    "no floor\n"
    "The rungs live in ui/qml/design/Typography.qml; add one there rather than "
    "reaching for a literal.\n"
)


def check(path: Path) -> list[str]:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return [f"{path}: cannot read: {exc}"]

    problems: list[str] = []
    for number, line in enumerate(text.splitlines(), start=1):
        match = DECLARATION.search(line)
        if match is None:
            continue

        prop, expression = match.groups()
        where = f"{path}:{number}"

        if prop == "pointSize":
            problems.append(
                f"{where}: font.pointSize is DPI-dependent; use font.pixelSize "
                f"with a Design.Typography rung: {expression}"
            )
            continue

        if TOKEN_SOURCE.search(expression):
            continue

        if BARE_NUMBER.search(expression):
            problems.append(f"{where}: hard-coded font size: {expression}")

    return problems


def main(argv: list[str]) -> int:
    paths = [Path(arg) for arg in argv] or sorted(Path("ui/qml").rglob("*.qml"))

    problems: list[str] = []
    for path in paths:
        if path.suffix != ".qml" or not path.is_file():
            continue
        problems.extend(check(path))

    for problem in problems:
        print(problem, file=sys.stderr)

    if problems:
        print(GUIDANCE, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
