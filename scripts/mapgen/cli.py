"""Command line for the map generator: see docs/MAP_GENERATOR.md."""

from __future__ import annotations

import argparse
import json
import statistics
import sys
import time
from pathlib import Path
from typing import Any, Sequence

from . import GENERATOR_VERSION
from .orchestrator import generate
from .request import STAGES, GenerationRequest, RequestError
from .schema import describe, load_presets


def _parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="soi-mapgen",
        description="Generate a playable Standard of Iron battlefield from a seed.",
    )
    parser.add_argument(
        "--describe", action="store_true", help="print the parameter schema"
    )
    parser.add_argument(
        "--preset", help="preset id; 'all' cycles presets in a --count batch"
    )
    parser.add_argument("--seed", type=int, help="global seed (0..4294967295)")
    parser.add_argument("--width", type=int)
    parser.add_argument("--height", type=int)
    parser.add_argument(
        "--set",
        action="append",
        default=[],
        metavar="KEY=VALUE",
        help="override one parameter, e.g. water.rivers=2 (repeatable)",
    )
    parser.add_argument(
        "--request", type=Path, help="request JSON, or a map carrying 'generation'"
    )
    parser.add_argument("--base", type=Path, help="map to reroll")
    parser.add_argument(
        "--lock", default="", help="comma-separated stages to keep from --base"
    )
    parser.add_argument("--output", type=Path, help="where to write the candidate map")
    parser.add_argument("--report", type=Path, help="where to write the report JSON")
    parser.add_argument(
        "--progress", action="store_true", help="JSON progress lines on stdout"
    )
    parser.add_argument(
        "--count", type=int, help="batch: generate this many consecutive seeds"
    )
    parser.add_argument("--report-dir", type=Path, help="batch output directory")
    return parser.parse_args(argv)


def _load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RequestError(f"cannot read {path}: {error}") from error


def build_request(
    args: argparse.Namespace, preset: str | None = None
) -> GenerationRequest:
    data: dict[str, Any] = {}
    if args.request:
        loaded = _load_json(args.request)
        data = (
            dict(loaded.get("generation") or loaded) if isinstance(loaded, dict) else {}
        )
    request = (
        GenerationRequest.from_json(data)
        if data
        else GenerationRequest(seed=0, preset="")
    )
    presets = load_presets()
    request.preset = preset or args.preset or request.preset
    if not request.preset:
        raise RequestError("choose a preset with --preset (see --describe)")
    if request.preset not in presets:
        raise RequestError(
            f"unknown preset '{request.preset}'; known: {', '.join(presets)}"
        )
    if args.seed is not None:
        request.seed = args.seed
    elif not data:
        raise RequestError("give a --seed")
    if not data or args.preset:
        request.width = presets[request.preset].get("width", 650)
        request.height = presets[request.preset].get("height", 650)
    if args.width:
        request.width = args.width
    if args.height:
        request.height = args.height
    for item in args.set:
        if "=" not in item:
            raise RequestError(f"--set expects KEY=VALUE, got '{item}'")
        key, value = item.split("=", 1)
        request.parameters[key.strip()] = value.strip()
    if args.lock:
        request.locks = [
            stage.strip() for stage in args.lock.split(",") if stage.strip()
        ]
    return request


