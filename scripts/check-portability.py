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

  includes Reads the include graph and requires every file to include the
          standard headers it uses.  MSVC's standard library does not hand out
          <array> or <utility> as a side effect of including something else,
          the way libstdc++ and libc++ do, so this is the one class of Windows
          break that no compiler on a Linux machine can reproduce -- the apple
          pass above cannot see it either, because libc++ leaks the header too.

The compiler warning set that belongs to a real build lives in CMakeLists.txt
behind SOI_STRICT_WARNINGS, not here, so that `make portability` and CI agree
with what an actual strict build enforces.

Usage:
    python3 scripts/check-portability.py [--build-dir build] [--require-all]
                                         [--only apple,glsl,windows,includes]

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
    "-Werror=mismatched-tags",
    "-Werror=range-loop-bind-reference",
    "-Werror=shadow-field",
    "-Werror=unused-private-field",
    "-Werror=deprecated-copy-with-dtor",
    "-Werror=conditional-uninitialized",
    "-Werror=loop-analysis",
    "-Werror=self-assign",
    "-Werror=unreachable-code",
    "-Werror=header-hygiene",
    "-Werror=cast-qual",
    "-Werror=suggest-override",
    "-Werror=newline-eof",
    "-Werror=switch",
    "-Werror=extra-semi",
    "-Werror=implicit-fallthrough",
    "-Werror=old-style-cast",
    "-Werror=unused-lambda-capture",
    "-Werror=unused-variable",
    "-Werror=unused-function",
    "-Werror=unused-const-variable",
    "-Wno-missing-field-initializers",
    "-Wno-unused-parameter",
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


