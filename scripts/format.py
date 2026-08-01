#!/usr/bin/env python3
"""Unified formatting, linting and source-transformation driver.

This is the single entry point behind every `make format*` / `make lint*`
target and behind the pre-commit hooks.  It deliberately keeps the three
classes of operation separate:

  * formatting            - whitespace-only, always safe, `--fix` / `--check`
  * linting               - diagnostics, `--lint` (+ `--fix` for autofixes)
  * source transformations- destructive, opt-in only (`--strip-comments`)

Formatting never rewrites semantics and never deletes comments.

Usage:
    python scripts/format.py --all --fix
    python scripts/format.py --all --check
    python scripts/format.py --changed origin/main --check
    python scripts/format.py --staged --fix
    python scripts/format.py --files game/foo.cpp ui/qml/Hud.qml
    python scripts/format.py --all --lint
    python scripts/format.py --all --lint --fix
    python scripts/format.py --all --lint --fix --unsafe-fixes
    python scripts/format.py --doctor
    python scripts/format.py --bootstrap
"""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath
from typing import Callable, Iterable, Sequence

REPO_ROOT = Path(__file__).resolve().parents[1]
VERSIONS_FILE = REPO_ROOT / "tools" / "versions.env"
STRIP_COMMENTS_SCRIPT = REPO_ROOT / "scripts" / "remove-comments.sh"


_USE_COLOR = sys.stdout.isatty() and os.environ.get("NO_COLOR") is None


def _c(code: str, text: str) -> str:
    return f"\033[{code}m{text}\033[0m" if _USE_COLOR else text


def bold(t: str) -> str:
    return _c("1", t)


def green(t: str) -> str:
    return _c("32", t)


def red(t: str) -> str:
    return _c("31", t)


def yellow(t: str) -> str:
    return _c("33", t)


def blue(t: str) -> str:
    return _c("34", t)


def dim(t: str) -> str:
    return _c("2", t)


def info(msg: str) -> None:
    print(msg, flush=True)


def warn(msg: str) -> None:
    print(f"{yellow('warning:')} {msg}", flush=True)


def error(msg: str) -> None:
    print(f"{red('error:')} {msg}", file=sys.stderr, flush=True)


def load_versions() -> dict[str, str]:
    """Parse tools/versions.env into a plain dict."""
    versions: dict[str, str] = {}
    if not VERSIONS_FILE.exists():
        warn(
            f"{VERSIONS_FILE.relative_to(REPO_ROOT)} is missing; version pinning disabled"
        )
        return versions
    for raw in VERSIONS_FILE.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        versions[key.strip()] = value.strip().strip("'\"")
    return versions


VERSIONS = load_versions()


EXCLUDED_DIR_NAMES = {
    ".git",
    ".venv",
    ".cache",
    "venv",
    "node_modules",
    "__pycache__",
    "third_party",
    "dist",
}

LANGUAGE_BY_SUFFIX: dict[str, str] = {
    ".c": "cxx",
    ".cc": "cxx",
    ".cpp": "cxx",
    ".cxx": "cxx",
    ".h": "cxx",
    ".hh": "cxx",
    ".hpp": "cxx",
    ".hxx": "cxx",
    ".ipp": "cxx",
    ".inl": "cxx",
    ".tpp": "cxx",
    ".frag": "glsl",
    ".vert": "glsl",
    ".glsl": "glsl",
    ".qml": "qml",
    ".py": "python",
    ".sh": "shell",
    ".bash": "shell",
    ".cmake": "cmake",
    ".yml": "yaml",
    ".yaml": "yaml",
    ".md": "markdown",
    ".json": "json",
}

LANGUAGE_BY_NAME: dict[str, str] = {
    "CMakeLists.txt": "cmake",
}

ALL_LANGUAGES = sorted(
    set(LANGUAGE_BY_SUFFIX.values()) | set(LANGUAGE_BY_NAME.values())
)


def is_excluded(rel_path: str) -> bool:
    parts = PurePosixPath(rel_path).parts
    if not parts:
        return True
    if any(part in EXCLUDED_DIR_NAMES for part in parts):
        return True

    return parts[0].startswith("build")


