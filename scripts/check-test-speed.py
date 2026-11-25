#!/usr/bin/env python3
"""Keeps the fast pull-request test profile fast.

The pull-request lane runs every test binary but subtracts the patterns in
`tests/extended_tests.txt` -- the headless battles and the sweeps over every
shipped asset. That split is only worth anything if it stays true, and it can
stop being true two ways:

  * a new test simulates a battle without anyone noticing, and the fast lane
    quietly grows another minute; or
  * a fixture is renamed and a pattern in the manifest stops matching, so a
    gate that reads as "runs weekly" runs nowhere at all.

This script catches both. It reads the GoogleTest JSON reports the fast run
writes (SOI_TEST_REPORT_DIR) and fails if any test that ran exceeded the
per-test budget, and it lists the tests in each binary to confirm every
manifest pattern still names something.

usage:
  scripts/check-test-speed.py --report-dir artifacts/test-reports [--build-dir build]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_MANIFEST = REPO_ROOT / "tests" / "extended_tests.txt"

DEFAULT_BUDGET_MS = 10000
"""A ceiling for one test on a shared CI runner, not a target.

With the manifest applied the slowest surviving test measures about two seconds
in a Debug build on a quiet machine. A shared runner is slower and its neighbour
is not this project's business, so the ceiling sits several times above that: it
is here to catch the class of test that took this lane from minutes to ninety
minutes, every one of which costs tens of seconds, not to argue over a test that
grew from one second to three.
"""

DEFAULT_TOTAL_BUDGET_MS = 900000
"""A ceiling for the sum of every fast-profile test's own time.

The sum is the same however the suites are scheduled, so it measures what the
tests cost and not how well they were overlapped. It was about 400 s on a
pull-request runner when this was written; the ceiling leaves room for a slow
neighbour and trips when the profile has roughly doubled, which a per-test
budget never would: a hundred tests of nine seconds each pass it.
"""

SUITES = (
    "simulation_tests",
    "combat_balance_tests",
    "ai_tests",
    "campaign_tests",
    "persistence_tests",
    "render_tests",
    "app_tests",
    "arena_tests",
    "tools_tests",
)
"""Kept in step with the suites array in scripts/run-tests.sh."""


def read_manifest(path: Path) -> list[str]:
    """The manifest's filter patterns, comments and blank lines removed."""
    patterns: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.split("#", 1)[0].strip()
        if stripped:
            patterns.append(stripped)
    return patterns


def pattern_to_regex(pattern: str) -> re.Pattern[str]:
    """A GoogleTest filter pattern as a regex.

    GoogleTest gives `*` and `?` their glob meanings and treats every other
    character literally -- `[` included, which is why this does not use
    fnmatch.
    """
    parts = []
    for char in pattern:
        if char == "*":
            parts.append(".*")
        elif char == "?":
            parts.append(".")
        else:
            parts.append(re.escape(char))
    return re.compile("".join(parts) + r"\Z")


def list_tests(binary: Path) -> list[str]:
    """Every `Suite.Test` name in a GoogleTest binary.

    `--gtest_list_tests` prints a suite header unindented and ending in a dot,
    then its tests indented beneath it. A value-parameterized test carries a
    `# GetParam() = ...` trailer that is not part of its name.
    """
    environment = dict(os.environ)
    environment.setdefault("QT_QPA_PLATFORM", "offscreen")
    completed = subprocess.run(
        [str(binary), "--gtest_list_tests"],
        cwd=REPO_ROOT,
        env=environment,
        capture_output=True,
        text=True,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"{binary} --gtest_list_tests exited {completed.returncode}:\n"
            f"{completed.stderr.strip()}"
        )

    names: list[str] = []
    suite = ""
    for raw in completed.stdout.splitlines():
        if not raw.strip() or raw.lstrip().startswith("Running main("):
            continue
        if not raw[0].isspace():
            suite = raw.strip()
            continue
        if not suite:
            continue
        test = raw.split("#", 1)[0].strip()
        if test:
            names.append(suite + test)
    return names


def parse_seconds(value: str | float | int) -> float:
    """GoogleTest reports a duration as a string like "1.234s"."""
    if isinstance(value, (int, float)):
        return float(value)
    return float(str(value).rstrip("s") or 0.0)


