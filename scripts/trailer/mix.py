#!/usr/bin/env python3
"""Build the trailer soundtrack from ``cut.json``: score, effects, beds, master.

Cue times are written against the picture: ``"at": 12.5`` is absolute, and
``"at": "clash+0.20"`` is 0.2 s after the event named ``clash`` starts
(``"clash@end-0.1"`` counts from its end). The event starts come from the same
cumulative durations ``conform.py`` cuts with, so a re-timed picture carries
its sound with it.

Buses:

* **music** -- edited windows of the game's own score (``assets/audio/music``),
  each with its own fades, gain and optional filter automation;
* **fx** -- one-shots from ``assets/audio`` placed with pan (static or moving),
  distance (air absorption and level), reverb and varispeed, plus synthesized
  trailer elements (``impact``, ``boom``, ``sub_drop``, ``riser``, ``whoosh``,
  ``reverse``, ``drone``) from ``dsp.py``;
* **beds** -- ambience loops, crossfaded and trimmed to their spans;
* **game** -- the arena's own recorded mix for an event, cut in sync with the
  picture.

The music is ducked under the effects, the buses are summed, glued with a slow
compressor, normalised to the target integrated loudness and limited to the
true-peak ceiling. A loudness report is printed and written next to the WAV.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dsp  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
AUDIO = REPO / "assets" / "audio"


def event_times(cut: dict) -> dict[str, tuple[float, float]]:
    times = {}
    t = 0.0
    for event in cut["events"]:
        name = event.get("name")
        if name:
            times[name] = (t, t + float(event["dur"]))
        t += float(event["dur"])
    times["__end__"] = (t, t)
    return times


def when(value, times: dict[str, tuple[float, float]]) -> float:
    if isinstance(value, (int, float)):
        return float(value)
    match = re.fullmatch(r"\s*([A-Za-z0-9_]+)(@end)?\s*([+-]\s*[0-9.]+)?\s*", str(value))
    if not match:
        raise SystemExit(f"cannot read cue time '{value}'")
    name, end, offset = match.groups()
    if name not in times:
        raise SystemExit(f"cue refers to unknown event '{name}'")
    base = times[name][1] if end else times[name][0]
    return base + (float(offset.replace(" ", "")) if offset else 0.0)


def resolve(path: str) -> Path:
    p = Path(path)
    if p.is_absolute() and p.exists():
        return p
    for root in (REPO, AUDIO):
        if (root / p).exists():
            return root / p
    raise SystemExit(f"audio file not found: {path}")


def shape(clip: np.ndarray, cue: dict, times) -> np.ndarray:
    """Apply the per-cue processing chain."""
    if cue.get("speed"):
        clip = dsp.varispeed(clip, float(cue["speed"]))
    if cue.get("src_in") or cue.get("dur"):
        clip = dsp.trim(clip, float(cue.get("src_in", 0.0)),
                        float(cue["dur"]) if cue.get("dur") else None)
    if cue.get("highpass"):
        clip = dsp.highpass(clip, float(cue["highpass"]))
    if cue.get("lowpass"):
        lp = cue["lowpass"]
        clip = dsp.sweep_lowpass(clip, [tuple(p) for p in lp]) if isinstance(lp, list) \
            else dsp.lowpass(clip, float(lp))
    if cue.get("low_shelf"):
        clip = dsp.shelf(clip, 120.0, float(cue["low_shelf"]), "low")
    if cue.get("high_shelf"):
        clip = dsp.shelf(clip, 6000.0, float(cue["high_shelf"]), "high")
    if cue.get("distance"):
        clip = dsp.distance(clip, float(cue["distance"]))
    if "pan" in cue:
        clip = dsp.pan(clip, cue["pan"] if not isinstance(cue["pan"], list)
                       else [tuple(p) for p in cue["pan"]])
    if cue.get("width") is not None:
        clip = dsp.width(clip, float(cue["width"]))
    if cue.get("reverb"):
        clip = dsp.reverb(clip, float(cue["reverb"]), decay=float(cue.get("decay", 2.4)),
                          damping=float(cue.get("damping", 6000)))
    if cue.get("reverse"):
        clip = clip[::-1].copy()
    clip = dsp.fade(clip, float(cue.get("fade_in", 0.0)), float(cue.get("fade_out", 0.0)))
    if cue.get("gain_curve"):
        clip = dsp.automate(clip, [tuple(p) for p in cue["gain_curve"]])
    return clip * dsp.db(float(cue.get("gain", 0.0)))


def synth(cue: dict) -> np.ndarray:
    kind = cue["synth"]
    seed = int(cue.get("seed", 3))
    if kind == "impact":
        return dsp.impact(float(cue.get("length", 4.0)), float(cue.get("weight", 1.0)),
                          float(cue.get("brightness", 0.5)), seed)
    if kind == "boom":
        return dsp.boom(float(cue.get("length", 5.0)), seed)
    if kind == "sub_drop":
        return dsp.sub_drop(float(cue.get("length", 3.5)), float(cue.get("from_hz", 62)),
                            float(cue.get("to_hz", 27)))
    if kind == "riser":
        return dsp.riser(float(cue.get("length", 4.0)), seed, float(cue.get("top_hz", 9000)))
    if kind == "whoosh":
        return dsp.whoosh(float(cue.get("length", 1.2)), seed, float(cue.get("centre", 0.55)))
    if kind == "drone":
        return dsp.drone(float(cue["length"]), float(cue.get("root_hz", 36.7)), seed,
                         float(cue.get("darkness", 900)))
    if kind == "reverse":
        source = dsp.load(resolve(cue["source"])) if cue.get("source") else dsp.impact(3.0, 1.0, 0.8, seed)
        return dsp.reverse_swell(source, float(cue.get("length", 2.0)))
    if kind == "silence":
        return dsp.silence(float(cue.get("length", 1.0)))
    raise SystemExit(f"unknown synth '{kind}'")


def place_cue(bus: np.ndarray, cue: dict, times) -> None:
    clip = synth(cue) if "synth" in cue else dsp.load(resolve(cue["file"]))
    clip = shape(clip, cue, times)
    at = when(cue["at"], times)
    if cue.get("anchor") == "end":
        at -= dsp.seconds(clip.shape[0])
    elif cue.get("anchor_at") is not None:
        at -= float(cue["anchor_at"])
    dsp.place(bus, clip, at)


def place_bed(bus: np.ndarray, bed: dict, times) -> None:
    source = dsp.load(resolve(bed["file"]))
    start = when(bed["at"], times)
    end = when(bed["until"], times) if "until" in bed else start + float(bed["dur"])
    length = end - start
    cross = min(1.5, dsp.seconds(source.shape[0]) * 0.25)
    out = np.zeros((dsp.samples(length) + dsp.samples(cross), 2), np.float32)
    offset = float(bed.get("src_in", 0.0))
    t = 0.0
    while t < length:
        seg = dsp.trim(source, offset, None)
        seg = dsp.fade(seg, cross if t > 0 else 0.0, cross)
        dsp.place(out, seg, t)
        t += dsp.seconds(seg.shape[0]) - cross
        offset = 0.0
    out = out[: dsp.samples(length)]
    out = shape(out, {k: v for k, v in bed.items() if k not in ("src_in", "dur", "speed")}, times)
    dsp.place(bus, out, start)


def place_music(bus: np.ndarray, cue: dict, times) -> None:
    source = dsp.load(resolve(cue["file"]))
    start = when(cue["at"], times)
    if "until" in cue:
        dur = when(cue["until"], times) - start
    else:
        dur = float(cue["dur"])
    clip = dsp.trim(source, float(cue.get("src_in", 0.0)), dur)
    if cue.get("gain_curve"):
        clip = dsp.automate(clip, [(when(t, times) - start if isinstance(t, str) else t, g)
                                   for t, g in cue["gain_curve"]])
    local = {k: v for k, v in cue.items() if k not in ("src_in", "dur", "gain_curve", "speed")}
    if isinstance(local.get("lowpass"), list):
        local["lowpass"] = [(when(t, times) - start if isinstance(t, str) else t, hz)
                            for t, hz in local["lowpass"]]
    clip = shape(clip, local, times)
    dsp.place(bus, clip, start)


def clip_audio(folder: Path, name: str) -> np.ndarray | None:
    """The arena's recorded mix for a clip: its WAV, or the MP4's audio track."""
    wavs = sorted(folder.glob(f"[0-9][0-9]_{name}.mp4.wav"))
    if wavs:
        return dsp.load(wavs[0])
    clips = sorted(folder.glob(f"[0-9][0-9]_{name}.mp4"))
    if not clips:
        return None
    cache = clips[0].with_suffix(".game.wav")
    if not cache.exists() or cache.stat().st_mtime < clips[0].stat().st_mtime:
        result = subprocess.run(["ffmpeg", "-y", "-v", "error", "-i", str(clips[0]), "-vn",
                                 "-ac", "2", "-ar", str(dsp.RATE), str(cache)],
                                capture_output=True)
        if result.returncode != 0:
            return None
    return dsp.load(cache)


def game_audio(bus: np.ndarray, cut: dict, clips: Path, times) -> None:
    """The arena's recorded mix under each event that asks for it."""
    t = 0.0
    for event in cut["events"]:
        gain = event.get("game_audio")
        if gain is not None and "clip" in event:
            folder, _, name = event["clip"].partition("/")
            src = clip_audio(clips / folder, name)
            if src is not None:
                speed = float(event.get("speed", 1.0))
                seg = dsp.trim(src, float(event.get("in", 0.0)), float(event["dur"]) * speed)
                if abs(speed - 1.0) > 1e-3:
                    seg = dsp.varispeed(seg, speed)
                seg = dsp.fade(seg, 0.04, 0.06)
                opts = event.get("game_audio_fx", {})
                seg = shape(seg, opts, times) * dsp.db(float(gain))
                dsp.place(bus, seg, t - float(opts.get("lead", 0.0)))
        t += float(event["dur"])