STD_FACILITIES = (
    ("<array>", ("array",), ("array",)),
    ("<span>", ("span",), ("span",)),
    ("<vector>", ("vector",), ("vector",)),
    ("<deque>", ("deque",), ("deque",)),
    ("<list>", ("list",), ("list",)),
    ("<set>", ("set", "multiset"), ("set",)),
    ("<map>", ("map", "multimap"), ("map",)),
    ("<unordered_map>", ("unordered_map", "unordered_multimap"), ("unordered_map",)),
    ("<unordered_set>", ("unordered_set", "unordered_multiset"), ("unordered_set",)),
    (
        "<string>",
        ("string", "to_string", "stoi", "stof", "stod", "stoul", "getline"),
        ("string",),
    ),
    ("<string_view>", ("string_view",), ("string_view",)),
    (
        "<optional>",
        ("optional", "nullopt", "nullopt_t", "make_optional"),
        ("optional",),
    ),
    (
        "<variant>",
        ("variant", "visit", "get_if", "holds_alternative", "monostate"),
        ("variant",),
    ),
    (
        "<tuple>",
        ("tuple", "make_tuple", "tie", "apply", "tuple_size", "tuple_element"),
        ("tuple",),
    ),
    (
        "<utility>",
        (
            "pair",
            "make_pair",
            "move",
            "forward",
            "exchange",
            "as_const",
            "swap",
            "declval",
        ),
        ("utility",),
    ),
    (
        "<memory>",
        (
            "unique_ptr",
            "shared_ptr",
            "weak_ptr",
            "make_unique",
            "make_shared",
            "enable_shared_from_this",
            "static_pointer_cast",
            "dynamic_pointer_cast",
            "addressof",
            "destroy_at",
            "uninitialized_copy",
        ),
        ("memory",),
    ),
    (
        "<functional>",
        (
            "function",
            "bind",
            "ref",
            "cref",
            "hash",
            "less",
            "greater",
            "invoke",
            "reference_wrapper",
            "plus",
            "identity",
        ),
        ("functional",),
    ),
    (
        "<algorithm>",
        (
            "sort",
            "stable_sort",
            "partial_sort",
            "find",
            "find_if",
            "find_if_not",
            "min",
            "max",
            "minmax",
            "clamp",
            "copy",
            "copy_if",
            "copy_n",
            "fill",
            "fill_n",
            "any_of",
            "all_of",
            "none_of",
            "count",
            "count_if",
            "transform",
            "remove",
            "remove_if",
            "lower_bound",
            "upper_bound",
            "binary_search",
            "for_each",
            "reverse",
            "unique",
            "min_element",
            "max_element",
            "minmax_element",
            "rotate",
            "shuffle",
            "partition",
            "stable_partition",
            "nth_element",
            "generate",
            "equal",
            "swap_ranges",
            "set_difference",
            "set_intersection",
            "search",
            "adjacent_find",
            "clamp",
        ),
        ("algorithm",),
    ),
    (
        "<numeric>",
        (
            "accumulate",
            "iota",
            "inner_product",
            "partial_sum",
            "reduce",
            "transform_reduce",
            "gcd",
            "lcm",
            "midpoint",
        ),
        ("numeric",),
    ),
    (
        "<cmath>",
        (
            "sqrt",
            "cbrt",
            "sin",
            "cos",
            "tan",
            "asin",
            "acos",
            "atan",
            "atan2",
            "pow",
            "exp",
            "exp2",
            "log",
            "log2",
            "log10",
            "fabs",
            "floor",
            "ceil",
            "round",
            "lround",
            "trunc",
            "fmod",
            "hypot",
            "isfinite",
            "isnan",
            "isinf",
            "copysign",
            "signbit",
            "nextafter",
            "lerp",
        ),
        ("cmath",),
    ),
    (
        "<cstdint>",
        (
            "int8_t",
            "int16_t",
            "int32_t",
            "int64_t",
            "uint8_t",
            "uint16_t",
            "uint32_t",
            "uint64_t",
            "intptr_t",
            "uintptr_t",
            "intmax_t",
            "uintmax_t",
        ),
        ("cstdint",),
    ),
    (
        "<cstddef>",
        ("size_t", "ptrdiff_t", "byte", "nullptr_t", "max_align_t"),
        ("cstddef", "cstdint", "cstdio", "cstring", "cstdlib", "ctime"),
    ),
    (
        "<cstring>",
        (
            "memcpy",
            "memmove",
            "memset",
            "memcmp",
            "strlen",
            "strcmp",
            "strncmp",
            "strcpy",
            "strncpy",
            "strchr",
            "strstr",
        ),
        ("cstring",),
    ),
    (
        "<cstdlib>",
        (
            "malloc",
            "free",
            "calloc",
            "realloc",
            "atoi",
            "atof",
            "strtol",
            "strtod",
            "exit",
            "abort",
            "getenv",
            "qsort",
            "rand",
            "srand",
            "div",
            "labs",
        ),
        ("cstdlib",),
    ),
    (
        "<cstdio>",
        (
            "printf",
            "fprintf",
            "sprintf",
            "snprintf",
            "sscanf",
            "fopen",
            "fclose",
            "fread",
            "fwrite",
            "fputs",
            "fgets",
            "FILE",
            "stderr",
            "stdout",
            "perror",
        ),
        ("cstdio",),
    ),
    ("<limits>", ("numeric_limits",), ("limits",)),
    (
        "<type_traits>",
        (
            "is_same",
            "is_same_v",
            "enable_if",
            "enable_if_t",
            "decay",
            "decay_t",
            "remove_reference",
            "remove_reference_t",
            "remove_cv",
            "remove_cv_t",
            "remove_cvref_t",
            "conditional",
            "conditional_t",
            "underlying_type",
            "underlying_type_t",
            "void_t",
            "common_type",
            "common_type_t",
            "is_base_of",
            "is_base_of_v",
            "is_integral",
            "is_integral_v",
            "is_floating_point",
            "is_floating_point_v",
            "is_enum",
            "is_enum_v",
            "is_pointer",
            "is_pointer_v",
            "is_trivially_copyable",
            "is_trivially_copyable_v",
            "is_convertible",
            "is_convertible_v",
            "is_invocable",
            "is_invocable_v",
            "true_type",
            "false_type",
            "integral_constant",
            "add_pointer_t",
            "make_signed_t",
            "make_unsigned_t",
        ),
        ("type_traits",),
    ),
    (
        "<atomic>",
        (
            "atomic",
            "atomic_flag",
            "memory_order",
            "memory_order_relaxed",
            "memory_order_acquire",
            "memory_order_release",
            "memory_order_seq_cst",
            "atomic_thread_fence",
        ),
        ("atomic",),
    ),
    (
        "<mutex>",
        (
            "mutex",
            "recursive_mutex",
            "lock_guard",
            "unique_lock",
            "scoped_lock",
            "once_flag",
            "call_once",
            "defer_lock",
            "adopt_lock",
        ),
        ("mutex",),
    ),
    ("<shared_mutex>", ("shared_mutex", "shared_lock"), ("shared_mutex",)),
    (
        "<condition_variable>",
        ("condition_variable", "condition_variable_any"),
        ("condition_variable",),
    ),
    ("<thread>", ("thread", "jthread", "this_thread"), ("thread",)),
    ("<chrono>", ("chrono",), ("chrono",)),
    (
        "<random>",
        (
            "mt19937",
            "mt19937_64",
            "random_device",
            "uniform_int_distribution",
            "uniform_real_distribution",
            "normal_distribution",
            "bernoulli_distribution",
            "discrete_distribution",
            "seed_seq",
            "default_random_engine",
            "minstd_rand",
            "shuffle_order_engine",
        ),
        ("random",),
    ),
    ("<filesystem>", ("filesystem",), ("filesystem",)),
    (
        "<sstream>",
        ("ostringstream", "istringstream", "stringstream", "wstringstream"),
        ("sstream",),
    ),
    ("<ostream>", ("ostream", "endl", "flush"), ("ostream", "iostream", "sstream")),
    ("<istream>", ("istream",), ("istream", "iostream", "sstream")),
    ("<iostream>", ("cout", "cerr", "cin", "clog"), ("iostream",)),
    ("<iomanip>", ("setprecision", "setw", "setfill"), ("iomanip",)),
    (
        "<ios>",
        ("hex", "dec", "oct", "fixed", "scientific", "boolalpha", "showpoint"),
        ("ios", "iomanip", "iostream", "ostream", "istream", "sstream", "fstream"),
    ),
    ("<fstream>", ("ifstream", "ofstream", "fstream"), ("fstream",)),
    (
        "<iterator>",
        (
            "begin",
            "end",
            "cbegin",
            "cend",
            "rbegin",
            "rend",
            "size",
            "data",
            "distance",
            "advance",
            "next",
            "prev",
            "back_inserter",
            "front_inserter",
            "inserter",
            "iterator_traits",
        ),
        ("iterator", "array", "vector", "string", "map", "set", "span"),
    ),
    ("<initializer_list>", ("initializer_list",), ("initializer_list",)),
    ("<bitset>", ("bitset",), ("bitset",)),
    (
        "<stdexcept>",
        (
            "runtime_error",
            "logic_error",
            "invalid_argument",
            "out_of_range",
            "length_error",
            "domain_error",
            "overflow_error",
        ),
        ("stdexcept",),
    ),
    (
        "<exception>",
        (
            "exception",
            "terminate",
            "current_exception",
            "rethrow_exception",
            "exception_ptr",
        ),
        ("exception", "stdexcept"),
    ),
    ("<charconv>", ("from_chars", "to_chars", "chars_format"), ("charconv",)),
    (
        "<bit>",
        (
            "bit_cast",
            "popcount",
            "countl_zero",
            "countr_zero",
            "has_single_bit",
            "bit_width",
            "rotl",
            "rotr",
            "endian",
        ),
        ("bit",),
    ),
    ("<ranges>", ("ranges",), ("ranges",)),
    ("<numbers>", ("numbers",), ("numbers",)),
    ("<format>", ("format", "format_to", "vformat"), ("format",)),
    (
        "<ctime>",
        ("time_t", "tm", "localtime", "gmtime", "strftime", "mktime", "difftime"),
        ("ctime",),
    ),
)


