#!/usr/bin/env python3
"""Generate and validate every runtime asset used by the campaign map."""

import os
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
    OUTPUT / "terrain_height.json",
)


def missing_outputs() -> list[Path]:
    return [
        path
        for path in REQUIRED_OUTPUTS
        if not path.is_file() or path.stat().st_size < 32
    ]


def main() -> int:
    # The generated assets are committed, so a build never depends on the
    # upstream Natural Earth and NOAA hosts being reachable. Regeneration is
    # still the source of truth: it runs first, and only an upstream failure
    # with every asset already in place is allowed to fall back to them.
    # Set CAMPAIGN_MAP_REQUIRE_REGENERATION=1 to turn that fallback off.
    require_regeneration = os.environ.get("CAMPAIGN_MAP_REQUIRE_REGENERATION") == "1"
    for generator in GENERATORS:
        result = subprocess.run([sys.executable, str(generator)], cwd=ROOT, check=False)
        if result.returncode == 0:
            continue
        if require_regeneration:
            print(
                f"{generator.relative_to(ROOT)} failed with exit status "
                f"{result.returncode}.",
                file=sys.stderr,
            )
            return result.returncode
        stale = missing_outputs()
        if stale:
            print(
                f"{generator.relative_to(ROOT)} failed with exit status "
                f"{result.returncode} and the committed assets cannot stand in:",
                file=sys.stderr,
            )
            for path in stale:
                print(f"  - {path.relative_to(ROOT)}", file=sys.stderr)
            return result.returncode
        print(
            f"WARNING: {generator.relative_to(ROOT)} failed with exit status "
            f"{result.returncode}; falling back to the committed campaign map "
            "assets. Set CAMPAIGN_MAP_REQUIRE_REGENERATION=1 to make this fatal.",
            file=sys.stderr,
        )
        break

    missing = missing_outputs()
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
