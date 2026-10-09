"""Episode and reel soundtrack: VO bus, ducked music, beds, game audio, SFX, master.

Buses (all 48 kHz stereo):

* **vo** -- every processed take at its resolved time, plus room tone filling
  the gaps between paragraphs so cuts between takes never fall to digital
  silence;
* **music** -- the edit's cues, each trimmed from ``src_in``, faded and placed;
  a cue with ``stems`` lays each stem separately so ``under_vo`` can keep only
  the bed stems (strings, drones) under words and lift the rest between lines;
* **beds** -- ambience loops; **game** -- the arena's recorded mix under the
  shots that ask for it (``game_audio_db``); **sfx** -- one-shots, never ducked.

Ducking is keyed by the narration itself: the speech segments of every take
(not just its outline) are merged across pauses shorter than ``merge_gap`` and
turned into a gain envelope that starts falling ``attack`` seconds *before*
the first word (the edit knows the future), holds ``hold`` seconds after the
last and recovers over ``release``. Music sits ``duck_db`` (-9 to -12 dB)
under words, ambience ``beds_duck_db`` and game audio ``game_duck_db``. The
envelope is a control-rate WAV multiplied in with ``amultiply``, so the depth is
exact and repeatable rather than depending on a compressor's threshold.

The sum is gain-matched to ``mix.lufs`` (-14 LUFS integrated) and peak
limited 0.6 dB under ``mix.true_peak`` because the AAC encode adds about that
much; the report records integrated loudness, true peak, the VO's short-term
consistency and how far the narration sits above the music under it.
"""

from __future__ import annotations

import array
import hashlib
import json
import statistics
from dataclasses import dataclass, field
from pathlib import Path

from . import EditError, media
from .timeline import Span, Timeline, VoSpan

CONTROL_RATE = 1000
BANNED_MUSIC = ("combat_last_defensive_wall",)


@dataclass
class GameClip:
    path: Path
    src_in: float
    start: float
    dur: float
    gain_db: float


@dataclass
class MixPlan:
    name: str
    total: float
    vo: list[VoSpan]
    music: list[Span]
    beds: list[Span] = field(default_factory=list)
    sfx: list[Span] = field(default_factory=list)
    game: list[GameClip] = field(default_factory=list)
    params: dict = field(default_factory=dict)
    room_tone: Path | None = None
    resolve: object = None


def db(value: float) -> float:
    return 10 ** (value / 20.0)


def merge_spans(
    spans: list[tuple[float, float]], gap: float
) -> list[tuple[float, float]]:
    merged: list[tuple[float, float]] = []
    for a, b in sorted(spans):
        if merged and a - merged[-1][1] <= gap:
            merged[-1] = (merged[-1][0], max(merged[-1][1], b))
        else:
            merged.append((a, b))
    return merged


def duck_envelope(
    speech: list[tuple[float, float]],
    total: float,
    depth_db: float,
    attack: float,
    hold: float,
    release: float,
    merge_gap: float,
    rate: int = CONTROL_RATE,
) -> list[float]:
    """Gain per control sample: 1.0 in the clear, ``depth_db`` under speech."""
    n = int(total * rate) + 2
    amount = [0.0] * n
    for a, b in merge_spans(speech, merge_gap):
        down_start, down_end = a - attack, a
        up_start, up_end = b + hold, b + hold + release
        i0 = max(0, int(down_start * rate))
        i1 = min(n, int(up_end * rate) + 1)
        for i in range(i0, i1):
            t = i / rate
            if t < down_end:
                v = (t - down_start) / attack if attack > 0 else 1.0
            elif t <= up_start:
                v = 1.0
            else:
                v = 1.0 - (t - up_start) / release if release > 0 else 0.0
            v = max(0.0, min(1.0, v))
            v = v * v * (3 - 2 * v)
            if v > amount[i]:
                amount[i] = v
    floor = db(depth_db)
    return [1.0 - (1.0 - floor) * v for v in amount]


