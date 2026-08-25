#!/usr/bin/env python3
"""Summarise a sequential RPG Arena gate run into one compact table.

The gate manifest (tools/arena/rpg_gate_manifest.json) is the authority for
which rpg_* scenarios are required, how many times each is repeated, and which
ones are pinned as expected-red while their owning gate is still open. This tool
reads the manifest and the per-scenario report.json files written by
``arena_app --batch`` and decides one verdict per scenario.

Three verdict axes are kept apart on purpose:

completion
    ``completed`` false means the run was cut short (watchdog, crash, missing
    report). That is never a behavioural result; it is reported as INCOMPLETE
    so a truncated run can never be mistaken for an ordinary failure with an
    empty issue list.

behaviour
    Every issue whose code is not a performance code. This is the part CI can
    enforce on any hardware.

performance
    frame_budget_exceeded and the performance_* codes, plus the p50/p95/max
    frame times. These are hardware-sensitive, so they are reported always and
    enforced only with --enforce-performance on named reference hardware.

Repeats exist because a verdict that changes between identical runs is not a
verdict. A scenario whose manifest entry asks for repeats is run that many times
and judged on all of them together: a required-green scenario that fails even
once is a failure, and a mixed result on any scenario is reported as FLAKY
unless the manifest already declares the entry "intermittent": true.

An entry may also declare "reproduction": "nondeterministic". That marks a real
defect the gate cannot provoke on demand, because something the scenario does not
control varies between identical runs. Such an entry never flips the gate red or
green on its own; it is reported as RED(unreproduced) when it happens to pass and
RED(known) when it happens to fail, and it is listed as owing a deterministic
reproduction. Gate 0 is not green while any entry still owes one.

Exit codes:
    0  every scenario matched its manifest expectation
    1  a scenario failed behaviourally against a required_green entry
    3  a run was incomplete, or its report was missing or unreadable
    4  an expected_red scenario passed every repeat, so the manifest is stale
    5  --enforce-performance was given and a frame budget was missed
    6  a scenario produced different verdicts across identical repeats and the
       manifest does not declare it intermittent
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_MANIFEST = REPO_ROOT / "tools" / "arena" / "rpg_gate_manifest.json"

PERFORMANCE_ISSUE_CODES = {"frame_budget_exceeded"}
PERFORMANCE_ISSUE_PREFIX = "performance_"


STRICT_P95_MS = 16.67
STRICT_P99_MS = 20.0
STRICT_MAX_MS = 33.3
STRICT_SIMULATION_P95_MS = 8.0

EXIT_OK = 0
EXIT_BEHAVIOUR = 1
EXIT_INCOMPLETE = 3
EXIT_STALE_MANIFEST = 4
EXIT_PERFORMANCE = 5
EXIT_UNDECLARED_FLAKE = 6


def is_performance_code(code: str) -> bool:
    return code in PERFORMANCE_ISSUE_CODES or code.startswith(PERFORMANCE_ISSUE_PREFIX)


def load_manifest(path: Path) -> dict:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def repeat_roots(artifact_root: Path, repeats: int) -> list[Path]:
    """Run 1 writes to the artifact root; later repeats get their own subroot."""
    roots = [artifact_root]
    for index in range(2, repeats + 1):
        roots.append(artifact_root / "repeats" / f"run-{index}")
    return roots


class RunResult:
    def __init__(self, root: Path, scenario_id: str):
        self.report_path = root / scenario_id / "report.json"
        self.timeout_path = root / scenario_id / "timeout.txt"
        self.timed_out = self.timeout_path.is_file()
        self.report: dict | None = None
        self.error = ""
        if not self.report_path.is_file():
            self.error = f"missing {self.report_path}"
        else:
            try:
                with self.report_path.open(encoding="utf-8") as handle:
                    self.report = json.load(handle)
            except (OSError, json.JSONDecodeError) as error:
                self.error = f"unreadable {self.report_path}: {error}"

        report = self.report or {}
        self.completed = bool(report.get("completed", False)) if self.report else False
        issues = list(report.get("issues", []))
        self.behaviour_issues = [
            issue
            for issue in issues
            if not is_performance_code(str(issue.get("code", "")))
        ]
        self.performance_issues = [
            issue for issue in issues if is_performance_code(str(issue.get("code", "")))
        ]
        self.performance = dict(report.get("performance", {}))
        self.behaviour_passed = self.completed and not self.behaviour_issues


class ScenarioOutcome:
    def __init__(self, entry: dict, artifact_root: Path, default_repeats: int):
        self.scenario_id = entry["id"]
        self.expected = entry.get("status", "required_green")
        self.gate = entry.get("gate", "")
        self.intermittent = bool(entry.get("intermittent", False))
        self.reproduction = entry.get("reproduction", "deterministic")
        self.repeats = int(entry.get("repeats", default_repeats))
        self.artifact_dir = artifact_root / self.scenario_id
        self.runs = [
            RunResult(root, self.scenario_id)
            for root in repeat_roots(artifact_root, self.repeats)
        ]

    @property
    def errors(self) -> list[str]:
        return [run.error for run in self.runs if run.error]

    @property
    def incomplete(self) -> bool:
        return any(run.report is None or not run.completed for run in self.runs)

    @property
    def timed_out(self) -> bool:
        return any(run.timed_out for run in self.runs)

    @property
    def passes(self) -> int:
        return sum(1 for run in self.runs if run.behaviour_passed)

    @property
    def behaviour_issues(self) -> list[dict]:
        collected = []
        for run in self.runs:
            collected.extend(run.behaviour_issues)
        return collected

    @property
    def performance_issues(self) -> list[dict]:
        collected = []
        for run in self.runs:
            collected.extend(run.performance_issues)
        return collected

    @property
    def verdict(self) -> str:
        if self.incomplete:
            return "TIMEOUT" if self.timed_out else "INCOMPLETE"
        total = len(self.runs)
        passed = self.passes
        if self.reproduction == "nondeterministic":
            return "RED(unreproduced)" if passed == total else "RED(known)"
        if passed == total:
            return "PASS" if self.expected == "required_green" else "FIXED"
        if passed == 0:
            return "FAIL" if self.expected == "required_green" else "RED(known)"
        if self.expected == "required_green":
            return "FLAKY-FAIL"
        return "RED(intermittent)" if self.intermittent else "FLAKY"

    @property
    def exit_code(self) -> int:
        verdict = self.verdict
        if verdict in ("INCOMPLETE", "TIMEOUT"):
            return EXIT_INCOMPLETE
        if verdict in ("FAIL", "FLAKY-FAIL"):
            return EXIT_BEHAVIOUR
        if verdict == "FIXED":
            return EXIT_STALE_MANIFEST
        if verdict == "FLAKY":
            return EXIT_UNDECLARED_FLAKE
        return EXIT_OK

    def issue_codes(self) -> str:
        codes: list[str] = []
        for run in self.runs:
            for issue in run.behaviour_issues + run.performance_issues:
                code = str(issue.get("code", "?"))
                if code not in codes:
                    codes.append(code)
        return ",".join(codes) if codes else "-"

    def strict_performance_failures(self) -> list[str]:
        """Gate 7.1's numeric contract, evaluated per repeat.

        Reported on every run so the numbers are visible; it only decides the
        exit status under --enforce-performance.
        """
        failures: list[str] = []
        for index, run in enumerate(self.runs):
            performance = run.performance
            if not performance:
                continue
            label = f"run {index + 1}/{len(self.runs)}"
            p95 = performance.get("p95_ms")
            p99 = performance.get("p99_ms")
            maximum = performance.get("max_ms")
            if p95 is not None and float(p95) > STRICT_P95_MS:
                failures.append(
                    f"{label} p95 {float(p95):.2f} ms over the {STRICT_P95_MS} ms budget"
                )
            if p99 is not None and float(p99) > STRICT_P99_MS:
                failures.append(
                    f"{label} p99 {float(p99):.2f} ms over the {STRICT_P99_MS} ms budget"
                )
            if maximum is not None and float(maximum) > STRICT_MAX_MS:
                failures.append(
                    f"{label} post-prewarm max {float(maximum):.2f} ms over the "
                    f"{STRICT_MAX_MS} ms hitch ceiling"
                )
            simulation = performance.get("simulation_p95_ms")
            if simulation is not None and float(simulation) > STRICT_SIMULATION_P95_MS:
                failures.append(
                    f"{label} simulation p95 {float(simulation):.2f} ms leaves under "
                    f"half the {STRICT_P95_MS} ms frame to presentation"
                )
            if performance.get("gpu_timed_frames", 0) == 0:
                failures.append(f"{label} reported no GPU timing")
            if "prewarm_frames" not in performance:
                failures.append(f"{label} did not mark its prewarm window")
        return failures

    def timing(self, key: str, aggregate=max) -> str:
        values = [
            float(run.performance[key])
            for run in self.runs
            if key in run.performance and run.performance[key] is not None
        ]
        if not values:
            return "-"
        return f"{aggregate(values):.2f}"


def render_table(rows: list[list[str]], headers: list[str]) -> str:
    widths = [len(header) for header in headers]
    for row in rows:
        for index, cell in enumerate(row):
            widths[index] = max(widths[index], len(cell))
    lines = ["  ".join(header.ljust(widths[i]) for i, header in enumerate(headers))]
    lines.append("  ".join("-" * width for width in widths))
    for row in rows:
        lines.append("  ".join(cell.ljust(widths[i]) for i, cell in enumerate(row)))
    return "\n".join(line.rstrip() for line in lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        prog="scripts/rpg_gate_report.py",
        description="Summarise an RPG Arena gate run and decide its exit status.",
    )
    parser.add_argument(
        "artifact_root",
        type=Path,
        help="Directory holding one subdirectory per scenario",
    )
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument(
        "--scenario",
        action="append",
        default=[],
        help="Restrict the summary to these scenario ids (repeatable).",
    )
    parser.add_argument(
        "--enforce-performance",
        action="store_true",
        help="Treat frame-budget issues as failures (reference hardware only).",
    )
    parser.add_argument(
        "--json",
        type=Path,
        default=None,
        help="Also write the machine-readable summary to this path.",
    )
    args = parser.parse_args()

    manifest = load_manifest(args.manifest)
    default_repeats = int(manifest.get("scenario_defaults", {}).get("repeats", 1))
    entries = manifest.get("scenarios", [])
    if args.scenario:
        wanted = set(args.scenario)
        entries = [entry for entry in entries if entry["id"] in wanted]
        missing = wanted - {entry["id"] for entry in entries}
        if missing:
            print(
                f"error: not in the gate manifest: {', '.join(sorted(missing))}",
                file=sys.stderr,
            )
            return EXIT_BEHAVIOUR

    artifact_root = args.artifact_root.resolve()
    outcomes = [
        ScenarioOutcome(entry, artifact_root, default_repeats) for entry in entries
    ]

    rows = []
    for outcome in outcomes:
        rows.append(
            [
                outcome.scenario_id,
                outcome.gate or "-",
                outcome.expected.replace("required_green", "green").replace(
                    "expected_red", "red"
                ),
                f"{outcome.passes}/{len(outcome.runs)}",
                "yes" if not outcome.incomplete else "NO",
                outcome.verdict,
                outcome.issue_codes(),
                outcome.timing("p50_ms"),
                outcome.timing("p95_ms"),
                outcome.timing("p99_ms"),
                outcome.timing("max_ms"),
                str(outcome.artifact_dir),
            ]
        )

    headers = [
        "scenario",
        "gate",
        "expect",
        "passed",
        "completed",
        "verdict",
        "issue codes",
        "p50",
        "p95",
        "p99",
        "max",
        "artifacts",
    ]
    print(render_table(rows, headers))
    print("\n(p50/p95/max are the worst repeat, in ms, and are reported not enforced")
    print(" unless --enforce-performance is given on named reference hardware.)\n")

    status = EXIT_OK
    for outcome in outcomes:
        code = outcome.exit_code
        if code != EXIT_OK and status == EXIT_OK:
            status = code
        for error in outcome.errors:
            print(f"  {outcome.scenario_id}: {error}")

    for outcome in outcomes:
        for message in outcome.strict_performance_failures():
            print(f"  performance contract: {outcome.scenario_id}: {message}")
            if args.enforce_performance and status == EXIT_OK:
                status = EXIT_PERFORMANCE
        for issue in outcome.performance_issues:
            label = (
                "performance"
                if args.enforce_performance
                else ("performance (reported, not enforced)")
            )
            print(
                f"  {label}: {outcome.scenario_id}: "
                f"{issue.get('code')}: {issue.get('message')}"
            )
        if (
            args.enforce_performance
            and outcome.performance_issues
            and status == EXIT_OK
        ):
            status = EXIT_PERFORMANCE

    stale = [o.scenario_id for o in outcomes if o.verdict == "FIXED"]
    if stale:
        print(
            "\nThese scenarios are pinned expected_red but passed every repeat. Fix "
            "the manifest in the same commit that fixed them:\n  " + "\n  ".join(stale)
        )

    undeclared = [o.scenario_id for o in outcomes if o.verdict == "FLAKY"]
    if undeclared:
        print(
            "\nThese scenarios gave different verdicts across identical repeats. "
            'Declare "intermittent": true in the manifest with what makes them '
            "vary, or make them deterministic:\n  " + "\n  ".join(undeclared)
        )

    flaky_required = [o.scenario_id for o in outcomes if o.verdict == "FLAKY-FAIL"]
    if flaky_required:
        print(
            "\nThese required-green scenarios failed at least one repeat. A gate "
            "that only sometimes passes is red:\n  " + "\n  ".join(flaky_required)
        )

    incomplete = [
        o.scenario_id for o in outcomes if o.verdict in ("INCOMPLETE", "TIMEOUT")
    ]
    if incomplete:
        print(
            "\nThese runs did not complete. A watchdog or crash exit is not a "
            "behavioural result:"
        )
        for outcome in outcomes:
            if outcome.verdict not in ("INCOMPLETE", "TIMEOUT"):
                continue
            for index, run in enumerate(outcome.runs, start=1):
                if run.report is not None and run.completed:
                    continue
                if run.timed_out:
                    reason = f"wall-clock watchdog ({run.timeout_path})"
                elif run.report is None:
                    reason = run.error or "no report"
                else:
                    reason = "report says completed=false"
                print(
                    f"  {outcome.scenario_id} run {index}/{len(outcome.runs)}: {reason}"
                )

    owes_repro = [
        o.scenario_id for o in outcomes if o.reproduction == "nondeterministic"
    ]
    if owes_repro:
        print(
            "\nThese entries are pinned red but the gate cannot provoke them on "
            "demand. Gate 0 is not green until each has a deterministic "
            "reproduction:\n  " + "\n  ".join(owes_repro)
        )

    green = sum(1 for o in outcomes if o.verdict == "PASS")
    timed_out = sum(1 for o in outcomes if o.verdict == "TIMEOUT")
    known_red = sum(
        1
        for o in outcomes
        if o.verdict in ("RED(known)", "RED(intermittent)", "RED(unreproduced)")
    )
    failed = sum(1 for o in outcomes if o.verdict in ("FAIL", "FLAKY-FAIL"))
    print(
        f"\n{green} green, {known_red} known-red, {failed} unexpected failures, "
        f"{len(undeclared)} undeclared flakes, "
        f"{len(incomplete)} incomplete ({timed_out} timed out), "
        f"out of {len(outcomes)} scenarios."
    )

    if args.json is not None:
        summary = {
            "artifact_root": str(artifact_root),
            "exit_code": status,
            "counts": {
                "green": green,
                "known_red": known_red,
                "failed": failed,
                "undeclared_flakes": len(undeclared),
                "owes_deterministic_reproduction": owes_repro,
                "incomplete": len(incomplete),
                "total": len(outcomes),
            },
            "scenarios": [
                {
                    "id": o.scenario_id,
                    "gate": o.gate,
                    "expected": o.expected,
                    "intermittent": o.intermittent,
                    "reproduction": o.reproduction,
                    "repeats": len(o.runs),
                    "passed_repeats": o.passes,
                    "completed": not o.incomplete,
                    "verdict": o.verdict,
                    "behaviour_issues": o.behaviour_issues,
                    "performance_issues": o.performance_issues,
                    "performance": [run.performance for run in o.runs],
                    "artifact_dir": str(o.artifact_dir),
                }
                for o in outcomes
            ],
        }
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")

    return status


if __name__ == "__main__":
    sys.exit(main())
