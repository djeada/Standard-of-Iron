#!/usr/bin/env python3
"""Recompute the god-file inventory in docs/REFACTORING_INVENTORY.md.

Lists every C++ file under app/ and game/ at or above a physical-line
threshold, with its longest function and its distinct project include count.
Function extents are approximated from column-zero definitions closed by a
column-zero `}` (the layout clang-format produces), so the figures are review
signals, not a parse.

  usage: refactor-inventory.py [--threshold N] [--check] [repo-root]

With --check, exit 1 when the files listed in the inventory document differ
from the files at or above the threshold, or when a recorded line count is
stale by more than TOLERANCE lines.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

DOC = "docs/REFACTORING_INVENTORY.md"
ROOTS = ("app", "game")
TOLERANCE = 0.10
DEFINITION = re.compile(
    r"^[A-Za-z_~][^;]*\)\s*(const\s*)?(noexcept\s*)?(->\s*[^{]+)?\{\s*$"
)
NAME = re.compile(r"([A-Za-z_~][A-Za-z_0-9:~]*)\s*\(")
PROJECT_INCLUDE = re.compile(r'^#include\s+"([^"]+)"')
ROW = re.compile(r"^\|\s*`([^`]+\.(?:cpp|h))`\s*\|")


SKIP_START = (
    "namespace",
    "struct",
    "class",
    "enum",
    "extern",
    "using",
    "typedef",
    "union",
    "static_assert",
)


def longest_function(lines: list[str]) -> tuple[str, int]:
    """A definition opens at a column-zero line that follows a blank line, a
    comment, a preprocessor line or a closing brace, and ends at the next
    column-zero `}`. Multi-line signatures are therefore handled."""
    best = ("-", 0)
    start = None
    name = "-"
    for index, line in enumerate(lines):
        if start is None:
            if not line or not (line[0].isalpha() or line[0] == "_"):
                continue
            if line.startswith(SKIP_START) or line.rstrip().endswith(";"):
                continue
            previous = lines[index - 1] if index else ""
            if (
                previous
                and not previous.startswith(("}", "#", "//", "*/", "template", "[["))
                and not previous.rstrip().endswith(("*/", "{", ";"))
            ):
                continue
            found = NAME.search(line)
            start, name = index, found.group(1) if found else "?"
        elif line.startswith("}"):
            length = index - start + 1
            if length > best[1]:
                best = (name, length)
            start = None
    return best


def scan(root: Path, threshold: int) -> list[dict]:
    rows = []
    for top in ROOTS:
        for path in sorted((root / top).rglob("*")):
            if path.suffix not in (".cpp", ".h"):
                continue
            lines = path.read_text(errors="replace").splitlines()
            if len(lines) < threshold:
                continue
            fn, fn_len = longest_function(lines)
            includes = {
                m.group(1).split("/")[0]
                for line in lines
                if (m := PROJECT_INCLUDE.match(line))
            }
            rows.append(
                {
                    "path": path.relative_to(root).as_posix(),
                    "lines": len(lines),
                    "fn": fn,
                    "fn_len": fn_len,
                    "includes": sum(1 for line in lines if PROJECT_INCLUDE.match(line)),
                    "areas": len(includes),
                }
            )
    rows.sort(key=lambda r: (-r["lines"], r["path"]))
    return rows


def render(rows: list[dict]) -> str:
    out = [
        "| File | Lines | Longest function (lines) | Project includes | Include areas |",
        "| --- | ---: | --- | ---: | ---: |",
    ]
    for r in rows:
        out.append(
            f"| `{r['path']}` | {r['lines']:,} | `{r['fn']}` ({r['fn_len']}) | {r['includes']} | {r['areas']} |"
        )
    return "\n".join(out)


def check(root: Path, threshold: int) -> int:
    rows = {r["path"]: r for r in scan(root, threshold)}
    documented = {}
    for line in (root / DOC).read_text().splitlines():
        m = ROW.match(line)
        if m and m.group(1).startswith(ROOTS):
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            documented[m.group(1)] = (cells[2], cells[-1])
    problems = []
    for path in sorted(rows):
        entry = documented.get(path)
        if entry is None:
            problems.append(
                f"{DOC}: {path} has {rows[path]['lines']} lines and is not in the inventory"
            )
            continue
        after, status = entry
        if status.startswith("done"):
            problems.append(
                f"{DOC}: {path} is marked done but still has {rows[path]['lines']} lines"
            )
        stated = (
            int(after.replace(",", "")) if after.replace(",", "").isdigit() else None
        )
        actual = rows[path]["lines"]
        if stated is not None and abs(actual - stated) > stated * TOLERANCE:
            problems.append(
                f"{DOC}: {path} says {stated} lines after, the file has {actual}"
            )
    for p in problems:
        print(p, file=sys.stderr)
    return 1 if problems else 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--threshold", type=int, default=1000)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("root", nargs="?", default=".")
    args = ap.parse_args()
    root = Path(args.root).resolve()
    if args.check:
        return check(root, args.threshold)
    print(render(scan(root, args.threshold)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
