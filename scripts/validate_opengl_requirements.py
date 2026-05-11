#!/usr/bin/env python3
"""OpenGL 3.3 Core Profile Requirements Validation

Checks that the codebase cannot accidentally regress from OpenGL 3.3 Core
Profile to Compatibility Profile or GLSL ES shaders.  No display or GPU
required — safe to run as a static analysis step on any CI runner.

Checks:
  1. Every shader file (.vert/.frag) must start with '#version 330 core'.
  2. main.cpp must not set QSurfaceFormat::CompatibilityProfile.
     All render classes derive from QOpenGLFunctions_3_3_Core, which on
     some Windows drivers fails to initialize when the context profile is
     Compatibility rather than Core — the historical root cause of the
     blank-screen bug.

Usage:
    python3 scripts/validate_opengl_requirements.py
"""

import sys
from pathlib import Path


def check_shader_versions(shader_dir: Path) -> list[str]:
    errors: list[str] = []
    shaders = sorted(
        list(shader_dir.glob("*.vert")) + list(shader_dir.glob("*.frag"))
    )
    if not shaders:
        errors.append(f"No shaders found in {shader_dir}")
        return errors
    for path in shaders:
        lines = path.read_text(encoding="utf-8").splitlines()
        first = lines[0].strip() if lines else ""
        if first != "#version 330 core":
            errors.append(
                f"  {path.name}: first line is '{first}'"
                f" — expected '#version 330 core'"
            )
    return errors


def check_no_compat_profile(root: Path) -> list[str]:
    errors: list[str] = []
    main_cpp = root / "main.cpp"
    if not main_cpp.exists():
        errors.append("main.cpp not found")
        return errors
    content = main_cpp.read_text(encoding="utf-8")
    if "CompatibilityProfile" in content:
        errors.append(
            "main.cpp: QSurfaceFormat::CompatibilityProfile is present."
            " All render classes derive from QOpenGLFunctions_3_3_Core and"
            " require Core Profile — do not override to CompatibilityProfile."
        )
    return errors


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    shader_dir = root / "assets" / "shaders"

    print("=== OpenGL 3.3 Core Profile Requirements ===")
    total_errors: list[str] = []

    shaders = sorted(
        list(shader_dir.glob("*.vert")) + list(shader_dir.glob("*.frag"))
    )
    print(f"\n[1/2] Shader GLSL version headers  ({len(shaders)} files)")
    errs = check_shader_versions(shader_dir)
    if errs:
        total_errors.extend(errs)
        for e in errs:
            print(f"  FAIL {e}")
    else:
        print(f"  OK   all {len(shaders)} shaders declare '#version 330 core'")

    print("\n[2/2] Surface format profile  (main.cpp)")
    errs = check_no_compat_profile(root)
    if errs:
        total_errors.extend(errs)
        for e in errs:
            print(f"  FAIL {e}")
    else:
        print("  OK   QSurfaceFormat::CompatibilityProfile is not present")

    print()
    if total_errors:
        print(f"FAILED — {len(total_errors)} error(s).")
        return 1
    print("PASSED — OpenGL 3.3 Core Profile requirements satisfied.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
