"""The narration bus (#1533): script, recordings, processing and room tone.

**Script.** Markdown with one numbered paragraph per recording::

    ## The ground
    [ep07_p05] The plain at Cannae is flat and open. Varro wanted it so.
    A second line joins the same paragraph until a blank line.

    ## Reel
    [ep07_r01] Eighty-six thousand Romans marched onto this field.

Ids are ``ep<NN>_p<NN>`` for episode paragraphs and ``ep<NN>_r<NN>`` for reel
lines. ``{stage directions}`` and ``<!-- notes -->`` are dropped from the
captions and the word count.

**Recordings.** One dry 48 kHz / 24-bit mono WAV per paragraph named after
its id (``ep07_p03.wav``). A retake is ``ep07_p03_t2.wav``; the highest take
wins unless the edit pins one (``"takes": {"p03": 1}``). Scratch reads for the
animatic go in ``<vo>/scratch/`` and are used only when no final take exists.
``ep07_roomtone.wav`` (30 s of the booth with nobody speaking) fills the
gaps between paragraphs; without it a pink-noise floor is synthesised.

**Processing.** Each take is trimmed to its speech (a short pre-roll and a
tail kept), then high-passed, gently equalised, de-essed and compressed, and
gain-matched to the edit's ``mix.vo_lufs`` so every paragraph sits at the same
level. The processed take is cached under ``<work>/vo`` by the file's size,
mtime and the chain, so a new take is processed on the next build and nothing
else is.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path

from . import EditError, media

PARA_RE = re.compile(r"^\[(?P<id>ep\d{2}_[pr]\d{2,3})\]\s*(?P<text>.*)$")
TAKE_RE = re.compile(r"^(?P<id>ep\d{2}_[pr]\d{2,3})(?:_(?:t|take)(?P<n>\d+))?\.(?:wav|flac)$")
PRE_ROLL = 0.12
TAIL = 0.30

CHAIN = (
    "highpass=f=80:poles=2,"
    "equalizer=f=220:t=q:w=1.0:g=-2.5,"
    "equalizer=f=3200:t=q:w=1.4:g=1.5,"
    "deesser=i=0.35:m=0.5:f=0.5:s=o,"
    "acompressor=threshold=-24dB:ratio=2.5:attack=6:release=90:knee=4:makeup=1"
)


@dataclass
class Paragraph:
    id: str
    text: str
    heading: str = ""

    @property
    def short(self) -> str:
        return self.id.split("_", 1)[1]

    @property
    def clean(self) -> str:
        return clean_text(self.text)

    @property
    def words(self) -> int:
        return len(self.clean.split())


@dataclass
class Take:
    """What the timeline needs to know about one paragraph's recording."""

    id: str
    duration: float
    segments: list[tuple[float, float]] = field(default_factory=list)
    source: Path | None = None
    processed: Path | None = None
    take: int = 0
    scratch: bool = False
    estimated: bool = False
    gain_db: float = 0.0


def clean_text(text: str) -> str:
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S)
    text = re.sub(r"\{[^}]*\}", "", text)
    return re.sub(r"\s+", " ", text).strip()


def parse_script(path: Path) -> list[Paragraph]:
    text = re.sub(r"<!--.*?-->", "", path.read_text(), flags=re.S)
    paragraphs: list[Paragraph] = []
    heading = ""
    current: Paragraph | None = None
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("#"):
            heading = line.lstrip("#").strip()
            current = None
            continue
        if not line:
            current = None
            continue
        match = PARA_RE.match(line)
        if match:
            current = Paragraph(match["id"], match["text"].strip(), heading)
            paragraphs.append(current)
        elif current is not None:
            current.text += " " + line
    seen: set[str] = set()
    for paragraph in paragraphs:
        if paragraph.id in seen:
            raise EditError(f"{path.name}: paragraph id {paragraph.id} appears twice")
        seen.add(paragraph.id)
    return paragraphs


def script_report(paragraphs: list[Paragraph], wpm: float) -> dict:
    episode = [p for p in paragraphs if "_p" in p.id]
    words = sum(p.words for p in episode)
    return {
        "paragraphs": len(episode),
        "reel_lines": len(paragraphs) - len(episode),
        "words": words,
        "estimated_seconds": round(words / wpm * 60.0, 1),
    }


def recording_name(paragraph_id: str, take: int = 1) -> str:
    return f"{paragraph_id}.wav" if take <= 1 else f"{paragraph_id}_t{take}.wav"


