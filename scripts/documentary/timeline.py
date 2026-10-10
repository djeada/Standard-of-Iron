"""Resolve an edit into absolute times, anchored to the real narration.

Every time in the edit is an expression::

    12.5                  absolute seconds
    shot:ring+0.4         0.4 s after shot 'ring' starts ('@end' counts from its end)
    section:battle@end-2  2 s before section 'battle' ends
    vo:p03+0.4            0.4 s after narration paragraph p03 starts
    vo:p03@end+1.2        1.2 s after p03's recording ends
    cue:battle_a+8        8 s into music cue 'battle_a'
    end-6                 6 s before the episode ends

Shots run back to back; a shot lasts ``dur`` seconds or ``until`` an
expression, which is how a picture is cut to the voice
(``"until": "vo:p05@end+1.0"``). A paragraph starts ``at`` an expression and
lasts exactly as long as its processed take, so recording a longer take
pushes back every shot, cue, graphic and subtitle that hangs off it. Shots are
quantised to whole frames when the picture is conformed.

A shot's ``in`` point may name an arena timeline event instead of a number
(``"in": "event:first_contact-2.0"``); it is resolved from the capture's
``timeline.json`` and the shot's ``scene_start`` (scene seconds at the clip's
first frame).
"""

from __future__ import annotations

import json
import re
from dataclasses import asdict, dataclass, field
from pathlib import Path

from . import EditError
from .episode import Episode, capture_manifest
from .narration import Paragraph, Take

EXPR_RE = re.compile(
    r"^\s*(?:(?P<kind>shot|section|vo|cue|graphic):(?P<id>[A-Za-z0-9_\-]+)(?P<edge>@end|@start)?"
    r"|(?P<end>end))?\s*(?P<offset>[+-]\s*[0-9]*\.?[0-9]+)?\s*$"
)
EVENT_RE = re.compile(
    r"^\s*event:(?P<name>[A-Za-z0-9_:]+?)\s*(?P<offset>[+-]\s*[0-9]*\.?[0-9]+)?\s*$"
)


def parse_time(expr) -> tuple[str | None, str | None, bool, float]:
    """``(kind, id, at_end, offset)`` of a time expression; kind None = absolute."""
    if isinstance(expr, (int, float)):
        return None, None, False, float(expr)
    try:
        return None, None, False, float(str(expr))
    except ValueError:
        pass
    match = EXPR_RE.match(str(expr))
    if not match or (not match["kind"] and not match["end"] and not match["offset"]):
        raise EditError(f"cannot read time '{expr}'")
    offset = float(match["offset"].replace(" ", "")) if match["offset"] else 0.0
    if match["end"]:
        return "end", None, True, offset
    return match["kind"], match["id"], match["edge"] == "@end", offset


@dataclass
class Span:
    id: str
    start: float
    end: float
    data: dict = field(default_factory=dict)

    @property
    def dur(self) -> float:
        return self.end - self.start


@dataclass
class ShotSpan(Span):
    section: str = ""
    in_point: float = 0.0


@dataclass
class VoSpan(Span):
    text: str = ""
    take: Take | None = None

    def speech(self) -> list[tuple[float, float]]:
        segments = (
            self.take.segments
            if self.take and self.take.segments
            else [(0.0, self.dur)]
        )
        return [(self.start + a, self.start + b) for a, b in segments]


@dataclass
class Timeline:
    episode: str
    fps: float
    duration: float
    sections: list[Span]
    shots: list[ShotSpan]
    vo: list[VoSpan]
    music: list[Span]
    beds: list[Span]
    sfx: list[Span]
    graphics: list[Span]
    warnings: list[str] = field(default_factory=list)

    def chapters(self) -> list[tuple[float, str]]:
        out: list[tuple[float, str]] = []
        for section in self.sections:
            title = section.data.get("chapter", section.data.get("title", section.id))
            if title is False or title is None:
                continue
            out.append((section.start, str(title)))
        if out and out[0][0] > 0:
            out[0] = (0.0, out[0][1])
        return out

    def speech(self) -> list[tuple[float, float]]:
        return sorted(span for item in self.vo for span in item.speech())

    def frame(self, seconds: float) -> int:
        return int(round(seconds * self.fps))

    def to_dict(self) -> dict:
        def strip(span: Span) -> dict:
            out = asdict(span)
            out.pop("take", None)
            out["start"] = round(span.start, 3)
            out["end"] = round(span.end, 3)
            return out

        return {
            "episode": self.episode,
            "fps": self.fps,
            "duration": round(self.duration, 3),
            "chapters": [
                {"at": round(t, 3), "title": title} for t, title in self.chapters()
            ],
            "sections": [strip(s) for s in self.sections],
            "shots": [strip(s) for s in self.shots],
            "vo": [
                {
                    **strip(v),
                    "take": (
                        {
                            "source": str(v.take.source) if v.take.source else None,
                            "take": v.take.take,
                            "scratch": v.take.scratch,
                            "estimated": v.take.estimated,
                        }
                        if v.take
                        else None
                    ),
                }
                for v in self.vo
            ],
            "music": [strip(s) for s in self.music],
            "beds": [strip(s) for s in self.beds],
            "sfx": [strip(s) for s in self.sfx],
            "graphics": [strip(s) for s in self.graphics],
            "warnings": self.warnings,
        }


