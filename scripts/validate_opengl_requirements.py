#!/usr/bin/env python3
"""OpenGL 3.3 Core Profile Requirements Validation

Checks that the codebase cannot accidentally regress from OpenGL 3.3 Core
Profile to Compatibility Profile or GLSL ES shaders.  No display or GPU
required — safe to run as a static analysis step on any CI runner.

OpenGL 3.3 Core remains the hardware floor.  A small, explicitly listed set of
shaders targets GLSL 4.30 for the optional GPU crowd-culling path; those are
reached only when the driver advertises compute shaders, SSBOs and indirect
draw, and every one of them must fail soft to the 3.3 path.  Adding a 4.30
shader without listing it here, or listing one that is not capability gated,
fails this check.

The capability probes gate on the context *version* (4.3) and nothing else.
That is deliberate, and it is the second half of the same rule as the shader
headers: a '#version 430' shader needs GLSL 4.30, which needs OpenGL 4.3, so
4.3 is both necessary and sufficient.  Probing the extension string instead is
wrong in both directions — an extension can be advertised on a 3.3 context,
where the shader cannot compile and the driver crashes partway through the
frame, and a strict 4.3+ core profile may omit the string precisely because
compute is core there, which would switch the fast path off on good hardware.

Checks:
  1. Every baseline shader (.vert/.frag) must start with '#version 330 core'.
     Shaders in OPTIONAL_GL43_SHADERS may declare '#version 430 core'.
  2. Every optional 4.30 shader must be gated behind a capability probe, and
     its owning pipeline must degrade instead of hard-failing.
  3. main.cpp must not set QSurfaceFormat::CompatibilityProfile, and must not
     request a context above 3.3 — raising the request raises the floor.
  4. Every shader, including compute shaders, must be compiled into assets.qrc.
  5. Every release workflow must execute the packaged renderer self-test, and
     must assert the driver actually granted the 3.3 Core floor.  Requesting a
     context is not the same as getting one: macOS caps OpenGL at 4.1 and can
     hand back a 2.1 compatibility context, which draws nothing.

Usage:
    python3 scripts/validate_opengl_requirements.py
"""

import sys
from pathlib import Path

OPTIONAL_GL43_SHADERS = {
    "character_skinned_gpudriven.vert",
    "directional_shadow_rigged_gpudriven.vert",
    "rigged_cull.comp",
    "rigged_cull_finalize.comp",
}


OPTIONAL_PATH_OWNER = Path("render/gl/backend/rigged_cull_pipeline.cpp")
REQUIRED_CAPABILITY_PROBES = (
    "GLCapabilities::has_compute_shaders()",
    "GLCapabilities::has_indirect_draw()",
)


def check_shader_versions(shader_dir: Path) -> list[str]:
    errors: list[str] = []
    shaders = sorted(
        list(shader_dir.glob("*.vert"))
        + list(shader_dir.glob("*.frag"))
        + list(shader_dir.glob("*.comp"))
    )
    if not shaders:
        errors.append(f"No shaders found in {shader_dir}")
        return errors
    for path in shaders:
        lines = path.read_text(encoding="utf-8").splitlines()
        first = lines[0].strip() if lines else ""
        if path.name in OPTIONAL_GL43_SHADERS:
            if first != "#version 430 core":
                errors.append(
                    f"  {path.name}: listed as an optional 4.30 shader but"
                    f" declares '{first}'"
                )
        elif first != "#version 330 core":
            errors.append(
                f"  {path.name}: first line is '{first}'"
                f" — expected '#version 330 core'."
                f" A 4.30 shader must be added to OPTIONAL_GL43_SHADERS and"
                f" reached only behind a capability probe."
            )
    on_disk = {p.name for p in shaders}
    for name in sorted(OPTIONAL_GL43_SHADERS - on_disk):
        errors.append(f"  {name}: listed in OPTIONAL_GL43_SHADERS but not on disk")
    return errors


def check_optional_path_is_gated(root: Path) -> list[str]:
    errors: list[str] = []
    if not OPTIONAL_GL43_SHADERS:
        return errors
    owner = root / OPTIONAL_PATH_OWNER
    if not owner.exists():
        return [f"{OPTIONAL_PATH_OWNER} not found; optional 4.30 shaders are ungated"]
    content = owner.read_text(encoding="utf-8")
    for probe in REQUIRED_CAPABILITY_PROBES:
        if probe not in content:
            errors.append(f"  {OPTIONAL_PATH_OWNER}: missing capability probe {probe}")
    for name in sorted(OPTIONAL_GL43_SHADERS):
        if name not in content:
            errors.append(
                f"  {name}: not referenced by {OPTIONAL_PATH_OWNER};"
                f" its capability gate cannot be verified"
            )
    return errors


