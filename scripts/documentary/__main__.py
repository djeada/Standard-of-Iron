"""Entry point: ``python3 scripts/documentary <command> ...``."""

import importlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
raise SystemExit(importlib.import_module("documentary.cli").main())
