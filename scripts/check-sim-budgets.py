#!/usr/bin/env python3
"""CPU-only deterministic performance gate.

Runs sim_benchmark and gates the parts of the Ultra plan that need no GPU and no
quiet machine: navigation query amplification and the replay digest. Wall-clock
tick and navigation times are measured and printed, but they do NOT fail the
gate unless --check-timings is given.

That split is measured, not assumed. Recording a baseline and immediately
re-checking it on the same box while a build was running moved tick p95 from
138 ms to 191 ms at 1,000 units, while every amplification counter came back
byte-identical. Timing budgets belong on the pinned hardware runner; this script
is what an ordinary shared CI runner can honestly enforce on every commit.
The pinned runner is .github/workflows/perf-nightly.yml, which runs this same
script with --check-timings and records the hardware it ran on.

The counter set covers navigation query amplification and, since the route
cache learned to invalidate by region rather than by flush, its hit ratio and
its eviction and flush counts. A ratio is gated from below: a cache that stops
hitting is a regression even when every absolute count falls with it.

What is still NOT gated anywhere: rendered frame percentiles, the portable
rigged fallback, allocation counts, the longest GUI stall during a save, and
creature-asset residency across a run of missions. Those need a GPU, a real
client, or a multi-mission harness; none of them can be honestly asserted from
a shared runner, and none of them should be inferred from the numbers here.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

DEFAULT_BASELINE = Path("tests/perf/sim_budgets.json")


AMPLIFICATION_COUNTERS = (
    "position_tests",
    "standability_tests",
    "standability_cells_scanned",
    "nearest_standable_cells_scanned",
    "cells_expanded",
    "route_cache_misses",
    "route_cache_evictions",
    "route_cache_flushes",
    "region_map_rebuilds",
    "dirty_cells_rebuilt",
)

RATIO_METRICS = ("route_cache_hit_ratio",)


def run_benchmark(binary: Path, units: int, ticks: int, out: Path) -> dict:
    result = subprocess.run(
        [str(binary), "--units", str(units), "--ticks", str(ticks), "--json", str(out)],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        print(result.stderr, file=sys.stderr)
        raise SystemExit(
            f"check-sim-budgets: sim_benchmark exited {result.returncode} "
            f"for {units} units"
        )
    with out.open(encoding="utf-8") as handle:
        return json.load(handle)


def scenario_metrics(scenario: dict) -> dict:
    ticks = max(1, scenario["ticks"])
    units = max(1, scenario["units"])
    nav = scenario.get("navigation", {})
    totals = nav.get("total", {})
    metrics = {
        "tick_p95_ms": scenario["tick_ms"]["p95"],
        "tick_average_ms": scenario["tick_ms"]["average"],
        "navigation_p95_ms": nav.get("p95_ms", 0.0),
        "navigation_average_ms": nav.get("average_ms", 0.0),
        "digest": scenario["digest"],
    }
    for counter in AMPLIFICATION_COUNTERS:
        metrics[f"{counter}_per_unit_tick"] = totals.get(counter, 0) / (units * ticks)
    hits = totals.get("route_cache_hits", 0)
    misses = totals.get("route_cache_misses", 0)
    metrics["route_cache_hit_ratio"] = hits / max(1, hits + misses)
    return metrics


TIMING_METRICS = (
    "tick_p95_ms",
    "tick_average_ms",
    "navigation_p95_ms",
    "navigation_average_ms",
)


def compare(name: str, measured: dict, budget: dict, tolerance: float) -> list[str]:
    failures: list[str] = []
    for key, limit in budget.items():
        if key == "digest":
            if measured["digest"] != limit:
                failures.append(
                    f"{name}: replay digest changed: {limit} -> {measured['digest']}"
                )
            continue
        value = measured.get(key)
        if value is None:
            continue
        if key in RATIO_METRICS:
            floor = limit * (1.0 - tolerance)
            if value < floor:
                failures.append(
                    f"{name}: {key} fell to {value:.4f}, baseline {limit:.4f} "
                    f"(-{tolerance:.0%} tolerance = {floor:.4f})"
                )
            continue
        allowed = limit * (1.0 + tolerance)
        if value > allowed:
            failures.append(
                f"{name}: {key} was {value:.4f}, budget {limit:.4f} "
                f"(+{tolerance:.0%} tolerance = {allowed:.4f})"
            )
    return failures


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--binary",
        type=Path,
        default=Path("build/bin/sim_benchmark"),
        help="sim_benchmark path (default build/bin/sim_benchmark)",
    )
    parser.add_argument(
        "--baseline",
        type=Path,
        default=DEFAULT_BASELINE,
        help=f"accepted budgets (default {DEFAULT_BASELINE})",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path("artifacts/perf"),
        help="where to write the raw benchmark JSON",
    )
    parser.add_argument(
        "--tolerance",
        type=float,
        default=0.25,
        help="fractional headroom over the budget before failing (default 0.25)",
    )
    parser.add_argument(
        "--record",
        action="store_true",
        help="write the measured numbers as the new accepted baseline",
    )
    parser.add_argument(
        "--check-digest",
        action="store_true",
        help="also require the replay digest to match the baseline",
    )
    parser.add_argument(
        "--check-timings",
        action="store_true",
        help="also gate wall-clock tick and navigation times; only meaningful on a "
        "machine with pinned power state and no competing load",
    )
    args = parser.parse_args(argv[1:])

    if not args.binary.exists():
        print(f"check-sim-budgets: {args.binary} not built", file=sys.stderr)
        return 2

    args.out_dir.mkdir(parents=True, exist_ok=True)

    if args.record:
        fixtures = [{"units": 1000, "ticks": 240}, {"units": 2000, "ticks": 240}]
    else:
        if not args.baseline.exists():
            print(
                f"check-sim-budgets: no baseline at {args.baseline}; "
                "run with --record first",
                file=sys.stderr,
            )
            return 2
        with args.baseline.open(encoding="utf-8") as handle:
            baseline = json.load(handle)
        fixtures = [
            {"units": entry["units"], "ticks": entry["ticks"]}
            for entry in baseline["fixtures"]
        ]

    measured: list[dict] = []
    for fixture in fixtures:
        report = run_benchmark(
            args.binary,
            fixture["units"],
            fixture["ticks"],
            args.out_dir / f"sim_{fixture['units']}.json",
        )
        for scenario in report["scenarios"]:
            entry = {
                "units": fixture["units"],
                "ticks": fixture["ticks"],
                **scenario_metrics(scenario),
            }
            measured.append(entry)

    if args.record:
        args.baseline.parent.mkdir(parents=True, exist_ok=True)
        args.baseline.write_text(
            json.dumps({"fixtures": measured}, indent=2) + "\n", encoding="utf-8"
        )
        print(
            f"check-sim-budgets: recorded {len(measured)} fixtures to {args.baseline}"
        )
        return 0

    failures: list[str] = []
    print(
        f"{'fixture':<14}{'tick p95':>10}{'nav p95':>10}"
        f"{'pos/unit/tick':>16}{'cells/unit/tick':>18}"
    )
    for entry, budget in zip(measured, baseline["fixtures"], strict=False):
        name = f"{entry['units']} units"
        print(
            f"{name:<14}{entry['tick_p95_ms']:>10.3f}{entry['navigation_p95_ms']:>10.3f}"
            f"{entry['position_tests_per_unit_tick']:>16.1f}"
            f"{entry['standability_cells_scanned_per_unit_tick']:>18.1f}"
        )
        skipped = {"units", "ticks", "digest"}
        if not args.check_timings:
            skipped |= set(TIMING_METRICS)
        gate = {key: value for key, value in budget.items() if key not in skipped}
        if args.check_digest:
            gate["digest"] = budget["digest"]
        failures += compare(name, entry, gate, args.tolerance)

    if failures:
        print("\ncheck-sim-budgets: FAILED", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1

    if args.check_timings:
        print("\ncheck-sim-budgets: amplification, digest and timing budgets held")
    else:
        print(
            "\ncheck-sim-budgets: amplification budgets held "
            "(timings advisory; pass --check-timings on a pinned machine)"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