class Resolver:
    def __init__(
        self, episode: Episode, paragraphs: list[Paragraph], takes: dict[str, Take]
    ):
        self.episode = episode
        self.paragraphs = {
            p.short: p for p in paragraphs if p.id.startswith(episode.id + "_p")
        }
        self.takes = takes
        self.shots: list[tuple[dict, dict]] = [
            (shot, section)
            for section in episode.sections
            for shot in section.get("shots", [])
        ]
        self.shot_index = {shot["id"]: i for i, (shot, _) in enumerate(self.shots)}
        self.sections = {section["id"]: section for section in episode.sections}
        self.warnings: list[str] = []
        self.vo_entries: dict[str, dict] = {}
        previous = None
        for entry in episode.data.get("vo", []):
            short = self._short(entry["id"])
            self.vo_entries[short] = {**entry, "id": short}
            previous = short
        for short in self.paragraphs:
            if short not in self.vo_entries:
                self.warnings.append(
                    f"paragraph {short} has no placement; chained after {previous}"
                )
                at = f"vo:{previous}@end+0.6" if previous else 0.0
                self.vo_entries[short] = {"id": short, "at": at}
                previous = short
        self.cues = {cue["id"]: cue for cue in episode.data.get("music", [])}
        self.graphic_defs = {g["id"]: g for g in episode.data.get("graphics", [])}
        self._memo: dict[tuple, float] = {}
        self._active: set[tuple] = set()

    def _short(self, ident: str) -> str:
        prefix = self.episode.id + "_"
        return ident[len(prefix) :] if ident.startswith(prefix) else ident

    def _guard(self, key: tuple, compute) -> float:
        if key in self._memo:
            return self._memo[key]
        if key in self._active:
            chain = " -> ".join(":".join(map(str, k)) for k in self._active)
            raise EditError(
                f"circular timing involving {':'.join(map(str, key))} ({chain})"
            )
        self._active.add(key)
        try:
            value = compute()
        finally:
            self._active.discard(key)
        self._memo[key] = value
        return value

    def shot_start(self, index: int) -> float:
        return 0.0 if index == 0 else self.shot_end(index - 1)

    def shot_end(self, index: int) -> float:
        def compute() -> float:
            shot, _ = self.shots[index]
            start = self.shot_start(index)
            if "dur" in shot:
                end = start + float(shot["dur"])
            else:
                end = self.when(shot["until"])
            if end - start < 0.25:
                raise EditError(
                    f"shot '{shot['id']}' would last {end - start:.2f}s "
                    f"(from {start:.2f}s to {end:.2f}s); check its 'until'"
                )
            return end

        return self._guard(("shot_end", index), compute)

    def vo_start(self, short: str) -> float:
        if short not in self.vo_entries:
            raise EditError(f"no narration paragraph '{short}' in the script")

        def compute() -> float:
            return self.when(self.vo_entries[short].get("at", 0.0))

        return self._guard(("vo", short), compute)

    def vo_duration(self, short: str) -> float:
        take = self.takes.get(f"{self.episode.id}_{short}")
        if take is None:
            raise EditError(f"no take or estimate for paragraph {short}")
        return take.duration

    def cue_start(self, ident: str) -> float:
        if ident not in self.cues:
            raise EditError(f"no music cue '{ident}'")
        return self._guard(("cue", ident), lambda: self.when(self.cues[ident]["at"]))

    def total(self) -> float:
        return self.shot_end(len(self.shots) - 1)

    def when(self, expr) -> float:
        kind, ident, at_end, offset = parse_time(expr)
        if kind is None:
            return offset
        if kind == "end":
            return self.total() + offset
        if kind == "shot":
            if ident not in self.shot_index:
                raise EditError(f"time '{expr}' names unknown shot '{ident}'")
            i = self.shot_index[ident]
            return (self.shot_end(i) if at_end else self.shot_start(i)) + offset
        if kind == "section":
            if ident not in self.sections:
                raise EditError(f"time '{expr}' names unknown section '{ident}'")
            ids = [shot["id"] for shot in self.sections[ident]["shots"]]
            if at_end:
                return self.shot_end(self.shot_index[ids[-1]]) + offset
            return self.shot_start(self.shot_index[ids[0]]) + offset
        if kind == "vo":
            short = self._short(ident)
            start = self.vo_start(short)
            return (start + self.vo_duration(short) if at_end else start) + offset
        if kind == "cue":
            start = self.cue_start(ident)
            if at_end:
                return self.span_end(self.cues[ident], start) + offset
            return start + offset
        if kind == "graphic":
            if ident not in self.graphic_defs:
                raise EditError(f"time '{expr}' names unknown graphic '{ident}'")
            start = self.when(self.graphic_defs[ident]["at"])
            return (
                self.span_end(self.graphic_defs[ident], start) if at_end else start
            ) + offset
        raise EditError(f"cannot read time '{expr}'")

    def span_end(self, item: dict, start: float) -> float:
        if "dur" in item:
            return start + float(item["dur"])
        if "until" in item:
            return self.when(item["until"])
        return start

    def in_point(self, shot: dict) -> float:
        value = shot.get("in", 0.0)
        if isinstance(value, (int, float)):
            return float(value)
        match = EVENT_RE.match(str(value))
        if not match:
            raise EditError(f"shot '{shot['id']}': cannot read in point '{value}'")
        ref = shot.get("clip", "")
        shots_json, timeline_json = capture_manifest(self.episode.clips, ref)
        name, _, side = match["name"].partition(":")
        when = None
        for match_data in timeline_json.get("matches", []):
            for event in match_data.get("events", []):
                if event.get("event") in (
                    match["name"],
                    f"{name}:{side}" if side else name,
                ):
                    when = float(event["at"])
                    break
            if when is not None:
                break
        if when is None:
            raise EditError(
                f"shot '{shot['id']}': event '{match['name']}' not in {ref}'s timeline.json"
            )
        scene_start = shot.get("scene_start")
        if scene_start is None:
            shot_name = ref.partition("/")[2]
            for entry in shots_json.get("shots", []):
                if entry.get("name") == shot_name and "scene_start" in entry:
                    scene_start = entry["scene_start"]
        if scene_start is None:
            raise EditError(
                f"shot '{shot['id']}': an event in point needs 'scene_start' "
                "(scene seconds at the clip's first frame)"
            )
        offset = float(match["offset"].replace(" ", "")) if match["offset"] else 0.0
        return max(0.0, when - float(scene_start) + offset)


