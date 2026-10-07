"""Runs the existing map pipeline tools over the working map.

Each tool is run as its own process on a scratch copy of the map, exactly as
an author runs it, so the generator exercises the same code paths and the
same validation gates. Their console output becomes stage notes.
"""

from __future__ import annotations

import json
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

SCRIPTS = Path(__file__).resolve().parents[1]


SURFACE = ["--surface", "off"]

TOOLS = {
    "water": ["generate-map-water.py", "--write"],
    "roads": ["generate-map-roads.py", "--write"],
    "settlements": ["generate-map-settlements.py", "--write"],
    "landmarks": ["generate-map-landmarks.py", "--write"],
    "dressing": ["generate-map-dressing.py", "--write", *SURFACE],
    "placement": [
        "fix-map-prop-overlaps.py",
        *SURFACE,
        "--drop-unplaceable",
        "--quiet",
    ],
}

GATES = {
    "water": ["generate-map-water.py", "--validate-only"],
    "roads": ["generate-map-roads.py", "--validate-only"],
    "settlements": ["generate-map-settlements.py", "--validate-only"],
    "placement": ["fix-map-prop-overlaps.py", *SURFACE, "--check"],
}


@dataclass
class ToolRun:
    tool: str
    ok: bool
    output: str
    seconds: float

    def lines(self, limit: int = 12) -> list[str]:
        lines = [line.strip() for line in self.output.splitlines() if line.strip()]
        lines = [line.replace(str(Path.cwd()), ".") for line in lines]
        return lines[-limit:]


def write_map(path: Path, definition: dict[str, Any]) -> None:
    path.write_text(json.dumps(definition, indent=2) + "\n", encoding="utf-8")


def _run(command: list[str], path: Path) -> tuple[bool, str, float]:
    started = time.monotonic()
    result = subprocess.run(
        [sys.executable, str(SCRIPTS / command[0]), *command[1:], str(path)],
        capture_output=True,
        text=True,
        check=False,
    )
    output = (result.stdout + result.stderr).replace(str(path), path.name)
    return result.returncode == 0, output, time.monotonic() - started


def run_tool(
    tool: str, definition: dict[str, Any], scratch: Path
) -> tuple[dict[str, Any], ToolRun]:
    path = scratch / f"{tool}.json"
    write_map(path, definition)
    ok, output, seconds = _run(TOOLS[tool], path)
    updated = json.loads(path.read_text(encoding="utf-8")) if ok else definition
    return updated, ToolRun(tool, ok, output, seconds)


def run_gate(gate: str, definition: dict[str, Any], scratch: Path) -> ToolRun:
    path = scratch / f"gate_{gate}.json"
    write_map(path, definition)
    ok, output, seconds = _run(GATES[gate], path)
    return ToolRun(gate, ok, output, seconds)