def fill_envelope(
    spans: list[tuple[float, float]],
    total: float,
    ramp: float = 0.05,
    rate: int = CONTROL_RATE,
) -> list[float]:
    """1.0 between recordings, 0.0 under them: where room tone must fill."""
    n = int(total * rate) + 2
    gain = [1.0] * n
    for a, b in spans:
        for i in range(
            max(0, int((a - ramp) * rate)), min(n, int((b + ramp) * rate) + 1)
        ):
            t = i / rate
            inside = min((t - (a - ramp)) / ramp, ((b + ramp) - t) / ramp, 1.0)
            gain[i] = min(gain[i], 1.0 - max(0.0, inside))
    return gain


def check_music_file(path: Path) -> None:
    for banned in BANNED_MUSIC:
        if banned in path.name:
            raise EditError(
                f"{path.name} is banned from every documentary and marketing cut "
                "(it reads as defeat); pick a driving cue"
            )


class Graph:
    def __init__(self) -> None:
        self.inputs: list[str] = []
        self.filters: list[str] = []
        self.count = 0

    def input(self, *args: str) -> int:
        self.inputs.extend(args)
        index = self.count
        self.count += 1
        return index

    def add(self, text: str) -> None:
        self.filters.append(text)


def _envelope_input(
    graph: Graph, work: Path, values: list[float], total: float, tag: str
) -> str:
    digest = hashlib.sha1(array.array("f", values).tobytes()).hexdigest()[:12]
    path = work / f"env_{tag}_{digest}.wav"
    if not path.exists():
        media.write_control_wav(path, values, CONTROL_RATE)
    index = graph.input("-i", str(path))
    label = f"env_{tag}"
    graph.add(
        f"[{index}:a]aresample={media.RATE}:filter_type=kaiser,"
        f"aformat=sample_fmts=fltp:channel_layouts=stereo,apad,atrim=0:{total:.4f}[{label}]"
    )
    return label


def _bus(graph: Graph, labels: list[str], name: str, total: float) -> str:
    if not labels:
        graph.add(f"anullsrc=r={media.RATE}:cl=stereo,atrim=0:{total:.4f}[{name}]")
        return name
    joined = "".join(f"[{label}]" for label in labels)
    graph.add(
        f"{joined}amix=inputs={len(labels)}:normalize=0:duration=longest,"
        f"aformat=sample_fmts=fltp:channel_layouts=stereo,apad,atrim=0:{total:.4f}[{name}]"
    )
    return name


def _place(start: float) -> str:
    ms = max(0, int(round(start * 1000)))
    return f"adelay={ms}:all=1"


def _cue_chain(cue: dict, dur: float, start: float, gain_db: float) -> str:
    fi = float(cue.get("fade_in", 0.05))
    fo = float(cue.get("fade_out", 0.4))
    chain = [
        f"atrim=start={float(cue.get('src_in', 0.0)):.4f}:duration={dur:.4f}",
        "asetpts=PTS-STARTPTS",
        f"aformat=sample_rates={media.RATE}:sample_fmts=fltp:channel_layouts=stereo",
    ]
    if cue.get("highpass"):
        chain.append(f"highpass=f={float(cue['highpass'])}")
    if cue.get("lowpass"):
        chain.append(f"lowpass=f={float(cue['lowpass'])}")
    if fi > 0:
        chain.append(f"afade=t=in:d={fi:.3f}")
    if fo > 0:
        chain.append(f"afade=t=out:st={max(0.0, dur - fo):.4f}:d={fo:.3f}")
    chain.append(f"volume={gain_db:.2f}dB")
    chain.append(_place(start))
    return ",".join(chain)


