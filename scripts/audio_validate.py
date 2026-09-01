#!/usr/bin/env python3
"""Validate the audio assets themselves, not the wiring.

scripts/audio_report.py answers "is every cue connected to something".  This
script answers the other half: are the files that answer actually shippable.
It never edits an asset.  Everything here either fails the build on a broken
link, or prints a finding for a human to listen to and decide.

Checks that fail:

  * a cue names a resource the manifest does not define
  * a manifest entry names a file that is not on disk
  * an embedded audio file (effects and voice) missing from assets.qrc, so the
    packaged build would silently lose it while a developer build kept working.
    Music and ambience are deliberately loose files -- they are copied whole by
    scripts/package-appimage.sh -- so they are not required to be in the QRC
  * an audio path in assets.qrc that is not on disk
  * two manifest entries claiming the same id, or the same alias, where the
    second one silently wins
Checks that only report:

  * a script naming an audio file that is not there. The promo and trailer
    scripts fall back to a silent cut rather than failing, so a stale name
    costs a trailer its music without anyone noticing. Which track should
    replace a retired one is an authoring decision, not a lint fix

Checks that only report (--scan, needs ffmpeg):

  * leading silence, which delays a cue that gameplay fired on an exact frame
  * a hard boundary at the first or last sample, which clicks on playback

Usage:
    python3 scripts/audio_validate.py            # link checks only
    python3 scripts/audio_validate.py --scan     # also decode and measure
"""

from __future__ import annotations

import argparse
import array
import json
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
AUDIO_DIR = REPO / "assets" / "audio"
MANIFEST = AUDIO_DIR / "audio_manifest.json"
CUES = AUDIO_DIR / "audio_cues.json"
QRC = REPO / "assets.qrc"

DIRECT_REFERENCE_ROOTS = ("scripts", "tools", "ui", "app", "game", "tests")
DIRECT_REFERENCE_SUFFIXES = (".py", ".qml", ".js", ".cpp", ".h", ".sh")
AUDIO_PATH_RE = re.compile(r"assets/audio/[\w/.-]+\.ogg")


MUSIC_VAR_RE = re.compile(r"\$\{music\}/([\w/.-]+\.ogg)")


EMBEDDED_CATEGORIES = ("sfx", "voice")


LEADING_SILENCE_WARN_MS = 60.0


BOUNDARY_WARN = 0.08
SCAN_RATE = 16000


def load_json(path: Path, key: str) -> list[dict]:
    data = json.loads(path.read_text(encoding="utf-8"))
    entries = data if isinstance(data, list) else data.get(key, [])
    return [entry for entry in entries if isinstance(entry, dict)]


def qrc_audio_paths() -> set[str]:
    text = QRC.read_text(encoding="utf-8")
    return set(re.findall(r"<file>(assets/audio/[^<]+)</file>", text))


def direct_references() -> dict[str, list[str]]:
    """Audio paths named literally in source, keyed by path."""
    found: dict[str, list[str]] = defaultdict(list)
    for root in DIRECT_REFERENCE_ROOTS:
        target = REPO / root
        if not target.exists():
            continue
        for source in sorted(target.rglob("*")):
            if source.suffix not in DIRECT_REFERENCE_SUFFIXES:
                continue
            text = source.read_text(encoding="utf-8", errors="ignore")
            for match in AUDIO_PATH_RE.findall(text):
                found[match].append(str(source.relative_to(REPO)))
            for match in MUSIC_VAR_RE.findall(text):
                found[f"assets/audio/music/{match}"].append(
                    str(source.relative_to(REPO))
                )
    return found


