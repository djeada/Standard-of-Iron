#!/usr/bin/env python3
"""Catch macOS and Windows build failures from a Linux machine.

The game is developed on Linux/GCC and shipped on macOS/AppleClang and
Windows/MSVC, so most of what breaks the other two toolchains is discovered by
a release build that has already been tagged.  Everything here runs on Linux
and looks for a class of problem that Linux/GCC/Mesa happens to accept.

Three passes:

  apple   Re-parses every first-party translation unit with Clang and libc++ —
          the macOS toolchain, near enough.  libstdc++ includes far more
          transitively than libc++ does, so a missing '#include <cstdint>'
          builds here and fails there; Clang also diagnoses several things GCC
          is silent about (unsequenced modification, dangling references,
          missing 'override').  Syntax-only, so no Qt-against-libc++ ABI
          question arises: nothing is linked.

  glsl    Runs every shader through glslangValidator after resolving the
          engine's own '#include' directives the way render/gl/shader.cpp
          resolves them.  Mesa's GLSL front end is permissive; Apple's is
          close to the letter of the spec, and a shader that fails to compile
          there is not a crash, it is a silently missing object on screen.

  windows Static checks for the constructs MSVC and NTFS reject: math
          constants that MSVC's <cmath> does not define, filenames Windows
          cannot create, and paths that overflow MAX_PATH once unpacked.

The compiler warning set that belongs to a real build lives in CMakeLists.txt
behind SOI_STRICT_WARNINGS, not here, so that `make portability` and CI agree
with what an actual strict build enforces.

Usage:
    python3 scripts/check-portability.py [--build-dir build] [--require-all]
                                         [--only apple,glsl,windows]

--require-all turns a missing clang/libc++/glslangValidator into a failure
instead of a skip.  CI passes it; a developer running this locally without the
tools installed gets the passes it can run.
"""

import argparse
import concurrent.futures
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

SKIP_PREFIXES = ("third_party", "build")

CLANG_FLAGS = [
    "-fsyntax-only",
    "-Wall",
    "-Wextra",
    "-Werror=return-type",
    "-Werror=unused-result",
    "-Werror=sign-compare",
    "-Werror=range-loop-construct",
    "-Werror=unsequenced",
    "-Werror=dangling-gsl",
    "-Werror=inconsistent-missing-override",
    "-Werror=nan-infinity-disabled",
    "-Wno-switch",
    "-Wno-missing-field-initializers",
    "-Wno-unused-parameter",
    "-Wno-unused-variable",
    "-Wno-unused-function",
    "-Wno-unused-private-field",
    "-Wno-unused-lambda-capture",
    "-Wno-unused-local-typedef",
    "-Wno-mismatched-tags",
    "-Wno-shadow-field",
    "-Wno-range-loop-bind-reference",
    "-Wno-unknown-warning-option",
    "-Wno-unused-command-line-argument",
]

GCC_ONLY_FLAGS = {
    "-ftree-vectorize",
    "-fno-optimize-sibling-calls",
    "-ggdb3",
    "-fprofile-update=atomic",
}

MSVC_UNDEFINED_MATH = (
    "M_PI",
    "M_PI_2",
    "M_PI_4",
    "M_E",
    "M_SQRT2",
    "M_LN2",
    "M_LN10",
    "M_1_PI",
    "M_2_PI",
)

WINDOWS_RESERVED = {
    "CON",
    "PRN",
    "AUX",
    "NUL",
    *(f"COM{i}" for i in range(1, 10)),
    *(f"LPT{i}" for i in range(1, 10)),
}

WINDOWS_FORBIDDEN_CHARS = set('<>:"|?*\\')

MAX_PATH = 260

MIN_CLANG_MAJOR = 18


def clang_major(clang):
    """Major version of the given clang, or None if it cannot be read."""
    proc = subprocess.run([clang, "--version"], capture_output=True, text=True)
    match = re.search(r"clang version (\d+)", proc.stdout)
    return int(match.group(1)) if match else None