def timings_from_report(path: Path) -> list[tuple[str, float]]:
    """Every `(name, milliseconds)` pair in one GoogleTest JSON report."""
    document = json.loads(path.read_text(encoding="utf-8"))
    timings: list[tuple[str, float]] = []
    for suite in document.get("testsuites", []):
        suite_name = suite.get("name", "")
        for case in suite.get("testsuite", []):
            name = f"{suite_name}.{case.get('name', '')}"
            timings.append((name, parse_seconds(case.get("time", 0)) * 1000.0))
    return timings


def check_budget(
    report_dir: Path, budget_ms: int, total_budget_ms: int
) -> tuple[list[str], list[str], list[str], str | None]:
    """Overspending tests, the slowest few, per-suite totals, and a total overrun.

    The last item is a message when the summed test time is over
    `total_budget_ms`, otherwise None.
    """
    reports = sorted(report_dir.glob("*.json"))
    if not reports:
        raise RuntimeError(
            f"no GoogleTest JSON reports in {report_dir}. The test step has to "
            "run with SOI_TEST_REPORT_DIR set to this directory."
        )

    measured: list[tuple[float, str, str]] = []
    for report in reports:
        for name, milliseconds in timings_from_report(report):
            measured.append((milliseconds, name, report.stem))

    measured.sort(reverse=True)
    over = [
        f"{name} ({milliseconds:.0f} ms in {suite})"
        for milliseconds, name, suite in measured
        if milliseconds > budget_ms
    ]
    slowest = [
        f"{milliseconds:7.0f} ms  {name}" for milliseconds, name, _ in measured[:10]
    ]

    per_suite: dict[str, float] = {}
    for milliseconds, _, suite in measured:
        per_suite[suite] = per_suite.get(suite, 0.0) + milliseconds
    suite_totals = [
        f"{milliseconds / 1000.0:7.1f} s  {suite}"
        for suite, milliseconds in sorted(
            per_suite.items(), key=lambda item: item[1], reverse=True
        )
    ]

    total = sum(per_suite.values())
    total_over = None
    if total > total_budget_ms:
        total_over = (
            f"the fast profile's tests add up to {total / 1000.0:.0f} s, over the "
            f"{total_budget_ms / 1000.0:.0f} s aggregate budget. Per suite:\n  "
            + "\n  ".join(suite_totals)
        )
    return over, slowest, suite_totals + [f"{total / 1000.0:7.1f} s  total"], total_over


def check_manifest(manifest: Path, build_dir: Path) -> list[str]:
    """Manifest patterns that no longer name a test in any binary."""
    patterns = read_manifest(manifest)
    if not patterns:
        raise RuntimeError(f"{manifest} lists no patterns.")

    known: set[str] = set()
    missing_binaries: list[str] = []
    binaries: list[Path] = []
    for suite in SUITES:
        binary = build_dir / "bin" / suite
        if not binary.exists():
            binary = build_dir / "bin" / f"{suite}.exe"
        if not binary.exists():
            missing_binaries.append(suite)
            continue
        binaries.append(binary)

    if missing_binaries:
        raise RuntimeError(
            "cannot validate the manifest without every test binary; missing: "
            + ", ".join(missing_binaries)
        )

    # Listing starts each binary, which costs seconds apiece; they are
    # independent, so they are listed together.
    with ThreadPoolExecutor(max_workers=4) as pool:
        for names in pool.map(list_tests, binaries):
            known.update(names)

    stale: list[str] = []
    for pattern in patterns:
        matcher = pattern_to_regex(pattern)
        if not any(matcher.match(name) for name in known):
            stale.append(pattern)
    return stale