def resolve(
    episode: Episode, paragraphs: list[Paragraph], takes: dict[str, Take]
) -> Timeline:
    r = Resolver(episode, paragraphs, takes)
    shots = []
    for i, (shot, section) in enumerate(r.shots):
        shots.append(
            ShotSpan(
                shot["id"],
                r.shot_start(i),
                r.shot_end(i),
                shot,
                section=section["id"],
                in_point=r.in_point(shot) if shot.get("clip") else 0.0,
            )
        )
    sections = []
    for section in episode.sections:
        first = r.shot_index[section["shots"][0]["id"]]
        last = r.shot_index[section["shots"][-1]["id"]]
        sections.append(
            Span(section["id"], r.shot_start(first), r.shot_end(last), section)
        )
    vo = []
    for short, entry in r.vo_entries.items():
        if short not in r.paragraphs:
            raise EditError(f"vo entry '{short}' is not a paragraph in the script")
        start = r.vo_start(short)
        paragraph = r.paragraphs[short]
        take = takes[paragraph.id]
        vo.append(
            VoSpan(short, start, start + take.duration, entry, paragraph.clean, take)
        )
    vo.sort(key=lambda v: v.start)

    def spans(items: list[dict], prefix: str) -> list[Span]:
        out = []
        for index, item in enumerate(items):
            start = r.when(item["at"])
            end = r.span_end(item, start)
            out.append(Span(item.get("id", f"{prefix}{index}"), start, end, item))
        return out

    timeline = Timeline(
        episode.id,
        episode.fps,
        r.total(),
        sections,
        shots,
        vo,
        spans(episode.data.get("music", []), "music"),
        spans(episode.data.get("beds", []), "bed"),
        spans(episode.data.get("sfx", []), "sfx"),
        spans(episode.data.get("graphics", []), "graphic"),
        r.warnings,
    )
    for item in timeline.vo:
        if item.end > timeline.duration + 0.01:
            timeline.warnings.append(
                f"paragraph {item.id} runs past the end of the picture"
            )
    return timeline


def write(timeline: Timeline, path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(timeline.to_dict(), indent=1))
    return path
