#!/usr/bin/env python3
"""Import new sound effects without touching a live asset until you say so.

The workflow this automates is approval-first, in three steps:

  1. `--scan`   read the drop directory, hash every file, report what is there
                and what is already in the repository under a different name
  2. (default)  print the proposed source -> destination map, the manifest
                entries and the QRC lines that would be added. Nothing is
                written
  3. `--apply`  do exactly what step 2 printed

The conversion is deliberately dumb: 48 kHz stereo Vorbis and nothing else. No
trim, no fade, no normalisation, no loudness match. The mastering chain in
`game/audio/audio_mastering.cpp` shapes a clip at decode time, so a file that
is shaped again on the way in is shaped twice, and the second pass is invisible
to anyone auditioning the first.

A destination is guessed from the file name and the directory the file sits in
under the drop root, e.g. `new sfx/combat/axe_hit.wav` -> `sfx/combat/axe_hit.ogg`
with the id `sfx.combat.axe_hit`. Guesses are printed, never applied silently.

Usage:
    python3 scripts/audio_import.py --scan
    python3 scripts/audio_import.py                 # propose
    python3 scripts/audio_import.py --apply         # after reading the proposal
    python3 scripts/audio_import.py --from "drop/"  # a different drop directory
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
AUDIO_DIR = REPO / "assets" / "audio"
MANIFEST = AUDIO_DIR / "audio_manifest.json"
QRC = REPO / "assets.qrc"
DEFAULT_DROP = REPO / "new sfx"

IMPORTABLE = (".wav", ".flac", ".ogg", ".mp3", ".aiff", ".aif", ".m4a")
TARGET_RATE = 48000
TARGET_CHANNELS = 2
NAME_RE = re.compile(r"[^a-z0-9]+")


@dataclass
class Incoming:
    source: Path
    digest: str
    codec: str
    channels: int
    rate: int
    seconds: float

    @property
    def stem(self) -> str:
        return NAME_RE.sub("_", self.source.stem.lower()).strip("_")


def digest_of(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def probe(path: Path) -> tuple[str, int, int, float]:
    result = subprocess.run(
        [
            "ffprobe",
            "-v",
            "quiet",
            "-print_format",
            "json",
            "-show_streams",
            "-select_streams",
            "a:0",
            str(path),
        ],
        capture_output=True,
        check=False,
        text=True,
    )
    if result.returncode != 0:
        return ("unreadable", 0, 0, 0.0)
    streams = json.loads(result.stdout).get("streams", [])
    if not streams:
        return ("no audio stream", 0, 0, 0.0)
    stream = streams[0]
    return (
        stream.get("codec_name", "?"),
        int(stream.get("channels", 0)),
        int(stream.get("sample_rate", 0)),
        float(stream.get("duration", 0.0)),
    )


def collect(drop: Path) -> list[Incoming]:
    incoming = []
    for path in sorted(drop.rglob("*")):
        if path.suffix.lower() not in IMPORTABLE or not path.is_file():
            continue
        codec, channels, rate, seconds = probe(path)
        incoming.append(Incoming(path, digest_of(path), codec, channels, rate, seconds))
    return incoming


def existing_digests() -> dict[str, str]:
    return {
        digest_of(path): str(path.relative_to(REPO))
        for path in AUDIO_DIR.rglob("*.ogg")
    }


def destination_for(item: Incoming, drop: Path) -> tuple[str, str]:
    """Relative audio path and manifest id guessed from where the file sits."""
    relative_parent = item.source.parent.relative_to(drop)
    family = relative_parent.parts[0] if relative_parent.parts else "combat"
    path = f"sfx/{family}/{item.stem}.ogg"
    return path, f"sfx.{family}.{item.stem}"


def convert(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            "ffmpeg",
            "-v",
            "error",
            "-y",
            "-i",
            str(source),
            "-ac",
            str(TARGET_CHANNELS),
            "-ar",
            str(TARGET_RATE),
            "-c:a",
            "libvorbis",
            "-q:a",
            "5",
            str(destination),
        ],
        check=True,
    )


def add_manifest_entries(entries: list[dict]) -> None:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    known = {track["id"] for track in manifest["tracks"]}
    for entry in entries:
        if entry["id"] in known:
            continue
        manifest["tracks"].append(entry)
    MANIFEST.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )


def add_qrc_entries(paths: list[str]) -> None:
    text = QRC.read_text(encoding="utf-8")
    for path in paths:
        line = f"        <file>assets/audio/{path}</file>\n"
        if line in text:
            continue
        anchor = "        <file>assets/audio/audio_manifest.json</file>\n"
        text = text.replace(anchor, anchor + line, 1)
    QRC.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--from", dest="drop", default=str(DEFAULT_DROP))
    parser.add_argument("--scan", action="store_true", help="report only, no proposal")
    parser.add_argument("--apply", action="store_true", help="perform the proposal")
    args = parser.parse_args()

    drop = Path(args.drop)
    if not drop.is_absolute():
        drop = REPO / drop
    if not drop.is_dir():
        print(f"nothing to import: {drop} does not exist")
        return 0

    incoming = collect(drop)
    if not incoming:
        print(f"nothing to import: no audio files under {drop}")
        return 0

    print(f"{len(incoming)} files in {drop}:")
    for item in incoming:
        print(
            f"  {item.source.relative_to(drop)}  {item.codec} "
            f"{item.channels}ch {item.rate}Hz {item.seconds:.2f}s"
        )

    by_digest: dict[str, list[Incoming]] = defaultdict(list)
    for item in incoming:
        by_digest[item.digest].append(item)
    for items in by_digest.values():
        if len(items) > 1:
            names = ", ".join(str(item.source.relative_to(drop)) for item in items)
            print(f"\nidentical files in the drop, import one: {names}")

    shipped = existing_digests()
    already = [
        (item, shipped[item.digest]) for item in incoming if item.digest in shipped
    ]
    for item, path in already:
        print(f"\nalready shipped as {path}: {item.source.relative_to(drop)}")

    if args.scan:
        return 0

    plan = []
    seen: set[str] = set()
    for item in incoming:
        if item.digest in shipped or item.digest in seen:
            continue
        seen.add(item.digest)
        path, resource_id = destination_for(item, drop)
        plan.append((item, path, resource_id))

    if not plan:
        print("\nnothing new to import.")
        return 0

    print("\nproposed import:")
    for item, path, resource_id in plan:
        print(f"  {item.source.relative_to(drop)}")
        print(f"    -> assets/audio/{path}")
        print(f"    -> manifest id {resource_id}")
        print("    -> assets.qrc entry")
    print(
        "\nEvery clip converts to 48 kHz stereo Vorbis with no filtering, so its "
        "timing and dynamics reach the game exactly as recorded."
    )
    print(
        "A new file is not audible until a cue names it: add it to a pool in "
        "assets/audio/audio_cues.json, then run make audio-report."
    )

    if not args.apply:
        print("\nNothing was written. Re-run with --apply to perform this.")
        return 0

    entries = []
    for item, path, resource_id in plan:
        convert(item.source, AUDIO_DIR / path)
        entries.append(
            {
                "id": resource_id,
                "path": path,
                "category": "sfx",
                "load_policy": "mission",
                "tags": {"source": "imported"},
                "provenance": {
                    "origin": f"imported from {item.source.name}",
                    "licence": "UNRECORDED - state the rights before shipping",
                },
            }
        )
    add_manifest_entries(entries)
    add_qrc_entries([path for _item, path, _id in plan])
    print(f"\nimported {len(entries)} files. Run make audio-check before committing.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
