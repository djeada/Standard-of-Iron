#!/usr/bin/env python3
"""Fail the build when a backend pipeline exposes its state.

A pipeline (render/backend/*_pipeline.h) owns its shaders, uniform tables,
meshes and scratch buffers and draws with them through its own methods; the
command executors dispatch to those methods with a PipelineDrawContext. Two
things are checked:

  * no pipeline class declares a data member (m_...) in a public section;
  * no file outside a pipeline's own sources reaches into a pipeline member
    through `m_<name>_pipeline->m_...`.

  usage: check-pipeline-interface.py [repo-root]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

MEMBER = re.compile(r"\bm_\w+\s*(?:\{[^}]*\}|=[^;]*)?;$")
CLASS_OPEN = re.compile(r"^(class|struct)\s+\w+[^;]*\{$")
REACH = re.compile(r"\bm_\w+_pipeline->m_\w+")


def public_members(header: Path) -> list[str]:
    problems: list[str] = []
    depth = 0
    access = "private"
    for number, raw in enumerate(header.read_text(errors="replace").splitlines(), 1):
        line = raw.split("//", 1)[0].strip()
        if depth == 0 and CLASS_OPEN.match(line):
            access = "private" if line.startswith("class") else "public"
        if line in ("public:", "protected:", "private:"):
            access = line[:-1]
        if (
            depth == 1
            and access == "public"
            and "(" not in line
            and MEMBER.search(line)
        ):
            problems.append(
                f"{header.as_posix()}:{number}: public data member '{line}'"
            )
        depth += line.count("{") - line.count("}")
    return problems


def main(argv: list[str]) -> int:
    root = Path(argv[1]) if len(argv) > 1 else Path(__file__).resolve().parent.parent
    root = root.resolve()
    problems: list[str] = []
    for header in sorted((root / "render" / "backend").glob("*_pipeline.h")):
        problems += [
            p.replace(f"{root.as_posix()}/", "") for p in public_members(header)
        ]
    for source in sorted((root / "render").rglob("*")):
        if source.suffix not in (".h", ".cpp"):
            continue
        for number, line in enumerate(
            source.read_text(errors="replace").splitlines(), 1
        ):
            if REACH.search(line):
                problems.append(
                    f"{source.relative_to(root).as_posix()}:{number}: "
                    "reaches into a pipeline member; call a pipeline method"
                )
    if problems:
        print("pipeline interface violations:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print("pipelines: state is private and reached only through methods")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
