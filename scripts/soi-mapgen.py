#!/usr/bin/env python3
"""Generate a playable Standard of Iron battlefield from a seed and a preset.

See docs/MAP_GENERATOR.md. Examples:

    python3 scripts/soi-mapgen.py --describe
    python3 scripts/soi-mapgen.py --preset river_crossing --seed 7 --output out.json
    python3 scripts/soi-mapgen.py --preset all --seed 1 --count 20 --report-dir artifacts/mapgen
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from mapgen.cli import main  # noqa: E402

if __name__ == "__main__":
    raise SystemExit(main())
