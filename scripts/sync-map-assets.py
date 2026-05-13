#!/usr/bin/env python3
"""Sync assets/maps/*.json entries in assets.qrc with files present on disk.

Run this before cmake configure so that any new or removed map definition
files are reflected in the Qt resource file used by the Qt5 build path.
"""

import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
qrc_path = root / "assets.qrc"
maps_dir = root / "assets" / "maps"

map_files = sorted(
    f"assets/maps/{f.name}" for f in maps_dir.iterdir() if f.suffix == ".json"
)

content = qrc_path.read_text()

entries = "\n".join(f"        <file>{f}</file>" for f in map_files)

new_content = re.sub(
    r"(?<=<!-- Map files -->)\s*(?:<file>assets/maps/[^<]+</file>\s*)+",
    f"\n{entries}\n        ",
    content,
)

if new_content == content:
    print(f"assets.qrc already up to date ({len(map_files)} map files).")
    sys.exit(0)

qrc_path.write_text(new_content)
print(f"Updated assets.qrc with {len(map_files)} map files.")
