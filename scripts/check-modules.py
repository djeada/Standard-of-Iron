#!/usr/bin/env python3
"""Fail the build when a module inside game/ points the wrong way.

Seven of the modules in scripts/module_rules.json are their own CMake target, so
a bad edge there is already a link error. The rest still share libgame_sim.a,
where the linker cannot tell them apart -- combat, movement, navigation,
formations, economy, units, world and wildlife all live in one archive, so
nothing today stops a terrain file from calling the damage pipeline.

This is what stops it. Each module declares which modules it may use; every
quoted include that resolves into game/ is checked against that. Edges that
already point the wrong way are recorded per module pair in the "baseline" map,
and the count may only go down. Getting a module's incoming baseline to zero is
what makes it extractable into its own target -- that is how soi_ai and
soi_persistence left the kernel.

  usage: check-modules.py [repo-root] [--update-baseline]

--update-baseline rewrites the counts in place. Use it when you have paid debt
down, and read the diff: a number that went up is a regression you are about to
bless.
"""

from __future__ import annotations

import json
import re
import sys
from collections import defaultdict
from pathlib import Path

INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')
RULES_FILE = Path(__file__).with_name("module_rules.json")
SOURCE_SUFFIXES = (".h", ".cpp")


def load_rules() -> dict:
    with RULES_FILE.open(encoding="utf-8") as handle:
        return json.load(handle)


class Ambiguous(Exception):
    """Two modules claim the same file just as specifically."""


def classify(relative: str, modules: dict) -> str | None:
    """Return the module a repo-relative path belongs to.

    Longest match wins, so game/map/minimap/ lands in render_bridge rather than
    world even though world claims game/map/. An explicit file entry beats a
    path entry at equal length, which is how the mission and campaign loaders
    are carved out of game/map/.

    A tie is an error rather than a coin flip: if two modules name the same file
    outright, which of them owns it would otherwise depend on dictionary order,
    and the answer decides whether an edge counts as a violation.
    """
    best_module: str | None = None
    best_score = -1
    tied: list[str] = []

    def offer(name: str, score: int) -> None:
        nonlocal best_module, best_score, tied
        if score > best_score:
            best_module, best_score, tied = name, score, [name]
        elif score == best_score and name not in tied:
            tied.append(name)

    for name, spec in modules.items():
        if any(relative.startswith(p) for p in spec.get("excluded_paths", ())):
            continue
        if relative in spec.get("excluded_files", ()):
            continue
        for prefix in spec.get("paths", ()):
            if relative.startswith(prefix):
                offer(name, len(prefix))
        for stem in spec.get("files", ()):
            for suffix in SOURCE_SUFFIXES:
                if relative == stem + suffix:
                    offer(name, len(relative) + 1)

    if len(tied) > 1:
        raise Ambiguous(f"{relative} is claimed by {', '.join(sorted(tied))}")
    return best_module


def resolve(include: str, source: Path, root: Path) -> Path | None:
    for candidate in (root / include, root / "game" / include, source.parent / include):
        if candidate.is_file():
            return candidate.resolve()
    return None


def measure(root: Path, modules: dict) -> tuple[dict[str, int], dict[str, list[str]]]:
    counts: dict[str, int] = defaultdict(int)
    examples: dict[str, list[str]] = defaultdict(list)

    for source in sorted((root / "game").rglob("*")):
        if source.suffix not in SOURCE_SUFFIXES or not source.is_file():
            continue
        relative = source.relative_to(root).as_posix()
        origin = classify(relative, modules)
        if origin is None:
            continue
        allowed = set(modules[origin].get("may_use", ()))
        try:
            text = source.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for number, line in enumerate(text.splitlines(), start=1):
            match = INCLUDE.match(line)
            if not match:
                continue
            target = resolve(match.group(1), source, root)
            if target is None:
                continue
            try:
                target_relative = target.relative_to(root).as_posix()
            except ValueError:
                continue
            destination = classify(target_relative, modules)
            if destination is None or destination == origin or destination in allowed:
                continue
            pair = f"{origin} -> {destination}"
            counts[pair] += 1
            examples[pair].append(f"{relative}:{number} includes {target_relative}")

    return dict(counts), dict(examples)


