#!/usr/bin/env python3
"""Fail the build when an include points up the render/ layer order.

render/ is split into top-level folders, each on one layer. A file may include
headers from its own folder or from any folder on a lower layer, never from a
higher one, so no two folders include each other. The order, lowest first:

  math, profiling, software   plain values, counters, the CPU rasteriser
  geom                        CPU geometry: Mesh, primitives, procedural meshes
                              and tessellation; no GL
  gl                          GL handles and wrappers: buffers, textures, shaders,
                              the per-backend MeshGpuTable
  frame                       what a frame is made of: draw commands, the draw
                              queue, submitter interfaces, render archetypes,
                              per-frame settings
  cache                       baked static building meshes and attachment specs
  backend                     pipelines and command executors, the GL and
                              software backends
  ground                      terrain, roads, water, scatter, fog
  creature                    skeletons, baking, animation, and the species
                              (humanoid, horse, elephant, wildlife)
  equipment                   armour, weapons, attachments
  entity                      buildings, units, siege, effects per nation
  overlay                     projectiles, rafts, range rings, order and
                              target markers drawn over the world
  scene                       scene walk, renderer orchestration, prewarm

Nothing may live in the render/ root except build files.

Inside render/creature, the generic creature code (everything outside the
species folders below) includes nothing from a species folder: it reaches a
species only through its SpeciesContract (pipeline/species_contract.h).
render/creature/species/ is the one place that names every species and may
include them all.

The GL-free layers (math, geom, frame, cache, ground, equipment, entity,
overlay) include nothing from render/gl or render/backend and no Qt OpenGL
header: they build plain data and point commands at it, and the backend owns
and uploads every GPU object (GL-free rule).

  usage: check-render-layers.py [repo-root]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

LAYERS: tuple[tuple[str, ...], ...] = (
    ("math", "profiling", "software"),
    ("geom",),
    ("gl",),
    ("frame",),
    ("cache",),
    ("backend",),
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

GL_FREE: tuple[str, ...] = (
    "math",
    "geom",
    "frame",
    "cache",
    "ground",
    "equipment",
    "entity",
    "overlay",
)
GL_LAYERS: tuple[str, ...] = ("gl", "backend")
QT_GL_INCLUDE = re.compile(
    r"^\s*#\s*include\s*<(QOpenGL\w*|QtOpenGL\w*|qopengl\w*)(\.h)?>"
)

CREATURE_SPECIES: tuple[str, ...] = (
    "humanoid",
    "horse",
    "elephant",
    "wildlife",
    "quadruped",
)
CREATURE_SPECIES_REGISTRY = "species"

SPECIES_INCLUDE = re.compile(
    r'^\s*#\s*include\s*"render/creature/(' + "|".join(CREATURE_SPECIES) + r')/[^"]+"'
)


def check_creature_species(root: Path) -> list[str]:
    creature = root / "render" / "creature"
    problems: list[str] = []
    if not creature.is_dir():
        return problems
    for source in sorted(creature.rglob("*")):
        if source.suffix not in SOURCE_SUFFIXES:
            continue
        parts = source.relative_to(creature).parts
        if len(parts) > 1 and parts[0] in (
            *CREATURE_SPECIES,
            CREATURE_SPECIES_REGISTRY,
        ):
            continue
        relative = source.relative_to(root).as_posix()
        for number, line in enumerate(
            source.read_text(errors="replace").splitlines(), 1
        ):
            match = SPECIES_INCLUDE.match(line)
            if match is not None:
                problems.append(
                    f"{relative}:{number}: generic creature code includes the "
                    f"{match.group(1)} species; go through its SpeciesContract "
                    "(creature species rule)"
                )
    return problems


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
        for number, line in enumerate(
            source.read_text(errors="replace").splitlines(), 1
        ):
            match = INCLUDE.match(line)
            target = match.group(1) if match is not None else None
            if folder in GL_FREE:
                qt_gl = QT_GL_INCLUDE.match(line)
                if target in GL_LAYERS or qt_gl is not None:
                    what = f"render/{target}" if qt_gl is None else qt_gl.group(1)
                    problems.append(
                        f"{relative}:{number}: render/{folder} includes {what}; "
                        "it must stay GL-free (GL-free rule)"
                    )
                    continue
            if match is None:
                continue
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
    problems = check(root.resolve()) + check_creature_species(root.resolve())
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