def tracked_files():
    """Every file git knows about, as repo-relative POSIX paths.

    Paths deleted in the working tree but still in the index are dropped: a
    refactor that removes a file should not make the gate crash before the
    deletion is staged.
    """
    out = subprocess.run(
        ["git", "-C", str(ROOT), "ls-files", "-z"],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    return [p for p in out.split("\0") if p and (ROOT / p).exists()]


def find_libcxx(clang):
    """Locate a libc++ header directory next to the given clang binary."""
    bindir = Path(clang).resolve().parent
    candidates = [
        bindir.parent / "include" / "c++" / "v1",
        Path("/usr/include/c++/v1"),
    ]
    candidates += sorted(Path("/usr/lib").glob("llvm-*/include/c++/v1"), reverse=True)
    for c in candidates:
        if (c / "vector").exists():
            return c
    return None


def resolve_shader_includes(source, include_dir, already):
    """Mirror resolve_shader_includes() in render/gl/shader.cpp.

    Same rules: the name is looked up in assets/shaders/include, each header is
    pasted at most once per shader, and resolution recurses.  If this ever
    drifts from the C++ the validator stops describing what the game compiles.
    """
    out = []
    for line in source.split("\n"):
        stripped = line.strip()
        if stripped.startswith("#include"):
            match = re.search(r'["<]([^">]+)[">]', stripped)
            if match:
                name = match.group(1)
                if name in already:
                    continue
                header = include_dir / name
                if header.exists():
                    already.add(name)
                    out.append(
                        resolve_shader_includes(
                            header.read_text(encoding="utf-8"), include_dir, already
                        )
                    )
                    continue
        out.append(line)
    return "\n".join(out)


def check_apple(build_dir, require):
    """Reparse every first-party TU with Clang and libc++."""
    clang = shutil.which("clang++") or shutil.which("clang++-18")
    if clang is None:
        msg = "clang++ not found (install clang)"
        return ([f"{msg}"], []) if require else ([], [f"SKIP {msg}"])

    libcxx = find_libcxx(clang)
    if libcxx is None:
        msg = "libc++ headers not found (install libc++-dev)"
        return ([f"{msg}"], []) if require else ([], [f"SKIP {msg}"])

    version = clang_major(clang)
    if version is not None and version < MIN_CLANG_MAJOR:
        msg = (
            f"clang {version} is too old: -Wnan-infinity-disabled arrived in "
            f"{MIN_CLANG_MAJOR}, and an unknown -Werror= name is only a warning, "
            "so the fast-math guard would pass without being checked"
        )
        return ([msg], []) if require else ([], [f"SKIP {msg}"])

    db_path = ROOT / build_dir / "compile_commands.json"
    if not db_path.exists():
        return (
            [f"{db_path} missing — configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"],
            [],
        )

    database = json.loads(db_path.read_text(encoding="utf-8"))
    seen, units = set(), []
    for entry in database:
        path = Path(entry["file"])
        try:
            rel = path.relative_to(ROOT).as_posix()
        except ValueError:
            continue
        if rel.startswith(SKIP_PREFIXES) or "_autogen" in rel or entry["file"] in seen:
            continue
        seen.add(entry["file"])
        units.append(entry)

    def compile_one(entry):
        args, skip_next = [], False
        for arg in shlex.split(entry["command"])[1:]:
            if skip_next:
                skip_next = False
                continue
            if arg == "-o":
                skip_next = True
                continue
            if arg == "-c" or arg in GCC_ONLY_FLAGS:
                continue
            args.append(arg)
        cmd = [clang, "-nostdinc++", "-isystem", str(libcxx)]
        cmd += CLANG_FLAGS + args + [entry["file"]]
        proc = subprocess.run(
            cmd, cwd=entry["directory"], capture_output=True, text=True
        )
        return proc.returncode, proc.stderr

    errors = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        for entry, (code, err) in zip(units, pool.map(compile_one, units), strict=True):
            if code != 0:
                rel = Path(entry["file"]).relative_to(ROOT).as_posix()
                first = next(
                    (ln for ln in err.split("\n") if "error:" in ln), err.split("\n")[0]
                )
                errors.append(f"{rel}: {first.strip()}")
    return errors, [f"{len(units)} translation units reparsed with Clang + libc++"]


def check_glsl(require):
    """Compile every shader with glslang after resolving engine includes."""
    glslang = shutil.which("glslangValidator") or shutil.which("glslang")
    if glslang is None:
        msg = "glslangValidator not found (install glslang-tools)"
        return ([f"{msg}"], []) if require else ([], [f"SKIP {msg}"])

    shader_dir = ROOT / "assets" / "shaders"
    include_dir = shader_dir / "include"
    shaders = sorted(
        p for p in shader_dir.iterdir() if p.suffix in (".vert", ".frag", ".comp")
    )

    errors = []
    for shader in shaders:
        resolved = resolve_shader_includes(
            shader.read_text(encoding="utf-8"), include_dir, set()
        )
        with tempfile.NamedTemporaryFile(
            "w", suffix=shader.suffix, delete=False, encoding="utf-8"
        ) as handle:
            handle.write(resolved)
            tmp = handle.name
        try:
            proc = subprocess.run([glslang, tmp], capture_output=True, text=True)
        finally:
            os.unlink(tmp)
        if proc.returncode != 0:
            detail = [
                ln.strip()
                for ln in (proc.stdout + proc.stderr).split("\n")
                if ln.startswith("ERROR:") and "compilation terminated" not in ln
            ]
            errors.append(
                f"{shader.name}: {detail[0] if detail else 'failed to compile'}"
            )
    return errors, [f"{len(shaders)} shaders compiled by glslang"]


def check_windows():
    """Static checks for what MSVC and NTFS reject."""
    errors = []
    files = tracked_files()

    sources = [
        f
        for f in files
        if f.endswith((".cpp", ".h", ".hpp", ".cc")) and not f.startswith(SKIP_PREFIXES)
    ]
    pattern = re.compile(r"\b(" + "|".join(MSVC_UNDEFINED_MATH) + r")\b")
    for rel in sources:
        text = (ROOT / rel).read_text(encoding="utf-8", errors="replace")
        if "_USE_MATH_DEFINES" in text:
            continue
        for number, line in enumerate(text.split("\n"), start=1):
            if line.lstrip().startswith("#"):
                continue
            match = pattern.search(line)
            if match:
                errors.append(
                    f"{rel}:{number}: {match.group(1)} is not defined by MSVC's "
                    "<cmath> unless _USE_MATH_DEFINES is defined first; use a "
                    "constant of your own or std::numbers"
                )

    for rel in files:
        name = Path(rel).name
        stem = name.split(".")[0].upper()
        if stem in WINDOWS_RESERVED:
            errors.append(f"{rel}: '{stem}' is a reserved device name on Windows")
        if WINDOWS_FORBIDDEN_CHARS & set(name):
            bad = "".join(sorted(WINDOWS_FORBIDDEN_CHARS & set(name)))
            errors.append(f"{rel}: filename contains {bad!r}, which Windows forbids")
        if name.endswith((".", " ")):
            errors.append(f"{rel}: Windows strips trailing dots and spaces from names")
        if len(rel) > MAX_PATH - len("C:\\standard_of_iron\\"):
            errors.append(
                f"{rel}: path is {len(rel)} chars; risks MAX_PATH once unpacked"
            )

    by_directory = {}
    for rel in files:
        directory, _, name = rel.rpartition("/")
        by_directory.setdefault(directory, {}).setdefault(name.lower(), []).append(name)
    for directory, names in by_directory.items():
        for variants in names.values():
            if len(variants) > 1:
                joined = ", ".join(sorted(variants))
                where = directory or "."
                errors.append(
                    f"{where}: {joined} differ only by case and cannot coexist on "
                    "macOS or Windows"
                )

    return errors, [
        f"{len(sources)} sources scanned for MSVC-only math constants",
        f"{len(files)} tracked paths checked against Windows filename rules",
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--require-all", action="store_true")
    parser.add_argument("--only", default="apple,glsl,windows")
    args = parser.parse_args()

    selected = {name.strip() for name in args.only.split(",") if name.strip()}
    failures = 0

    if "apple" in selected:
        print("[apple]   Clang + libc++ reparse  (macOS toolchain)")
        errors, notes = check_apple(args.build_dir, args.require_all)
        failures += report(errors, notes)

    if "glsl" in selected:
        print("[glsl]    glslang shader compile  (Apple GLSL strictness)")
        errors, notes = check_glsl(args.require_all)
        failures += report(errors, notes)

    if "windows" in selected:
        print("[windows] MSVC and NTFS source lint")
        errors, notes = check_windows()
        failures += report(errors, notes)

    print()
    if failures:
        print(f"PORTABILITY=fail — {failures} error(s).")
        return 1
    print("PORTABILITY=pass")
    return 0


def report(errors, notes):
    for note in notes:
        print(f"  {note}" if note.startswith("SKIP") else f"  OK   {note}")
    for error in errors:
        print(f"  FAIL {error}")
    return len(errors)


if __name__ == "__main__":
    sys.exit(main())
