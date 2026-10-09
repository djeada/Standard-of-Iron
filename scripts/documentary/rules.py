"""The series' editorial rules (epic #1547), checked against a resolved timeline.

Errors stop ``package``; warnings are reported and kept in ``upload.json``.

* Under narration only beds, tension and elegy cues may play, ducked
  9-12 dB; theme, battle and climax cues never run under words.
* Hits (``"hit": true`` effects and cue ``hits``) land between lines.
* A climax is preceded by 0.5-1.5 s of near-silence: no words and no other
  music.
* Frame zero is a lit picture: the first shot is footage with no fade-in.
* The title sequence is at most 10 s; the cold open about 20 s.
* The episode ends on the Steam CTA (an ``end_card`` graphic).
* ``combat_last_defensive_wall`` is never used; slow, melancholic library
  tracks are flagged in reels (marketing music is driving and upbeat).
* Script 550-750 words, narration covering about half the runtime, 5-8 min.
"""

from __future__ import annotations

from dataclasses import dataclass

from .mix import BANNED_MUSIC
from .timeline import Timeline

UNDER_VO_ROLES = {"bed", "tension", "elegy"}
DUCK_RANGE = (-12.0, -9.0)
SUSPECT_FOR_MARKETING = (
    "_at_dusk",
    "uneasy_rest",
    "campfire_shadows",
    "sentinels_of_the_peak",
    "hearth_and_harbor",
    "defeat_",
    "retreat_",
)
HIT_MARGIN = 0.2


@dataclass
class Finding:
    level: str
    message: str

    def __str__(self) -> str:
        return f"{self.level}: {self.message}"


def _overlap(a: tuple[float, float], b: tuple[float, float]) -> float:
    return max(0.0, min(a[1], b[1]) - max(a[0], b[0]))


def music_files(item: dict) -> list[str]:
    if "stems" in item:
        return [str(v) for v in item["stems"].values()]
    return [str(item["file"])] if "file" in item else []


def check_music_choice(files: list[str], marketing: bool) -> list[Finding]:
    out = []
    for name in files:
        if any(banned in name for banned in BANNED_MUSIC):
            out.append(
                Finding("error", f"{name} is banned from every cut (the 'sad music')")
            )
        elif marketing and any(s in name for s in SUSPECT_FOR_MARKETING):
            out.append(
                Finding(
                    "warning",
                    f"{name} reads slow or melancholic; reels need driving music",
                )
            )
    return out


def check(episode, timeline: Timeline, words: int | None = None) -> list[Finding]:
    found: list[Finding] = []
    mix = episode.mix
    speech = timeline.speech()
    duck = float(mix["duck_db"])
    if not DUCK_RANGE[0] <= duck <= DUCK_RANGE[1]:
        found.append(
            Finding(
                "error",
                f"mix.duck_db {duck} is outside {DUCK_RANGE[0]}..{DUCK_RANGE[1]} dB",
            )
        )
    for cue in timeline.music:
        data = cue.data
        found += check_music_choice(music_files(data), marketing=False)
        own = data.get("duck_db")
        if own is not None and not DUCK_RANGE[0] <= float(own) <= DUCK_RANGE[1]:
            found.append(
                Finding(
                    "error",
                    f"cue {cue.id} ducks {own} dB; keep music 9-12 dB under words",
                )
            )
        fi, fo = float(data.get("fade_in", 0.05)), float(data.get("fade_out", 0.4))
        body = (cue.start + fi, cue.end - fo)
        if data["role"] not in UNDER_VO_ROLES and not data.get("stems"):
            words_under = sum(_overlap(body, s) for s in speech)
            if words_under > 0.25:
                found.append(
                    Finding(
                        "error",
                        f"{data['role']} cue {cue.id} plays under {words_under:.1f}s of narration",
                    )
                )
        for offset in data.get("hits", []):
            t = cue.start + float(offset)
            if any(a - HIT_MARGIN <= t <= b + HIT_MARGIN for a, b in speech):
                found.append(
                    Finding("error", f"hit in cue {cue.id} at {t:.2f}s lands on a line")
                )
        if data["role"] == "climax":
            pre = float(data.get("pre_silence", 0.0))
            if not 0.5 <= pre <= 1.5:
                found.append(
                    Finding(
                        "error",
                        f"climax {cue.id} needs pre_silence 0.5-1.5 s (has {pre})",
                    )
                )
            window = (cue.start - max(pre, 0.5), cue.start)
            if any(_overlap(window, s) > 0.02 for s in speech):
                found.append(
                    Finding(
                        "error",
                        f"narration runs into the silence before climax {cue.id}",
                    )
                )
            for other in timeline.music:
                if (
                    other is not cue
                    and _overlap(window, (other.start, other.end)) > 0.05
                ):
                    found.append(
                        Finding(
                            "error",
                            f"cue {other.id} plays in the silence before climax {cue.id}",
                        )
                    )
    for effect in timeline.sfx:
        if effect.data.get("hit") and any(
            a - HIT_MARGIN <= effect.start <= b + HIT_MARGIN for a, b in speech
        ):
            found.append(
                Finding(
                    "error",
                    f"hit {effect.data['file']} at {effect.start:.2f}s lands on a line",
                )
            )
    if timeline.shots:
        first = timeline.shots[0].data
        if not first.get("clip") or float(first.get("fade_in", 0.0)) > 0:
            found.append(
                Finding(
                    "error",
                    "frame zero must be lit footage: open on a clip with no fade-in",
                )
            )
    for g in timeline.graphics:
        if g.data["type"] == "title_sequence" and g.dur > 10.05:
            found.append(
                Finding("error", f"title sequence {g.id} runs {g.dur:.1f}s (max 10)")
            )
    for section in timeline.sections:
        if section.data.get("kind") == "cold_open" and section.dur > 22.0:
            found.append(
                Finding(
                    "warning", f"cold open runs {section.dur:.1f}s (template: <= 20 s)"
                )
            )
    if not any(g.data["type"] == "end_card" for g in timeline.graphics):
        found.append(
            Finding("error", "no end_card graphic: every episode ends on the Steam CTA")
        )
    elif (
        timeline.graphics
        and max(g.end for g in timeline.graphics if g.data["type"] == "end_card")
        < timeline.duration - 0.5
    ):
        found.append(
            Finding(
                "warning", "the end card is not on screen at the end of the episode"
            )
        )
    covered = sum(v.dur for v in timeline.vo)
    share = covered / timeline.duration if timeline.duration else 0.0
    if not 0.4 <= share <= 0.62:
        found.append(
            Finding(
                "warning",
                f"narration covers {share:.0%} of the runtime (aim for about 50%)",
            )
        )
    if not 300 <= timeline.duration <= 480:
        found.append(
            Finding(
                "warning",
                f"runtime {timeline.duration / 60:.1f} min (episodes are 5-8 min)",
            )
        )
    if words is not None and not 550 <= words <= 750:
        found.append(Finding("warning", f"script has {words} words (aim for 550-750)"))
    for warning in timeline.warnings:
        found.append(Finding("warning", warning))
    return found