def find_takes(vo_dir: Path) -> dict[str, list[tuple[int, Path, bool]]]:
    """Every take on disk: ``{id: [(take, path, scratch), ...]}``, finals first."""
    found: dict[str, list[tuple[int, Path, bool]]] = {}
    for folder, scratch in ((vo_dir, False), (vo_dir / "scratch", True)):
        if not folder.is_dir():
            continue
        for path in sorted(folder.iterdir()):
            match = TAKE_RE.match(path.name)
            if match:
                take = int(match["n"] or 1)
                found.setdefault(match["id"], []).append((take, path, scratch))
    for takes in found.values():
        takes.sort(key=lambda t: (t[2], -t[0]))
    return found


def choose_take(
    paragraph_id: str, takes: dict[str, list[tuple[int, Path, bool]]], pinned: int | None = None
) -> tuple[int, Path, bool] | None:
    options = takes.get(paragraph_id, [])
    if pinned is not None:
        for option in options:
            if option[0] == pinned and not option[2]:
                return option
        raise EditError(f"{paragraph_id}: pinned take {pinned} is not in the VO folder")
    return options[0] if options else None


def estimate(paragraph: Paragraph, wpm: float) -> Take:
    """A placeholder length for a paragraph nobody has read yet (animatic timing)."""
    seconds = max(1.5, paragraph.words / wpm * 60.0 + 0.4)
    return Take(paragraph.id, round(seconds, 3), [(0.0, round(seconds, 3))], estimated=True)


def _trim_window(segments: list[tuple[float, float]], total: float) -> tuple[float, float]:
    if not segments:
        return 0.0, total
    head = max(0.0, segments[0][0] - PRE_ROLL)
    tail = min(total, segments[-1][1] + TAIL)
    return head, tail


def process_take(
    paragraph_id: str,
    source: Path,
    work: Path,
    target_lufs: float,
    silence_db: float,
    take: int = 1,
    scratch: bool = False,
) -> Take:
    """Trim, clean and level one take; cached by file identity and chain."""
    out_dir = work / "vo"
    out_dir.mkdir(parents=True, exist_ok=True)
    key = media.file_key(source, CHAIN, target_lufs, silence_db, PRE_ROLL, TAIL)
    processed = out_dir / f"{paragraph_id}.{key}.wav"
    meta = processed.with_suffix(".json")
    if processed.exists() and meta.exists():
        info = json.loads(meta.read_text())
        return Take(
            paragraph_id,
            info["duration"],
            [tuple(s) for s in info["segments"]],
            source,
            processed,
            take,
            scratch,
            gain_db=info.get("gain_db", 0.0),
        )
    total = media.duration(source)
    raw_segments = media.speech_segments(source, silence_db)
    head, tail = _trim_window(raw_segments, total)
    stage = out_dir / f"{paragraph_id}.{key}.stage.wav"
    media.ffmpeg(
        [
            "-i",
            str(source),
            "-af",
            f"atrim=start={head:.4f}:end={tail:.4f},asetpts=PTS-STARTPTS,"
            f"aformat=sample_rates={media.RATE}:channel_layouts=mono,{CHAIN},"
            "afade=t=in:d=0.03",
            "-c:a",
            "pcm_s24le",
            str(stage),
        ],
        f"VO chain for {paragraph_id}",
    )
    measured = media.loudness(stage)["integrated_lufs"]
    gain = 0.0 if measured <= -69 else target_lufs - measured
    length = tail - head
    media.ffmpeg(
        [
            "-i",
            str(stage),
            "-af",
            f"volume={gain:.3f}dB,afade=t=out:st={max(0.0, length - 0.08):.4f}:d=0.08",
            "-c:a",
            "pcm_s24le",
            str(processed),
        ],
        f"VO level for {paragraph_id}",
    )
    stage.unlink(missing_ok=True)
    segments = [
        (round(max(0.0, a - head), 4), round(min(length, b - head), 4))
        for a, b in raw_segments
        if b > head and a < tail
    ] or [(0.0, round(length, 4))]
    info = {"duration": round(length, 4), "segments": segments, "gain_db": round(gain, 2)}
    meta.write_text(json.dumps(info))
    for stale in out_dir.glob(f"{paragraph_id}.*.wav"):
        if stale != processed and not stale.name.endswith(".stage.wav"):
            stale.unlink(missing_ok=True)
            stale.with_suffix(".json").unlink(missing_ok=True)
    return Take(paragraph_id, info["duration"], segments, source, processed, take, scratch, gain_db=gain)


