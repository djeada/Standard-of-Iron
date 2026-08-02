#!/usr/bin/env python3
"""Wire the synthesised sounds into the manifest and the cue catalog.

Rendering a file is only a third of the job: the manifest has to describe it
and the cue has to point at it, or the game stays silent. Running this after
synthesize_cues.py keeps all three in step. It is idempotent.
"""

from __future__ import annotations

import json
import sys
from collections import OrderedDict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from cues import RECIPES

REPO = Path(__file__).resolve().parent.parent.parent
AUDIO_DIR = REPO / "assets" / "audio"
MANIFEST = AUDIO_DIR / "audio_manifest.json"
CATALOG = AUDIO_DIR / "audio_cues.json"


REUSED_IDS = {
    "ui.hover": "ui.hover.soft",
    "ui.click": "ui.click.primary",
}


def resource_id(cue_id: str, take: int = 0) -> str:
    """Manifest id for one take of a cue's generated sound."""
    if cue_id in REUSED_IDS:
        base = REUSED_IDS[cue_id]
    else:
        family, _, leaf = cue_id.partition(".")
        base = (
            f"ui.{leaf}.synth"
            if family == "ui"
            else f"sfx.{family}.{leaf.replace('.', '_')}"
        )
    return base if take == 0 else f"{base}_v{take + 1}"


def load(path: Path) -> OrderedDict:
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=OrderedDict)


def main() -> int:
    manifest = load(MANIFEST)
    catalog = load(CATALOG)

    tracks = manifest["tracks"]
    by_id = {track["id"]: track for track in tracks}
    cues = {cue["id"]: cue for cue in catalog["cues"]}

    added = 0
    updated = 0
    for cue_id, recipe in RECIPES.items():
        if cue_id not in cues:
            print(f"warning: no catalog entry for {cue_id}", file=sys.stderr)
            continue
        cue = cues[cue_id]
        bound: list[str] = []

        for take in range(recipe.takes):
            rel_path = recipe.take_path(take)
            if not (AUDIO_DIR / rel_path).exists():
                print(f"warning: {rel_path} not rendered yet", file=sys.stderr)
                continue

            track_id = resource_id(cue_id, take)
            entry = by_id.get(track_id)
            if entry is None:
                entry = OrderedDict()
                entry["id"] = track_id
                tracks.append(entry)
                by_id[track_id] = entry
                added += 1
            else:
                updated += 1

            entry["path"] = rel_path
            entry["category"] = "sfx"
            entry["priority"] = int(cue.get("priority", 5))
            entry["cooldown_ms"] = int(cue.get("cooldown_ms", 0))

            entry["load_policy"] = "startup"
            entry.setdefault("tags", OrderedDict())
            entry["tags"]["source"] = "synth"
            bound.append(track_id)

        if not bound:
            continue

        cue["resources"] = bound
        cue.pop("placeholder", None)

    MANIFEST.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    CATALOG.write_text(
        json.dumps(catalog, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(f"manifest: {added} tracks added, {updated} updated")
    print(f"catalog: {len(RECIPES)} cues bound")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
