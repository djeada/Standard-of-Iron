#!/usr/bin/env python3
"""Fail the build when a layer includes something it must not.

render/ reads game state to draw it; the simulation must never depend on the
renderer.  game_systems no longer links render_gl, so most violations surface as
link errors -- but a header-only include would slip through, and the error would
be confusing.  This runs as part of the build and names the file directly.

Shared types belong in one of the leaf layers instead:
  scene/          view types (camera, environment lighting state)
  animation/rig   skeleton, proportions, attachment frames, reach
  animation/bpat  baked pose data
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')

# layer -> prefixes it must not include
RULES = {
    "game": ("render/",),
    "scene": ("render/", "game/"),
    "animation": ("render/", "game/"),
}


def normalise(path: str) -> str:
    while path.startswith("../"):
        path = path[3:]
    return path


def violations(root: Path) -> list[str]:
    found: list[str] = []
    for layer, forbidden in RULES.items():
        layer_dir = root / layer
        if not layer_dir.is_dir():
            continue
        for source in sorted(layer_dir.rglob("*")):
            if source.suffix not in (".h", ".cpp"):
                continue
            try:
                text = source.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            for number, line in enumerate(text.splitlines(), start=1):
                match = INCLUDE.match(line)
                if not match:
                    continue
                target = normalise(match.group(1))
                for prefix in forbidden:
                    if target.startswith(prefix):
                        rel = source.relative_to(root).as_posix()
                        found.append(f"{rel}:{number}: {layer}/ must not include {target}")
    return found


def main() -> int:
    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
    found = violations(root)
    if not found:
        return 0
    print("Layering violation(s):", file=sys.stderr)
    for entry in found:
        print(f"  {entry}", file=sys.stderr)
    print(
        "\nPut the shared type in scene/, animation/rig or animation/bpat, "
        "or invert the call so render/ reads from game/.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