def audit_ownership(root: Path, modules: dict) -> tuple[list[str], list[str]]:
    """Files under game/ that no module claims, and files two modules claim.

    An unclaimed file is invisible to this check, so a new directory must be
    given a module rather than silently escaping the rules. A doubly claimed one
    is worse: it would be checked against whichever module happened to win.
    """
    orphans: list[str] = []
    conflicts: list[str] = []
    for source in sorted((root / "game").rglob("*")):
        if source.suffix not in SOURCE_SUFFIXES or not source.is_file():
            continue
        relative = source.relative_to(root).as_posix()
        try:
            if classify(relative, modules) is None:
                orphans.append(relative)
        except Ambiguous as error:
            conflicts.append(str(error))
    return orphans, conflicts


def report(counts: dict[str, int], baseline: dict[str, int], examples: dict) -> int:
    status = 0
    regressions = []
    for pair, count in sorted(counts.items()):
        allowed = baseline.get(pair, 0)
        if count > allowed:
            regressions.append((pair, count, allowed))
    if regressions:
        status = 1
        print("Module boundary regression(s):", file=sys.stderr)
        for pair, count, allowed in regressions:
            print(
                f"  {pair}: {count} edges, baseline allows {allowed}", file=sys.stderr
            )
            for example in examples[pair][: count - allowed + 2]:
                print(f"      {example}", file=sys.stderr)
        print(
            "\nA module may only include the modules listed in its may_use in\n"
            "scripts/module_rules.json. Invert the call, move the shared type down\n"
            "into components, or -- if the new edge is genuinely part of the design --\n"
            "change may_use and say why in the review.",
            file=sys.stderr,
        )

    stale = []
    for pair, allowed in sorted(baseline.items()):
        actual = counts.get(pair, 0)
        if actual < allowed:
            stale.append((pair, actual, allowed))
    if stale:
        status = 1
        print("\nBaseline is now too generous:", file=sys.stderr)
        for pair, actual, allowed in stale:
            print(
                f"  {pair}: {actual} edges left, baseline still says {allowed}",
                file=sys.stderr,
            )
        print(
            "\nRun scripts/check-modules.py --update-baseline to record the progress.\n"
            "The count is a ratchet: leaving it high lets the edge come back for free.",
            file=sys.stderr,
        )
    return status


def write_baseline(baseline: dict[str, int]) -> None:
    """Rewrite only the "baseline" object, leaving the rest of the file alone.

    Re-serialising the whole document with json.dump would expand every short
    array onto three lines, which prettier then collapses again -- so
    `--update-baseline` would leave the repository unformatted every time. The
    baseline is a flat map of long keys, which prettier always renders one entry
    per line, so emitting it directly round-trips cleanly.
    """
    text = RULES_FILE.read_text(encoding="utf-8")
    marker = '  "baseline": {'
    start = text.index(marker)
    end = text.index("\n  }", start) + len("\n  }")
    if baseline:
        body = ",\n".join(
            f"    {json.dumps(pair)}: {count}" for pair, count in baseline.items()
        )
        block = marker + "\n" + body + "\n  }"
    else:
        block = '  "baseline": {}'
    RULES_FILE.write_text(text[:start] + block + text[end:], encoding="utf-8")


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    update = "--update-baseline" in sys.argv[1:]
    root = Path(args[0]).resolve() if args else Path.cwd()

    rules = load_rules()
    modules = rules["modules"]

    orphans, conflicts = audit_ownership(root, modules)
    if conflicts:
        print(
            "Files claimed by more than one module in module_rules.json:",
            file=sys.stderr,
        )
        for conflict in conflicts:
            print(f"  {conflict}", file=sys.stderr)
        print("\nGive each file exactly one owner.", file=sys.stderr)
        return 1
    if orphans:
        print(
            "Files under game/ that no module in module_rules.json claims:",
            file=sys.stderr,
        )
        for orphan in orphans:
            print(f"  {orphan}", file=sys.stderr)
        print(
            "\nAdd them to a module. A file no module owns is a file this check "
            "cannot see.",
            file=sys.stderr,
        )
        return 1

    counts, examples = measure(root, modules)

    if update:
        write_baseline(dict(sorted(counts.items())))
        total = sum(counts.values())
        print(f"Baseline updated: {len(counts)} module pairs, {total} edges.")
        return 0

    return report(counts, rules.get("baseline", {}), examples)


if __name__ == "__main__":
    raise SystemExit(main())
