"""The vertical reel (#1536): hook, three beats, consequence, series tag, < 60 s.

The reel is built from the same sources as the episode: the script's reel
lines (``ep07_r01``...) recorded and processed exactly like the episode's
paragraphs, vertical takes of the same scenario passes (the capture specs'
vertical variants, read from ``paths.vertical_clips``), the series graphics and
the same mix chain. It is described in the edit's ``reel`` block::

    "reel": {"music": {"file": ..., "src_in": 2.0},
             "parts": [{"id": "hook", "role": "hook", "clip": "ep07_vertical/ring",
                        "vo": "r01", "text": "86,000 Romans marched onto this field."},
                       {"id": "beat1", "role": "beat", "clip": ..., "vo": "r02", "pad": 0.7},
                       ...,
                       {"id": "tag", "role": "tag", "clip": ..., "dur": 4.0}]}

A part lasts its line plus ``pad`` (or ``dur``). Frame one is the hook: its
claim is on screen at full opacity from the first frame, over lit footage.
Every voiced part after it gets burned-in captions; the tag card says "full
battle on the channel" and carries the Steam link. The cut is refused at 60 s
or longer, or with the banned track in its music.
"""

from __future__ import annotations

import json
import re
import subprocess
from pathlib import Path

from . import EditError, captions, conform, delivery, graphics, media, mix, style
from .narration import Paragraph, Take
from .rules import check_music_choice
from .timeline import Span, VoSpan

ROLES = ("hook", "beat", "consequence", "tag")
MAX_SECONDS = 60.0
VO_OFFSET = 0.3


def plan(episode, paragraphs: list[Paragraph], takes: dict[str, Take]) -> dict:
    """Resolve the reel's parts, lines, captions and cues to times."""
    spec = episode.data.get("reel")
    if not spec:
        raise EditError(f"{episode.id} has no 'reel' block")
    parts_spec = spec.get("parts", [])
    roles = [p.get("role") for p in parts_spec]
    if (
        not roles
        or roles[0] != "hook"
        or roles[-1] != "tag"
        or roles.count("beat") != 3
        or "consequence" not in roles
    ):
        raise EditError("a reel is hook, three beats, consequence, tag (in that order)")
    texts = {p.id: p for p in paragraphs}
    t = 0.0
    parts, vo, caps = [], [], []
    for part in parts_spec:
        start = t
        line = None
        if part.get("vo"):
            pid = (
                part["vo"]
                if part["vo"].startswith(episode.id)
                else f"{episode.id}_{part['vo']}"
            )
            if pid not in texts:
                raise EditError(f"reel part {part['id']} names unknown line {pid}")
            take = takes[pid]
            offset = float(part.get("vo_offset", VO_OFFSET))
            line = VoSpan(
                pid,
                start + offset,
                start + offset + take.duration,
                part,
                texts[pid].clean,
                take,
            )
            vo.append(line)
            dur = float(
                part.get("dur", offset + take.duration + float(part.get("pad", 0.6)))
            )
            if part["role"] != "hook":
                caps += captions.paragraph_captions(
                    pid,
                    line.text,
                    line.start,
                    take.segments,
                    take.duration,
                    captions.REEL,
                )
        else:
            dur = float(part.get("dur", 3.0))
        parts.append(Span(part["id"], start, start + dur, part))
        t += dur
    total = t
    if total >= MAX_SECONDS:
        raise EditError(
            f"the reel runs {total:.1f}s; it must stay under {MAX_SECONDS:.0f}s"
        )
    music = []
    cue = spec.get("music")
    if cue:
        music.append(
            Span("reel_music", 0.0, total, {"role": "bed", "fade_in": 0.0, **cue})
        )
    sfx = []
    for effect in spec.get("sfx", []):
        sfx.append(
            Span(
                effect.get("id", "sfx"),
                resolve_part_time(effect["at"], parts),
                0.0,
                effect,
            )
        )
    return {
        "total": total,
        "parts": parts,
        "vo": vo,
        "captions": captions.finalise(caps, captions.REEL),
        "music": music,
        "sfx": sfx,
    }


def resolve_part_time(expr, parts: list[Span]) -> float:
    if isinstance(expr, (int, float)):
        return float(expr)
    match = re.fullmatch(
        r"\s*part:([A-Za-z0-9_\-]+)(@end)?\s*([+-]\s*[0-9.]+)?\s*", str(expr)
    )
    if not match:
        raise EditError(f"reel time '{expr}' must be seconds or part:<id>[@end][+-s]")
    for part in parts:
        if part.id == match[1]:
            base = part.end if match[2] else part.start
            return base + (float(match[3].replace(" ", "")) if match[3] else 0.0)
    raise EditError(f"reel time '{expr}' names unknown part")


def check(episode, reel: dict) -> list:
    found = []
    for span in reel["music"]:
        found += check_music_choice([str(span.data.get("file", ""))], marketing=True)
    return found


def _elements(
    episode, reel: dict
) -> list[tuple[str, str, float, float, dict, float, float]]:
    """``(id, kind, start, end, data, fade_in_window, fade_out_window)`` for the overlay track."""
    data = episode.data
    elements = []
    for part in reel["parts"]:
        if part.data["role"] == "hook" and part.data.get("text"):
            elements.append(
                (
                    "hook",
                    "reel_hook",
                    0.0,
                    part.end,
                    {"text": part.data["text"], "kicker": part.data.get("kicker", "")},
                    0.0,
                    0.35,
                )
            )
        if part.data["role"] == "tag":
            fields = graphics.end_card_fields(part.data.get("end_card", "steam_demo"))
            tag = {
                "series": style.SERIES_TITLE,
                "episode_line": f"EPISODE {style.roman(int(data['number']))}  ·  {data['title'].upper()}",
                "subtitle": fields["subtitle"],
                "end_card_destination": fields["end_card_destination"],
            }
            elements.append(("tag", "reel_tag", part.start, part.end, tag, 1.2, 0.0))
    for i, cap in enumerate(reel["captions"]):
        elements.append(
            (
                f"cap{i}",
                "reel_caption",
                cap.start,
                cap.end,
                {"lines": cap.lines(captions.REEL)},
                0.09,
                0.09,
            )
        )
    return elements


