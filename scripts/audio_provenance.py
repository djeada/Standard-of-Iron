#!/usr/bin/env python3
"""Track where every shipped audio file came from and under what licence.

THIRD_PARTY_LICENSES.md is the prose record and stays authoritative for the
wording an attribution needs.  This script is the machine-readable half: it
asks, per manifest track, whether anyone can answer "where did this come from
and are we allowed to ship it" without reading a paragraph.

Provenance lives on the manifest track itself:

    "provenance": {
      "origin": "tools/audio_synth/recipes.py",
      "licence": "Original work of this repository",
      "notes": "rendered by make audio-assets",
      "recorded": "2026-08-31"
    }

Two of those are backfilled automatically because the repository already
proves them: anything tagged `"source": "synth"` is generated here by our own
code, and anything tagged `"source": "field"` is cut by tools/audio_field from
the catalogued recordings.  Everything else needs a person, because guessing a
licence is worse than admitting it is unknown.

`--check` is a ratchet, not a wall.  The files whose rights are not yet written
down are listed in assets/audio/audio_provenance_baseline.json; the check fails
when a track outside that list has no provenance, which is to say when a *new*
asset arrives without its rights recorded.

Usage:
    python3 scripts/audio_provenance.py            # report
    python3 scripts/audio_provenance.py --backfill # write what is provable
    python3 scripts/audio_provenance.py --check    # fail on a new unknown
    python3 scripts/audio_provenance.py --accept   # rewrite the baseline
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
AUDIO_DIR = REPO / "assets" / "audio"
MANIFEST = AUDIO_DIR / "audio_manifest.json"
BASELINE = AUDIO_DIR / "audio_provenance_baseline.json"

PROVABLE = {
    "synth": {
        "origin": "tools/audio_synth",
        "licence": "Original work of this repository",
        "notes": "rendered from a recipe by make audio-assets; nothing is sampled",
    },
    "field": {
        "origin": "tools/audio_field",
        "licence": "See THIRD_PARTY_LICENSES.md",
        "notes": "cut from a catalogued recording by tools/audio_field",
    },
}

REQUIRED_FIELDS = ("origin", "licence")


def load_manifest() -> dict:
    return json.loads(MANIFEST.read_text(encoding="utf-8"))


def write_manifest(manifest: dict) -> None:
    MANIFEST.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )


def load_baseline() -> set[str]:
    if not BASELINE.is_file():
        return set()
    data = json.loads(BASELINE.read_text(encoding="utf-8"))
    return set(data.get("rights_not_yet_recorded", []))


def has_provenance(track: dict) -> bool:
    provenance = track.get("provenance")
    if not isinstance(provenance, dict):
        return False
    return all(provenance.get(field) for field in REQUIRED_FIELDS)


def backfill(manifest: dict) -> int:
    filled = 0
    for track in manifest["tracks"]:
        if has_provenance(track):
            continue
        source = track.get("tags", {}).get("source")
        template = PROVABLE.get(source)
        if template is None:
            continue
        track["provenance"] = dict(template)
        filled += 1
    return filled


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--backfill", action="store_true", help="write what is provable"
    )
    parser.add_argument("--check", action="store_true", help="fail on a new unknown")
    parser.add_argument("--accept", action="store_true", help="rewrite the baseline")
    args = parser.parse_args()

    manifest = load_manifest()

    if args.backfill:
        filled = backfill(manifest)
        write_manifest(manifest)
        print(f"provenance written for {filled} tracks that the repository can prove")
        manifest = load_manifest()

    missing = sorted(
        track["id"] for track in manifest["tracks"] if not has_provenance(track)
    )
    recorded = len(manifest["tracks"]) - len(missing)

    if args.accept:
        BASELINE.write_text(
            json.dumps(
                {
                    "comment": (
                        "Tracks whose origin and licence are not written down yet. "
                        "Shrink this list; scripts/audio_provenance.py --check fails "
                        "when a track outside it has no provenance."
                    ),
                    "rights_not_yet_recorded": missing,
                },
                indent=2,
                ensure_ascii=False,
            )
            + "\n",
            encoding="utf-8",
        )
        print(f"baseline accepts {len(missing)} tracks with unrecorded rights")
        return 0

    print(
        f"{recorded} of {len(manifest['tracks'])} tracks record their origin and licence"
    )

    baseline = load_baseline()
    new_unknowns = [track_id for track_id in missing if track_id not in baseline]
    if new_unknowns:
        print(
            f"\n{len(new_unknowns)} tracks ship with unknown usage rights:",
            file=sys.stderr,
        )
        for track_id in new_unknowns:
            print(f"  {track_id}", file=sys.stderr)
        print(
            "  Add a provenance block naming the origin and licence, or record it in "
            "THIRD_PARTY_LICENSES.md and rerun with --accept.",
            file=sys.stderr,
        )
        if args.check:
            return 1

    if missing:
        print(
            f"{len(missing)} tracks still need a person to state their origin and "
            f"licence (see {BASELINE.relative_to(REPO)})"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