def link_failures() -> list[str]:
    manifest = load_json(MANIFEST, "tracks")
    cues = load_json(CUES, "cues")
    failures: list[str] = []

    ids: dict[str, int] = defaultdict(int)
    aliases: dict[str, list[str]] = defaultdict(list)
    embedded_files: set[str] = set()

    for entry in manifest:
        ids[entry["id"]] += 1
        for alias in entry.get("aliases", []):
            aliases[alias].append(entry["id"])
        relative = entry["path"]
        if entry.get("category", "sfx") in EMBEDDED_CATEGORIES:
            embedded_files.add(f"assets/audio/{relative}")
        if not (AUDIO_DIR / relative).is_file():
            failures.append(
                f"manifest entry {entry['id']} names a missing file: {relative}"
            )

    for resource_id, count in sorted(ids.items()):
        if count > 1:
            failures.append(
                f"manifest defines {resource_id} {count} times; the last one silently wins"
            )
    for alias, owners in sorted(aliases.items()):
        if len(owners) > 1:
            failures.append(f"alias {alias} is claimed by {', '.join(sorted(owners))}")
        if alias in ids:
            failures.append(f"alias {alias} shadows a real resource id")

    known_ids = set(ids)
    for cue in cues:
        for resource_id in cue.get("resources", []):
            if resource_id not in known_ids:
                failures.append(
                    f"cue {cue['id']} names a resource the manifest does not define: "
                    f"{resource_id}"
                )

    packaged = qrc_audio_paths()
    for path in sorted(embedded_files - packaged):
        failures.append(
            f"embedded audio is not in assets.qrc, so a packaged build loses it: {path}"
        )
    for path in sorted(packaged):
        if not (REPO / path).is_file():
            failures.append(f"assets.qrc names a file that is not on disk: {path}")

    return failures


def stale_reference_findings() -> list[str]:
    findings = []
    for path, sources in sorted(direct_references().items()):
        if (REPO / path).is_file():
            continue
        findings.append(
            f"{path} is named by {', '.join(sorted(set(sources)))} but does not exist"
        )
    return findings


def decode(path: Path) -> array.array | None:
    """Mono 16-bit samples through ffmpeg, or None when it cannot be read."""
    result = subprocess.run(
        [
            "ffmpeg",
            "-v",
            "quiet",
            "-i",
            str(path),
            "-ac",
            "1",
            "-ar",
            str(SCAN_RATE),
            "-f",
            "s16le",
            "-",
        ],
        capture_output=True,
        check=False,
    )
    if result.returncode != 0 or not result.stdout:
        return None
    samples = array.array("h")
    samples.frombytes(result.stdout[: len(result.stdout) // 2 * 2])
    return samples


def scan_findings() -> list[str]:
    findings: list[str] = []
    for path in sorted(AUDIO_DIR.rglob("*.ogg")):
        samples = decode(path)
        relative = path.relative_to(REPO)
        if samples is None:
            findings.append(f"{relative}: ffmpeg could not decode it")
            continue
        if not samples:
            findings.append(f"{relative}: decodes to nothing")
            continue

        threshold = 32768 * 0.005
        leading = 0
        for value in samples:
            if abs(value) > threshold:
                break
            leading += 1
        leading_ms = leading * 1000.0 / SCAN_RATE
        if leading_ms > LEADING_SILENCE_WARN_MS:
            findings.append(
                f"{relative}: {leading_ms:.0f} ms of silence before it starts"
            )

        head = abs(samples[0]) / 32768.0
        tail = abs(samples[-1]) / 32768.0
        if head > BOUNDARY_WARN:
            findings.append(
                f"{relative}: starts at {head:.2f} of full scale, which clicks"
            )
        if tail > BOUNDARY_WARN:
            findings.append(
                f"{relative}: ends at {tail:.2f} of full scale, which clicks"
            )
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--scan",
        action="store_true",
        help="decode every file and report silence and boundary findings",
    )
    args = parser.parse_args()

    failures = link_failures()
    for failure in failures:
        print(f"  {failure}", file=sys.stderr)

    stale = stale_reference_findings()
    if stale:
        print(f"\n{len(stale)} audio files named by a script but not present:")
        for finding in stale:
            print(f"  {finding}")
        print(
            "  These scripts fall back to a silent cut. Choosing a replacement "
            "track is an authoring decision."
        )

    if args.scan:
        findings = scan_findings()
        print(f"\n{len(findings)} listening findings (nothing was changed):")
        for finding in findings:
            print(f"  {finding}")

    if failures:
        print(f"\n{len(failures)} broken audio asset links", file=sys.stderr)
        return 1
    print("Audio assets: manifest, files, QRC and direct references all agree.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
