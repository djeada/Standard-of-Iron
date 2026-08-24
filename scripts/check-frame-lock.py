#!/usr/bin/env python3
"""QML entry points must not read simulation state directly.

Three threads share the world: the Qt GUI thread runs QML, a simulation thread
runs `GameEngine::simulate`, and the scene-graph render thread runs
`GameEngine::update_presentation` and `GameEngine::render`. The simulation and
render threads both take `GameEngine::m_frame_mutex`, so they never see each
other's half-written state. QML has no such guarantee: a `Q_INVOKABLE` or a
`Q_PROPERTY READ` runs on the GUI thread the moment a binding re-evaluates or a
Timer fires, right in the middle of a render-thread frame.

Reaching the world from there without the frame lock is a data race, and the
failure mode is not a wrong pixel: `ActivityViewModel::pop_combat_damage_events`
drained `CombatFeedbackStore::m_pending` from a 33 ms QML Timer while the render
thread was inside `CombatFeedbackStore::update`, and the render thread walked
off the end of the buffer.

Taking the frame lock on the read path fixes the race but not the cost -- the
lock is held for a whole presentation pass, and measured in a real match the GUI
thread took it about ninety times per rendered frame and blocked for hundreds of
milliseconds in the worst case. So shared state is split three ways:

  * State that genuinely crosses threads as a queue owns its own lock, next to
    the data (`CombatFeedbackStore`, `PlayerFeedbackBus`). Callers cannot get it
    wrong and the GUI thread never waits on frame work.
  * State QML polls every frame is published once per frame by
    `GameEngine::publish_frame_snapshots`, on the thread that already holds the
    frame lock, and read back through `App::Core::Published` (see
    app/core/published.h). Reads never block.
  * Commands -- input handlers that mutate the world -- still take
    `m_host.lock_frame()`. They are user-driven, not per-frame.

This check enforces the first rule of that split: a Q_INVOKABLE or Q_PROPERTY
READ body may not reach world/session/selection state unless it holds the frame
lock. Bodies that only read a published snapshot touch none of it and pass.

To exempt a body that is genuinely safe, put a reason on it:

    // frame-lock-exempt: reads an atomic snapshot published by the GUI thread

  usage: check-frame-lock.py [repo-root]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

SOURCES = (
    ("app/viewmodels", None),
    ("app/core", re.compile(r"game_engine.*\.cpp$")),
)


SHARED = re.compile(
    r"""
      m_context\.(?:world|selection|input|cursor|session|visibility|camera
                 |active_camera|camera_controller|commands|production|picking
                 |rts_camera|commander_camera)\b
    | \bm_world\b
    | \bm_session\b
    | \bm_control\b
    | get_system\s*<
    | ->\s*get_entity\s*\(
    | collect_entities_with
    | entities_with\s*[<(]
    """,
    re.VERBOSE,
)

HELD = re.compile(r"lock_frame\s*\(|frame_lock\b|m_frame_mutex")
EXEMPT = re.compile(r"//\s*frame-lock-exempt:")


def qml_surface(header: Path) -> set[str]:
    """Names declared Q_INVOKABLE or exposed as a Q_PROPERTY READ."""
    text = header.read_text(errors="ignore")
    names = {m.group(1) for m in re.finditer(r"Q_INVOKABLE[^;{]*?(\w+)\s*\(", text)}
    names |= {
        m.group(1) for m in re.finditer(r"Q_PROPERTY\([^)]*?\bREAD\s+(\w+)", text, re.S)
    }
    return names


def body_of(text: str, start: int) -> tuple[str, int] | None:
    """Return the brace-matched body that begins at or after `start`."""
    open_at = text.find("{", start)
    if open_at < 0:
        return None
    depth = 0
    for i in range(open_at, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[open_at : i + 1], i
    return None


def definitions(text: str, name: str):
    """Yield (body, line) for every out-of-line definition of `name` in `text`."""
    pattern = re.compile(
        r"\n[\w:<>&*\[\] ]*?\b\w+::" + re.escape(name) + r"\s*\([^;{]*?\{",
        re.S,
    )
    for match in pattern.finditer(text):
        found = body_of(text, match.start())
        if found is None:
            continue
        body, _ = found
        yield body, text.count("\n", 0, match.start()) + 2


PUBLISH_CALL = re.compile(r"(?<!::)\bpublish_frame\s*\(\s*\)")


PUBLISH_DEFINITION = re.compile(r"\bGameEngine::publish_frame_snapshots\s*\([^;{]*\{")


def check_publish_sites(root: Path) -> list[str]:
    findings: list[str] = []
    for directory in ("app", "ui", "tools"):
        base = root / directory
        if not base.is_dir():
            continue
        for source in sorted(base.rglob("*.cpp")):
            text = source.read_text(errors="ignore")
            if not PUBLISH_CALL.search(text):
                continue

            allowed = (0, 0)
            definition = PUBLISH_DEFINITION.search(text)
            if definition is not None:
                found = body_of(text, definition.start())
                if found is not None:
                    allowed = (definition.start(), found[1])

            for match in PUBLISH_CALL.finditer(text):
                if allowed[0] <= match.start() < allowed[1]:
                    continue
                line = text.count("\n", 0, match.start()) + 1
                findings.append(
                    f"{source.relative_to(root)}:{line}: publish_frame() called "
                    "outside GameEngine::publish_frame_snapshots(), where the "
                    "frame lock is not held"
                )
    return findings


def check(root: Path) -> list[str]:
    findings: list[str] = []
    for directory, name_filter in SOURCES:
        base = root / directory
        if not base.is_dir():
            continue
        for source in sorted(base.glob("*.cpp")):
            if name_filter is not None and not name_filter.search(source.name):
                continue
            header = source.with_suffix(".h")
            if not header.exists():

                header = base / (
                    re.sub(r"_[a-z]+\.cpp$", ".cpp", source.name)[:-4] + ".h"
                )
            if not header.exists():
                continue
            surface = qml_surface(header)
            if not surface:
                continue
            text = source.read_text(errors="ignore")
            for name in sorted(surface):
                for body, line in definitions(text, name):
                    if HELD.search(body) or EXEMPT.search(body):
                        continue
                    hit = SHARED.search(body)
                    if hit is None:
                        continue
                    rel = source.relative_to(root)
                    findings.append(
                        f"{rel}:{line}: {name}() reaches {hit.group(0).strip()} "
                        f"from the QML thread without the frame lock"
                    )
    return findings


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    findings = check(root) + check_publish_sites(root)
    if findings:
        print("QML entry points touching simulation state without the frame lock:\n")
        for finding in findings:
            print(f"  {finding}")
        print(
            f"\n{len(findings)} unguarded entry point(s). Take "
            "`m_host.lock_frame()` at the top of the body, or mark it "
            "`// frame-lock-exempt: <reason>` if it is genuinely safe."
        )
        return 1
    print("✓ Every QML entry point that touches simulation state holds the frame lock")
    return 0


if __name__ == "__main__":
    sys.exit(main())