def self_test() -> int:
    """Compiler-free check of the two pieces with any subtlety in them."""
    failures: list[str] = []

    cases = [
        ("AiDuelMatchTest.*", "AiDuelMatchTest.ScipioAndFabius", True),
        ("AiDuelMatchTest.*", "AiDuelMatchTestX.Scipio", False),
        ("HillNavigationTest.Routes", "HillNavigationTest.Routes", True),
        ("HillNavigationTest.Routes", "HillNavigationTest.RoutesMore", False),
        ("CaptureScenarios/Move.*", "CaptureScenarios/Move.Gate/0", True),
        ("A.B?", "A.BC", True),
        ("A.B?", "A.BCD", False),
    ]
    for pattern, name, expected in cases:
        if bool(pattern_to_regex(pattern).match(name)) != expected:
            failures.append(f"pattern {pattern!r} vs {name!r} should be {expected}")

    if abs(parse_seconds("1.234s") - 1.234) > 1e-9:
        failures.append("parse_seconds does not strip the trailing 's'")
    if abs(parse_seconds(2) - 2.0) > 1e-9:
        failures.append("parse_seconds does not accept a number")

    patterns = read_manifest(DEFAULT_MANIFEST)
    if not patterns:
        failures.append(f"{DEFAULT_MANIFEST} parsed to nothing")
    if any("#" in pattern or " " in pattern for pattern in patterns):
        failures.append("manifest parsing left a comment or a space in a pattern")

    import tempfile

    with tempfile.TemporaryDirectory() as scratch:
        report = Path(scratch) / "demo_tests.json"
        report.write_text(
            json.dumps(
                {
                    "testsuites": [
                        {
                            "name": "S",
                            "testsuite": [
                                {"name": "A", "time": "2.000s"},
                                {"name": "B", "time": "3.000s"},
                            ],
                        }
                    ]
                }
            ),
            encoding="utf-8",
        )
        over, _, totals, total_over = check_budget(Path(scratch), 2500, 10000)
        if over != ["S.B (3000 ms in demo_tests)"] or total_over is not None:
            failures.append(f"check_budget misjudged a 5 s total: {over} {total_over}")
        if not totals[-1].strip().startswith("5.0 s"):
            failures.append(f"check_budget summed the suite wrongly: {totals}")
        _, _, _, total_over = check_budget(Path(scratch), 2500, 4000)
        if total_over is None:
            failures.append("check_budget missed a total over the aggregate budget")

    for failure in failures:
        print(f"self-test: {failure}", file=sys.stderr)
    print(f"self-test: {'failed' if failures else 'passed'}")
    return 1 if failures else 0


def display_path(path: Path) -> str:
    """A path as the reader typed it, repo-relative when it is inside the repo."""
    try:
        return str(path.resolve().relative_to(REPO_ROOT))
    except ValueError:
        return str(path)


def summarize(lines: list[str]) -> None:
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if not summary:
        return
    with open(summary, "a", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--report-dir",
        type=Path,
        help="directory of GoogleTest JSON reports written by run-tests.sh",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=REPO_ROOT / "build",
        help="build directory holding bin/<suite> (default: build)",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=DEFAULT_MANIFEST,
        help="the extended-test manifest to validate",
    )
    parser.add_argument(
        "--budget-ms",
        type=int,
        default=DEFAULT_BUDGET_MS,
        help=f"per-test ceiling in the fast profile (default: {DEFAULT_BUDGET_MS})",
    )
    parser.add_argument(
        "--total-budget-ms",
        type=int,
        default=DEFAULT_TOTAL_BUDGET_MS,
        help=(
            "ceiling for the summed time of every test in the fast profile "
            f"(default: {DEFAULT_TOTAL_BUDGET_MS})"
        ),
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="check this script's own pattern matching and parsing, then exit",
    )
    arguments = parser.parse_args()

    if arguments.self_test:
        return self_test()

    problems: list[str] = []
    summary_lines = ["## Fast test profile"]

    try:
        stale = check_manifest(arguments.manifest, arguments.build_dir)
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    if stale:
        problems.append(
            f"these patterns in {display_path(arguments.manifest)} match no "
            "test any more, so the checks they name run nowhere. Fix the "
            "pattern or delete the line:\n  " + "\n  ".join(stale)
        )

    if arguments.report_dir is not None:
        try:
            over, slowest, suite_totals, total_over = check_budget(
                arguments.report_dir, arguments.budget_ms, arguments.total_budget_ms
            )
        except RuntimeError as error:
            print(f"error: {error}", file=sys.stderr)
            return 1
        if over:
            problems.append(
                f"these tests ran longer than the {arguments.budget_ms} ms "
                "per-test budget for the pull-request profile. Make the test "
                f"cheaper, or add it to {display_path(arguments.manifest)} "
                "with the reason it belongs to the extended profile:\n  "
                + "\n  ".join(over)
            )
        if total_over:
            problems.append(total_over)
        summary_lines += ["", "Slowest tests in the fast profile:", "", "```"]
        summary_lines += slowest
        summary_lines.append("```")
        summary_lines += ["", "Summed test time per suite:", "", "```"]
        summary_lines += suite_totals
        summary_lines.append("```")

    if problems:
        summary_lines += [""] + [f"- {problem.splitlines()[0]}" for problem in problems]
        summarize(summary_lines)
        for problem in problems:
            print(f"error: {problem}", file=sys.stderr)
        return 1

    summarize(summary_lines)
    print("fast test profile: within budget, manifest current")
    return 0


if __name__ == "__main__":
    sys.exit(main())
