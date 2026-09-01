#!/usr/bin/env python3
"""Report which game audio exists, which is missing, and which is orphaned.

Three files have to agree for a sound to be heard in game:

  assets/audio/audio_cues.json      the cues the game can fire, and the
                                    manifest resources each one draws from
  assets/audio/audio_manifest.json  resource id -> file, category, tags
  assets/audio/**.ogg               the files themselves

plus a call site in C++ or QML that actually fires the cue.  This script
checks every link in that chain and writes the result to
docs/AUDIO_WISHLIST.md, so "what audio do we still need" is a command
rather than a memory exercise.

The report is the wiring matrix for the whole catalogue.  Four states are
tracked separately, because a cue can pass three of them and still be silent
in a real game: declared (it is in the catalogue), called by code (a call site
fires it), resource loaded (its pool resolves to files that exist), and heard
in gameplay.  The last one cannot be read out of the source tree at all -- it
comes from a mission summary written by a real run
(SOI_AUDIO_TRACE_SUMMARY, see docs/AUDIO_RUNTIME_TRACE.md) and from the
scenario tests that drive production gameplay paths.

Usage:
    python3 scripts/audio_report.py             # rewrite docs/AUDIO_WISHLIST.md
    python3 scripts/audio_report.py --stdout    # print instead of writing
    python3 scripts/audio_report.py --check     # exit 1 on a broken link
    python3 scripts/audio_report.py --runtime artifacts/audio/mission.json
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
AUDIO_DIR = REPO / "assets" / "audio"
MANIFEST = AUDIO_DIR / "audio_manifest.json"
CUES = AUDIO_DIR / "audio_cues.json"
CUE_HEADER = REPO / "game" / "audio" / "cue_ids.h"
REPORT = REPO / "docs" / "AUDIO_WISHLIST.md"


CALL_SITE_ROOTS = ("app", "game", "render", "scene", "ui", "tools", "main.cpp")
VERIFICATION_ROOTS = ("tests",)
CALL_SITE_SUFFIXES = (".cpp", ".h", ".qml", ".js")
NOT_A_CALL_SITE = {
    REPO / "game" / "audio" / "audio_cues.h",
    REPO / "game" / "audio" / "audio_cues.cpp",
    REPO / "game" / "audio" / "cue_ids.h",
}


TAG_RESOLVED_CATEGORIES = {"voice", "music", "ambience"}


TAG_RESOLVED_SFX_TAGS = ("state",)

CONSTANT_RE = re.compile(
    r'inline\s+constexpr\s+const\s+char\*\s+(k_\w+)\s*=\s*\n?\s*"([^"]+)"'
)


IMPORTANCE_LEVELS = ("required", "optional", "ambient")


PACING_BUCKETS = (
    (250, "continuous"),
    (1000, "frequent"),
    (5000, "occasional"),
)


def pacing(cooldown_ms: int) -> str:
    for ceiling, name in PACING_BUCKETS:
        if cooldown_ms < ceiling:
            return name
    return "rare"


@dataclass
class Findings:
    missing_files: list[str] = field(default_factory=list)
    unmanifested_files: list[str] = field(default_factory=list)
    unknown_cue_resources: list[str] = field(default_factory=list)
    header_only_cues: list[str] = field(default_factory=list)
    catalog_only_cues: list[str] = field(default_factory=list)
    silent_cues: list[dict] = field(default_factory=list)
    placeholder_cues: list[dict] = field(default_factory=list)
    unwired_cues: list[str] = field(default_factory=list)
    cue_call_sites: dict[str, list[str]] = field(default_factory=dict)
    cue_verification: dict[str, list[str]] = field(default_factory=dict)
    cue_audience: dict[str, str] = field(default_factory=dict)
    orphan_resources: list[str] = field(default_factory=list)
    silent_required_cues: list[str] = field(default_factory=list)
    unwired_required_cues: list[str] = field(default_factory=list)
    unknown_importance: list[str] = field(default_factory=list)

    def structural_problems(self) -> list[str]:
        problems = []
        for path in self.missing_files:
            problems.append(
                f"manifest entry points at a file that is not there: {path}"
            )
        for ref in self.unknown_cue_resources:
            problems.append(
                f"cue references a resource the manifest does not define: {ref}"
            )
        for cue in self.header_only_cues:
            problems.append(f"cue constant in cue_ids.h has no catalog entry: {cue}")
        for cue in self.catalog_only_cues:
            problems.append(f"catalog cue has no constant in cue_ids.h: {cue}")
        for cue in self.silent_required_cues:
            problems.append(
                f"required cue has no playable binding, so the action is silent: {cue}"
            )
        for cue in self.unwired_required_cues:
            problems.append(f"required cue is never fired from the game sources: {cue}")
        for cue in self.unknown_importance:
            problems.append(
                f"cue declares an importance that is not required/optional/ambient: "
                f"{cue}"
            )
        return problems


def load_json(path: Path, key: str) -> list[dict]:
    data = json.loads(path.read_text(encoding="utf-8"))
    entries = data if isinstance(data, list) else data.get(key, [])
    return [entry for entry in entries if isinstance(entry, dict)]


def header_cue_ids() -> dict[str, str]:
    """Cue id -> C++ constant name, read from cue_ids.h."""
    text = CUE_HEADER.read_text(encoding="utf-8")
    return {cue_id: name for name, cue_id in CONSTANT_RE.findall(text)}


def source_files(roots: tuple[str, ...]) -> list[tuple[Path, str]]:
    files = []
    for root in roots:
        target = REPO / root
        sources = [target] if target.is_file() else sorted(target.rglob("*"))
        for source in sources:
            if source.suffix not in CALL_SITE_SUFFIXES:
                continue
            if source in NOT_A_CALL_SITE:
                continue
            files.append((source, source.read_text(encoding="utf-8", errors="ignore")))
    return files


def call_site_files() -> list[tuple[Path, str]]:
    return source_files(CALL_SITE_ROOTS)


def call_site_text() -> str:
    return "\n".join(text for _path, text in call_site_files())


def audience_of(
    cue_id: str, constant: str | None, files: list[tuple[Path, str]]
) -> str:
    """Who the cue is played to, read off the call site that fires it.

    `AudioCueEvent::for_owner` scopes a cue to one player; anything else
    reaches whoever is listening. The distinction is the difference between a
    correct silence and a wiring bug, so the matrix has to state it.
    """
    needles = [f'"{cue_id}"']
    if constant:
        needles.append(constant)

    scoped = False
    unscoped = False
    for _path, text in files:
        lines = text.splitlines()
        for index, line in enumerate(lines):
            if not any(needle in line for needle in needles):
                continue
            window = "\n".join(lines[max(0, index - 2) : index + 3])
            if "for_owner" in window:
                scoped = True
            else:
                unscoped = True
    if scoped and unscoped:
        return "mixed"
    if scoped:
        return "owner"
    return "everyone" if unscoped else "-"


def load_runtime_summaries(paths: list[str]) -> dict[str, dict]:
    """Merge mission summaries written by SOI_AUDIO_TRACE_SUMMARY."""
    merged: dict[str, dict] = {}
    for raw in paths:
        path = Path(raw)
        if not path.is_absolute():
            path = REPO / path
        data = json.loads(path.read_text(encoding="utf-8"))
        for cue in data.get("cues", []):
            entry = merged.setdefault(cue["cue"], {"requests": 0, "accepted": 0})
            entry["requests"] += int(cue.get("requests", 0))
            entry["accepted"] += int(cue.get("accepted", 0))
    return merged


def where_fired(
    cue_id: str, constant: str | None, files: list[tuple[Path, str]]
) -> list[str]:
    """Files that fire this cue, so "is it wired" is answerable per cue.

    A cue can be bound to a perfectly good asset and still never play, which
    from the player's side is the same as having no sound at all. Listing the
    call site makes that visible instead of leaving it to a grep.
    """
    patterns = [re.escape(f'"{cue_id}"')]
    if constant:

        patterns.append(rf"\b{re.escape(constant)}\b")
    needle = re.compile("|".join(patterns))
    return [str(path.relative_to(REPO)) for path, text in files if needle.search(text)]


def source_files_for_verification() -> list[tuple[Path, str]]:
    return source_files(VERIFICATION_ROOTS)


def collect() -> tuple[Findings, list[dict], list[dict]]:
    manifest = load_json(MANIFEST, "tracks")
    cues = load_json(CUES, "cues")
    found = Findings()

    by_id: dict[str, dict] = {}
    manifest_paths: set[str] = set()
    for entry in manifest:
        by_id[entry["id"]] = entry
        manifest_paths.add(entry["path"])
        if not (AUDIO_DIR / entry["path"]).is_file():
            found.missing_files.append(f"{entry['id']} -> {entry['path']}")

    on_disk = {str(path.relative_to(AUDIO_DIR)) for path in AUDIO_DIR.rglob("*.ogg")}
    found.unmanifested_files = sorted(on_disk - manifest_paths)

    header = header_cue_ids()
    catalog_ids = {cue["id"] for cue in cues}
    found.header_only_cues = sorted(set(header) - catalog_ids)
    found.catalog_only_cues = sorted(catalog_ids - set(header))

    source_files = call_site_files()
    test_files = source_files_for_verification()
    referenced_resources: set[str] = set()

    for cue in cues:
        cue_id = cue["id"]
        resources = cue.get("resources", [])
        playable = []
        for resource_id in resources:
            if resource_id not in by_id:
                found.unknown_cue_resources.append(f"{cue_id} -> {resource_id}")
                continue
            referenced_resources.add(resource_id)
            playable.append(resource_id)

        importance = cue.get("importance", "optional")
        if importance not in IMPORTANCE_LEVELS:
            found.unknown_importance.append(f"{cue_id} -> {importance}")

        if not playable:
            found.silent_cues.append(cue)
            if importance == "required":
                found.silent_required_cues.append(cue_id)
        elif cue.get("placeholder"):
            found.placeholder_cues.append(cue)

        constant = header.get(cue_id)
        sites = where_fired(cue_id, constant, source_files)
        found.cue_call_sites[cue_id] = sites
        found.cue_audience[cue_id] = audience_of(cue_id, constant, source_files)
        found.cue_verification[cue_id] = where_fired(cue_id, constant, test_files)
        if not sites:
            found.unwired_cues.append(cue_id)
            if importance == "required":
                found.unwired_required_cues.append(cue_id)

    for entry in manifest:
        if entry["id"] in referenced_resources:
            continue
        if entry.get("category", "sfx") in TAG_RESOLVED_CATEGORIES:
            continue
        tags = entry.get("tags", {})
        if any(tag in tags for tag in TAG_RESOLVED_SFX_TAGS):
            continue
        found.orphan_resources.append(entry["id"])

    found.orphan_resources.sort()
    return found, manifest, cues


def bullet(lines: list[str], items: list[str], empty: str) -> None:
    if not items:
        lines.append(f"- {empty}")
        return
    lines.extend(f"- `{item}`" for item in items)


def render(
    found: Findings,
    manifest: list[dict],
    cues: list[dict],
    runtime: dict[str, dict] | None = None,
) -> str:
    covered = len(cues) - len(found.silent_cues)
    lines: list[str] = []
    add = lines.append

    add("# Audio coverage and wishlist")
    add("")
    add(
        "Generated by `make audio-report` (`scripts/audio_report.py`). Do not edit by hand."
    )
    add("")
    add("## Coverage")
    add("")
    add(f"- Audio files on disk: **{len(list(AUDIO_DIR.rglob('*.ogg')))}**")
    add(f"- Manifest resources: **{len(manifest)}**")
    add(f"- Declared cues: **{len(cues)}**")
    add(f"- Cues that can play something today: **{covered}**")
    add(f"- Cues waiting on an asset: **{len(found.silent_cues)}**")
    add(f"- Cues playing a stand-in: **{len(found.placeholder_cues)}**")
    unverified = [
        cue["id"] for cue in cues if not found.cue_verification.get(cue["id"])
    ]
    add(f"- Cues no test fires through gameplay: **{len(unverified)}**")
    if runtime:
        never = [
            cue["id"]
            for cue in cues
            if cue["id"] in runtime and runtime[cue["id"]]["accepted"] == 0
        ]
        add(f"- Cues measured as requested but never heard: **{len(never)}**")
    add("")

    add("## Sounds we should have but do not")
    add("")
    add("These cues are fired by the game already. They resolve to nothing, so they")
    add("play silence until an asset is added to the manifest and listed here.")
    add("")
    if found.silent_cues:
        for cue in found.silent_cues:
            add(f"### `{cue['id']}`")
            add("")
            add(f"- **Fires when:** {cue.get('description', 'unspecified')}")
            add(f"- **Wanted:** {cue.get('wanted', 'unspecified')}")
            add("")
    else:
        add("- Every declared cue resolves to at least one asset.")
        add("")

    add("## Cues playing a stand-in")
    add("")
    add("These are audible but reuse a clip authored for something else.")
    add("")
    if found.placeholder_cues:
        for cue in found.placeholder_cues:
            resources = ", ".join(f"`{r}`" for r in cue.get("resources", []))
            add(f"- **`{cue['id']}`** currently uses {resources}.")
            add(f"  Wanted: {cue.get('wanted', 'a dedicated clip')}")
    else:
        add("- None.")
    add("")

    add("## Cues with no call site")
    add("")
    add("Declared in the catalog but never fired from C++ or QML. Either the")
    add("gameplay hook is still missing or the cue should be dropped.")
    add("")
    bullet(lines, found.unwired_cues, "Every cue is fired from somewhere.")
    add("")

    add("## Assets the game cannot reach")
    add("")
    add("Manifest resources in a cue-driven category that no cue references.")
    add("")
    bullet(lines, found.orphan_resources, "None.")
    add("")

    add("## Files not in the manifest")
    add("")
    bullet(lines, found.unmanifested_files, "None.")
    add("")

    add("## Wiring matrix")
    add("")
    add(
        "One row per cue, from the gameplay action down to whether a player has "
        "actually heard it. A cue can be declared, called and fully loaded and "
        "still be silent in a real match, so the last two columns are kept apart "
        "from the first three."
    )
    add("")
    add(
        "**Audience** is read off the call site: `owner` means the cue is scoped "
        "to one player with `AudioCueEvent::for_owner` and is *meant* to be silent "
        "for everyone else. **Pacing** is the cue cooldown bucketed: continuous "
        "(<0.25 s), frequent (<1 s), occasional (<5 s), rare. **Verified by** lists "
        "the tests that fire the cue through a production path. **Heard** is filled "
        "in from mission summaries passed with `--runtime`; without one it reads "
        "`not measured`, which is not the same as silent."
    )
    add("")
    add(
        "| Cue | Fires when | Fired from | Audience | Pacing | Pool | Verified by | Heard |"
    )
    add(
        "| --- | ---------- | ---------- | -------- | ------ | ---- | ----------- | ----- |"
    )
    for cue in cues:
        cue_id = cue["id"]
        sites = found.cue_call_sites.get(cue_id, [])
        where = ", ".join(f"`{site}`" for site in sites) if sites else "**nothing**"
        checks = found.cue_verification.get(cue_id, [])
        verified = (
            ", ".join(f"`{site}`" for site in checks) if checks else "**nothing**"
        )
        pool = len(cue.get("resources", []))
        heard = "not measured"
        if runtime:
            measured = runtime.get(cue_id)
            if measured is None:
                heard = "never requested"
            elif measured["accepted"] == 0:
                heard = f"**0 of {measured['requests']}**"
            else:
                heard = f"{measured['accepted']} of {measured['requests']}"
        add(
            f"| `{cue_id}` | {cue.get('description', '-')} | {where} | "
            f"{found.cue_audience.get(cue_id, '-')} | "
            f"{pacing(int(cue.get('cooldown_ms', 0)))} | {pool} | {verified} | {heard} |"
        )
    add("")

    problems = found.structural_problems()
    add("## Broken links")
    add("")
    bullet(lines, problems, "None.")
    add("")

    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--stdout", action="store_true", help="print instead of writing the report"
    )
    parser.add_argument(
        "--check", action="store_true", help="exit non-zero on a broken link"
    )
    parser.add_argument(
        "--runtime",
        action="append",
        default=[],
        metavar="SUMMARY.json",
        help="mission summary from SOI_AUDIO_TRACE_SUMMARY; repeatable",
    )
    args = parser.parse_args()

    found, manifest, cues = collect()
    runtime = load_runtime_summaries(args.runtime) if args.runtime else None
    report = render(found, manifest, cues, runtime)

    if args.stdout:
        print(report, end="")
    else:
        REPORT.write_text(report, encoding="utf-8")
        print(f"Wrote {REPORT.relative_to(REPO)}")

    print(
        f"{len(cues)} cues: {len(cues) - len(found.silent_cues)} covered, "
        f"{len(found.silent_cues)} awaiting assets, "
        f"{len(found.placeholder_cues)} on stand-ins, "
        f"{len(found.unwired_cues)} unwired, "
        f"{sum(1 for cue in cues if not found.cue_verification.get(cue['id']))} "
        f"unverified.",
        file=sys.stderr,
    )

    problems = found.structural_problems()
    if problems:
        print("Broken audio links:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        if args.check:
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