def master(mixbus: np.ndarray, spec: dict) -> np.ndarray:
    target = float(spec.get("lufs", -14.0))
    ceiling = float(spec.get("ceiling", -1.0))
    glued = dsp.compress(mixbus, threshold_db=float(spec.get("glue_threshold", -16.0)),
                         ratio=1.8, attack=0.03, release=0.35)
    glued = dsp.highpass(glued, 24.0, 2)
    loud = dsp.integrated_lufs(glued)
    glued = glued * dsp.db(target - loud)
    out = dsp.limit(glued, ceiling - 0.3)
    for _ in range(3):
        loud = dsp.integrated_lufs(out)
        if abs(loud - target) < 0.15:
            break
        out = dsp.limit(out * dsp.db(target - loud), ceiling - 0.3)
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--cut", type=Path, required=True)
    parser.add_argument("--clips", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--stems", action="store_true", help="also write bus stems")
    args = parser.parse_args()

    cut = json.loads(args.cut.read_text())
    audio = cut.get("audio", {})
    times = event_times(cut)
    total = times["__end__"][0] + float(audio.get("tail", 0.0))
    n = dsp.samples(total)
    buses = {name: np.zeros((n, 2), np.float32) for name in ("music", "fx", "beds", "game")}

    for cue in audio.get("music", []):
        place_music(buses["music"], cue, times)
    for cue in audio.get("fx", []):
        place_cue(buses["fx"], cue, times)
    for bed in audio.get("beds", []):
        place_bed(buses["beds"], bed, times)
    game_audio(buses["game"], cut, args.clips, times)

    levels = audio.get("bus_gain", {})
    for name in buses:
        buses[name] *= dsp.db(float(levels.get(name, 0.0)))
    duck = audio.get("duck")
    if duck:
        key = buses["fx"] + buses["game"]
        buses["music"] = dsp.duck(buses["music"], key, float(duck.get("depth", -4.0)),
                                  float(duck.get("threshold", -24.0)))
    mixbus = sum(buses.values())
    final = master(mixbus, audio.get("master", {}))
    final = dsp.fade(final, 0.0, float(audio.get("end_fade", 0.02)))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    dsp.save(args.out, final)
    if args.stems:
        for name, bus in buses.items():
            dsp.save(args.out.with_name(f"{args.out.stem}.{name}.wav"), bus)
    report = {
        "integrated_lufs": round(dsp.integrated_lufs(final), 2),
        "true_peak_dbtp": round(dsp.true_peak_db(final), 2),
        "seconds": round(total, 3),
        "short_term_max": round(max(v for _, v in dsp.short_term_lufs(final)), 2),
    }
    args.out.with_suffix(".report.json").write_text(json.dumps(report, indent=1))
    print(f"mix: {args.out} {report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
