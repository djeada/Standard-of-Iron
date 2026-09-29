#!/usr/bin/env python3
"""Fail when a C++ declaration uses a Qt keyword macro as an identifier.

Without QT_NO_KEYWORDS, `slots`, `signals`, `emit` and `foreach` are macros.
`signals` and `slots` expand to access specifiers and `emit` to nothing, so a
parameter named `slots` compiles as an unnamed parameter and `m_slots(slots)`
silently initialises the member to nullptr. That shipped once as a crash ten
minutes into a match.

  usage: check-qt-keyword-identifiers.py [repo-root]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOTS = ("app", "game", "render", "scene", "ui", "tools", "tests")
KEYWORDS = "slots|signals|emit|foreach|forever"
DECLARATION = re.compile(
    rf"(?:[\w:>]+[*&]+\s*|[\w:>]+\s+[*&]*)\b({KEYWORDS})\b\s*(?:[,)=;{{]|\[)"
)
INITIALISER = re.compile(rf"\bm_\w+\(\s*({KEYWORDS})\s*\)")
SKIP_PREFIXES = (
    "emit ",
    "signals:",
    "public slots",
    "private slots",
    "protected slots",
)


def check(root: Path) -> list[str]:
    problems: list[str] = []
    for top in ROOTS:
        base = root / top
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix not in (".cpp", ".h") or {
                "third_party",
                "venv",
                ".venv",
                "site-packages",
            } & set(path.parts):
                continue
            for number, line in enumerate(
                path.read_text(errors="replace").splitlines(), 1
            ):
                stripped = line.strip()
                if stripped.startswith(("//", "*", "#")) or stripped.startswith(
                    SKIP_PREFIXES
                ):
                    continue
                if re.search(r"\bQ_(?:SIGNALS|SLOTS|EMIT)\b", line):
                    continue
                if INITIALISER.search(line) or (
                    DECLARATION.search(line) and not stripped.startswith(("emit",))
                ):
                    if re.match(
                        rf"\s*(?:public|private|protected)?\s*(?:{KEYWORDS})\s*:", line
                    ):
                        continue
                    problems.append(
                        f"{path.relative_to(root)}:{number}: {stripped[:100]}"
                    )
    return problems


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    problems = check(root)
    for problem in problems:
        print(f"Qt keyword macro used as an identifier: {problem}", file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