def check_no_compat_profile(root: Path) -> list[str]:
    errors: list[str] = []
    for relative in ("main.cpp", "tools/arena/main.cpp"):
        source = root / relative
        if not source.exists():
            errors.append(f"{relative} not found")
            continue
        content = source.read_text(encoding="utf-8")
        if "CompatibilityProfile" in content:
            errors.append(
                f"{relative}: QSurfaceFormat::CompatibilityProfile is present."
                " All render classes derive from QOpenGLFunctions_3_3_Core and"
                " require Core Profile — do not override to CompatibilityProfile."
            )
        if "setVersion(3, 3)" not in content:
            errors.append(
                f"{relative}: does not request OpenGL 3.3."
                " Requesting a higher context raises the hardware floor;"
                " optional features must be probed at runtime instead."
            )
    return errors


def check_embedded_shaders(root: Path, shader_dir: Path) -> list[str]:
    qrc = root / "assets.qrc"
    if not qrc.exists():
        return ["assets.qrc not found"]
    content = qrc.read_text(encoding="utf-8")
    return [
        f"  {path.name}: missing from assets.qrc"
        for path in sorted(shader_dir.glob("*"))
        if path.suffix in {".vert", ".frag", ".comp"}
        and f"<file>assets/shaders/{path.name}</file>" not in content
    ]


def check_release_renderer_self_tests(root: Path) -> list[str]:
    errors: list[str] = []
    main_content = (root / "main.cpp").read_text(encoding="utf-8")
    for marker in ("--renderer-self-test", "SOI_RENDERER_SELF_TEST: PASS"):
        if marker not in main_content:
            errors.append(f"main.cpp: missing renderer self-test marker {marker!r}")

    for platform in ("windows", "macos", "linux"):
        workflow = root / ".github" / "workflows" / f"build-{platform}.yml"
        if not workflow.exists():
            errors.append(f"{workflow.relative_to(root)} not found")
            continue
        content = workflow.read_text(encoding="utf-8")
        if "--renderer-self-test" not in content:
            errors.append(
                f"{workflow.relative_to(root)}: packaged renderer test missing"
            )
        if "SOI_RENDERER_SELF_TEST: PASS" not in content:
            errors.append(
                f"{workflow.relative_to(root)}: strict PASS marker check missing"
            )
        if "SOI_GL_FLOOR: PASS" not in content:
            errors.append(
                f"{workflow.relative_to(root)}: does not assert the OpenGL 3.3"
                " Core floor was actually granted"
            )
    return errors


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    shader_dir = root / "assets" / "shaders"

    print("=== OpenGL 3.3 Core Profile Requirements ===")
    total_errors: list[str] = []

    shaders = sorted(
        list(shader_dir.glob("*.vert"))
        + list(shader_dir.glob("*.frag"))
        + list(shader_dir.glob("*.comp"))
    )
    baseline = len(shaders) - len(OPTIONAL_GL43_SHADERS)
    print(f"\n[1/5] Shader GLSL version headers  ({len(shaders)} files)")
    errs = check_shader_versions(shader_dir)
    if errs:
        total_errors.extend(errs)
        for e in errs:
            print(f"  FAIL {e}")
    else:
        print(
            f"  OK   {baseline} shaders declare '#version 330 core';"
            f" {len(OPTIONAL_GL43_SHADERS)} optional 4.30 shaders listed"
        )

    print("\n[2/5] Optional 4.30 path is capability gated")
    errs = check_optional_path_is_gated(root)
    if errs:
        total_errors.extend(errs)
        for e in errs:
            print(f"  FAIL {e}")
    else:
        print("  OK   every 4.30 shader sits behind a runtime capability probe")

    print("\n[3/5] Surface format profile  (entry points)")
    errs = check_no_compat_profile(root)
    if errs:
        total_errors.extend(errs)
        for e in errs:
            print(f"  FAIL {e}")
    else:
        print("  OK   Core Profile, OpenGL 3.3 requested by every entry point")

    print("\n[4/5] Embedded shader resources  (assets.qrc)")
    errs = check_embedded_shaders(root, shader_dir)
    if errs:
        total_errors.extend(errs)
        for e in errs:
            print(f"  FAIL {e}")
    else:
        print(f"  OK   all {len(shaders)} shaders are embedded")

    print("\n[5/5] Packaged renderer self-tests  (release workflows)")
    errs = check_release_renderer_self_tests(root)
    if errs:
        total_errors.extend(errs)
        for e in errs:
            print(f"  FAIL {e}")
    else:
        print("  OK   Windows, macOS and Linux require a presented gameplay frame")

    print()
    if total_errors:
        print(f"FAILED — {len(total_errors)} error(s).")
        return 1
    print("PASSED — OpenGL 3.3 Core Profile requirements satisfied.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
