#!/usr/bin/env python3
"""Drive the map editor on a nested X display and record it.

The editor is a widget app, so it films fine on Xvfb with software GL, and
``ffmpeg -f x11grab`` records that display at a steady frame rate. Input is
injected with python-xlib's XTEST extension because the box has no xdotool.

    scripts/film-editor.py --map river.map.json --steps steps.json --out editor.mp4

Steps are a JSON list of objects with ``do`` in: move (x,y), click (x,y,button),
dblclick (x,y), drag (x,y,x2,y2,steps), wheel (x,y,clicks), key (keysym, ctrl,
shift), type (text), sleep (seconds), record (start recording), stop,
screenshot (path). Coordinates are display pixels. A ~0.4 s dwell between a
move and a press is inserted automatically: Qt processes the press at the
previous pointer position without it.
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from Xlib import XK, X, display
from Xlib.ext import xtest

ROOT = Path(__file__).resolve().parent.parent
EDITOR = ROOT / "build" / "bin" / "map_editor"


class Driver:
    def __init__(self, disp: str):
        self.disp = display.Display(disp)
        self.root = self.disp.screen().root

    def flush(self) -> None:
        self.disp.sync()

    def move(self, x: int, y: int) -> None:
        xtest.fake_input(self.disp, X.MotionNotify, x=x, y=y)
        self.flush()

    def press(self, button: int) -> None:
        xtest.fake_input(self.disp, X.ButtonPress, button)
        self.flush()

    def release(self, button: int) -> None:
        xtest.fake_input(self.disp, X.ButtonRelease, button)
        self.flush()

    def click(self, x: int, y: int, button: int = 1, dwell: float = 0.4) -> None:
        self.move(x, y)
        time.sleep(dwell)
        self.press(button)
        time.sleep(0.08)
        self.release(button)

    def dblclick(self, x: int, y: int) -> None:
        self.click(x, y)
        time.sleep(0.12)
        self.press(1)
        time.sleep(0.06)
        self.release(1)

    def drag(
        self, x: int, y: int, x2: int, y2: int, steps: int = 24, button: int = 1
    ) -> None:
        self.move(x, y)
        time.sleep(0.4)
        self.press(button)
        time.sleep(0.1)
        for i in range(1, steps + 1):
            t = i / steps
            self.move(int(x + (x2 - x) * t), int(y + (y2 - y) * t))
            time.sleep(0.03)
        time.sleep(0.15)
        self.release(button)

    def wheel(self, x: int, y: int, clicks: int) -> None:
        self.move(x, y)
        time.sleep(0.3)
        button = 4 if clicks > 0 else 5
        for _ in range(abs(clicks)):
            self.press(button)
            self.release(button)
            time.sleep(0.12)

    def key(self, keysym_name: str, ctrl: bool = False, shift: bool = False) -> None:
        keysym = XK.string_to_keysym(keysym_name)
        code = self.disp.keysym_to_keycode(keysym)
        mods = []
        if ctrl:
            mods.append(self.disp.keysym_to_keycode(XK.string_to_keysym("Control_L")))
        if shift:
            mods.append(self.disp.keysym_to_keycode(XK.string_to_keysym("Shift_L")))
        for m in mods:
            xtest.fake_input(self.disp, X.KeyPress, m)
        xtest.fake_input(self.disp, X.KeyPress, code)
        xtest.fake_input(self.disp, X.KeyRelease, code)
        for m in reversed(mods):
            xtest.fake_input(self.disp, X.KeyRelease, m)
        self.flush()

    def type_text(self, text: str, delay: float = 0.06) -> None:
        for ch in text:
            name = {
                " ": "space",
                "\n": "Return",
                ".": "period",
                ",": "comma",
                "-": "minus",
                "_": "underscore",
                ":": "colon",
                '"': "quotedbl",
                "{": "braceleft",
                "}": "braceright",
                "[": "bracketleft",
                "]": "bracketright",
            }.get(ch, ch)
            shift = ch.isupper() or ch in '_:"{}'
            self.key(name, shift=shift)
            time.sleep(delay)

    def focus_named_window(self, needle: str) -> bool:
        def walk(win):
            try:
                name = win.get_wm_name()
            except Exception:
                name = None
            if name and needle in str(name):
                return win
            try:
                for child in win.query_tree().children:
                    found = walk(child)
                    if found:
                        return found
            except Exception:
                return None
            return None

        win = walk(self.root)
        if win is None:
            return False
        win.set_input_focus(X.RevertToParent, X.CurrentTime)
        win.configure(x=0, y=0)
        self.flush()
        return True

    def resize_named_window(self, needle: str, width: int, height: int) -> bool:
        def walk(win):
            try:
                name = win.get_wm_name()
            except Exception:
                name = None
            if name and needle in str(name):
                return win
            try:
                for child in win.query_tree().children:
                    found = walk(child)
                    if found:
                        return found
            except Exception:
                return None
            return None

        win = walk(self.root)
        if win is None:
            return False
        win.configure(x=0, y=0, width=width, height=height)
        self.flush()
        return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--map", required=True)
    parser.add_argument("--steps", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--display", default=":99")
    parser.add_argument("--size", default="1920x1080")
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--settle", type=float, default=6.0)
    args = parser.parse_args()

    width, height = (int(v) for v in args.size.split("x"))
    steps = json.loads(Path(args.steps).read_text())
    out = Path(args.out).resolve()
    out.parent.mkdir(parents=True, exist_ok=True)

    work = Path(tempfile.mkdtemp(prefix="film-editor-"))

    source = Path(args.map).resolve()
    for sibling in source.parent.glob("*.json"):
        shutil.copy(sibling, work / sibling.name)
    map_copy = work / source.name
    cfg = work / "cfg" / "djeada"
    cfg.mkdir(parents=True)
    (cfg / "StandardOfIron.ini").write_text("[audio]\nmaster_volume=0\n")

    xvfb = subprocess.Popen(
        [
            "Xvfb",
            args.display,
            "-screen",
            "0",
            f"{width}x{height}x24",
            "-nolisten",
            "tcp",
            "-ac",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    time.sleep(1.5)
    env = dict(
        os.environ,
        DISPLAY=args.display,
        XDG_CONFIG_HOME=str(work / "cfg"),
        QT_QPA_PLATFORM="xcb",
        QT_SCALE_FACTOR="1",
    )
    editor = subprocess.Popen(
        [str(EDITOR), str(map_copy)],
        env=env,
        stdout=open(work / "editor.log", "w"),
        stderr=subprocess.STDOUT,
        cwd=str(ROOT / "build" / "bin"),
    )
    recorder = None
    try:
        time.sleep(args.settle)
        drv = Driver(args.display)
        drv.resize_named_window("Editor", width, height) or drv.resize_named_window(
            "Standard", width, height
        )
        drv.focus_named_window("Editor") or drv.focus_named_window("Standard")
        time.sleep(1.0)
        for step in steps:
            do = step["do"]
            if do == "record":
                recorder = subprocess.Popen(
                    [
                        "ffmpeg",
                        "-v",
                        "error",
                        "-y",
                        "-f",
                        "x11grab",
                        "-framerate",
                        str(args.fps),
                        "-video_size",
                        f"{width}x{height}",
                        "-i",
                        f"{args.display}.0",
                        "-c:v",
                        "libx264",
                        "-preset",
                        "veryfast",
                        "-crf",
                        "18",
                        "-pix_fmt",
                        "yuv420p",
                        str(out),
                    ],
                    stdin=subprocess.PIPE,
                )
                time.sleep(0.5)
            elif do == "stop":
                if recorder:
                    recorder.stdin.write(b"q")
                    recorder.stdin.flush()
                    recorder.wait(timeout=30)
                    recorder = None
            elif do == "sleep":
                time.sleep(float(step["seconds"]))
            elif do == "move":
                drv.move(step["x"], step["y"])
            elif do == "click":
                drv.click(step["x"], step["y"], step.get("button", 1))
            elif do == "dblclick":
                drv.dblclick(step["x"], step["y"])
            elif do == "drag":
                drv.drag(
                    step["x"],
                    step["y"],
                    step["x2"],
                    step["y2"],
                    step.get("steps", 24),
                    step.get("button", 1),
                )
            elif do == "wheel":
                drv.wheel(step["x"], step["y"], step["clicks"])
            elif do == "key":
                drv.key(step["key"], step.get("ctrl", False), step.get("shift", False))
            elif do == "type":
                drv.type_text(step["text"])
            elif do == "screenshot":
                subprocess.run(
                    [
                        "ffmpeg",
                        "-v",
                        "error",
                        "-y",
                        "-f",
                        "x11grab",
                        "-video_size",
                        f"{width}x{height}",
                        "-i",
                        f"{args.display}.0",
                        "-frames:v",
                        "1",
                        step["path"],
                    ],
                    check=False,
                )
            else:
                print(f"unknown step {do}", file=sys.stderr)
        if recorder:
            recorder.stdin.write(b"q")
            recorder.stdin.flush()
            recorder.wait(timeout=30)
    finally:

        try:
            listing = subprocess.run(
                ["ps", "-eo", "pid,args"], capture_output=True, text=True
            ).stdout
            for line in listing.splitlines():
                if str(work) in line and "standard_of_iron" in line:
                    os.kill(int(line.split()[0]), signal.SIGTERM)
        except Exception:
            pass
        editor.terminate()
        try:
            editor.wait(timeout=5)
        except subprocess.TimeoutExpired:
            editor.kill()
        xvfb.send_signal(signal.SIGTERM)
        xvfb.wait(timeout=5)
    print(f"film-editor: work dir {work}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
