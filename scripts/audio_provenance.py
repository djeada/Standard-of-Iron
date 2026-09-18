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
code, anything tagged `"source": "recorded"` was supplied to the project and is
never rebuilt, and anything tagged `"source": "field"` is cut by tools/audio_field from
the catalogued recordings.  Everything else needs a person, because guessing a
licence is worse than admitting it is unknown.

`--check` is a ratchet, not a wall.  The files whose rights are not yet written
down are listed in assets/audio/audio_provenance_baseline.json; the check fails
when a track outside that list has no provenance, which is to say when a *new*
asset arrives without its rights recorded.  It also fails when an .ogg ships
without a manifest entry, and when docs/AUDIO_LICENSES.md no longer matches the
manifest: that per-file list is rendered from the provenance blocks, so edit
the manifest and rerun `--doc` rather than editing the table.

Usage:
    python3 scripts/audio_provenance.py            # report
    python3 scripts/audio_provenance.py --backfill # write what is provable
    python3 scripts/audio_provenance.py --check    # fail on a new unknown
    python3 scripts/audio_provenance.py --accept   # rewrite the baseline
    python3 scripts/audio_provenance.py --doc      # re-render the licence list
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
DOC = REPO / "docs" / "AUDIO_LICENSES.md"
DOC_MARKER = (
    "<!-- Rendered by scripts/audio_provenance.py --doc from the manifest's "
    "provenance blocks. Edit those, not the tables below. -->"
)

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


def cell(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", " ")


def render_listing(manifest: dict) -> str:
    tracks = sorted(manifest["tracks"], key=lambda track: track["path"])
    by_licence: dict[str, int] = {}
    for track in tracks:
        licence = track.get("provenance", {}).get("licence", "not recorded")
        by_licence[licence] = by_licence.get(licence, 0) + 1

    lines = [DOC_MARKER, "", "<!-- prettier-ignore-start -->", ""]
    lines += ["### Summary", "", "| Licence | Files |", "| --- | ---: |"]
    for licence, count in sorted(
        by_licence.items(), key=lambda item: (-item[1], item[0])
    ):
        lines.append(f"| {cell(licence)} | {count} |")

    directory = None
    for track in tracks:
        parent = track["path"].rpartition("/")[0]
        if parent != directory:
            directory = parent
            lines += ["", f"### `{directory}/`", "", "| File | Origin | Licence |"]
            lines.append("| --- | --- | --- |")
        provenance = track.get("provenance", {})
        lines.append(
            f"| `{track['path'].rpartition('/')[2]}` "
            f"| {cell(provenance.get('origin', 'not recorded'))} "
            f"| {cell(provenance.get('licence', 'not recorded'))} |"
        )
    lines += ["", "<!-- prettier-ignore-end -->", ""]
    return "\n".join(lines)


def rendered_doc(manifest: dict) -> str | None:
    if not DOC.is_file():
        return None
    text = DOC.read_text(encoding="utf-8")
    head, marker, _ = text.partition(DOC_MARKER)
    if not marker:
        return None
    return head + render_listing(manifest)


def orphan_files(manifest: dict) -> list[str]:
    listed = {track["path"] for track in manifest["tracks"]}
    return sorted(
        path.relative_to(AUDIO_DIR).as_posix()
        for path in AUDIO_DIR.rglob("*.ogg")
        if path.relative_to(AUDIO_DIR).as_posix() not in listed
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--backfill", action="store_true", help="write what is provable"
    )
    parser.add_argument("--check", action="store_true", help="fail on a new unknown")
    parser.add_argument("--accept", action="store_true", help="rewrite the baseline")
    parser.add_argument(
        "--doc", action="store_true", help="re-render docs/AUDIO_LICENSES.md"
    )
    args = parser.parse_args()

    manifest = load_manifest()

    if args.backfill:
        filled = backfill(manifest)
        write_manifest(manifest)
        print(f"provenance written for {filled} tracks that the repository can prove")
        manifest = load_manifest()

    if args.doc:
        doc = rendered_doc(manifest)
        if doc is None:
            print(f"{DOC.relative_to(REPO)} has no render marker", file=sys.stderr)
            return 1
        DOC.write_text(doc, encoding="utf-8")
        print(f"rendered {DOC.relative_to(REPO)}")

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

    failed = False
    orphans = orphan_files(manifest)
    if orphans:
        print(f"\n{len(orphans)} files ship without a manifest entry:", file=sys.stderr)
        for path in orphans:
            print(f"  {path}", file=sys.stderr)
        failed = True

    doc = rendered_doc(manifest)
    if doc is None or doc != DOC.read_text(encoding="utf-8"):
        print(
            f"\n{DOC.relative_to(REPO)} does not match the manifest; "
            "rerun with --doc",
            file=sys.stderr,
        )
        failed = True

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
        failed = True

    if missing:
        print(
            f"{len(missing)} tracks still need a person to state their origin and "
            f"licence (see {BASELINE.relative_to(REPO)})"
        )
    return 1 if failed and args.check else 0


if __name__ == "__main__":
    raise SystemExit(main())
