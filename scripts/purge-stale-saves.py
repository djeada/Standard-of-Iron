#!/usr/bin/env python3
"""
Purge save slots whose source map JSON is newer than the save timestamp.

When a map JSON file in assets/maps/ is edited after a game was saved, the
serialised world state in the save is stale (rivers, terrain, biome settings
are all baked into the save).  Loading such a save would show the old map
design, not the edited one.

This script runs automatically before `make run` and deletes any saves that
would load an outdated map.
"""

import json
import os
import pathlib
import sqlite3
import sys
from datetime import datetime, timezone


def resolve_map_path(raw_path: str, project_root: pathlib.Path) -> pathlib.Path | None:
    """Return the filesystem Path for a map JSON, or None if unresolvable."""

    if raw_path.startswith(":/"):
        raw_path = raw_path[2:]

    marker = "assets/maps/"
    idx = raw_path.find(marker)
    if idx == -1:
        return None
    relative = raw_path[idx:]

    candidate = project_root / relative
    return candidate if candidate.exists() else None


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: purge-stale-saves.py <project_root>", file=sys.stderr)
        return 1

    project_root = pathlib.Path(sys.argv[1]).resolve()

    xdg_data = pathlib.Path(
        os.environ.get("XDG_DATA_HOME", pathlib.Path.home() / ".local" / "share")
    )
    db_path = xdg_data / "standard_of_iron" / "saves" / "saves.sqlite"

    if not db_path.exists():
        return 0

    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()
        cur.execute("SELECT slot_name, timestamp, metadata FROM saves")
        rows = cur.fetchall()
    finally:
        conn.close()

    stale_slots: list[str] = []
    for slot_name, ts_str, metadata_raw in rows:
        try:
            meta = json.loads(metadata_raw or "{}")
        except json.JSONDecodeError:
            continue

        raw_map_path = meta.get("map_path", "")
        if not raw_map_path:
            continue

        map_file = resolve_map_path(raw_map_path, project_root)
        if map_file is None:
            continue

        try:
            ts_str_clean = ts_str.replace("Z", "+00:00")
            save_time = datetime.fromisoformat(ts_str_clean)
            if save_time.tzinfo is None:
                save_time = save_time.replace(tzinfo=timezone.utc)
        except (ValueError, TypeError):
            continue

        map_mtime = datetime.fromtimestamp(map_file.stat().st_mtime, tz=timezone.utc)
        if map_mtime > save_time:
            stale_slots.append((slot_name, map_file.name, map_mtime, save_time))

    if not stale_slots:
        return 0

    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()
        for slot_name, map_filename, map_mtime, save_time in stale_slots:
            cur.execute("DELETE FROM saves WHERE slot_name = ?", (slot_name,))
            print(
                f"  Purged stale save '{slot_name}' "
                f"(map {map_filename} modified {map_mtime:%Y-%m-%d %H:%M:%S UTC}, "
                f"save from {save_time:%Y-%m-%d %H:%M:%S UTC})"
            )
        conn.commit()
    finally:
        conn.close()

    print(f"  Purged {len(stale_slots)} stale save(s) due to updated map JSON files.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
