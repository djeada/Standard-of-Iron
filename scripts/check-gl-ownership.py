#!/usr/bin/env python3
"""Fail the build when the renderer owns a GL object by hand.

Every GL object under render/ is owned by a move-only handle from
render/gl/gl_handle.h (BufferHandle, TextureHandle, FenceHandle, ...). A handle
releases its object through the deferred-delete queue, so it is safe to destroy
on any thread and nothing needs a hand-written shutdown() to avoid leaking.
That only holds while nobody bypasses the handles, so three things are checked:

  * glGen*/glCreate{Shader,Program,...}/glDelete*/glFenceSync are called only in
    the handle implementation (render/gl/gl_handle.cpp) and the deferred-delete
    queue (render/gl/gl_lifetime.cpp);
  * no file under render/ outside render/gl/ (GL objects and wrappers) and
    render/backend/ (pipelines and executors) calls a gl* function at all:
    those are the only layers that talk to the driver;
  * no header under render/ declares a raw GLuint/GLsync member whose name says
    it owns an object (vao, buffer, texture, fbo, ...). A member that only
    borrows a name owned elsewhere is declared as `BorrowedGlName`;
  * no process-wide singleton (a header declaring `instance()`) holds a GL
    object: GL state belongs to a backend's RenderContext, created on its
    context and destroyed with it, so two renderers never share or outlive it.
    Mesh (render/geom/mesh.h) is geometry and holds no GL object either; each
    backend uploads its own copy through its MeshGpuTable.

  usage: check-gl-ownership.py [repo-root]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

HANDLE_IMPLEMENTATION = {
    "render/gl/gl_handle.cpp",
    "render/gl/gl_lifetime.cpp",
}

GL_LAYERS = ("render/gl/", "render/backend/")

LIFETIME_CALL = re.compile(
    r"""\bgl(?:
        Gen(?:Buffers|VertexArrays|Textures|Framebuffers|Renderbuffers|Samplers|Queries
            |TransformFeedbacks|ProgramPipelines)
      | Create(?:Shader|Program|Buffers|Textures|VertexArrays|Framebuffers
            |Renderbuffers|Samplers|Queries)
      | Delete[A-Za-z]*
      | FenceSync
    )\s*\(""",
    re.VERBOSE,
)

ANY_GL_CALL = re.compile(r"(?<![\w.])(?:[\w]+(?:->|\.))?gl[A-Z][A-Za-z0-9]*\s*\(")

RAW_HANDLE_MEMBER = re.compile(
    r"""^\s*(?:GLuint|GLsync|unsigned\s+int)\s+
        (?P<name>\w*(?:vao|vbo|ubo|ssbo|buffer|texture|fbo|framebuffer|renderbuffer
            |sampler|query|queries|program|fence)\w*)
        \s*(?:\{[^}]*\}|=\s*[^;]+)?;""",
    re.VERBOSE | re.IGNORECASE,
)

COMMENT = re.compile(r"//.*$")

SINGLETON = re.compile(r"\binstance\(\)\s*->")

GL_OWNING_MEMBER = re.compile(
    r"""\b(?:(?:Buffer|VertexArray|Texture|Program|Shader|Framebuffer|Renderbuffer
        |Sampler|Query|Fence|Gl)Handle|VertexArray|Buffer|Texture|Shader|RiggedMesh|MeshGpuTable
        |RenderContext|ResourceManager|ShaderCache)\b[^;()]*\bm_\w+""",
    re.VERBOSE,
)


def sources(root: Path) -> list[Path]:
    directory = root / "render"
    if not directory.is_dir():
        return []
    return sorted(p for p in directory.rglob("*") if p.suffix in (".h", ".cpp"))


def strip_strings(line: str) -> str:
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', line)


GEOMETRY_ONLY = ("render/geom/mesh.h",)


def check(root: Path) -> list[str]:
    problems: list[str] = []
    for relative in GEOMETRY_ONLY:
        path = root / relative
        if not path.exists():
            continue
        for number, raw in enumerate(path.read_text(errors="replace").splitlines(), 1):
            code = COMMENT.sub("", raw)
            if GL_OWNING_MEMBER.search(code) or "render/gl/buffer.h" in code:
                problems.append(
                    f"{relative}:{number}: Mesh is geometry; its GL objects belong "
                    "in a MeshGpuTable"
                )
    for source in sources(root):
        relative = source.relative_to(root).as_posix()
        in_backend = relative.startswith(GL_LAYERS)
        text = source.read_text(errors="replace")
        singleton = source.suffix == ".h" and SINGLETON.search(text) is not None
        for number, raw in enumerate(
            source.read_text(errors="replace").splitlines(), 1
        ):
            code = COMMENT.sub("", strip_strings(raw))
            where = f"{relative}:{number}"
            if relative not in HANDLE_IMPLEMENTATION and LIFETIME_CALL.search(code):
                problems.append(
                    f"{where}: GL object created or deleted by hand; use a handle "
                    "from render/gl/gl_handle.h"
                )
            if not in_backend and ANY_GL_CALL.search(code):
                problems.append(
                    f"{where}: gl* call outside render/gl/ and render/backend/; "
                    "move it behind a render/gl interface"
                )
            if source.suffix == ".h" and singleton and GL_OWNING_MEMBER.search(code):
                problems.append(
                    f"{where}: a process-wide singleton holds a GL object; own it "
                    "in the backend's RenderContext instead"
                )
            if source.suffix == ".h" and RAW_HANDLE_MEMBER.search(code):
                problems.append(
                    f"{where}: raw GL name member; own it with a handle, or "
                    "declare a borrowed name as BorrowedGlName"
                )
    return problems


def main(argv: list[str]) -> int:
    root = Path(argv[1]) if len(argv) > 1 else Path(__file__).resolve().parent.parent
    problems = check(root.resolve())
    if problems:
        print("GL ownership violations:")
        for problem in problems:
            print(f"  {problem}")
        print(f"{len(problems)} violation(s). See render/gl/gl_handle.h.")
        return 1
    print("GL ownership: every GL object under render/ is owned by a handle")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
