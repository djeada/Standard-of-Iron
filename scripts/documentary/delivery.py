"""The YouTube upload package (#1535): one command from edit to files.

``python3 scripts/documentary package EDIT --out DIR`` writes::

    ep07_cannae_2160p.mp4   H.264 High (or H.265 with --codec hevc), 3840x2160,
                            BT.709, AAC-LC 320k 48 kHz, faststart
    ep07_cannae.en.srt/.vtt subtitles from the script and the VO timing
    chapters.txt            YouTube timestamps from the edit's sections
    thumbnail.jpg           1280x720 title treatment over the best frame
    thumbnail_candidates.jpg the frames the picker scored, best first
    description.txt         summary, chapters, sources, credits, AI-audio
                            disclosure, Steam and GitHub links
    upload.json             what was built, its measurements and QC

The encode is checked before it is kept (the checks of
``scripts/trailer/deliver.py``, reused): picture and sound lengths agree,
integrated loudness within 1 LU of -14 LUFS and true peak at or under -1 dBTP,
frame zero lit (it is the thumbnail on every platform), no stray black or
frozen frames outside graphics and dips, and the frame size of the profile. A
failing file is quarantined as ``*.rejected.mp4``.

Narration from ``synth-vo`` (a ``.synthetic`` marker in the VO folder) or
estimated paragraphs make the package refuse unless ``--allow-scratch``, so
an animatic can be packaged for review but never mistaken for a master.
"""

from __future__ import annotations

import importlib.util
import json
import shutil
import sys
from functools import lru_cache
from pathlib import Path

from . import REPO, EditError, captions, conform, media, mix, style
from .timeline import Timeline

STEAM_URL = "https://store.steampowered.com/app/5129960/Standard_of_Iron/"
GITHUB_URL = "https://github.com/djeada/standard-of-iron"
AI_AUDIO_DISCLOSURE = (
    "The music is from Standard of Iron's own score, generated with AI (ElevenLabs) "
    "under a licence that permits commercial use. The narration is a human voice."
)
FOOTAGE_NOTE = (
    "Every image is real Standard of Iron footage, rendered in the game's own engine "
    "and shot through its arena cinematic tools; nothing is shown that the game cannot do."
)
YOUTUBE_MIN_CHAPTERS = 3
YOUTUBE_MIN_CHAPTER_SECONDS = 10.0