def build(plan: MixPlan, work: Path) -> dict:
    """Render the mix and its bus stems; returns the loudness report."""
    p = plan.params
    work.mkdir(parents=True, exist_ok=True)
    total = plan.total
    graph = Graph()
    resolve = plan.resolve or (lambda value: Path(value))

    vo_labels: list[str] = []
    for item in plan.vo:
        if item.take is None or item.take.processed is None:
            continue
        index = graph.input("-i", str(item.take.processed))
        label = f"vo{index}"
        graph.add(
            f"[{index}:a]aformat=sample_rates={media.RATE}:sample_fmts=fltp:channel_layouts=stereo,"
            f"{_place(item.start)}[{label}]"
        )
        vo_labels.append(label)
    recorded = [(v.start, v.end) for v in plan.vo if v.take and v.take.processed]
    if recorded:
        gains = [v.take.gain_db for v in plan.vo if v.take and v.take.processed]
        if plan.room_tone is not None:
            index = graph.input("-stream_loop", "-1", "-i", str(plan.room_tone))
            graph.add(
                f"[{index}:a]atrim=0:{total:.4f},asetpts=PTS-STARTPTS,"
                f"aformat=sample_rates={media.RATE}:sample_fmts=fltp:channel_layouts=stereo,"
                f"highpass=f=80,volume={statistics.median(gains):.2f}dB[rt_raw]"
            )
        else:
            index = graph.input(
                "-f",
                "lavfi",
                "-i",
                f"anoisesrc=color=pink:amplitude={db(float(p['room_tone_db'])):.6f}:"
                f"r={media.RATE}:d={total:.4f}:seed=11",
            )
            graph.add(
                f"[{index}:a]aformat=sample_rates={media.RATE}:sample_fmts=fltp:channel_layouts=stereo,"
                "highpass=f=80,lowpass=f=9000[rt_raw]"
            )
        env = _envelope_input(
            graph, work, fill_envelope(recorded, total), total, "fill"
        )
        graph.add(f"[rt_raw][{env}]amultiply[roomtone]")
        vo_labels.append("roomtone")
    vo_bus = _bus(graph, vo_labels, "vo", total)

    speech = [span for item in plan.vo for span in item.speech()]
    envelopes: dict[float, str] = {}

    def envelope_for(depth: float) -> str:
        depth = round(depth, 2)
        if depth not in envelopes:
            values = duck_envelope(
                speech,
                total,
                depth,
                float(p["attack"]),
                float(p["hold"]),
                float(p["release"]),
                float(p["merge_gap"]),
            )
            tag = f"d{abs(int(depth * 10))}"
            envelopes[depth] = _envelope_input(graph, work, values, total, tag)
        return envelopes[depth]

    music_groups: dict[float, list[str]] = {}
    for span in plan.music:
        cue = span.data
        dur = max(0.05, span.end - span.start)
        default_depth = float(cue.get("duck_db", p["duck_db"]))
        stems = cue.get("stems")
        sources = (
            [(name, resolve(path)) for name, path in stems.items()]
            if stems
            else [("full", resolve(cue["file"]))]
        )
        allowed = set(cue.get("under_vo", [])) if stems else None
        for stem_name, path in sources:
            check_music_file(path)
            loop = ["-stream_loop", "-1"] if cue.get("loop") else []
            index = graph.input(*loop, "-i", str(path))
            label = f"mu{index}"
            gain = float(cue.get("gain_db", 0.0)) + float(
                cue.get("stem_gain_db", {}).get(stem_name, 0.0) if stems else 0.0
            )
            graph.add(f"[{index}:a]{_cue_chain(cue, dur, span.start, gain)}[{label}]")
            depth = default_depth if allowed is None or stem_name in allowed else -40.0
            music_groups.setdefault(depth, []).append(label)
    music_parts = []
    for depth, labels in sorted(music_groups.items()):
        raw = _bus(graph, labels, f"music_raw{len(music_parts)}", total)
        env = envelope_for(depth)
        out = f"music_d{len(music_parts)}"
        graph.add(f"[{raw}][{env}]amultiply[{out}]")
        music_parts.append(out)
    music_bus = _bus(graph, music_parts, "music", total)

    bed_labels = []
    for span in plan.beds:
        bed = span.data
        path = resolve(bed["file"])
        index = graph.input("-stream_loop", "-1", "-i", str(path))
        label = f"bed{index}"
        cue = {
            "fade_in": bed.get("fade_in", 2.0),
            "fade_out": bed.get("fade_out", 2.0),
            **bed,
        }
        graph.add(
            f"[{index}:a]{_cue_chain(cue, max(0.05, span.end - span.start), span.start, float(bed.get('gain_db', 0.0)))}[{label}]"
        )
        bed_labels.append(label)
    beds_raw = _bus(graph, bed_labels, "beds_raw", total)
    graph.add(f"[{beds_raw}][{envelope_for(float(p['beds_duck_db']))}]amultiply[beds]")

    game_labels = []
    for clip in plan.game:
        index = graph.input(
            "-ss", f"{clip.src_in:.4f}", "-t", f"{clip.dur:.4f}", "-i", str(clip.path)
        )
        label = f"game{index}"
        graph.add(
            f"[{index}:a]aformat=sample_rates={media.RATE}:sample_fmts=fltp:channel_layouts=stereo,"
            f"afade=t=in:d=0.04,afade=t=out:st={max(0.0, clip.dur - 0.06):.4f}:d=0.06,"
            f"volume={clip.gain_db:.2f}dB,{_place(clip.start)}[{label}]"
        )
        game_labels.append(label)
    game_raw = _bus(graph, game_labels, "game_raw", total)
    graph.add(f"[{game_raw}][{envelope_for(float(p['game_duck_db']))}]amultiply[game]")

    sfx_labels = []
    for span in plan.sfx:
        cue = span.data
        index = graph.input("-i", str(resolve(cue["file"])))
        label = f"sfx{index}"
        graph.add(
            f"[{index}:a]aformat=sample_rates={media.RATE}:sample_fmts=fltp:channel_layouts=stereo,"
            f"volume={float(cue.get('gain_db', 0.0)):.2f}dB,{_place(span.start)}[{label}]"
        )
        sfx_labels.append(label)
    sfx_bus = _bus(graph, sfx_labels, "sfx", total)

    names = ["vo", "music", "beds", "game", "sfx"]
    graph.add(f"[{vo_bus}]asplit=2[vo_a][vo_out]")
    graph.add(f"[{music_bus}]asplit=2[music_a][music_out]")
    graph.add("[beds]asplit=2[beds_a][beds_out]")
    graph.add("[game]asplit=2[game_a][game_out]")
    graph.add(f"[{sfx_bus}]asplit=2[sfx_a][sfx_out]")
    graph.add(
        "[vo_a][music_a][beds_a][game_a][sfx_a]amix=inputs=5:normalize=0:duration=longest,"
        f"atrim=0:{total:.4f}[premix]"
    )
    script = work / f"{plan.name}.graph.txt"
    script.write_text(";\n".join(graph.filters))
    stems = {name: work / f"{plan.name}.{name}.wav" for name in names}
    premix = work / f"{plan.name}.premix.wav"
    outputs: list[str] = []
    for name in names:
        outputs += ["-map", f"[{name}_out]", "-c:a", "pcm_f32le", str(stems[name])]
    outputs += ["-map", "[premix]", "-c:a", "pcm_f32le", str(premix)]
    media.ffmpeg(
        [*graph.inputs, "-filter_complex_script", str(script), *outputs],
        f"{plan.name} mix",
    )

    master_path = work / f"{plan.name}.master.wav"
    report = master(premix, master_path, float(p["lufs"]), float(p["true_peak"]))
    report.update(vo_report(plan, stems, speech))
    report["stems"] = {name: str(path) for name, path in stems.items()}
    report["master"] = str(master_path)
    report["duck_db"] = float(p["duck_db"])
    (work / f"{plan.name}.mix.json").write_text(json.dumps(report, indent=1))
    return report