INCLUDE_ROOTS = ("", "game")

BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_COMMENT = re.compile(r"//[^\n]*")
RAW_STRING = re.compile(r'R"([^(]*)\((.*?)\)\1"', re.DOTALL)
STRING_LITERAL = re.compile(r'"(?:[^"\\\n]|\\.)*"')
CHAR_LITERAL = re.compile(r"'(?:[^'\\\n]|\\.)*'")
INCLUDE_LINE = re.compile(r'^\s*#\s*include\s*([<"])([^">]+)[>"]', re.MULTILINE)


def strip_noncode(text):
    """Blank out comments and literals, keeping line numbers intact."""

    def blank(match):
        return re.sub(r"[^\n]", " ", match.group(0))

    for pattern in (
        BLOCK_COMMENT,
        RAW_STRING,
        LINE_COMMENT,
        STRING_LITERAL,
        CHAR_LITERAL,
    ):
        text = pattern.sub(blank, text)
    return text


def parse_includes(text):
    """The angle-bracket and quoted includes a file spells for itself."""
    angled, quoted = set(), []
    for kind, name in INCLUDE_LINE.findall(text):
        if kind == "<":
            angled.add(name)
        else:
            quoted.append(name)
    return angled, quoted


def resolve_quoted(rel, name):
    """Repo-relative path of a `#include "..."`, or None if it is not ours.

    Searched the way the compiler searches: the including file's own directory
    first (where a target's -I${CMAKE_CURRENT_SOURCE_DIR} would land it), then
    INCLUDE_ROOTS -- the repo root, which every target adds, and game/, which
    the game and test targets add.
    """
    base = Path(rel).parent
    for root in INCLUDE_ROOTS:
        for candidate in ((base / name), (Path(root) / name) if root else Path(name)):
            resolved = os.path.normpath(candidate.as_posix())
            if not resolved.startswith("..") and (ROOT / resolved).is_file():
                return resolved
    return None