def _write(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def _summary_line(report: dict[str, Any]) -> str:
    marks = {"pass": "ok  ", "warn": "warn", "fail": "FAIL"}
    lines = [
        f"{'ok' if report['ok'] else 'FAILED'}: seed {report['request']['seed']} "
        f"preset {report['request']['preset']} in {report['metrics'].get('seconds')} s "
        f"({report['metrics'].get('attempts')} attempt(s))"
    ]
    for check in report["checks"]:
        lines.append(
            f"  [{marks.get(check['status'], check['status'])}] {check['message']}"
        )
    return "\n".join(lines)


def _generate_one(args: argparse.Namespace) -> int:
    if args.output is None:
        raise RequestError("give --output for the candidate map")
    request = build_request(args)
    base = _load_json(args.base) if args.base else None
    progress = None
    if args.progress:

        def progress(event: dict[str, Any]) -> None:
            print(json.dumps(event), flush=True)

    result = generate(request, base=base, progress=progress)
    if result.definition is not None:
        _write(args.output, result.definition)
    report_path = args.report or args.output.with_suffix(".report.json")
    _write(report_path, result.report)
    if args.progress:
        print(
            json.dumps(
                {
                    "event": "done",
                    "ok": result.ok,
                    "output": str(args.output),
                    "report": str(report_path),
                }
            ),
            flush=True,
        )
    else:
        print(_summary_line(result.report))
    return 0 if result.ok else 1


def _batch(args: argparse.Namespace) -> int:
    if not args.report_dir:
        raise RequestError("a --count batch needs --report-dir")
    if args.seed is None:
        raise RequestError("give a --seed to start the batch from")
    presets = list(load_presets())
    results = []
    started = time.monotonic()
    for i in range(args.count):
        seed = args.seed + i
        preset = presets[i % len(presets)] if args.preset == "all" else None
        sub = argparse.Namespace(**vars(args))
        sub.seed = seed
        request = build_request(sub, preset=preset)
        result = generate(request)
        stem = f"seed_{seed}"
        if result.definition is not None:
            _write(args.report_dir / f"{stem}.json", result.definition)
        _write(args.report_dir / f"{stem}.report.json", result.report)
        failures = [c["id"] for c in result.report["checks"] if c["status"] == "fail"]
        results.append(
            {
                "seed": seed,
                "preset": request.preset,
                "ok": result.ok,
                "attempts": result.report["metrics"].get("attempts"),
                "seconds": result.report["metrics"].get("seconds"),
                "failures": failures,
                "metrics": result.report["metrics"],
            }
        )
        print(
            f"seed {seed} {request.preset}: {'ok' if result.ok else 'FAILED ' + ','.join(failures)} "
            f"({result.report['metrics'].get('seconds')} s, "
            f"{result.report['metrics'].get('attempts')} attempt(s))",
            flush=True,
        )
    seconds = [r["seconds"] for r in results]
    by_check: dict[str, int] = {}
    for r in results:
        for check in r["failures"]:
            by_check[check] = by_check.get(check, 0) + 1
    median = statistics.median(seconds) if seconds else 0.0
    numeric = sorted(
        {
            k
            for r in results
            for k, v in r["metrics"].items()
            if isinstance(v, (int, float))
        }
    )
    summary = {
        "version": GENERATOR_VERSION,
        "count": len(results),
        "failures": sum(1 for r in results if not r["ok"]),
        "failures_by_check": dict(sorted(by_check.items())),
        "pathological_seeds": [
            r["seed"]
            for r in results
            if not r["ok"] or (r["attempts"] or 1) > 1 or r["seconds"] > 2.0 * median
        ],
        "seconds_total": round(time.monotonic() - started, 2),
        "seconds_mean": round(statistics.mean(seconds), 2) if seconds else 0.0,
        "mean_metrics": {
            key: round(
                statistics.mean(
                    r["metrics"][key]
                    for r in results
                    if isinstance(r["metrics"].get(key), (int, float))
                ),
                3,
            )
            for key in numeric
        },
        "results": [{k: v for k, v in r.items() if k != "metrics"} for r in results],
    }
    _write(args.report_dir / "summary.json", summary)
    print(
        f"{summary['count']} seed(s), {summary['failures']} failed, "
        f"mean {summary['seconds_mean']} s; summary: {args.report_dir / 'summary.json'}"
    )
    return 1 if summary["failures"] else 0


def main(argv: Sequence[str] | None = None) -> int:
    args = _parse_args(sys.argv[1:] if argv is None else argv)
    try:
        if args.describe:
            print(json.dumps(describe(), indent=2))
            return 0
        if args.count:
            return _batch(args)
        return _generate_one(args)
    except RequestError as error:
        message = str(error)
        if args.progress:
            print(json.dumps({"event": "error", "message": message}), flush=True)
        print(f"soi-mapgen: {message}", file=sys.stderr)
        if args.report and not args.count:
            _write(
                args.report,
                {"version": GENERATOR_VERSION, "ok": False, "error": message},
            )
        return 2


__all__ = ["main", "STAGES"]
