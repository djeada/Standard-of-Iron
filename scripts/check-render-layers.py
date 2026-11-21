#!/usr/bin/env python3
"""Fail the build when an include points up the render/ layer order.

render/ is split into top-level folders, each on one layer. A file may include
headers from its own folder or from any folder on a lower layer, never from a
higher one, so no two folders include each other. The order, lowest first:

  math, profiling, software   plain values, counters, the CPU rasteriser
  gl                          GL handles and wrappers: buffers, textures, shaders
  frame                       what a frame is made of: draw commands, the draw
                              queue, submitter interfaces, per-frame settings
  geom                        procedural meshes and tessellation
  backend                     pipelines and command executors, the GL and
                              software backends
  cache                       baked static meshes and render archetypes
  ground                      terrain, roads, water, scatter, fog
  creature                    skeletons, baking, animation, and the species
                              (humanoid, horse, elephant, wildlife)
  equipment                   armour, weapons, attachments
  entity                      buildings, units, siege, effects per nation
  overlay                     projectiles, rafts, range rings, order and
                              target markers drawn over the world
  scene                       scene walk, renderer orchestration, prewarm

Nothing may live in the render/ root except build files.

  usage: check-render-layers.py [repo-root]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

LAYERS: tuple[tuple[str, ...], ...] = (
    ("math", "profiling", "software"),
    ("gl",),
    ("frame",),
    ("geom",),
    ("backend",),
    ("cache",),
    ("ground",),
    ("creature",),
    ("equipment",),
    ("entity",),
    ("overlay",),
    ("scene",),
)

RANK = {folder: rank for rank, group in enumerate(LAYERS) for folder in group}

INCLUDE = re.compile(r'^\s*#\s*include\s*"render/([^"/]+)/[^"]+"')

SOURCE_SUFFIXES = (".h", ".cpp")


def check(root: Path) -> list[str]:
    render = root / "render"
    problems: list[str] = []
    for loose in sorted(render.iterdir()):
        if loose.is_file() and loose.suffix in SOURCE_SUFFIXES:
            problems.append(f"render/{loose.name}: source file in the render/ root")
    for source in sorted(render.rglob("*")):
        if source.suffix not in SOURCE_SUFFIXES or source.parent == render:
            continue
        relative = source.relative_to(root).as_posix()
        folder = relative.split("/")[1]
        if folder not in RANK:
            problems.append(f"{relative}: folder render/{folder} has no layer")
            continue
        for number, line in enumerate(source.read_text(errors="replace").splitlines(), 1):
            match = INCLUDE.match(line)
            if match is None:
                continue
            target = match.group(1)
            if target == folder or target not in RANK:
                continue
            if RANK[target] > RANK[folder]:
                problems.append(
                    f"{relative}:{number}: render/{folder} includes render/{target}, "
                    "a higher layer"
                )
            elif RANK[target] == RANK[folder]:
                problems.append(
                    f"{relative}:{number}: render/{folder} includes render/{target}, "
                    "a sibling on the same layer"
                )
    return problems


def main(argv: list[str]) -> int:
    root = Path(argv[1]) if len(argv) > 1 else Path(__file__).resolve().parent.parent
    problems = check(root.resolve())
    if problems:
        print("render/ layer violations:")
        for problem in problems:
            print(f"  {problem}")
        print(f"{len(problems)} violation(s). See the order in {Path(__file__).name}.")
        return 1
    print("render/ layers: every include points down")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