def gather_takes(
    paragraphs: list[Paragraph],
    vo_dir: Path,
    work: Path | None,
    mix: dict,
    pinned: dict | None = None,
    analyse: bool = True,
) -> dict[str, Take]:
    """The take for every paragraph: processed when recorded, estimated when not.

    ``analyse=False`` skips ffmpeg entirely and times every paragraph from
    its word count (used to plan before any recording exists).
    """
    on_disk = find_takes(vo_dir) if analyse else {}
    pinned = pinned or {}
    takes: dict[str, Take] = {}
    for paragraph in paragraphs:
        pin = pinned.get(paragraph.short, pinned.get(paragraph.id))
        chosen = choose_take(paragraph.id, on_disk, pin) if analyse else None
        if chosen is None or work is None:
            takes[paragraph.id] = estimate(paragraph, float(mix["words_per_minute"]))
            continue
        number, path, scratch = chosen
        takes[paragraph.id] = process_take(
            paragraph.id,
            path,
            work,
            float(mix["vo_lufs"]),
            float(mix["vo_silence_db"]),
            number,
            scratch,
        )
    return takes


def room_tone(vo_dir: Path, episode_id: str) -> Path | None:
    for name in (f"{episode_id}_roomtone.wav", "roomtone.wav"):
        if (vo_dir / name).exists():
            return vo_dir / name
    return None


SYNTH_MARKER = ".synthetic"


def sentence_spans(text: str) -> list[str]:
    from .captions import split_sentences

    return split_sentences(clean_text(text))


def synthesise(
    paragraphs: list[Paragraph],
    out_dir: Path,
    words_per_second: float = 2.6,
    take: int = 1,
    only: list[str] | None = None,
    stretch: float = 1.0,
) -> list[Path]:
    """Stand-in recordings for testing and animatics: never ship these.

    Each sentence becomes a burst of voiced, syllable-modulated tone with a
    little breath noise, sentences are separated by 0.45 s pauses, and every
    file carries 0.6 s of room noise at both ends so trimming is exercised.
    ``espeak-ng`` is used instead when it is installed. A ``.synthetic``
    marker in the folder makes delivery refuse the narration.
    """
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / SYNTH_MARKER).write_text("synthetic narration; not for delivery\n")
    written = []
    for index, paragraph in enumerate(paragraphs):
        if only and paragraph.id not in only and paragraph.short not in only:
            continue
        target = out_dir / recording_name(paragraph.id, take)
        if media.have("espeak-ng"):
            raw = out_dir / f"{paragraph.id}.espeak.wav"
            media.run(
                ["espeak-ng", "-s", str(int(words_per_second * 60 / stretch)), "-w", str(raw), paragraph.clean],
                "espeak-ng",
            )
            media.ffmpeg(
                [
                    "-i",
                    str(raw),
                    "-af",
                    f"adelay=600,apad=pad_dur=0.6,aformat=sample_rates={media.RATE}:channel_layouts=mono",
                    "-c:a",
                    "pcm_s24le",
                    str(target),
                ]
            )
            raw.unlink(missing_ok=True)
            written.append(target)
            continue
        gates = []
        t = 0.6
        for sentence in sentence_spans(paragraph.text):
            length = max(0.6, len(sentence.split()) / words_per_second * stretch)
            gates.append((t, t + length))
            t += length + 0.45
        total = t - 0.45 + 0.6
        gate = "+".join(f"between(t,{a:.3f},{b:.3f})" for a, b in gates) or "0"
        pitch = 110 + 9 * (index % 5)
        expr = (
            f"({gate})*(0.20*sin(2*PI*{pitch}*t)+0.10*sin(2*PI*{pitch * 2.02:.1f}*t)"
            f"+0.05*sin(2*PI*{pitch * 3.1:.1f}*t))*(0.55+0.45*sin(2*PI*4.3*t))"
        )
        media.ffmpeg(
            [
                "-f",
                "lavfi",
                "-i",
                f"aevalsrc='{expr}':s={media.RATE}:d={total:.3f}",
                "-f",
                "lavfi",
                "-i",
                f"anoisesrc=color=pink:amplitude=0.0015:d={total:.3f}:r={media.RATE}:seed={7 + index}",
                "-filter_complex",
                "[0:a][1:a]amix=inputs=2:normalize=0,aformat=channel_layouts=mono",
                "-c:a",
                "pcm_s24le",
                str(target),
            ],
            f"synthetic VO {paragraph.id}",
        )
        written.append(target)
    return written


def is_synthetic(vo_dir: Path) -> bool:
    return (vo_dir / SYNTH_MARKER).exists()
