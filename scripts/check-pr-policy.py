#!/usr/bin/env python3
"""Run compiler-free pull-request source-policy checks with useful diagnostics.

The same command is used locally and by GitHub Actions:

    python3 scripts/check-pr-policy.py

A pull request is responsible for regressions it introduces, not policy debt
already present on its target branch. Each failing head check is therefore run
against the resolved base ref too: an identical base failure is reported as
inherited debt and does not block the PR; a new or changed failure still fails.
Release/tag runs remain strict because they have no pull-request base ref.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Check:
    key: str
    title: str
    script: str
    args: tuple[str, ...] = (".",)
    category: str = "architecture"
    budget_file: str | None = None


CHECKS: tuple[Check, ...] = (
    Check("layering", "Layer direction", "scripts/check-layering.py"),
    Check("modules", "Game module boundaries", "scripts/check-modules.py"),
    Check("render-boundary", "Render boundary", "scripts/check-render-boundary.py"),
    Check(
        "command-boundary",
        "Client command boundary",
        "scripts/check-command-boundary.py",
    ),
    Check(
        "architecture-doc",
        "Architecture document contract",
        "scripts/check-architecture-doc.py",
    ),
    Check("frame-lock", "QML frame-lock boundary", "scripts/check-frame-lock.py", ()),
    Check(
        "ambient-instances",
        "Ambient-session lookup ratchet",
        "scripts/check-ambient-instances.py",
        category="migration",
        budget_file="scripts/ambient_instance_budget.json",
    ),
    Check(
        "world-scans",
        "Full-world scan ratchet",
        "scripts/check-world-scans.py",
        category="migration",
        budget_file="scripts/world_scan_budget.json",
    ),
    Check(
        "entity-access",
        "Entity-object access ratchet",
        "scripts/check-entity-access.py",
        category="migration",
        budget_file="scripts/entity_access_budget.json",
    ),
)


@dataclass
class Result:
    check: Check
    returncode: int
    output: str
    inherited: bool = False

    @property
    def raw_ok(self) -> bool:
        return self.returncode == 0

    @property
    def ok(self) -> bool:
        return self.raw_ok or self.inherited


def run(command: list[str], root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=root,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def git(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return run(["git", *args], root)


def ref_exists(root: Path, ref: str) -> bool:
    return (
        git(root, "rev-parse", "--verify", "--quiet", f"{ref}^{{commit}}").returncode
        == 0
    )


def resolve_base(root: Path, explicit: str | None) -> tuple[str | None, str]:
    candidates: list[tuple[str, str]] = []
    if explicit:
        candidates.append((explicit, "--base-ref"))
        if "/" not in explicit:
            candidates.append((f"origin/{explicit}", "--base-ref remote fallback"))
    else:
        github_base = os.environ.get("GITHUB_BASE_REF", "").strip()
        if github_base:
            candidates.extend(
                [
                    (f"origin/{github_base}", "GITHUB_BASE_REF"),
                    (github_base, "GITHUB_BASE_REF local fallback"),
                    ("HEAD^1", "pull-request merge parent fallback"),
                ]
            )
        elif os.environ.get("GITHUB_ACTIONS") != "true":
            candidates.extend(
                [
                    ("origin/main", "origin/main fallback"),
                    ("main", "main fallback"),
                ]
            )

    seen: set[str] = set()
    for ref, source in candidates:
        if ref in seen:
            continue
        seen.add(ref)
        if not ref_exists(root, ref):
            continue
        sha = git(root, "rev-parse", ref).stdout.strip()
        return ref, f"{source}: {ref} ({sha[:12]})"
    return None, "no target ref resolved; tracked budgets still enforce pass/fail"


def read_budget(path: Path) -> dict[str, int]:
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
        return {str(key): int(value) for key, value in raw.items()}
    except (OSError, ValueError, TypeError):
        return {}


def read_budget_at_ref(root: Path, ref: str, path: str) -> dict[str, int]:
    proc = git(root, "show", f"{ref}:{path}")
    if proc.returncode != 0:
        return {}
    try:
        raw = json.loads(proc.stdout)
        return {str(key): int(value) for key, value in raw.items()}
    except (ValueError, TypeError):
        return {}


def budget_diagnostics(root: Path, check: Check, base_ref: str | None) -> list[str]:
    if check.budget_file is None or base_ref is None:
        return []
    current = read_budget(root / check.budget_file)
    baseline = read_budget_at_ref(root, base_ref, check.budget_file)
    if not baseline:
        return [f"target budget unavailable at {base_ref}:{check.budget_file}"]

    changed: list[str] = []
    for bucket in sorted(set(current) | set(baseline)):
        now = current.get(bucket)
        before = baseline.get(bucket)
        if now != before:
            changed.append(f"{bucket}: target {before!s} -> PR {now!s}")
    if not changed:
        return [f"target budget: {base_ref}:{check.budget_file} (unchanged)"]
    return [f"target budget: {base_ref}:{check.budget_file}", *changed]


def run_check(root: Path, check: Check) -> Result:
    command = [sys.executable, check.script, *check.args]
    proc = run(command, root)
    return Result(check, proc.returncode, proc.stdout.rstrip())


def normalize_output(output: str, roots: tuple[Path, ...]) -> str:
    normalized = output.replace("\\", "/").strip()
    for root in roots:
        normalized = normalized.replace(str(root).replace("\\", "/"), "<repo>")
    return normalized


def materialize_base(
    root: Path, base_ref: str | None
) -> tuple[tempfile.TemporaryDirectory[str] | None, Path | None]:
    if base_ref is None:
        return None, None

    temporary = tempfile.TemporaryDirectory(prefix="soi-policy-base-")
    base_root = Path(temporary.name) / "repo"
    proc = git(root, "worktree", "add", "--detach", str(base_root), base_ref)
    if proc.returncode != 0:
        print("warning: could not materialize target branch for regression comparison")
        if proc.stdout:
            print(proc.stdout.rstrip())
        temporary.cleanup()
        return None, None
    return temporary, base_root


def classify_against_base(
    head: Result,
    head_root: Path,
    base_root: Path | None,
) -> Result:
    if head.raw_ok or base_root is None:
        return head

    base = run_check(base_root, head.check)
    if base.raw_ok:
        return head

    head_output = normalize_output(head.output, (head_root, base_root))
    base_output = normalize_output(base.output, (head_root, base_root))
    if head_output == base_output:
        head.inherited = True
    return head


def markdown_escape(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", " ")


def result_label(result: Result) -> str:
    if result.inherited:
        return "⚠️ inherited target debt"
    return "✅ pass" if result.raw_ok else "❌ regression"


def append_summary(
    path: Path,
    results: list[Result],
    baseline: str,
    diagnostics: dict[str, list[str]],
) -> None:
    lines = [
        "## PR source policy",
        "",
        f"Comparison baseline: `{markdown_escape(baseline)}`",
        "",
        "A PR blocks only on policy regressions it introduces. Identical failures",
        "already present on the target branch are reported as inherited debt.",
        "",
        "| Gate | Category | Result |",
        "| --- | --- | --- |",
    ]
    for result in results:
        lines.append(
            f"| {markdown_escape(result.check.title)} | {result.check.category} | "
            f"{result_label(result)} |"
        )

    failures = [result for result in results if not result.ok]
    inherited = [result for result in results if result.inherited]
    if failures:
        first = failures[0]
        lines.extend(
            [
                "",
                "### First actionable regression",
                "",
                f"**{first.check.title}**",
                "",
                "```text",
                (first.output or "check exited non-zero without output")[:8000].replace(
                    "```", "` ` `"
                ),
                "```",
            ]
        )
    else:
        lines.extend(["", "No source-policy regression was introduced by this PR."])

    if inherited:
        lines.extend(["", "### Inherited target debt", ""])
        for result in inherited:
            lines.append(f"**{result.check.title}**")
            lines.append("")
            lines.append("```text")
            lines.append(
                (result.output or "target and PR both fail without output")[
                    :4000
                ].replace("```", "` ` `")
            )
            lines.append("```")
            lines.append("")

    notes = [
        (result.check.title, diagnostics.get(result.check.key, []))
        for result in results
        if diagnostics.get(result.check.key)
    ]
    if notes:
        lines.extend(["", "### Ratchet baseline diagnostics", ""])
        for title, items in notes:
            lines.append(f"**{title}**")
            lines.extend(f"- {markdown_escape(item)}" for item in items)
            lines.append("")

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as handle:
        handle.write("\n".join(lines).rstrip() + "\n")


def run_all(root: Path, base: str | None, summary: Path | None) -> int:
    base_ref, baseline = resolve_base(root, base)
    print(f"PR policy comparison baseline: {baseline}")
    temporary, base_root = materialize_base(root, base_ref)

    results: list[Result] = []
    diagnostics: dict[str, list[str]] = {}
    try:
        for check in CHECKS:
            print(f"\n== {check.title} [{check.category}] ==")
            diagnostics[check.key] = budget_diagnostics(root, check, base_ref)
            for note in diagnostics[check.key]:
                print(f"baseline: {note}")

            result = classify_against_base(run_check(root, check), root, base_root)
            results.append(result)
            if result.output:
                print(result.output)
            if result.inherited:
                print("INHERITED TARGET FAILURE (non-blocking)")
            else:
                print("PASS" if result.raw_ok else "FAIL: NEW/CHANGED REGRESSION")
    finally:
        if base_root is not None:
            git(root, "worktree", "remove", "--force", str(base_root))
        if temporary is not None:
            temporary.cleanup()

    if summary is not None:
        append_summary(summary, results, baseline, diagnostics)

    print("\n== PR source policy summary ==")
    for result in results:
        if result.inherited:
            state = "DEBT"
        else:
            state = "PASS" if result.raw_ok else "FAIL"
        print(f"{state:4}  {result.check.title}")

    failures = [result for result in results if not result.ok]
    if failures:
        print(f"\nFirst actionable regression: {failures[0].check.title}")
        return 1

    inherited_count = sum(result.inherited for result in results)
    if inherited_count:
        print(
            f"\nNo new source-policy regression; {inherited_count} target-branch "
            "failure(s) remain as inherited debt."
        )
    else:
        print("\nAll PR source-policy gates passed.")
    return 0


def self_test() -> int:
    keys = [check.key for check in CHECKS]
    scripts = [check.script for check in CHECKS]
    if len(keys) != len(set(keys)) or len(scripts) != len(set(scripts)):
        print("self-test: duplicate policy gate", file=sys.stderr)
        return 1

    root = Path(__file__).resolve().parents[1]
    missing = [check.script for check in CHECKS if not (root / check.script).is_file()]
    if missing:
        print(
            f"self-test: missing configured script(s): {', '.join(missing)}",
            file=sys.stderr,
        )
        return 1

    check = Check("fixture", "Fixture", "fixture.py", ())
    inherited = Result(check, 1, "same failure")
    baseline = Result(check, 1, "same failure")
    if normalize_output(inherited.output, (root,)) != normalize_output(
        baseline.output, (root,)
    ):
        print("self-test: output normalization failed", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory() as tmp:
        fixture = Path(tmp)
        passing = fixture / "pass.py"
        failing = fixture / "fail.py"
        passing.write_text("print('fixture pass')\n", encoding="utf-8")
        failing.write_text(
            "import sys\nprint('fixture failure at exact/path.cpp:7')\nsys.exit(1)\n",
            encoding="utf-8",
        )
        pass_result = run([sys.executable, str(passing)], fixture)
        fail_result = run([sys.executable, str(failing)], fixture)
        if pass_result.returncode != 0 or fail_result.returncode == 0:
            print("self-test: subprocess result handling failed", file=sys.stderr)
            return 1
        if "exact/path.cpp:7" not in fail_result.stdout:
            print("self-test: failure output was not preserved", file=sys.stderr)
            return 1

    print(f"check-pr-policy: self-test ok ({len(CHECKS)} configured gates)")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-ref", help="target branch/ref for baseline diagnostics")
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root",
    )
    parser.add_argument(
        "--summary-file",
        type=Path,
        help="append Markdown summary here (defaults to GITHUB_STEP_SUMMARY)",
    )
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.self_test:
        return self_test()

    summary = args.summary_file
    if summary is None and os.environ.get("GITHUB_STEP_SUMMARY"):
        summary = Path(os.environ["GITHUB_STEP_SUMMARY"])
    return run_all(args.root.resolve(), args.base_ref, summary)


if __name__ == "__main__":
    raise SystemExit(main())