@lru_cache(maxsize=1)
def trailer_deliver():
    """``scripts/trailer/deliver.py``: shared delivery checks and thresholds."""
    path = REPO / "scripts" / "trailer" / "deliver.py"
    spec = importlib.util.spec_from_file_location("trailer_deliver", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules.setdefault("trailer_deliver", module)
    spec.loader.exec_module(module)
    return module


def stamp(seconds: float) -> str:
    s = int(seconds)
    return (
        f"{s // 60}:{s % 60:02d}"
        if s < 3600
        else f"{s // 3600}:{s % 3600 // 60:02d}:{s % 60:02d}"
    )


def chapter_problems(chapters: list[tuple[float, str]], total: float) -> list[str]:
    """YouTube only shows chapters when all of these hold."""
    problems = []
    if len(chapters) < YOUTUBE_MIN_CHAPTERS:
        problems.append(
            f"{len(chapters)} chapters; YouTube needs at least {YOUTUBE_MIN_CHAPTERS}"
        )
    if chapters and int(chapters[0][0]) != 0:
        problems.append("the first chapter must start at 0:00")
    bounds = [t for t, _ in chapters] + [total]
    for i, (start, title) in enumerate(chapters):
        if bounds[i + 1] - start < YOUTUBE_MIN_CHAPTER_SECONDS:
            problems.append(
                f"chapter '{title}' lasts {bounds[i + 1] - start:.1f}s (< 10 s)"
            )
    return problems


def chapters_text(chapters: list[tuple[float, str]]) -> str:
    return "\n".join(f"{stamp(t)} {title}" for t, title in chapters) + "\n"


def description(episode, chapters: list[tuple[float, str]]) -> str:
    data = episode.data
    number = int(data["number"])
    date = data.get("date", {})
    year = f" ({date['year']} {date.get('era', 'BC')})" if date.get("year") else ""
    lines = [
        f"{data['title']}{year} | {style.SERIES_NAME}, Episode {number}",
        "",
        data.get("summary", ""),
        "",
        "Chapters",
        chapters_text(chapters).rstrip(),
        "",
        "Sources",
    ]
    for source in data.get("sources", []):
        lines.append(
            f"- {source['author']}, {source.get('work', '')} {source.get('refs', '')}".rstrip()
        )
    lines += ["", "About the footage", FOOTAGE_NOTE]
    if data.get("scale_note"):
        lines.append(data["scale_note"])
    lines += ["", "Credits"]
    credits = data.get("credits", {})
    for role, name in credits.items():
        lines.append(f"{role.replace('_', ' ').capitalize()}: {name}")
    lines += [
        "Music: Standard of Iron original score",
        "",
        "AI disclosure",
        AI_AUDIO_DISCLOSURE,
        "",
        f"Play the free demo of Standard of Iron on Steam: {STEAM_URL}",
        f"Standard of Iron is free and open source: {GITHUB_URL}",
    ]
    nxt = data.get("next_episode")
    if nxt:
        lines += ["", f"Next: Episode {nxt['number']}, {nxt['title']}"]
    return "\n".join(lines).strip() + "\n"


def colourfulness(image) -> float:
    pixels = list(image.convert("RGB").resize((96, 54)).getdata())
    rg = [r - g for r, g, b in pixels]
    yb = [0.5 * (r + g) - b for r, g, b in pixels]

    def mean_std(values):
        m = sum(values) / len(values)
        return m, (sum((v - m) ** 2 for v in values) / len(values)) ** 0.5

    mrg, srg = mean_std(rg)
    myb, syb = mean_std(yb)
    return (srg**2 + syb**2) ** 0.5 + 0.3 * (mrg**2 + myb**2) ** 0.5


def frame_score(image) -> dict:
    from PIL import ImageFilter, ImageStat

    gray = image.convert("L").resize((320, 180))
    edges = ImageStat.Stat(gray.filter(ImageFilter.FIND_EDGES)).mean[0]
    mean = ImageStat.Stat(gray).mean[0] / 255.0
    exposure = max(0.0, 1.0 - abs(mean - 0.45) * 2.2)
    return {"sharpness": edges, "colour": colourfulness(image), "exposure": exposure}


def pick_thumbnail(
    picture: Path, timeline: Timeline, work: Path, at: float | None = None
) -> tuple[float, list]:
    """Score a frame from each shot and return the best time and the ranking.

    Sharpness (edge energy), colourfulness and exposure are scored against the
    best candidate; shots marked ``"thumbnail": true`` get a bonus, and an
    explicit ``thumbnail.at`` in the edit always wins.
    """
    from PIL import Image

    candidates: list[tuple[float, float]] = []
    if at is not None:
        candidates.append((at, 10.0))
    for shot in timeline.shots:
        if not shot.data.get("clip"):
            continue
        mark = bool(shot.data.get("thumbnail"))
        offset = (
            float(shot.data.get("thumbnail_at", shot.dur * 0.55))
            if mark
            else shot.dur * 0.5
        )
        candidates.append(
            (shot.start + min(offset, shot.dur - 0.1), 0.15 if mark else 0.0)
        )
    if not candidates:
        raise EditError("no frames to pick a thumbnail from")
    folder = work / "thumbnail"
    folder.mkdir(parents=True, exist_ok=True)
    scored = []
    for t, bonus in candidates:
        path = media.extract_frame(picture, t, folder / f"cand_{t:08.2f}.png", 640)
        scored.append((t, bonus, frame_score(Image.open(path)), path))
    top = {k: max(s[2][k] for s in scored) or 1.0 for k in ("sharpness", "colour")}

    def total(entry) -> float:
        _, bonus, s, _ = entry
        return (
            0.45 * s["sharpness"] / top["sharpness"]
            + 0.35 * s["colour"] / top["colour"]
            + 0.2 * s["exposure"]
            + bonus
        )

    ranked = sorted(scored, key=total, reverse=True)
    return ranked[0][0], [(t, round(total(e), 3), e[3]) for e in ranked for t in [e[0]]]


def thumbnail(episode, frame_path: Path, out: Path, size=(1280, 720)) -> Path:
    """Title treatment: the frame, a left-hand shade, kicker, title and series mark."""
    from PIL import Image, ImageDraw

    data = episode.data
    spec = data.get("thumbnail", {})
    w, h = size
    image = Image.open(frame_path).convert("RGB")
    scale = max(w / image.width, h / image.height)
    image = image.resize(
        (int(image.width * scale + 0.5), int(image.height * scale + 0.5)), Image.LANCZOS
    )
    image = image.crop(
        (
            (image.width - w) // 2,
            (image.height - h) // 2,
            (image.width - w) // 2 + w,
            (image.height - h) // 2 + h,
        )
    ).convert("RGBA")
    shade = Image.new("L", (w, 1))
    shade.putdata([int(235 * max(0.0, 1 - x / (w * 0.62)) ** 1.4) for x in range(w)])
    shade = shade.resize((w, h))
    dark = Image.new("RGBA", (w, h), style.COLORS["iron"] + (0,))
    dark.putalpha(shade)
    image.alpha_composite(dark)
    ink = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(ink)
    u = h / 720
    x = int(64 * u)
    title = spec.get("title", data["title"]).upper()
    size_title = int(150 * u)
    while (
        style.text_width(title, size_title, "display", 0.04) > w * 0.58
        and size_title > 40
    ):
        size_title -= 4
    style.draw_text(
        d,
        (x, h * 0.36),
        spec.get("kicker", "").upper(),
        int(44 * u),
        style.COLORS["gold_bright"] + (255,),
        tracking=0.2,
    )
    style.draw_text(
        d,
        (x, h * 0.36 + size_title * 1.0),
        title,
        size_title,
        style.COLORS["ink"] + (255,),
        tracking=0.04,
    )
    rule_y = h * 0.36 + size_title * 1.0 + 30 * u
    d.rectangle(
        [x, rule_y, x + 170 * u, rule_y + 4 * u], fill=style.COLORS["gold"] + (255,)
    )
    if spec.get("line"):
        style.draw_text(
            d,
            (x, rule_y + 54 * u),
            spec["line"].upper(),
            int(30 * u),
            style.COLORS["ink"] + (255,),
            tracking=0.14,
        )
    mark = f"{style.SERIES_TITLE}  ·  EPISODE {style.roman(int(data['number']))}"
    style.draw_text(
        d,
        (x, h - 48 * u),
        mark,
        int(24 * u),
        style.COLORS["ink_dim"] + (255,),
        tracking=0.24,
    )
    from PIL import ImageFilter

    shadow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    shadow.putalpha(
        ink.getchannel("A")
        .filter(ImageFilter.GaussianBlur(8 * u))
        .point(lambda v: min(255, v * 2))
    )
    image.alpha_composite(shadow)
    image.alpha_composite(ink)
    out.parent.mkdir(parents=True, exist_ok=True)
    quality = 92
    while True:
        image.convert("RGB").save(out, "JPEG", quality=quality, optimize=True)
        if out.stat().st_size < 2_000_000 or quality <= 60:
            break
        quality -= 8
    return out


def contact_sheet(ranked: list, out: Path) -> Path:
    from PIL import Image, ImageDraw

    thumbs = [
        Image.open(p).convert("RGB").resize((320, 180)) for _, _, p in ranked[:12]
    ]
    cols = 4
    rows = (len(thumbs) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * 320, rows * 180), (0, 0, 0))
    for i, (thumb, (t, score, _)) in enumerate(zip(thumbs, ranked, strict=False)):
        ImageDraw.Draw(thumb).text(
            (6, 6), f"#{i + 1} {stamp(t)} {score}", fill=(255, 230, 160)
        )
        sheet.paste(thumb, ((i % cols) * 320, (i // cols) * 180))
    sheet.save(out, "JPEG", quality=85)
    return out


def encode(
    picture: Path,
    layers: list[tuple[Path, float]],
    audio: Path,
    out: Path,
    prof: conform.Profile,
    fps: float,
    codec: str = "h264",
) -> Path:
    inputs, filters, last = conform.overlay_graph(layers, 1)
    audio_index = 1 + len(layers)
    filters.append(
        f"[{last}]format=yuv420p,scale=out_color_matrix=bt709:out_range=tv[vout]"
    )
    script = out.with_suffix(".graph.txt")
    script.write_text(";\n".join(filters))
    gop = str(int(round(fps * 2)))
    if codec == "hevc":
        video = [
            "-c:v",
            "libx265",
            "-preset",
            prof.preset,
            "-crf",
            "17",
            "-tag:v",
            "hvc1",
            "-x265-params",
            f"vbv-maxrate=60000:vbv-bufsize=120000:keyint={gop}:log-level=error",
        ]
    else:
        video = [
            "-c:v",
            "libx264",
            "-preset",
            prof.preset,
            "-profile:v",
            "high",
            "-crf",
            "15",
            "-maxrate",
            "68M",
            "-bufsize",
            "136M",
            "-g",
            gop,
            "-bf",
            "2",
        ]
    media.ffmpeg(
        [
            "-i",
            str(picture),
            *inputs,
            "-i",
            str(audio),
            "-filter_complex_script",
            str(script),
            "-map",
            "[vout]",
            "-map",
            f"{audio_index}:a",
            *video,
            "-pix_fmt",
            "yuv420p",
            "-color_primaries",
            "bt709",
            "-color_trc",
            "bt709",
            "-colorspace",
            "bt709",
            "-r",
            f"{fps}",
            "-c:a",
            "aac",
            "-b:a",
            "320k",
            "-ar",
            "48000",
            "-ac",
            "2",
            "-shortest",
            "-movflags",
            "+faststart",
            str(out),
        ],
        f"encoding {out.name}",
    )
    script.unlink(missing_ok=True)
    return out


def qc(
    path: Path,
    prof: conform.Profile,
    fps: float,
    lufs: float,
    true_peak: float,
    allow_black: list[tuple[float, float]],
    max_seconds: float | None = None,
) -> dict:
    deliver = trailer_deliver()
    info = media.probe(path)
    problems = []
    if (info.get("width"), info.get("height")) != (prof.width, prof.height):
        problems.append(
            f"frame is {info.get('width')}x{info.get('height')}, not {prof.width}x{prof.height}"
        )
    vdur = info.get("video_duration", info["duration"])
    adur = info.get("audio_duration", info["duration"])
    if abs(vdur - adur) > 1.5 / fps + 0.03:
        problems.append(f"picture {vdur:.3f}s and sound {adur:.3f}s differ")
    if max_seconds is not None and vdur >= max_seconds:
        problems.append(f"runs {vdur:.2f}s; must be under {max_seconds:.0f}s")
    loud = media.loudness(path)
    if abs(loud["integrated_lufs"] - lufs) > 1.0:
        problems.append(
            f"integrated loudness {loud['integrated_lufs']} LUFS, target {lufs}"
        )
    if loud["true_peak_dbtp"] > true_peak + 0.1:
        problems.append(f"true peak {loud['true_peak_dbtp']} dBTP over {true_peak}")
    luma = media.luma_series(path)
    opening = luma[: max(1, int(round(fps * deliver.OPENING_SECONDS)))]
    if not opening or min(opening) < deliver.OPENING_MIN_LUMA:
        problems.append(
            f"opening frames reach mean luma {min(opening) if opening else 0:.1f} (< {deliver.OPENING_MIN_LUMA}): "
            "frame zero is the thumbnail, open on a lit image"
        )

    def allowed(t: float) -> bool:
        return any(a - 0.05 <= t <= b + 0.05 for a, b in allow_black)

    stray = [i / fps for i, y in enumerate(luma) if y < 17.5 and not allowed(i / fps)]
    if stray:
        problems.append(f"{len(stray)} black frame(s), first at {stray[0]:.2f}s")
    for start, end in deliver.freeze_runs(path, int(fps * 2), fps):
        if not allowed(start):
            problems.append(f"frozen picture from {start:.2f}s to {end}")
    return {
        "seconds": round(vdur, 3),
        "width": info.get("width"),
        "height": info.get("height"),
        "integrated_lufs": loud["integrated_lufs"],
        "true_peak_dbtp": loud["true_peak_dbtp"],
        "bytes": path.stat().st_size,
        "video_mbps": round(path.stat().st_size * 8 / max(vdur, 0.01) / 1e6, 2),
        "problems": problems,
    }


def finish(staged: Path, final: Path, report: dict) -> bool:
    if report["problems"]:
        rejected = final.with_suffix(".rejected.mp4")
        staged.replace(rejected)
        final.unlink(missing_ok=True)
        return False
    staged.replace(final)
    return True


def package(
    episode,
    timeline: Timeline,
    subtitles: list,
    prof: conform.Profile,
    out: Path,
    codec: str,
    allow_scratch: bool,
    findings: list,
) -> dict:
    """Build every file of the upload package; returns the ``upload.json`` content."""
    from .narration import is_synthetic

    work = episode.work
    fps = prof.fps or episode.fps
    scratch = [
        v.id
        for v in timeline.vo
        if v.take is None or v.take.estimated or v.take.scratch
    ]
    synthetic = is_synthetic(episode.vo)
    if (scratch or synthetic) and not allow_scratch:
        raise EditError(
            "narration is not final ("
            + (
                f"estimated/scratch paragraphs {scratch}"
                if scratch
                else "synthetic test VO"
            )
            + "); record it, or pass --allow-scratch for a review package"
        )
    errors = [f for f in findings if f.level == "error"]
    if errors:
        raise EditError(
            "the edit breaks delivery rules:\n  "
            + "\n  ".join(f.message for f in errors)
        )
    problems = chapter_problems(timeline.chapters(), timeline.duration)
    if problems:
        raise EditError("chapters: " + "; ".join(problems))
    out.mkdir(parents=True, exist_ok=True)
    stem = f"{episode.id}_{episode.data.get('slug', 'episode')}"
    report = mix.build(mix.episode_plan(episode, timeline), work / "mix")
    picture = conform.build_picture(episode, timeline, prof, work)
    layers = conform.graphics_layers(episode, timeline, prof, work)
    video = out / f"{stem}_{prof.name}.mp4"
    staged = out / f"{stem}_{prof.name}.staged.mp4"
    encode(
        picture,
        [(p, s) for p, s, _ in layers],
        Path(report["master"]),
        staged,
        prof,
        fps,
        codec,
    )
    allow = [(s.start, s.end) for s in timeline.shots if not s.data.get("clip")]
    allow += [
        (s.start, s.end)
        for s in timeline.graphics
        if s.data["type"] in ("end_card", "title_sequence")
    ]
    allow += [(s.start, s.end) for s in timeline.shots if s.data.get("hold")]
    for s in timeline.shots:
        for key, at_end in (("fade_in", False), ("fade_out", True)):
            if s.data.get(key):
                d = float(s.data[key])
                allow.append((s.end - d, s.end) if at_end else (s.start, s.start + d))
    result = qc(
        staged,
        prof,
        fps,
        float(episode.mix["lufs"]),
        float(episode.mix["true_peak"]),
        allow,
    )
    kept = finish(staged, video, result)
    (out / f"{stem}.en.srt").write_text(captions.to_srt(subtitles))
    (out / f"{stem}.en.vtt").write_text(captions.to_vtt(subtitles))
    (out / "chapters.txt").write_text(chapters_text(timeline.chapters()))
    (out / "description.txt").write_text(description(episode, timeline.chapters()))
    at = episode.data.get("thumbnail", {}).get("at")
    best, ranked = pick_thumbnail(
        picture, timeline, work, None if at is None else float(at)
    )
    full = media.extract_frame(picture, best, work / "thumbnail" / "best.png")
    thumbnail(episode, full, out / "thumbnail.jpg")
    contact_sheet(ranked, out / "thumbnail_candidates.jpg")
    shutil.copy(work / "mix" / f"{episode.id}.mix.json", out / "mix.json")
    manifest = {
        "episode": episode.id,
        "title": episode.data["title"],
        "profile": prof.name,
        "codec": codec,
        "video": video.name if kept else video.with_suffix(".rejected.mp4").name,
        "accepted": kept,
        "qc": result,
        "mix": {k: v for k, v in report.items() if k not in ("stems", "master")},
        "chapters": [
            {"at": stamp(t), "title": title} for t, title in timeline.chapters()
        ],
        "subtitles": [f"{stem}.en.srt", f"{stem}.en.vtt"],
        "thumbnail": {"file": "thumbnail.jpg", "at": round(best, 3)},
        "narration": {"synthetic": synthetic, "not_final": scratch},
        "warnings": [f.message for f in findings if f.level == "warning"]
        + report.get("warnings", []),
    }
    (out / "upload.json").write_text(json.dumps(manifest, indent=1))
    if not kept:
        raise EditError(f"{video.name} failed QC: " + "; ".join(result["problems"]))
    return manifest
