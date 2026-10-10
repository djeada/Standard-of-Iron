"""Subtitles from the script text and the narration's real timing.

A paragraph's words are split into sentences, long sentences are broken at a
comma, semicolon, dash or conjunction nearest their middle, and each piece is
wrapped onto at most two lines of 42 characters. Timing comes from the take:
sentence boundaries snap to the pauses ``silencedetect`` found in the
recording, and the words inside a sentence share its span by length. A
caption lingers a little after the voice stops, never overlaps the next one,
and stays on screen between ``MIN_SECONDS`` and ``MAX_SECONDS``.

The reel uses the same machinery with short one-line chunks (``REEL``).
"""

from __future__ import annotations

import re
from dataclasses import dataclass

SENTENCE_RE = re.compile(r"(?<=[.!?])[\"')\]]*\s+(?=[\"'(\[]?[A-Z0-9])")
BREAK_AFTER = re.compile(r"[,;:—–]$|--$")
CONJUNCTIONS = {
    "and",
    "but",
    "while",
    "as",
    "who",
    "which",
    "that",
    "where",
    "when",
    "until",
    "or",
    "so",
}


@dataclass(frozen=True)
class Style:
    max_line: int = 42
    max_lines: int = 2
    min_seconds: float = 1.0
    max_seconds: float = 7.0
    linger: float = 0.35
    gap: float = 0.084
    upper: bool = False


EPISODE = Style()
REEL = Style(
    max_line=18,
    max_lines=2,
    min_seconds=0.6,
    max_seconds=2.4,
    linger=0.12,
    gap=0.0,
    upper=True,
)


@dataclass
class Caption:
    start: float
    end: float
    text: str
    paragraph: str = ""

    def lines(self, style: Style = EPISODE) -> list[str]:
        return wrap_lines(self.text, style.max_line, style.max_lines)


def split_sentences(text: str) -> list[str]:
    text = re.sub(r"\s+", " ", text).strip()
    if not text:
        return []
    return [s.strip() for s in SENTENCE_RE.split(text) if s.strip()]


def wrap_lines(text: str, max_line: int, max_lines: int = 2) -> list[str]:
    """Balanced wrap: the split that makes the lines most even, each <= max_line."""
    words = text.split()
    if len(text) <= max_line or len(words) == 1:
        return [text]
    if max_lines == 2:
        best = None
        for cut in range(1, len(words)):
            a, b = " ".join(words[:cut]), " ".join(words[cut:])
            if len(a) <= max_line and len(b) <= max_line:
                score = abs(len(a) - len(b)) - (
                    4 if BREAK_AFTER.search(words[cut - 1]) else 0
                )
                if best is None or score < best[0]:
                    best = (score, [a, b])
        if best:
            return best[1]
    lines: list[str] = []
    for word in words:
        if lines and len(lines[-1]) + 1 + len(word) <= max_line:
            lines[-1] += " " + word
        else:
            lines.append(word)
    return lines


def _fits(text: str, style: Style) -> bool:
    lines = wrap_lines(text, style.max_line, style.max_lines)
    return len(lines) <= style.max_lines and all(
        len(line) <= style.max_line for line in lines
    )


def chunk(sentence: str, style: Style) -> list[str]:
    """Break a sentence into caption-sized pieces at natural seams."""
    if _fits(sentence, style):
        return [sentence]
    words = sentence.split()
    if len(words) == 1:
        return [sentence]
    middle = len(sentence) / 2
    best = None
    position = 0
    for i, word in enumerate(words[:-1]):
        position += len(word) + 1
        nxt = words[i + 1].lower().strip(",;:")
        seam = 0
        if BREAK_AFTER.search(word):
            seam = 3
        elif nxt in CONJUNCTIONS:
            seam = 2
        score = abs(position - middle) / max(1.0, len(sentence)) - 0.12 * seam
        if best is None or score < best[0]:
            best = (score, i + 1)
    cut = best[1]
    return chunk(" ".join(words[:cut]), style) + chunk(" ".join(words[cut:]), style)


def _weight(text: str) -> float:
    return len(re.sub(r"[^A-Za-z0-9]", "", text)) + 3.0


def _pauses(
    segments: list[tuple[float, float]], min_gap: float = 0.2
) -> list[tuple[float, float]]:
    return [
        (segments[i][1], segments[i + 1][0])
        for i in range(len(segments) - 1)
        if segments[i + 1][0] - segments[i][1] >= min_gap
    ]