def render_overlay(
    episode, reel: dict, prof: conform.Profile, fps: float, out: Path
) -> Path:
    """One RGBA track for the whole reel; identical held frames are reused."""
    elements = _elements(episode, reel)
    key = media.data_key(
        [(e[0], e[1], round(e[2], 3), round(e[3], 3), e[4]) for e in elements],
        prof.width,
        prof.height,
        fps,
        media.file_key(Path(graphics.__file__)),
    )
    target = out.with_name(f"{out.stem}.{key}.mov")
    if target.exists():
        return target
    out.parent.mkdir(parents=True, exist_ok=True)
    frames = int(round(reel["total"] * fps))
    proc = subprocess.Popen(
        [
            "ffmpeg",
            "-hide_banner",
            "-v",
            "error",
            "-y",
            "-f",
            "rawvideo",
            "-pix_fmt",
            "rgba",
            "-s",
            f"{prof.width}x{prof.height}",
            "-r",
            f"{fps}",
            "-i",
            "-",
            "-c:v",
            "qtrle",
            "-pix_fmt",
            "argb",
            str(target),
        ],
        stdin=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    cache: dict = {}
    for n in range(frames):
        t = n / fps
        active = [e for e in elements if e[2] <= t < e[3]]
        state = []
        for ident, _, start, end, _, fin, fout in active:
            local = t - start
            moving = local < fin or (end - start) - local < fout
            state.append((ident, round(local, 4) if moving else "hold"))
        state = tuple(state)
        if state not in cache:
            cv = graphics.Canvas(prof.width, prof.height)
            for _, kind, start, end, data, _, _ in active:
                graphics.KINDS[kind](cv, t - start, end - start, data)
            cache = (
                {state: cv.compose().tobytes()}
                if len(cache) > 64
                else {**cache, state: cv.compose().tobytes()}
            )
        proc.stdin.write(cache[state])
    proc.stdin.close()
    err = proc.stderr.read().decode(errors="replace")
    if proc.wait() != 0:
        raise EditError(f"reel overlay failed: {err[-600:]}")
    return target


def build(
    episode,
    paragraphs,
    takes,
    prof: conform.Profile,
    out_dir: Path,
    findings_out: list | None = None,
) -> dict:
    reel = plan(episode, paragraphs, takes)
    findings = check(episode, reel)
    if findings_out is not None:
        findings_out.extend(findings)
    errors = [f.message for f in findings if f.level == "error"]
    if errors:
        raise EditError("reel: " + "; ".join(errors))
    fps = prof.fps or float(episode.data["reel"].get("fps", 30))
    work = episode.work / f"reel_{prof.name}"
    segments = []
    for part in reel["parts"]:
        frames = int(round(part.end * fps)) - int(round(part.start * fps))
        source = {
            "kind": "clip",
            "path": episode.find_clip(part.data["clip"], vertical=True),
            "in": float(part.data.get("in", 0.0)),
            "grade": part.data.get("grade"),
        }
        segments.append(
            conform.render_segment(
                work / "segments", f"reel_{part.id}", source, frames, prof, fps
            )
        )
    picture = conform.concat(segments, work / f"{episode.id}_reel_picture.mkv")
    overlay = render_overlay(episode, reel, prof, fps, work / "overlay.mov")
    params = {**episode.mix, **episode.data["reel"].get("mix", {})}
    report = mix.build(
        mix.MixPlan(
            name=f"{episode.id}_reel",
            total=reel["total"],
            vo=reel["vo"],
            music=reel["music"],
            sfx=reel["sfx"],
            params=params,
            resolve=episode.resolve,
        ),
        work / "mix",
    )
    out_dir.mkdir(parents=True, exist_ok=True)
    stem = f"{episode.id}_{episode.data.get('slug', 'episode')}_reel"
    final = out_dir / f"{stem}.mp4"
    staged = out_dir / f"{stem}.staged.mp4"
    delivery.encode(
        picture, [(overlay, 0.0)], Path(report["master"]), staged, prof, fps
    )
    result = delivery.qc(
        staged,
        prof,
        fps,
        float(params["lufs"]),
        float(params["true_peak"]),
        [],
        MAX_SECONDS,
    )
    kept = delivery.finish(staged, final, result)
    (out_dir / f"{stem}.en.srt").write_text(
        captions.to_srt(reel["captions"], captions.REEL)
    )
    manifest = {
        "reel": final.name if kept else final.with_suffix(".rejected.mp4").name,
        "accepted": kept,
        "seconds": round(reel["total"], 3),
        "parts": [
            {
                "id": p.id,
                "role": p.data["role"],
                "start": round(p.start, 3),
                "end": round(p.end, 3),
            }
            for p in reel["parts"]
        ],
        "captions": len(reel["captions"]),
        "qc": result,
        "mix": {k: v for k, v in report.items() if k not in ("stems", "master")},
        "warnings": [f.message for f in findings if f.level == "warning"],
    }
    (out_dir / f"{stem}.json").write_text(json.dumps(manifest, indent=1))
    if not kept:
        raise EditError(f"{final.name} failed QC: " + "; ".join(result["problems"]))
    return manifest
