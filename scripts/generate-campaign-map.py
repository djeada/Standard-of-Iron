#!/usr/bin/env python3
"""Generate and validate every runtime asset used by the campaign map."""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAP_PIPELINE = ROOT / "tools" / "map_pipeline"
OUTPUT = ROOT / "assets" / "campaign_map"

GENERATORS = (
    MAP_PIPELINE / "pipeline.py",
    MAP_PIPELINE / "provinces.py",
)

REQUIRED_OUTPUTS = (
    OUTPUT / "campaign_base_color.png",
    OUTPUT / "campaign_water.png",
    OUTPUT / "coastlines_uv.json",
    OUTPUT / "rivers_uv.json",
    OUTPUT / "land_mesh.bin",
    OUTPUT / "provinces.json",
    OUTPUT / "terrain_height.png",
)


def main() -> int:
    for generator in GENERATORS:
        subprocess.run([sys.executable, str(generator)], cwd=ROOT, check=True)

    missing = [
        path
        for path in REQUIRED_OUTPUTS
        if not path.is_file() or path.stat().st_size < 32
    ]
    if missing:
        print(
            "Campaign map generation did not produce required runtime assets:",
            file=sys.stderr,
        )
        for path in missing:
            print(f"  - {path.relative_to(ROOT)}", file=sys.stderr)
        return 1

    print("Campaign map runtime assets generated successfully:")
    for path in REQUIRED_OUTPUTS:
        print(f"  - {path.relative_to(ROOT)} ({path.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
