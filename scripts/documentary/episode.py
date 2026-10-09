"""Load and validate an episode edit (``soi-documentary-episode/1``).

The edit is the single source for an episode. Its shape is documented in
``docs/DOCUMENTARY.md``; this module checks it structurally (types, unique
ids, references that point somewhere), resolves its paths and finds the
captured clips it cuts from. Editorial rules (ducking range, hits between
lines, chapters) live in ``rules.py``.

Relative paths are looked up beside the edit first and then from the
repository root, so a sample edit can name ``assets/audio/...`` directly.
Command-line ``--clips``/``--vo``/``--overlays``/``--work`` override the
edit's ``paths`` block.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path

from . import REPO, EditError

SCHEMA = "soi-documentary-episode/1"
ID_RE = re.compile(r"^[A-Za-z0-9_\-]+$")
SECTION_KINDS = (
    "cold_open",
    "title",
    "road",
    "armies",
    "ground",
    "battle",
    "climax",
    "aftermath",
    "end",
)
GRAPHIC_TYPES = (
    "title_sequence",
    "place_date",
    "order_of_battle",
    "name_plate",
    "army_counter",
    "casualty_tally",
    "chapter_card",
    "end_card",
)
MUSIC_ROLES = ("bed", "tension", "battle", "climax", "elegy", "theme", "sting")

DEFAULT_MIX = {
    "duck_db": -10.0,
    "attack": 0.35,
    "hold": 0.2,
    "release": 0.9,
    "merge_gap": 0.9,
    "lufs": -14.0,
    "true_peak": -1.0,
    "vo_lufs": -17.0,
    "vo_silence_db": -45.0,
    "room_tone_db": -66.0,
    "game_duck_db": -4.0,
    "beds_duck_db": -6.0,
    "words_per_minute": 150.0,
}


@dataclass
class Episode:
    path: Path
    data: dict
    clips: Path
    vertical_clips: Path
    vo: Path
    overlays: Path
    work: Path
    mix: dict = field(default_factory=dict)

    @property
    def id(self) -> str:
        return self.data["id"]

    @property
    def fps(self) -> float:
        return float(self.data.get("fps", 24))

    @property
    def sections(self) -> list[dict]:
        return self.data.get("sections", [])

    def shots(self) -> list[dict]:
        return [shot for section in self.sections for shot in section.get("shots", [])]

    def resolve(self, value: str | Path) -> Path:
        return resolve_path(value, self.path.parent)

    def script_path(self) -> Path:
        return self.resolve(self.data["script"])

    def side(self, side_id: str) -> dict:
        for side in self.data.get("sides", []):
            if side["id"] == side_id:
                return side
        raise EditError(f"{self.id}: no side '{side_id}'")

    def commander(self, commander_id: str) -> tuple[dict, dict]:
        for side in self.data.get("sides", []):
            for commander in side.get("commanders", []):
                if commander["id"] == commander_id:
                    return commander, side
        raise EditError(f"{self.id}: no commander '{commander_id}'")

    def section(self, section_id: str) -> dict:
        for section in self.sections:
            if section["id"] == section_id:
                return section
        raise EditError(f"{self.id}: no section '{section_id}'")

    def find_clip(self, ref: str, vertical: bool = False) -> Path:
        return find_clip(self.vertical_clips if vertical else self.clips, ref, self.path.parent)


def resolve_path(value: str | Path, base: Path) -> Path:
    path = Path(value).expanduser()
    if path.is_absolute():
        return path
    for root in (base, REPO):
        if (root / path).exists():
            return (root / path).resolve()
    return (base / path).resolve()


def find_clip(root: Path, ref: str, base: Path | None = None) -> Path:
    """A clip by ``capture/shot`` (the arena's ``NN_shot.mp4``) or by file path."""
    direct = Path(ref)
    if direct.suffix:
        for candidate in (root / direct, (base or root) / direct, REPO / direct, direct):
            if candidate.exists():
                return candidate
        raise EditError(f"clip file not found: {ref} (looked under {root})")
    folder, _, name = ref.partition("/")
    if not name:
        raise EditError(f"clip reference '{ref}' must be 'capture/shot' or a file path")
    matches = sorted((root / folder).glob(f"[0-9][0-9]_{name}.mp4"))
    if not matches:
        matches = sorted((root / folder).glob(f"{name}.*"))
    if not matches:
        raise EditError(f"no clip for '{ref}' under {root / folder}")
    return matches[0]


def capture_manifest(root: Path, ref: str) -> tuple[dict, dict]:
    """``shots.json`` and ``timeline.json`` of the capture a clip came from."""
    folder = root / ref.partition("/")[0]
    shots = json.loads((folder / "shots.json").read_text()) if (folder / "shots.json").exists() else {}
    timeline = (
        json.loads((folder / "timeline.json").read_text())
        if (folder / "timeline.json").exists()
        else {}
    )
    return shots, timeline


def load(
    path: Path,
    clips: Path | None = None,
    vertical_clips: Path | None = None,
    vo: Path | None = None,
    overlays: Path | None = None,
    work: Path | None = None,
) -> Episode:
    path = Path(path).resolve()
    try:
        data = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        raise EditError(f"cannot read edit {path}: {exc}") from exc
    validate(data, path)
    paths = data.get("paths", {})
    base = path.parent

    def pick(override: Path | None, key: str, default: str) -> Path:
        if override is not None:
            return Path(override).resolve()
        return resolve_path(paths.get(key, default), base)

    eid = data["id"]
    episode = Episode(
        path=path,
        data=data,
        clips=pick(clips, "clips", f"artifacts/documentary/{eid}/clips"),
        vertical_clips=pick(
            vertical_clips, "vertical_clips", paths.get("clips", f"artifacts/documentary/{eid}/clips")
        ),
        vo=pick(vo, "vo", f"artifacts/documentary/{eid}/vo"),
        overlays=pick(overlays, "overlays", f"artifacts/documentary/{eid}/overlays"),
        work=pick(work, "work", f"artifacts/documentary/{eid}/work"),
    )
    if vertical_clips is None and clips is not None and "vertical_clips" not in paths:
        episode.vertical_clips = episode.clips
    episode.mix = {**DEFAULT_MIX, **data.get("mix", {})}
    return episode


def _ids(items: list[dict], what: str, problems: list[str]) -> set[str]:
    seen: set[str] = set()
    for item in items:
        ident = item.get("id")
        if not isinstance(ident, str) or not ID_RE.match(ident):
            problems.append(f"{what} has a missing or malformed id: {item}")
            continue
        if ident in seen:
            problems.append(f"duplicate {what} id '{ident}'")
        seen.add(ident)
    return seen


def validate(data: dict, path: Path | None = None) -> None:
    """Structural checks; raises :class:`EditError` listing every problem found."""
    problems: list[str] = []
    where = str(path) if path else "edit"
    if data.get("schema") != SCHEMA:
        problems.append(f"schema must be '{SCHEMA}'")
    for key in ("id", "number", "title", "script", "sections"):
        if key not in data:
            problems.append(f"missing '{key}'")
    if problems:
        raise EditError(f"{where}: " + "; ".join(problems))
    if not re.fullmatch(r"ep\d{2}", data["id"]):
        problems.append("id must look like 'ep07'")
    sections = data["sections"]
    _ids(sections, "section", problems)
    shots = [shot for section in sections for shot in section.get("shots", [])]
    _ids(shots, "shot", problems)
    for section in sections:
        if not section.get("shots"):
            problems.append(f"section '{section.get('id')}' has no shots")
        kind = section.get("kind")
        if kind is not None and kind not in SECTION_KINDS:
            problems.append(f"section '{section.get('id')}' kind '{kind}' not in {SECTION_KINDS}")
    for shot in shots:
        sources = [k for k in ("clip", "graphic", "black") if shot.get(k)]
        if len(sources) != 1:
            problems.append(f"shot '{shot.get('id')}' needs exactly one of clip/graphic/black")
        if ("dur" in shot) == ("until" in shot):
            problems.append(f"shot '{shot.get('id')}' needs exactly one of dur/until")
        if shot.get("graphic") and shot["graphic"] not in GRAPHIC_TYPES:
            problems.append(f"shot '{shot.get('id')}' graphic '{shot['graphic']}' unknown")
        for layer in shot.get("overlays", []):
            if "file" not in layer:
                problems.append(f"shot '{shot.get('id')}' overlay without 'file'")
    _ids(data.get("vo", []), "vo", problems)
    _ids(data.get("music", []), "music cue", problems)
    _ids(data.get("graphics", []), "graphic", problems)
    for cue in data.get("music", []):
        if cue.get("role") not in MUSIC_ROLES:
            problems.append(f"music cue '{cue.get('id')}' role must be one of {MUSIC_ROLES}")
        if "file" not in cue and "stems" not in cue:
            problems.append(f"music cue '{cue.get('id')}' needs 'file' or 'stems'")
        if "at" not in cue or (("dur" in cue) == ("until" in cue)):
            problems.append(f"music cue '{cue.get('id')}' needs 'at' and one of dur/until")
    for graphic in data.get("graphics", []):
        if graphic.get("type") not in GRAPHIC_TYPES:
            problems.append(f"graphic '{graphic.get('id')}' type must be one of {GRAPHIC_TYPES}")
        if "at" not in graphic or (("dur" in graphic) == ("until" in graphic)):
            problems.append(f"graphic '{graphic.get('id')}' needs 'at' and one of dur/until")
    for cue in data.get("sfx", []) + data.get("beds", []):
        if "file" not in cue or "at" not in cue:
            problems.append(f"sfx/bed entry needs 'file' and 'at': {cue}")
    side_ids = _ids(data.get("sides", []), "side", problems)
    commanders = [c for side in data.get("sides", []) for c in side.get("commanders", [])]
    _ids(commanders, "commander", problems)
    for tally in data.get("casualties", []):
        if tally.get("side") not in side_ids:
            problems.append(f"casualty entry names unknown side '{tally.get('side')}'")
    if problems:
        raise EditError(f"{where}:\n  " + "\n  ".join(problems))
