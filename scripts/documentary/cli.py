"""``python3 scripts/documentary <command>``: every documentary stage from one entry.

Episode commands take the edit file and, optionally, ``--clips``,
``--vertical-clips``, ``--vo``, ``--overlays`` and ``--work`` to override its
``paths`` block. See ``docs/DOCUMENTARY.md``.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from . import EditError


def _episode_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("edit", type=Path, help="episode edit (JSON)")
    for name in ("clips", "vertical-clips", "vo", "overlays", "work"):
        parser.add_argument(f"--{name}", type=Path)


def _load(args):
    from . import episode as episode_mod
    from . import narration, timeline

    ep = episode_mod.load(
        args.edit, args.clips, args.vertical_clips, args.vo, args.overlays, args.work
    )
    paragraphs = narration.parse_script(ep.script_path())
    analyse = getattr(args, "analyse", True)
    takes = narration.gather_takes(
        paragraphs,
        ep.vo,
        ep.work if analyse else None,
        ep.mix,
        ep.data.get("takes"),
        analyse=analyse,
    )
    tl = timeline.resolve(ep, paragraphs, takes)
    return ep, paragraphs, takes, tl


def _subtitles(tl, ep):
    from . import captions

    caps = []
    for item in tl.vo:
        caps += captions.paragraph_captions(
            item.id, item.text, item.start, item.take.segments, item.take.duration
        )
    return captions.finalise(caps)


def _findings(ep, tl, paragraphs):
    from . import narration, rules

    words = narration.script_report(paragraphs, float(ep.mix["words_per_minute"]))[
        "words"
    ]
    return rules.check(ep, tl, words)


def cmd_style(args) -> int:
    from . import style

    if args.json:
        style.save_tokens(args.json)
        print(f"wrote {args.json}")
    else:
        print(style.to_json())
    return 0


def cmd_script(args) -> int:
    from . import episode as episode_mod
    from . import narration

    ep = episode_mod.load(args.edit, vo=args.vo)
    paragraphs = narration.parse_script(ep.script_path())
    report = narration.script_report(paragraphs, float(ep.mix["words_per_minute"]))
    takes = narration.find_takes(ep.vo)
    print(json.dumps(report, indent=1))
    print(f"recordings in {ep.vo}:")
    for p in paragraphs:
        found = takes.get(p.id, [])
        state = (
            ", ".join(
                f"{path.name}{' (scratch)' if scratch else ''}"
                for _, path, scratch in found
            )
            or "MISSING"
        )
        print(f"  {p.id:<10} {p.words:>3} words  {state}")
    return 0


def cmd_synth_vo(args) -> int:
    from . import episode as episode_mod
    from . import narration

    ep = episode_mod.load(args.edit, vo=args.vo)
    paragraphs = narration.parse_script(ep.script_path())
    written = narration.synthesise(
        paragraphs, ep.vo, take=args.take, only=args.only, stretch=args.stretch
    )
    print(
        f"wrote {len(written)} synthetic takes to {ep.vo} (marked .synthetic: never deliver these)"
    )
    return 0


def cmd_placeholders(args) -> int:
    from . import placeholders

    args.analyse = True
    ep, _, _, tl = _load(args)
    size = tuple(int(v) for v in args.size.split("x"))
    written = placeholders.generate(
        ep, ep.clips, ep.overlays, ep.vertical_clips, size, args.fps, tl
    )
    print(f"wrote {len(written)} placeholder clips/overlays")
    return 0


def cmd_timeline(args) -> int:
    from . import timeline

    args.analyse = not args.estimate
    ep, _, _, tl = _load(args)
    out = args.out or ep.work / f"{ep.id}.timeline.json"
    timeline.write(tl, out)
    print(
        f"{ep.id}: {tl.duration:.2f}s, {len(tl.shots)} shots, {len(tl.vo)} paragraphs -> {out}"
    )
    for t, title in tl.chapters():
        print(f"  {int(t) // 60}:{int(t) % 60:02d} {title}")
    for w in tl.warnings:
        print(f"  warning: {w}")
    return 0


def cmd_subtitles(args) -> int:
    from . import captions

    args.analyse = True
    ep, _, _, tl = _load(args)
    caps = _subtitles(tl, ep)
    args.out.mkdir(parents=True, exist_ok=True)
    stem = f"{ep.id}_{ep.data.get('slug', 'episode')}"
    (args.out / f"{stem}.en.srt").write_text(captions.to_srt(caps))
    (args.out / f"{stem}.en.vtt").write_text(captions.to_vtt(caps))
    print(f"{len(caps)} captions -> {args.out}")
    return 0


def cmd_check(args) -> int:
    args.analyse = not args.estimate
    ep, paragraphs, _, tl = _load(args)
    findings = _findings(ep, tl, paragraphs)
    for f in findings:
        print(f)
    errors = [f for f in findings if f.level == "error"]
    print(f"{ep.id}: {len(errors)} error(s), {len(findings) - len(errors)} warning(s)")
    return 1 if errors else 0


def cmd_graphics(args) -> int:
    from . import conform, graphics, style

    args.analyse = False
    ep, _, _, tl = _load(args)
    numbers = conform.section_numbers(ep)
    sizes = [tuple(int(v) for v in s.split("x")) for s in args.sizes.split(",")]
    count = 0
    for span in tl.graphics:
        data = graphics.graphic_data(ep, span.data, numbers)
        for w, h in sizes:
            graphics.render_still(
                span.data["type"],
                data,
                w,
                h,
                args.stills / f"{span.id}_{w}x{h}.png",
                span.dur,
            )
            count += 1
    reel = ep.data.get("reel")
    if reel:
        hook = next((p for p in reel["parts"] if p.get("role") == "hook"), None)
        fields = graphics.end_card_fields("steam_demo")
        extras = {
            "reel_hook": (
                {"text": hook.get("text", ""), "kicker": hook.get("kicker", "")}
                if hook
                else None
            ),
            "reel_caption": {"lines": ["HANNIBAL PUT HIS", "WEAKEST TROOPS"]},
            "reel_tag": {
                "series": "THE BARCID ROAD",
                "episode_line": f"EPISODE {style.roman(int(ep.data['number']))}",
                "subtitle": fields["subtitle"],
                "end_card_destination": fields["end_card_destination"],
            },
        }
        for kind, data in extras.items():
            if data:
                graphics.render_still(
                    kind, data, 1080, 1920, args.stills / f"{kind}_1080x1920.png", 4.0
                )
                count += 1
    print(f"{count} stills -> {args.stills}")
    return 0


def cmd_mix(args) -> int:
    from . import mix

    args.analyse = True
    ep, _, _, tl = _load(args)
    report = mix.build(mix.episode_plan(ep, tl), ep.work / "mix")
    print(json.dumps({k: v for k, v in report.items() if k != "stems"}, indent=1))
    return 0


def cmd_package(args) -> int:
    from . import conform, delivery

    args.analyse = True
    ep, paragraphs, _, tl = _load(args)
    findings = _findings(ep, tl, paragraphs)
    for f in findings:
        print(f)
    prof = conform.profile(args.profile)
    manifest = delivery.package(
        ep,
        tl,
        _subtitles(tl, ep),
        prof,
        args.out,
        args.codec,
        args.allow_scratch,
        findings,
    )
    print(
        json.dumps(
            {k: manifest[k] for k in ("video", "accepted", "qc", "chapters")}, indent=1
        )
    )
    return 0


def cmd_reel(args) -> int:
    from . import conform, reel

    args.analyse = True
    ep, paragraphs, takes, _ = _load(args)
    manifest = reel.build(
        ep, paragraphs, takes, conform.profile(args.profile, reel=True), args.out
    )
    print(json.dumps({k: manifest[k] for k in ("reel", "seconds", "qc")}, indent=1))
    return 0


def cmd_score(args) -> int:
    from . import score

    plan = score.load_plan()
    cues = score.expand(plan)
    if args.action == "plan":
        out = score.summary(cues)
        if args.json:
            out = {"summary": out, "cues": cues}
        print(json.dumps(out, indent=1))
        return 0
    if args.action == "manifest":
        data = score.manifest(cues, plan)
        target = args.out or Path("score_requests.json")
        target.write_text(json.dumps(data, indent=1))
        print(f"{data['count']} generation requests -> {target} (nothing generated)")
        print(score.PROVENANCE_STEPS)
        return 0
    if args.action == "normalize-stems":
        if not args.out:
            raise EditError("normalize-stems needs --out")
        for folder in score.cue_folders(args.path):
            print(json.dumps(score.normalize(folder, args.out, plan)))
        return 0
    if args.action == "validate-stems":
        index = {c["id"]: c for c in cues}
        bad = 0
        for folder in score.cue_folders(args.path):
            problems = score.validate(folder, plan, index)
            bad += bool(problems)
            print(f"{folder.name}: {'ok' if not problems else '; '.join(problems)}")
        return 1 if bad else 0
    if args.action == "check-reuse":
        from . import episode as episode_mod
        from . import narration, timeline

        found = []
        for edit in args.edits:
            ep = episode_mod.load(edit)
            paragraphs = narration.parse_script(ep.script_path())
            takes = narration.gather_takes(
                paragraphs, ep.vo, None, ep.mix, analyse=False
            )
            found += score.passages(ep, timeline.resolve(ep, paragraphs, takes))
        signatures = {c["id"] for c in cues if c.get("signature")}
        problems = score.reuse(found, signatures)
        for p in problems:
            print(p)
        print(f"{len(found)} passages checked, {len(problems)} reused")
        return 1 if problems else 0
    raise EditError(f"unknown score action {args.action}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="scripts/documentary",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("style", help="print or write the series style tokens as JSON")
    p.add_argument("--json", type=Path)
    p.set_defaults(func=cmd_style)

    p = sub.add_parser("script", help="word counts and which recordings exist")
    p.add_argument("edit", type=Path)
    p.add_argument("--vo", type=Path)
    p.set_defaults(func=cmd_script)

    p = sub.add_parser(
        "synth-vo", help="synthetic stand-in narration (tests, animatics)"
    )
    p.add_argument("edit", type=Path)
    p.add_argument("--vo", type=Path)
    p.add_argument("--take", type=int, default=1)
    p.add_argument("--only", nargs="*")
    p.add_argument("--stretch", type=float, default=1.0)
    p.set_defaults(func=cmd_synth_vo)

    p = sub.add_parser(
        "placeholders",
        help="stand-in captures and overlays for every clip the edit names",
    )
    _episode_args(p)
    p.add_argument("--size", default="640x360")
    p.add_argument("--fps", type=float)
    p.set_defaults(func=cmd_placeholders)

    for name, func, text in (
        ("timeline", cmd_timeline, "resolve the edit to absolute times"),
        ("check", cmd_check, "editorial and delivery rules"),
    ):
        p = sub.add_parser(name, help=text)
        _episode_args(p)
        p.add_argument(
            "--estimate", action="store_true", help="time paragraphs from word counts"
        )
        if name == "timeline":
            p.add_argument("--out", type=Path)
        p.set_defaults(func=func)

    p = sub.add_parser(
        "subtitles", help="SRT and VTT from the script and the VO timing"
    )
    _episode_args(p)
    p.add_argument("--out", type=Path, required=True)
    p.set_defaults(func=cmd_subtitles)

    p = sub.add_parser(
        "graphics", help="stills of every graphic in the edit, both aspects"
    )
    _episode_args(p)
    p.add_argument("--stills", type=Path, required=True)
    p.add_argument("--sizes", default="1920x1080,1080x1920")
    p.set_defaults(func=cmd_graphics)

    p = sub.add_parser("mix", help="VO processing, ducking and the -14 LUFS master")
    _episode_args(p)
    p.set_defaults(func=cmd_mix)

    p = sub.add_parser("package", help="the full upload package")
    _episode_args(p)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--profile", default="2160p")
    p.add_argument("--codec", choices=("h264", "hevc"), default="h264")
    p.add_argument(
        "--allow-scratch",
        action="store_true",
        help="package with synthetic/scratch VO for review",
    )
    p.set_defaults(func=cmd_package)

    p = sub.add_parser("reel", help="the vertical reel (< 60 s, 1080x1920)")
    _episode_args(p)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--profile", default="1080p")
    p.set_defaults(func=cmd_reel)

    p = sub.add_parser("score", help="series score plan, requests and stem tools")
    p.add_argument(
        "action",
        choices=(
            "plan",
            "manifest",
            "normalize-stems",
            "validate-stems",
            "check-reuse",
        ),
    )
    p.add_argument("path", type=Path, nargs="?")
    p.add_argument("--out", type=Path)
    p.add_argument("--json", action="store_true")
    p.add_argument("--edits", type=Path, nargs="*", default=[])
    p.set_defaults(func=cmd_score)

    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except EditError as exc:
        print(f"documentary: {exc}", file=sys.stderr)
        return 1
