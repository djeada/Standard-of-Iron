#!/usr/bin/env python3
"""Fill the source-language catalogue with identity translations.

app_en.ts is the source language, so every entry's translation is its own
source text. lupdate cannot know that and leaves new entries `unfinished`,
which trips the coverage gate in `make translations-check` for strings that
have nothing left to translate.

Run this straight after lupdate, before lrelease.

Implementation note: this rewrites the raw XML rather than parsing and
re-serialising it. An ElementTree round-trip is lossy against Qt's own writer
(it emits `'` where lupdate emits `&apos;`), which makes the file differ from
what lupdate would produce and so reports permanently stale.

Usage:
    scripts/seed-source-translations.py [--ts translations/app_en.ts] [--check]
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


UNFINISHED = re.compile(
    r"<source>([^<]*)</source>(\s*)<translation type=\"unfinished\">([^<]*)</translation>"
)


def seed(text: str) -> tuple[str, int]:
    count = 0

    def replace(match: re.Match[str]) -> str:
        nonlocal count
        source, gap, existing = match.group(1), match.group(2), match.group(3)

        if existing == source:
            return match.group(0)
        count += 1
        return f"<source>{source}</source>{gap}<translation>{source}</translation>"

    return UNFINISHED.sub(replace, text), count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--ts", type=Path, default=REPO_ROOT / "translations" / "app_en.ts"
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="exit non-zero if any entry still needs seeding",
    )
    args = parser.parse_args()

    if not args.ts.exists():
        raise SystemExit(f"error: {args.ts} not found")

    original = args.ts.read_text(encoding="utf-8")
    seeded, count = seed(original)

    if args.check:
        if count:
            print(
                f"error: {args.ts.name} has {count} unseeded entries; run "
                "scripts/seed-source-translations.py",
                file=sys.stderr,
            )
            return 1
        print(f"[OK] {args.ts.name} is fully seeded")
        return 0

    if count:
        args.ts.write_text(seeded, encoding="utf-8")
    print(f"[OK] seeded {count} source-language entries in {args.ts.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
