#!/usr/bin/env python3
"""Fail the build when a module inside game/ points the wrong way.

Every module in scripts/module_rules.json is backed by a CMake target, so a
symbol-level edge in the wrong direction is already a link error. This check
is the header-level counterpart, and it is finer than the linker in two ways:
it sees an include that only pulls in inline code, and it separates the modules
that still share one archive (unit_catalog / registries / world in soi_world;
simulation / session in game_sim).

Each module declares which modules it may use; every quoted include that
resolves into game/ is checked against that. There is no tolerated backlog:
the first wrong-way edge fails the build and names the file.

  usage: check-modules.py [repo-root]
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


LIBRARY = re.compile(r"add_library\(\s*(\w+)\s+(?:STATIC|OBJECT|SHARED)\b(.*?)\)", re.S)


def audit_targets(root: Path, modules: dict) -> list[str]:
    """Sources game/CMakeLists.txt puts in a target their module does not own.

    The module map and the CMake target list are two descriptions of the same
    split. If a file sits in libsoi_combat.a but the map says it is navigation,
    one of them is lying and the linker is enforcing a boundary this check no
    longer describes. Every .cpp in every kernel target has to classify to a
    module that names that target.
    """
    lists = root / "game" / "CMakeLists.txt"
    try:
        text = lists.read_text(encoding="utf-8")
    except OSError:
        return [f"{lists} is missing"]
    owned_by = {name: set(spec.get("targets", ())) for name, spec in modules.items()}
    problems: list[str] = []
    for match in LIBRARY.finditer(text):
        target = match.group(1)
        if target == "accessibility_runtime":
            continue
        body = re.sub(r"#[^\n]*", "", match.group(2))
        for token in body.split():
            if not token.endswith(".cpp"):
                continue
            relative = f"game/{token}"
            try:
                module = classify(relative, modules)
            except Ambiguous as error:
                problems.append(str(error))
                continue
            if module is None:
                problems.append(
                    f"{relative} is in target {target} but no module claims it"
                )
            elif target not in owned_by[module]:
                problems.append(
                    f"{relative} is in target {target}, but its module '{module}' "
                    f"is backed by {sorted(owned_by[module])}"
                )
    return problems


def report(counts: dict[str, int], examples: dict) -> int:
    if not counts:
        return 0
    print("Module boundary violation(s):", file=sys.stderr)
    for pair, count in sorted(counts.items()):
        print(f"  {pair}: {count} edge(s)", file=sys.stderr)
        for example in examples[pair]:
            print(f"      {example}", file=sys.stderr)
    print(
        "\nA module may only include the modules listed in its may_use in\n"
        "scripts/module_rules.json. Invert the call, move the shared type down\n"
        "into components, or -- if the new edge is genuinely part of the design --\n"
        "change may_use and say why in the review.",
        file=sys.stderr,
    )
    return 1


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
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

    misplaced = audit_targets(root, modules)
    if misplaced:
        print(
            "game/CMakeLists.txt and scripts/module_rules.json disagree about "
            "which target owns a file:",
            file=sys.stderr,
        )
        for problem in misplaced:
            print(f"  {problem}", file=sys.stderr)
        print(
            "\nMove the source to the target its module names, or move the module "
            "in the map. The two must describe the same split.",
            file=sys.stderr,
        )
        return 1

    counts, examples = measure(root, modules)
    return report(counts, examples)


if __name__ == "__main__":
    raise SystemExit(main())
