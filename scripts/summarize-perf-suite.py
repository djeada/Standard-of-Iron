#!/usr/bin/env python3
"""Turn a run-perf-suite.sh output directory into one machine-readable verdict.

Every runtime benchmark and arena report already carries its own budget verdict.
This collapses the repeats into one line per fixture -- the median of the run
medians, so a single unlucky repeat cannot decide a gate -- and exits non-zero
when any gate the plan names is red, so CI can call it directly.
"""

from __future__ import annotations

import json
import statistics
import sys
from pathlib import Path


def load(path: Path) -> dict | None:
    try:
        with path.open(encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, json.JSONDecodeError):
        return None


def group_runs(out_dir: Path) -> dict[str, list[dict]]:
    grouped: dict[str, list[dict]] = {}
    for path in sorted(out_dir.glob("*.run*.json")):
        label = path.name.split(".run", 1)[0]
        report = load(path)
        if report is None:
            continue
        grouped.setdefault(label, []).append(report)
    return grouped


def arena_runs(out_dir: Path) -> dict[str, dict]:
    reports: dict[str, dict] = {}
    arena_dir = out_dir / "arena"
    if not arena_dir.is_dir():
        return reports
    for path in sorted(arena_dir.glob("*/report.json")):
        report = load(path)
        if report is not None:
            reports[path.parent.name] = report
    return reports


def median(values: list[float]) -> float:
    return statistics.median(values) if values else 0.0


def runtime_row(label: str, runs: list[dict]) -> dict:
    valid = [run for run in runs if run.get("valid")]
    row: dict = {
        "fixture": label,
        "runs": len(runs),
        "valid_runs": len(valid),
        "failures": [],
    }
    if not valid:
        row["passed"] = False
        row["failures"] = ["no run produced a valid report"]
        return row

    def pick(path: tuple[str, ...]) -> list[float]:
        out = []
        for run in valid:
            node: object = run
            for key in path:
                if not isinstance(node, dict) or key not in node:
                    node = None
                    break
                node = node[key]
            if isinstance(node, (int, float)):
                out.append(float(node))
        return out

    row["frame_p50_ms"] = median(pick(("render_ms", "p50_ms")))
    row["frame_p95_ms"] = median(pick(("render_ms", "p95_ms")))
    row["frame_p99_ms"] = median(pick(("render_ms", "p99_ms")))
    row["frame_max_ms"] = median(pick(("render_ms", "max_ms")))
    row["update_p95_ms"] = median(pick(("update_ms", "p95_ms")))
    row["gpu_shadow_p95_ms"] = median(pick(("gpu_shadow_ms", "p95_ms")))
    row["gpu_color_p95_ms"] = median(pick(("gpu_color_ms", "p95_ms")))
    row["navigation_p95_ms"] = median(pick(("navigation", "tick_ms", "p95_ms")))
    row["post_load_asset_work"] = median(
        pick(("asset_counters", "post_load_asset_work"))
    )

    failures: list[str] = []
    for run in valid:
        verdict = run.get("budget")
        if not isinstance(verdict, dict):
            failures.append("run produced no budget verdict")
            continue
        for failure in verdict.get("failures", []):
            if failure not in failures:
                failures.append(failure)
    row["failures"] = failures
    row["passed"] = not failures
    return row


def arena_row(scenario: str, report: dict) -> dict:
    performance = report.get("performance", {})
    verdict = report.get("budget", {})
    counters = report.get("asset_counters", {})
    failures = list(verdict.get("failures", []))
    if not report.get("passed", False):
        codes = sorted({issue.get("code", "?") for issue in report.get("issues", [])})
        if codes:
            failures.append("scenario issues: " + ", ".join(codes))
    return {
        "fixture": scenario,
        "runs": 1,
        "valid_runs": 1 if performance else 0,
        "frame_p50_ms": performance.get("p50_ms", 0.0),
        "frame_p95_ms": performance.get("p95_ms", 0.0),
        "frame_p99_ms": performance.get("p99_ms", 0.0),
        "frame_max_ms": performance.get("max_ms", 0.0),
        "update_p95_ms": performance.get("simulation_p95_ms", 0.0),
        "gpu_shadow_p95_ms": 0.0,
        "gpu_color_p95_ms": 0.0,
        "navigation_p95_ms": report.get("navigation", {})
        .get("tick_ms", {})
        .get("p95_ms", 0.0),
        "post_load_asset_work": counters.get("post_load_asset_work", 0),
        "failures": failures,
        "passed": not failures,
    }


def render(rows: list[dict], manifest: dict | None) -> str:
    lines: list[str] = []
    if manifest:
        lines.append(
            "commit {commit}  preset {preset}  {seconds}s x{repeats}".format(
                commit=manifest.get("commit", "?")[:12],
                preset=manifest.get("graphics_preset", "?"),
                seconds=manifest.get("measured_seconds", "?"),
                repeats=manifest.get("repeats", "?"),
            )
        )
        lines.append(f"cpu {manifest.get('cpu', '?')}")
        lines.append(f"gpu {manifest.get('gpu', '?')}")
        if manifest.get("working_tree_dirty_files", 0):
            lines.append(
                "WARNING: {} uncommitted files; this run is not reproducible".format(
                    manifest["working_tree_dirty_files"]
                )
            )
        lines.append("")

    header = (
        f"{'fixture':<28}{'runs':>5}{'p50':>8}{'p95':>8}{'p99':>8}"
        f"{'max':>8}{'upd95':>8}{'nav95':>8}{'bakes':>8}  verdict"
    )
    lines.append(header)
    lines.append("-" * len(header))
    for row in rows:
        lines.append(
            f"{row['fixture']:<28}{row['valid_runs']:>5}"
            f"{row['frame_p50_ms']:>8.2f}{row['frame_p95_ms']:>8.2f}"
            f"{row['frame_p99_ms']:>8.2f}{row['frame_max_ms']:>8.2f}"
            f"{row['update_p95_ms']:>8.2f}{row['navigation_p95_ms']:>8.2f}"
            f"{row['post_load_asset_work']:>8.0f}"
            f"  {'PASS' if row['passed'] else 'FAIL'}"
        )
    lines.append("")
    for row in rows:
        if row["passed"]:
            continue
        lines.append(f"{row['fixture']}:")
        for failure in row["failures"]:
            lines.append(f"  - {failure}")
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: summarize-perf-suite.py <run-directory>", file=sys.stderr)
        return 2
    out_dir = Path(argv[1])
    if not out_dir.is_dir():
        print(f"summarize-perf-suite: {out_dir} is not a directory", file=sys.stderr)
        return 2

    rows = [
        runtime_row(label, runs) for label, runs in sorted(group_runs(out_dir).items())
    ]
    rows += [
        arena_row(name, report) for name, report in sorted(arena_runs(out_dir).items())
    ]

    if not rows:
        print("summarize-perf-suite: no reports found", file=sys.stderr)
        return 2

    manifest = load(out_dir / "manifest.json")
    summary = render(rows, manifest)
    print(summary)

    verdict = {
        "passed": all(row["passed"] for row in rows),
        "manifest": manifest,
        "fixtures": rows,
    }
    (out_dir / "verdict.json").write_text(
        json.dumps(verdict, indent=2) + "\n", encoding="utf-8"
    )
    return 0 if verdict["passed"] else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