def master(premix: Path, out: Path, lufs: float, true_peak: float) -> dict:
    """Gain to the target integrated loudness, then limit under the true-peak ceiling."""
    ceiling = true_peak - 0.6
    measured = media.loudness(premix)
    if measured["integrated_lufs"] <= -69:
        raise EditError("the mix is silent; nothing to master")
    gain = lufs - measured["integrated_lufs"]
    limit = db(ceiling - 0.4)
    result = {}
    for _ in range(4):
        media.ffmpeg(
            [
                "-i",
                str(premix),
                "-af",
                f"volume={gain:.3f}dB,alimiter=limit={limit:.5f}:attack=4:release=60:level=disabled,"
                f"aformat=sample_rates={media.RATE}:channel_layouts=stereo",
                "-c:a",
                "pcm_s24le",
                str(out),
            ],
            "master",
        )
        result = media.loudness(out)
        off = lufs - result["integrated_lufs"]
        if result["true_peak_dbtp"] > ceiling:
            limit *= db(ceiling - result["true_peak_dbtp"] - 0.1)
            continue
        if abs(off) <= 0.2:
            break
        gain += off
    return {
        "integrated_lufs": round(result["integrated_lufs"], 2),
        "true_peak_dbtp": round(result["true_peak_dbtp"], 2),
        "lra_lu": round(result["lra_lu"], 2),
        "target_lufs": lufs,
        "ceiling_dbtp": round(ceiling, 2),
        "premix_lufs": round(measured["integrated_lufs"], 2),
    }