def classify(rel_path: str) -> str | None:
    name = PurePosixPath(rel_path).name
    if name in LANGUAGE_BY_NAME:
        return LANGUAGE_BY_NAME[name]
    return LANGUAGE_BY_SUFFIX.get(PurePosixPath(rel_path).suffix.lower())


def git(*args: str, check: bool = True) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    if check and result.returncode != 0:
        raise RuntimeError(f"git {' '.join(args)} failed: {result.stderr.strip()}")
    return result.stdout


def _resolve_diff_base(base: str) -> str:
    """Return a revision usable as the left side of a diff."""
    probe = subprocess.run(
        ["git", "rev-parse", "--verify", "--quiet", f"{base}^{{commit}}"],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    if probe.returncode != 0:
        warn(f"base '{base}' is unavailable; falling back to HEAD~1")
        base = "HEAD~1"
    merge_base = subprocess.run(
        ["git", "merge-base", base, "HEAD"],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    if merge_base.returncode == 0 and merge_base.stdout.strip():
        return merge_base.stdout.strip()
    return base


def select_all() -> list[str]:

    out = git("ls-files", "-z", "--cached", "--others", "--exclude-standard")
    return [p for p in out.split("\0") if p]


def select_changed(base: str) -> list[str]:
    ref = _resolve_diff_base(base)
    out = git("diff", "--name-only", "-z", "--diff-filter=ACMR", f"{ref}...HEAD")
    return [p for p in out.split("\0") if p]


def select_staged() -> list[str]:
    out = git("diff", "--cached", "--name-only", "-z", "--diff-filter=ACMR")
    return [p for p in out.split("\0") if p]


def normalize(paths: Iterable[str]) -> list[str]:
    """Filter to existing, non-excluded, repo-relative files."""
    seen: set[str] = set()
    result: list[str] = []
    for raw in paths:
        candidate = Path(raw)
        if not candidate.is_absolute():
            candidate = REPO_ROOT / candidate
        try:
            rel = candidate.resolve().relative_to(REPO_ROOT).as_posix()
        except ValueError:
            warn(f"skipping path outside the repository: {raw}")
            continue
        if rel in seen or is_excluded(rel):
            continue
        if not (REPO_ROOT / rel).is_file():
            continue
        seen.add(rel)
        result.append(rel)
    return sorted(result)


def group_by_language(paths: Sequence[str]) -> dict[str, list[str]]:
    grouped: dict[str, list[str]] = {}
    for rel in paths:
        language = classify(rel)
        if language is not None:
            grouped.setdefault(language, []).append(rel)
    return grouped


@dataclass
class Tool:
    """A formatter or linter plus everything needed to find and run it."""

    name: str
    kind: str
    languages: tuple[str, ...]
    candidates: tuple[str, ...] = ()
    env_var: str | None = None
    version_key: str | None = None
    version_args: tuple[str, ...] = ("--version",)
    extra_search_paths: tuple[str, ...] = ()

    advisory: bool = False

    optional: bool = False

    deep_only: bool = False
    install_hint: str = ""
    pip_package: str | None = None
    npm_package: str | None = None
    batch: int = 64
    path_filter: Callable[[str], bool] | None = None
    check_cmd: Callable[[str, list[str]], list[str]] | None = None
    fix_cmd: Callable[[str, list[str]], list[str]] | None = None
    unsafe_fix_args: tuple[str, ...] = ()
    custom_check: Callable[[str, list[str]], list[str]] | None = None
    builtin_check: Callable[[list[str]], list[str]] | None = None
    needs_compile_db: bool = False

    _resolved: str | None = field(default=None, init=False, repr=False)
    _resolved_done: bool = field(default=False, init=False, repr=False)

    def resolve(self) -> str | None:
        """Locate the executable, honouring the tool's environment override."""
        if self._resolved_done:
            return self._resolved
        self._resolved_done = True
        if self.builtin_check is not None:
            self._resolved = sys.executable
            return self._resolved
        override = os.environ.get(self.env_var) if self.env_var else None
        if override:
            found = shutil.which(override) or (
                override if Path(override).is_file() else None
            )
            if found:
                self._resolved = found
                return found
            warn(
                f"{self.env_var}={override} is not executable; falling back to PATH lookup"
            )
        for candidate in self.candidates:
            found = shutil.which(candidate)
            if found:
                self._resolved = found
                return found
        for path in self.extra_search_paths:
            if Path(path).is_file() and os.access(path, os.X_OK):
                self._resolved = path
                return path
        self._resolved = None
        return None

    def version(self) -> str | None:
        exe = self.resolve()
        if exe is None or self.builtin_check is not None:
            return None
        try:
            proc = subprocess.run(
                [exe, *self.version_args],
                capture_output=True,
                text=True,
                check=False,
                timeout=30,
            )
        except (OSError, subprocess.SubprocessError):
            return None
        blob = f"{proc.stdout}\n{proc.stderr}"
        match = re.search(r"(\d+\.\d+(?:\.\d+)?)", blob)
        return match.group(1) if match else None

    def pinned(self) -> str | None:
        return VERSIONS.get(self.version_key) if self.version_key else None

    def select(self, grouped: dict[str, list[str]]) -> list[str]:
        files: list[str] = []
        for language in self.languages:
            files.extend(grouped.get(language, []))
        if self.path_filter is not None:
            files = [f for f in files if self.path_filter(f)]
        return sorted(files)


def _chunks(items: Sequence[str], size: int) -> Iterable[list[str]]:
    for start in range(0, len(items), size):
        yield list(items[start : start + size])


def _ensure_qmllint_symlinks(build_dir: str) -> None:
    """Create symlinks so qmllint can resolve StandardOfIron.Design from source."""
    _link_to(
        Path(build_dir) / "StandardOfIron" / "Design",
        REPO_ROOT / "ui" / "qml" / "design",
    )
    _link_to(
        Path(build_dir) / "StandardOfIron" / "TestSupport",
        REPO_ROOT / "tests" / "ui" / "qml" / "TestSupport",
    )


def _link_to(link_path: Path, source: Path) -> None:
    if not source.is_dir():
        return
    try:
        target = link_path.resolve()
        if link_path.is_symlink() or link_path.exists():
            if target == source:
                return
            link_path.unlink()
        link_path.parent.mkdir(parents=True, exist_ok=True)
        link_path.symlink_to(source, target_is_directory=True)
    except (FileNotFoundError, NotADirectoryError, OSError):
        pass


def _qmllint_check(exe: str, files: list[str]) -> list[str]:
    """Check structural QML rules without runtime-only context and type metadata."""
    _ensure_qmllint_symlinks(str(REPO_ROOT / "build"))
    cmd = [
        exe,
        "--unqualified",
        "disable",
        "--property",
        "disable",
        "--type",
        "disable",
        "--import",
        "disable",
        "--deferred-property-id",
        "disable",
        "-I",
        str(REPO_ROOT / "ui/qml"),
        "-I",
        str(REPO_ROOT / "build"),
        *files,
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if proc.returncode != 0:
        return [(proc.stdout + proc.stderr).strip()]
    return []


def _qmlformat_check(exe: str, files: list[str]) -> list[str]:
    """qmlformat has no --check mode; compare formatted output to the file."""
    offenders: list[str] = []
    for rel in files:
        path = REPO_ROOT / rel
        proc = subprocess.run(
            [exe, "-w", "4", str(path)],
            capture_output=True,
            text=True,
            check=False,
        )
        if proc.returncode != 0:
            offenders.append(f"{rel}: qmlformat failed: {proc.stderr.strip()}")
            continue
        if proc.stdout != path.read_text(encoding="utf-8", errors="replace"):
            offenders.append(f"{rel}: needs formatting")
    return offenders


def _json_syntax_check(files: list[str]) -> list[str]:
    offenders: list[str] = []
    for rel in files:
        try:
            json.loads((REPO_ROOT / rel).read_text(encoding="utf-8"))
        except (json.JSONDecodeError, UnicodeDecodeError) as exc:
            offenders.append(f"{rel}: invalid JSON: {exc}")
    return offenders


def _not_in_assets(rel: str) -> bool:
    """Generated map/content JSON is data, not source; never reflow it."""
    return not rel.startswith("assets/")


def build_tools() -> list[Tool]:
    qt_bin_guesses = (
        "/usr/lib/qt6/bin/qmlformat",
        "/usr/lib/qt5/bin/qmlformat",
        "/opt/homebrew/opt/qt/bin/qmlformat",
    )
    qt_lint_guesses = (
        "/usr/lib/qt6/bin/qmllint",
        "/usr/lib/qt5/bin/qmllint",
        "/opt/homebrew/opt/qt/bin/qmllint",
    )

    return [
        Tool(
            name="clang-format",
            kind="format",
            languages=("cxx", "glsl"),
            candidates=("clang-format-18", "clang-format"),
            env_var="CLANG_FORMAT",
            version_key="CLANG_FORMAT_VERSION",
            install_hint="apt install clang-format-18 | brew install clang-format",
            batch=64,
            check_cmd=lambda exe, files: [
                exe,
                "--dry-run",
                "-Werror",
                "--style=file",
                *files,
            ],
            fix_cmd=lambda exe, files: [exe, "-i", "--style=file", *files],
        ),
        Tool(
            name="qmlformat",
            kind="format",
            languages=("qml",),
            candidates=("qmlformat", "qmlformat6", "qmlformat-qt6"),
            env_var="QMLFORMAT",
            version_key="QMLFORMAT_VERSION",
            extra_search_paths=qt_bin_guesses,
            install_hint="ships with Qt (qt6-declarative-dev-tools)",
            batch=16,
            custom_check=_qmlformat_check,
            fix_cmd=lambda exe, files: [exe, "-i", "-w", "4", *files],
        ),
        Tool(
            name="black",
            kind="format",
            languages=("python",),
            candidates=("black",),
            env_var="BLACK",
            version_key="BLACK_VERSION",
            pip_package="black",
            install_hint="pip install black",
            batch=128,
            check_cmd=lambda exe, files: [exe, "--check", "--quiet", *files],
            fix_cmd=lambda exe, files: [exe, "--quiet", *files],
        ),
        Tool(
            name="shfmt",
            kind="format",
            languages=("shell",),
            candidates=("shfmt",),
            env_var="SHFMT",
            version_key="SHFMT_VERSION",
            optional=True,
            install_hint="apt install shfmt | brew install shfmt | go install mvdan.cc/sh/v3/cmd/shfmt@latest",
            batch=64,
            check_cmd=lambda exe, files: [exe, "-d", "-i", "2", "-ci", *files],
            fix_cmd=lambda exe, files: [exe, "-w", "-i", "2", "-ci", *files],
        ),
        Tool(
            name="gersemi",
            kind="format",
            languages=("cmake",),
            candidates=("gersemi",),
            env_var="GERSEMI",
            version_key="GERSEMI_VERSION",
            pip_package="gersemi",
            optional=True,
            install_hint="pip install gersemi",
            batch=64,
            check_cmd=lambda exe, files: [exe, "--check", *files],
            fix_cmd=lambda exe, files: [exe, "--in-place", *files],
        ),
        Tool(
            name="prettier",
            kind="format",
            languages=("yaml", "markdown", "json"),
            candidates=("prettier",),
            env_var="PRETTIER",
            version_key="PRETTIER_VERSION",
            npm_package="prettier",
            optional=True,
            install_hint="npm install -g prettier",
            batch=64,
            path_filter=_not_in_assets,
            check_cmd=lambda exe, files: [
                exe,
                "--check",
                "--log-level",
                "warn",
                *files,
            ],
            fix_cmd=lambda exe, files: [exe, "--write", "--log-level", "warn", *files],
        ),
        Tool(
            name="ruff",
            kind="lint",
            languages=("python",),
            candidates=("ruff",),
            env_var="RUFF",
            version_key="RUFF_VERSION",
            pip_package="ruff",
            install_hint="pip install ruff",
            batch=256,
            check_cmd=lambda exe, files: [exe, "check", *files],
            fix_cmd=lambda exe, files: [exe, "check", "--fix", *files],
            unsafe_fix_args=("--unsafe-fixes",),
        ),
        Tool(
            name="shellcheck",
            kind="lint",
            languages=("shell",),
            candidates=("shellcheck",),
            env_var="SHELLCHECK",
            version_key="SHELLCHECK_VERSION",
            install_hint="apt install shellcheck | brew install shellcheck",
            batch=64,
            check_cmd=lambda exe, files: [exe, "--severity=warning", *files],
        ),
        Tool(
            name="yamllint",
            kind="lint",
            languages=("yaml",),
            candidates=("yamllint",),
            env_var="YAMLLINT",
            version_key="YAMLLINT_VERSION",
            pip_package="yamllint",
            install_hint="pip install yamllint",
            batch=128,
            check_cmd=lambda exe, files: [exe, *files],
        ),
        Tool(
            name="json-syntax",
            kind="lint",
            languages=("json",),
            builtin_check=_json_syntax_check,
            batch=512,
        ),
        Tool(
            name="clang-tidy",
            kind="lint",
            languages=("cxx",),
            candidates=("clang-tidy-18", "clang-tidy"),
            env_var="CLANG_TIDY",
            version_key="CLANG_TIDY_VERSION",
            advisory=True,
            deep_only=True,
            needs_compile_db=True,
            install_hint="apt install clang-tidy-18 | brew install llvm",
            batch=8,
            check_cmd=lambda exe, files: [exe, "--quiet", *files],
        ),
        Tool(
            name="qmllint",
            kind="lint",
            languages=("qml",),
            candidates=("qmllint", "qmllint6", "qmllint-qt6"),
            env_var="QMLLINT",
            version_key="QMLLINT_VERSION",
            extra_search_paths=qt_lint_guesses,
            advisory=True,
            install_hint="ships with Qt (qt6-declarative-dev-tools)",
            batch=32,
            custom_check=_qmllint_check,
        ),
        Tool(
            name="markdownlint",
            kind="lint",
            languages=("markdown",),
            candidates=("markdownlint",),
            env_var="MARKDOWNLINT",
            version_key="MARKDOWNLINT_CLI_VERSION",
            npm_package="markdownlint-cli",
            advisory=True,
            optional=True,
            install_hint="npm install -g markdownlint-cli",
            batch=128,
            check_cmd=lambda exe, files: [exe, *files],
        ),
    ]


@dataclass
class Outcome:
    tool: str
    status: str
    files: int
    detail: str = ""
    advisory: bool = False


def _run(cmd: list[str]) -> tuple[int, str]:
    proc = subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    return proc.returncode, (proc.stdout + proc.stderr).strip()


def _compile_db_args(build_dir: str) -> list[str] | None:
    for candidate in (Path(build_dir), REPO_ROOT / build_dir):
        if (candidate / "compile_commands.json").is_file():
            return ["-p", str(candidate)]
    return None


def run_tool(
    tool: Tool,
    files: list[str],
    *,
    fix: bool,
    unsafe_fixes: bool,
    jobs: int,
    build_dir: str,
    deep: bool,
) -> Outcome:
    if not files:
        return Outcome(tool.name, "skipped", 0, "no matching files", tool.advisory)

    if tool.deep_only and not deep:
        return Outcome(
            tool.name,
            "skipped",
            len(files),
            "deep lane only (use `make lint-changed` or `make lint-deep`)",
            tool.advisory,
        )

    exe = tool.resolve()
    if exe is None:
        return Outcome(tool.name, "skipped", len(files), "not installed", tool.advisory)

    prefix: list[str] = []
    if tool.needs_compile_db:
        prefix_args = _compile_db_args(build_dir)
        if prefix_args is None:
            return Outcome(
                tool.name,
                "skipped",
                len(files),
                f"no compile_commands.json in '{build_dir}' (run `make configure`)",
                tool.advisory,
            )
        prefix = prefix_args

    if tool.builtin_check is not None:
        problems = tool.builtin_check(files)
        if problems:
            return Outcome(
                tool.name, "failed", len(files), "\n".join(problems), tool.advisory
            )
        return Outcome(tool.name, "ok", len(files), "", tool.advisory)

    if fix and tool.fix_cmd is None:

        if tool.kind == "lint":
            return Outcome(
                tool.name, "skipped", len(files), "no autofix support", tool.advisory
            )

    if not fix and tool.custom_check is not None:
        problems = tool.custom_check(exe, files)
        if problems:
            return Outcome(
                tool.name, "failed", len(files), "\n".join(problems), tool.advisory
            )
        return Outcome(tool.name, "ok", len(files), "", tool.advisory)

    builder = tool.fix_cmd if (fix and tool.fix_cmd is not None) else tool.check_cmd
    if builder is None:
        return Outcome(tool.name, "skipped", len(files), "nothing to do", tool.advisory)

    batches = list(_chunks(files, tool.batch))
    failures: list[str] = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, jobs)) as pool:
        futures = []
        for batch in batches:
            cmd = builder(exe, batch)
            if fix and unsafe_fixes and tool.unsafe_fix_args:
                cmd = [
                    *cmd[: -len(batch)],
                    *tool.unsafe_fix_args,
                    *cmd[-len(batch) :],
                ]
            if prefix:
                cmd = [cmd[0], *prefix, *cmd[1:]]
            futures.append(pool.submit(_run, cmd))
        for future in futures:
            code, output = future.result()
            if code != 0:
                failures.append(output or f"{tool.name} exited with status {code}")

    if failures:
        return Outcome(
            tool.name, "failed", len(files), "\n".join(failures), tool.advisory
        )
    return Outcome(tool.name, "ok", len(files), "", tool.advisory)


def report(outcomes: list[Outcome], *, verbose: bool) -> None:
    for outcome in outcomes:
        label = (
            f"{outcome.tool} ({outcome.files} file{'s' if outcome.files != 1 else ''})"
        )
        if outcome.status == "ok":
            info(f"  {green('ok')}      {label}")
        elif outcome.status == "skipped":
            if verbose or outcome.detail not in ("no matching files",):
                info(f"  {dim('skip')}    {label} - {outcome.detail}")
        else:
            tag = yellow("advisory") if outcome.advisory else red("FAIL")
            info(f"  {tag}    {label}")
            if outcome.detail:
                for line in outcome.detail.splitlines():
                    info(f"          {line}")


def execute(
    tools: list[Tool],
    grouped: dict[str, list[str]],
    *,
    fix: bool,
    unsafe_fixes: bool,
    jobs: int,
    build_dir: str,
    strict: bool,
    fail_on_advisory: bool,
    deep: bool,
    verbose: bool,
) -> int:
    outcomes: list[Outcome] = []
    for tool in tools:
        files = tool.select(grouped)
        outcomes.append(
            run_tool(
                tool,
                files,
                fix=fix,
                unsafe_fixes=unsafe_fixes,
                jobs=jobs,
                build_dir=build_dir,
                deep=deep,
            )
        )

    report(outcomes, verbose=verbose)

    exit_code = 0
    missing_required = [
        o
        for o in outcomes
        if o.status == "skipped" and o.detail == "not installed" and o.files > 0
    ]
    for outcome in missing_required:
        tool = next(t for t in tools if t.name == outcome.tool)
        if tool.optional:
            continue
        if strict:
            error(f"{tool.name} is required but not installed ({tool.install_hint})")
            exit_code = 1
        else:
            warn(f"{tool.name} is not installed; {outcome.files} file(s) unchecked")

    for outcome in outcomes:
        if outcome.status != "failed":
            continue
        if outcome.advisory and not fail_on_advisory:
            continue
        exit_code = 1

    return exit_code


def _version_status(pinned: str | None, found: str | None) -> tuple[str, bool]:
    """Return (label, is_error) for a pinned/found version pair."""
    if found is None:
        return ("missing", True)
    if pinned is None:
        return ("unpinned", False)
    if found == pinned:
        return ("ok", False)
    if found.split(".")[0] != pinned.split(".")[0]:
        return ("major mismatch", True)
    return ("minor drift", False)


def action_doctor(tools: list[Tool]) -> int:
    info(bold("Formatting toolchain (pins from tools/versions.env)"))
    info("")
    header = f"  {'tool':<14} {'pinned':<10} {'found':<10} {'status':<15} path"
    info(bold(header))
    exit_code = 0
    for tool in tools:
        pinned = tool.pinned() or "-"
        found = tool.version()
        status, is_error = _version_status(tool.pinned(), found)
        path = tool.resolve() or "-"
        if tool.builtin_check is not None:
            status, is_error, pinned, found, path = "builtin", False, "-", "-", "python"
        line = (
            f"  {tool.name:<14} {pinned:<10} {(found or '-'):<10} {status:<15} {path}"
        )
        if is_error and (tool.optional or tool.advisory):
            info(yellow(line))
        elif is_error:
            info(red(line))
            exit_code = 1
        elif status == "minor drift":
            info(yellow(line))
        else:
            info(green(line))
    info("")
    if exit_code:
        error("required tools are missing or have an incompatible major version")
        info(f"Run {bold('make format-bootstrap')} to install the pinned toolchain.")
    else:
        info(green("toolchain is usable"))
    return exit_code


def action_bootstrap(tools: list[Tool], *, dry_run: bool) -> int:
    pip_specs: list[str] = []
    npm_specs: list[str] = []
    manual: list[Tool] = []

    for tool in tools:
        pinned = tool.pinned()
        if tool.pip_package:
            pip_specs.append(
                f"{tool.pip_package}=={pinned}" if pinned else tool.pip_package
            )
        elif tool.npm_package:
            npm_specs.append(
                f"{tool.npm_package}@{pinned}" if pinned else tool.npm_package
            )
        elif tool.builtin_check is None and tool.resolve() is None:
            manual.append(tool)

    pre_commit_version = VERSIONS.get("PRE_COMMIT_VERSION")
    pip_specs.append(
        f"pre-commit=={pre_commit_version}" if pre_commit_version else "pre-commit"
    )

    exit_code = 0

    if pip_specs:
        cmd = [
            sys.executable,
            "-m",
            "pip",
            "install",
            "--upgrade",
            *sorted(set(pip_specs)),
        ]
        info(bold("Installing pinned Python tools"))
        info(f"  {' '.join(cmd)}")
        if not dry_run:
            code, output = _run(cmd)
            if code != 0:
                error(output)
                exit_code = 1

    if npm_specs:
        info(bold("Installing pinned Node tools"))
        if shutil.which("npm"):
            cmd = ["npm", "install", "--global", *sorted(set(npm_specs))]
            info(f"  {' '.join(cmd)}")
            if not dry_run:
                code, output = _run(cmd)
                if code != 0:
                    warn(f"npm install failed (optional tools): {output}")
        else:
            warn("npm not found; optional YAML/Markdown formatters will be skipped")
            for spec in sorted(set(npm_specs)):
                info(f"  npm install -g {spec}")

    if manual:
        info("")
        info(bold("Install these with your system package manager:"))
        for tool in manual:
            pinned = tool.pinned() or "any"
            info(f"  {tool.name:<14} (pinned {pinned}) - {tool.install_hint}")

    info("")
    info(f"Verify with {bold('make format-doctor')}.")
    return exit_code


def action_strip_comments(grouped: dict[str, list[str]], *, dry_run: bool) -> int:
    languages = ("cxx", "glsl", "qml", "python")
    files: list[str] = []
    for language in languages:
        files.extend(grouped.get(language, []))
    files = sorted(set(files))
    if not files:
        info("strip-comments: no matching files")
        return 0
    if not STRIP_COMMENTS_SCRIPT.exists():
        error(f"{STRIP_COMMENTS_SCRIPT} not found")
        return 2

    info(bold(red("DESTRUCTIVE: stripping comments from %d file(s)" % len(files))))
    cmd = ["bash", str(STRIP_COMMENTS_SCRIPT)]
    if dry_run:
        cmd.append("--dry-run")
    cmd.extend(files)
    code, output = _run(cmd)
    if output:
        info(output)
    if code != 0:
        error("comment stripping failed")
    return code


def default_jobs() -> int:
    return os.cpu_count() or 4


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="scripts/format.py",
        description="Formatting, linting and source-transformation driver.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.split("Usage:", 1)[1] if "Usage:" in __doc__ else None,
    )

    selection = parser.add_argument_group("file selection")
    selection.add_argument(
        "--all", action="store_true", help="every tracked file (default)"
    )
    selection.add_argument(
        "--changed",
        nargs="?",
        const="origin/main",
        metavar="BASE",
        help="files changed against BASE (default origin/main)",
    )
    selection.add_argument(
        "--staged", action="store_true", help="files staged in the index"
    )
    selection.add_argument(
        "--files", nargs="+", metavar="PATH", help="explicit file list"
    )
    selection.add_argument(
        "--languages",
        nargs="+",
        choices=ALL_LANGUAGES,
        metavar="LANG",
        help=f"restrict to languages: {', '.join(ALL_LANGUAGES)}",
    )

    mode = parser.add_argument_group("mode")
    mode.add_argument(
        "--check", action="store_true", help="verify only, never write (default)"
    )
    mode.add_argument("--fix", action="store_true", help="apply changes in place")
    mode.add_argument(
        "--unsafe-fixes",
        action="store_true",
        help="enable Ruff's unsafe autofixes (requires --lint --fix)",
    )

    action = parser.add_argument_group("action")
    action.add_argument(
        "--lint", action="store_true", help="run linters instead of formatters"
    )
    action.add_argument(
        "--strip-comments",
        action="store_true",
        help="DESTRUCTIVE: remove comments from selected sources",
    )
    action.add_argument(
        "--doctor", action="store_true", help="report toolchain versions"
    )
    action.add_argument(
        "--bootstrap", action="store_true", help="install the pinned toolchain"
    )

    behaviour = parser.add_argument_group("behaviour")
    behaviour.add_argument(
        "--jobs", type=int, default=default_jobs(), help="parallel jobs"
    )
    behaviour.add_argument(
        "--build-dir",
        default=os.environ.get("BUILD_DIR", "build"),
        help="build tree holding compile_commands.json (for clang-tidy)",
    )
    behaviour.add_argument(
        "--strict",
        action="store_true",
        help="fail when a required tool is missing",
    )
    behaviour.add_argument(
        "--fail-on-advisory",
        action="store_true",
        help="treat advisory linter findings as failures",
    )
    behaviour.add_argument(
        "--deep",
        action="store_true",
        help="also run the slow whole-project linters (clang-tidy)",
    )
    behaviour.add_argument(
        "--dry-run", action="store_true", help="print actions without running"
    )
    behaviour.add_argument(
        "-v", "--verbose", action="store_true", help="show skipped tools"
    )

    args = parser.parse_args(argv)

    if args.check and args.fix:
        parser.error("--check and --fix are mutually exclusive")
    if args.unsafe_fixes and (not args.lint or not args.fix):
        parser.error("--unsafe-fixes requires --lint --fix")

    selectors = [
        bool(args.all),
        args.changed is not None,
        bool(args.staged),
        bool(args.files),
    ]
    if sum(selectors) > 1:
        parser.error("use exactly one of --all / --changed / --staged / --files")

    return args


