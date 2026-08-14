#!/usr/bin/env python3
"""Fail the build when the renderer resolves its own inputs.

The rule: the renderer draws the match it was *handed*. Everything it needs
about the simulation arrives on a `Render::WorldView`, set once per frame by
whoever owns the session. It never asks a process-wide accessor which match it
should be drawing.

That is what makes multiplayer possible. A client renders state it received
over the wire; a spectator renders a session it is not simulating; a replay
renders a session that finished. None of those are expressible while the
renderer calls `SessionContext::active()` -- directly, or through the
`instance()` accessor of any service that resolves through it.

Two things are checked:

  * no file under render/ may call `Game::...::instance()` or reach
    `SessionContext::active()`, except the one file whose whole job is to bind
    the view;
  * nothing under game/ may include anything from render/. That direction is
    also covered by scripts/check-layering.py; it is restated here because it is
    half of the same rule and this is where someone will come looking.

  usage: check-render-boundary.py [repo-root]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

SEAM = "render/world_view.cpp"

AMBIENT = re.compile(
    r"""
    (?:Game::[A-Za-z_:]+::instance\s*\()      # any game service singleton
    | (?:SessionContext::active(?:_or_null)?\s*\()
    """,
    re.VERBOSE,
)

RENDER_INCLUDE = re.compile(r'^\s*#\s*include\s*"((?:\.\./)*render/[^"]+)"')

SOURCE_SUFFIXES = (".h", ".cpp")


def sources(root: Path, layer: str) -> list[Path]:
    directory = root / layer
    if not directory.is_dir():
        return []
    return sorted(p for p in directory.rglob("*") if p.suffix in SOURCE_SUFFIXES)


def ambient_lookups(root: Path) -> list[str]:
    found: list[str] = []
    for source in sources(root, "render"):
        relative = source.relative_to(root).as_posix()
        if relative == SEAM:
            continue
        for number, line in enumerate(
            source.read_text(errors="ignore").splitlines(), 1
        ):
            stripped = line.lstrip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue
            match = AMBIENT.search(line)
            if match:
                found.append(f"{relative}:{number}: {match.group(0).strip()}")
    return found


def render_includes_in_game(root: Path) -> list[str]:
    found: list[str] = []
    for source in sources(root, "game"):
        relative = source.relative_to(root).as_posix()
        for number, line in enumerate(
            source.read_text(errors="ignore").splitlines(), 1
        ):
            match = RENDER_INCLUDE.match(line)
            if match:
                found.append(f"{relative}:{number}: includes {match.group(1)}")
    return found


def main() -> int:
    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
    status = 0

    lookups = ambient_lookups(root)
    if lookups:
        status = 1
        print("The renderer is resolving its own inputs:", file=sys.stderr)
        for entry in lookups:
            print(f"  {entry}", file=sys.stderr)
        print(
            f"\nA renderer draws the match it was handed. Take what you need from\n"
            f"`Render::WorldView` -- reachable as `renderer.world_view()`, `world()`\n"
            f"on a render pass, or `ctx.world_view` in an entity renderer -- and if it\n"
            f"is not on the view yet, add it there and bind it in {SEAM}.\n"
            f"Resolving the ambient session here is what stops a client from rendering\n"
            f"a match it is not simulating.",
            file=sys.stderr,
        )

    inversions = render_includes_in_game(root)
    if inversions:
        status = 1
        print("\ngame/ depends on render/:", file=sys.stderr)
        for entry in inversions:
            print(f"  {entry}", file=sys.stderr)
        print(
            "\nThe simulation must build and run with no renderer at all -- that is\n"
            "what a headless server is. Put the shared type in scene/ or animation/,\n"
            "or invert the call so render/ reads from game/.",
            file=sys.stderr,
        )

    return status


if __name__ == "__main__":
    raise SystemExit(main())