def build_include_index(sources):
    """Per file: the standard headers it includes and the first-party ones."""
    index = {}
    for rel in sources:
        text = (ROOT / rel).read_text(encoding="utf-8", errors="replace")
        angled, quoted = parse_includes(text)
        index[rel] = (
            angled,
            [r for r in (resolve_quoted(rel, name) for name in quoted) if r],
        )
    return index


def reachable_standard_headers(rel, index, cache):
    """Standard headers reachable through a file's own first-party includes.

    Qt and third-party headers are deliberately not followed: relying on
    <QString> to hand you <string> is the same bet as relying on libstdc++ to
    hand you <array>, and it is the bet that fails on the other toolchain.
    """
    if rel in cache:
        return cache[rel]
    cache[rel] = set()
    angled, quoted = index.get(rel, (set(), []))
    result = set(angled)
    for dependency in quoted:
        if dependency in index:
            result |= reachable_standard_headers(dependency, index, cache)
    cache[rel] = result
    return result


def missing_includes(sources):
    """(file, line, std::name, header) for every unincluded facility used.

    Reads STD_FACILITIES, whose entries are (header to name in the report, the
    std:: names that need it, the headers that count as providing them). That
    third field is wider than the first wherever the standard actually says so
    -- <ostream> arrives with <iostream>, std::hex is <ios> and every stream
    header includes it -- and is a single entry wherever it does not, because
    "MSVC happens to include it today" is the assumption this pass exists to
    stop.

    Only names spelled with an explicit `std::` are searched for, so an
    unqualified `size_t`, or a `sort()` of our own, never registers, and
    comments and literals are blanked first so that prose about std::array is
    not mistaken for a use of it.
    """
    index = build_include_index(sources)
    cache = {}
    findings = []
    for rel in sources:
        text = strip_noncode((ROOT / rel).read_text(encoding="utf-8", errors="replace"))
        if "std::" not in text:
            continue
        available = reachable_standard_headers(rel, index, cache)
        for header, names, accepted in STD_FACILITIES:
            if available & set(accepted):
                continue
            pattern = re.compile(r"\bstd::(" + "|".join(names) + r")\b")
            match = pattern.search(text)
            if match is None:
                continue
            line = text.count("\n", 0, match.start()) + 1
            findings.append((rel, line, f"std::{match.group(1)}", header))
    return findings


def check_includes():
    """Every file must include the standard headers it uses itself.

    MSVC's standard library does not hand out <array>, <span> or <optional> as
    a side effect of including something else, the way libstdc++ and libc++ do.
    A file that names std::array without including <array> therefore builds in
    every Linux and macOS lane and fails only on Windows, where it is a hard
    error and not a warning -- and no compiler on this machine can see it,
    because both of the standard libraries here leak the header.  This pass
    reads the include graph instead of compiling anything, so it is the same
    answer on any machine.

    A first-party header in the include chain counts: what must not count is a
    standard header arriving through another standard header.
    """
    sources = [
        f
        for f in tracked_files()
        if f.endswith((".cpp", ".h", ".hpp", ".cc")) and not f.startswith(SKIP_PREFIXES)
    ]
    findings = missing_includes(sources)
    errors = [
        f"{rel}:{line}: uses {name} without {header}; MSVC's standard library "
        "does not provide it transitively"
        for rel, line, name, header in findings
    ]
    return errors, [f"{len(sources)} sources checked for unincluded standard headers"]


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
    parser.add_argument("--only", default="apple,glsl,windows,includes")
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

    if "includes" in selected:
        print("[includes] standard headers spelled out  (MSVC's leaner stdlib)")
        errors, notes = check_includes()
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