def collect_files(args: argparse.Namespace) -> list[str]:
    if args.files:
        raw = args.files
    elif args.staged:
        raw = select_staged()
    elif args.changed is not None:
        raw = select_changed(args.changed)
    else:
        raw = select_all()
    return normalize(raw)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)

    tools = build_tools()

    if args.doctor:
        return action_doctor(tools)
    if args.bootstrap:
        return action_bootstrap(tools, dry_run=args.dry_run)

    files = collect_files(args)
    grouped = group_by_language(files)
    if args.languages:
        grouped = {k: v for k, v in grouped.items() if k in set(args.languages)}

    total = sum(len(v) for v in grouped.values())
    if total == 0:
        info("nothing to do (no supported files selected)")
        return 0

    if args.strip_comments:
        code = action_strip_comments(grouped, dry_run=args.dry_run)
        if code != 0:
            return code
        if not args.fix and not args.lint:
            return code

    fix = bool(args.fix)
    kind = "lint" if args.lint else "format"
    selected = [t for t in tools if t.kind == kind]

    verb = {
        ("format", True): "Formatting",
        ("format", False): "Checking formatting of",
        ("lint", True): "Auto-fixing lint findings in",
        ("lint", False): "Linting",
    }[(kind, fix)]
    summary = ", ".join(f"{len(v)} {k}" for k, v in sorted(grouped.items()))
    info(bold(blue(f"{verb} {total} file(s) [{summary}]")))

    if args.dry_run:
        for tool in selected:
            count = len(tool.select(grouped))
            if count:
                info(f"  would run {tool.name} on {count} file(s)")
        return 0

    narrowed = args.changed is not None or bool(args.staged) or bool(args.files)
    deep = args.deep or args.fail_on_advisory or narrowed

    exit_code = execute(
        selected,
        grouped,
        fix=fix,
        unsafe_fixes=args.unsafe_fixes,
        jobs=args.jobs,
        build_dir=args.build_dir,
        strict=args.strict,
        fail_on_advisory=args.fail_on_advisory,
        deep=deep,
        verbose=args.verbose,
    )

    info("")
    if exit_code == 0:
        info(green(f"✓ {kind} passed"))
    elif kind == "format" and not fix:
        error("formatting check failed - run `make format` to fix")
    else:
        error(f"{kind} failed")
    return exit_code


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
    except RuntimeError as exc:
        error(str(exc))
        sys.exit(2)