def sentence_times(
    sentences: list[str], segments: list[tuple[float, float]], duration: float
) -> list[tuple[float, float]]:
    """Span of each sentence inside a take (seconds from the take's start)."""
    if not sentences:
        return []
    if not segments:
        segments = [(0.0, duration)]
    start, end = segments[0][0], segments[-1][1]
    weights = [_weight(s) for s in sentences]
    total = sum(weights)
    expected = []
    acc = 0.0
    for w in weights[:-1]:
        acc += w
        expected.append(start + (end - start) * acc / total)
    pauses = _pauses(segments)
    average = (end - start) / len(sentences)
    tolerance = max(1.0, 0.45 * average)
    bounds: list[tuple[float, float]] = []
    used = -1
    for guess in expected:
        best = None
        for i, (a, b) in enumerate(pauses):
            if i <= used:
                continue
            mid = (a + b) / 2
            if abs(mid - guess) <= tolerance and (
                best is None or abs(mid - guess) < best[0]
            ):
                best = (abs(mid - guess), i)
        if best is not None:
            used = best[1]
            bounds.append(pauses[best[1]])
        else:
            bounds.append((guess, guess))
    spans = []
    cursor = start
    for a, b in bounds:
        spans.append((cursor, max(cursor + 0.05, a)))
        cursor = max(cursor + 0.05, b)
    spans.append((cursor, max(cursor + 0.05, end)))
    return spans


def paragraph_captions(
    paragraph_id: str,
    text: str,
    start: float,
    segments: list[tuple[float, float]],
    duration: float,
    style: Style = EPISODE,
) -> list[Caption]:
    """Captions for one paragraph placed at ``start`` on the episode timeline."""
    sentences = split_sentences(text)
    captions: list[Caption] = []
    for sentence, (a, b) in zip(
        sentences, sentence_times(sentences, segments, duration), strict=True
    ):
        pieces = chunk(sentence, style)
        weights = [_weight(p) for p in pieces]
        total = sum(weights)
        t = a
        for piece, weight in zip(pieces, weights, strict=True):
            length = (b - a) * weight / total
            words = piece.upper() if style.upper else piece
            captions.append(Caption(start + t, start + t + length, words, paragraph_id))
            t += length
    return captions


def _split_long(captions: list[Caption], style: Style) -> list[Caption]:
    out: list[Caption] = []
    for caption in captions:
        words = caption.text.split()
        if caption.end - caption.start <= style.max_seconds or len(words) < 2:
            out.append(caption)
            continue
        cut = len(words) // 2
        mid = caption.start + (caption.end - caption.start) * _weight(
            " ".join(words[:cut])
        ) / _weight(caption.text)
        out += _split_long(
            [
                Caption(caption.start, mid, " ".join(words[:cut]), caption.paragraph),
                Caption(mid, caption.end, " ".join(words[cut:]), caption.paragraph),
            ],
            style,
        )
    return out


def finalise(captions: list[Caption], style: Style = EPISODE) -> list[Caption]:
    """Add linger, enforce min/max duration and the gap between captions."""
    captions = _split_long(sorted(captions, key=lambda c: c.start), style)
    out: list[Caption] = []
    for i, caption in enumerate(captions):
        limit = (
            captions[i + 1].start - style.gap if i + 1 < len(captions) else float("inf")
        )
        end = min(caption.end + style.linger, limit)
        if end - caption.start < style.min_seconds:
            end = min(caption.start + style.min_seconds, limit)
        end = min(end, caption.start + style.max_seconds)
        start = caption.start
        if out and start < out[-1].end + style.gap:
            start = out[-1].end + style.gap
        if end - start < 0.2:
            end = start + 0.2
        out.append(
            Caption(round(start, 3), round(end, 3), caption.text, caption.paragraph)
        )
    return out


def _stamp(seconds: float, sep: str) -> str:
    ms = int(round(max(0.0, seconds) * 1000))
    h, ms = divmod(ms, 3_600_000)
    m, ms = divmod(ms, 60_000)
    s, ms = divmod(ms, 1000)
    return f"{h:02d}:{m:02d}:{s:02d}{sep}{ms:03d}"


def to_srt(captions: list[Caption], style: Style = EPISODE) -> str:
    blocks = []
    for i, caption in enumerate(captions, 1):
        blocks.append(
            f"{i}\n{_stamp(caption.start, ',')} --> {_stamp(caption.end, ',')}\n"
            + "\n".join(caption.lines(style))
        )
    return "\n\n".join(blocks) + "\n"


def to_vtt(captions: list[Caption], style: Style = EPISODE) -> str:
    blocks = ["WEBVTT"]
    for caption in captions:
        blocks.append(
            f"{_stamp(caption.start, '.')} --> {_stamp(caption.end, '.')}\n"
            + "\n".join(caption.lines(style))
        )
    return "\n\n".join(blocks) + "\n"


def parse_srt(text: str) -> list[Caption]:
    out = []
    for block in re.split(r"\n\s*\n", text.strip()):
        lines = block.strip().splitlines()
        if len(lines) < 3:
            continue
        a, b = (part.strip() for part in lines[1].split("-->"))

        def seconds(stamp: str) -> float:
            h, m, rest = stamp.replace(",", ".").split(":")
            return int(h) * 3600 + int(m) * 60 + float(rest)

        out.append(Caption(seconds(a), seconds(b), " ".join(lines[2:])))
    return out
