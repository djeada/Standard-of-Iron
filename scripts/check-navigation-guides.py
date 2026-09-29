#!/usr/bin/env python3
"""Fail when a navigation guide names a call-path step the code no longer has.

app/README.md and game/README.md carry worked call paths whose steps are written
as a file and a symbol: `path/to/file.cpp` `Class::function`. A step is stale
when the file is gone or the symbol's final name no longer appears in it. The
guides must also be linked from docs/ARCHITECTURE.md.

  usage: check-navigation-guides.py [repo-root]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

GUIDES = ("app/README.md", "game/README.md")
STEP = re.compile(r"`([\w./-]+\.(?:cpp|h|qml))`\s+`([\w:~]+)`")


def check(root: Path) -> list[str]:
    problems: list[str] = []
    architecture = (root / "docs/ARCHITECTURE.md").read_text()
    for guide in GUIDES:
        path = root / guide
        if not path.exists():
            problems.append(f"{guide}: missing")
            continue
        if guide not in architecture:
            problems.append(f"docs/ARCHITECTURE.md: does not link {guide}")
        steps = STEP.findall(path.read_text())
        if not steps:
            problems.append(f"{guide}: no call-path steps found")
        for file, symbol in steps:
            source = root / file
            if not source.exists():
                problems.append(f"{guide}: {file} does not exist")
                continue
            name = symbol.split("::")[-1]
            if name not in source.read_text(errors="replace"):
                problems.append(f"{guide}: {file} no longer mentions {symbol}")
    return problems


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    problems = check(root)
    for problem in problems:
        print(problem, file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