def _during(
    series: list[tuple[float, float]],
    spans: list[tuple[float, float]],
    skip: float = 0.0,
) -> list[float]:
    values = []
    for t, value in series:
        if value <= -70:
            continue
        for a, b in spans:
            if a + skip <= t <= b:
                values.append(value)
                break
    return values


def _percentile(values: list[float], q: float) -> float:
    ordered = sorted(values)
    if not ordered:
        return 0.0
    k = (len(ordered) - 1) * q
    lo, hi = int(k), min(len(ordered) - 1, int(k) + 1)
    return ordered[lo] + (ordered[hi] - ordered[lo]) * (k - lo)


def vo_report(
    plan: MixPlan, stems: dict[str, Path], speech: list[tuple[float, float]]
) -> dict:
    """Short-term consistency of the narration and its margin over the music."""
    recorded = [v for v in plan.vo if v.take and v.take.processed]
    if not recorded:
        return {"vo": {"recorded": 0}}
    vo_series = media.short_term(stems["vo"])
    spans = merge_spans(speech, float(plan.params["merge_gap"]))
    vo_values = _during(vo_series, spans, skip=1.5)
    music_values = _during(media.short_term(stems["music"]), spans, skip=1.5)
    paragraph_medians = {}
    for item in recorded:
        values = _during(vo_series, item.speech(), skip=1.0)
        if values:
            paragraph_medians[item.id] = round(statistics.median(values), 2)
    overall = statistics.median(vo_values) if vo_values else -70.0
    spread = (
        _percentile(vo_values, 0.9) - _percentile(vo_values, 0.1) if vo_values else 0.0
    )
    outliers = {k: v for k, v in paragraph_medians.items() if abs(v - overall) > 2.0}
    margin = overall - statistics.median(music_values) if music_values else None
    warnings = []
    if spread > 6.0:
        warnings.append(f"VO short-term spread {spread:.1f} LU (p10-p90) is over 6 LU")
    if outliers:
        warnings.append(
            f"paragraphs off the VO median by more than 2 LU: {sorted(outliers)}"
        )
    if margin is not None and margin < 8.0:
        warnings.append(
            f"music sits only {margin:.1f} LU under the narration (want >= 8)"
        )
    return {
        "vo": {
            "recorded": len(recorded),
            "estimated": len(plan.vo) - len(recorded),
            "short_term_median_lufs": round(overall, 2),
            "short_term_spread_lu": round(spread, 2),
            "paragraph_medians": paragraph_medians,
            "vo_over_music_lu": round(margin, 2) if margin is not None else None,
        },
        "warnings": warnings,
    }


def episode_plan(episode, timeline: Timeline) -> MixPlan:
    """Mix plan for an episode: its VO, cues, beds, SFX and shots' game audio."""
    from .narration import room_tone

    game = []
    for shot in timeline.shots:
        gain = shot.data.get("game_audio_db")
        if gain is None or not shot.data.get("clip"):
            continue
        path = episode.find_clip(shot.data["clip"])
        if not media.probe(path)["has_audio"]:
            continue
        game.append(GameClip(path, shot.in_point, shot.start, shot.dur, float(gain)))
    return MixPlan(
        name=episode.id,
        total=timeline.duration,
        vo=timeline.vo,
        music=timeline.music,
        beds=timeline.beds,
        sfx=timeline.sfx,
        game=game,
        params=episode.mix,
        room_tone=room_tone(episode.vo, episode.id),
        resolve=episode.resolve,
    )
