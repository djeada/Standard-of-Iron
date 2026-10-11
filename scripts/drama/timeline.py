"""Lay an episode's beats out in time.

A scene is a run of beats: a line (its recorded length), a hold (silence), or a
mark (a named instant that staging and sound cues hang off). Every time a stage
step, camera cut or sound cue uses is resolved from here, so a re-recorded line
moves everything after it.
"""

import json
import re
from pathlib import Path


def load_json(path):
    return json.loads(Path(path).read_text())


class Timeline:
    def __init__(self, beats_path, voice_path):
        spec = load_json(beats_path)
        voice = load_json(voice_path)
        default_gap = spec.get("default_gap", 0.35)
        self.scenes = []
        self.lines = {}
        self.marks = {}
        start = 0.0
        for scene in spec["scenes"]:
            t = scene.get("pre", 0.0)
            first = True
            lines = []
            for beat in scene["beats"]:
                gap = beat.get("gap", 0.0 if first else default_gap)
                if "line" in beat:
                    t += gap
                    duration = voice[beat["line"]]["duration"]
                    entry = {
                        "gesture": beat.get("gesture"),
                        "id": beat["line"],
                        "who": voice[beat["line"]]["who"],
                        "scene": scene["id"],
                        "start": round(t, 3),
                        "end": round(t + duration, 3),
                    }
                    self.lines[beat["line"]] = entry
                    lines.append(entry)
                    t += duration
                    first = False
                elif "hold" in beat:
                    t += beat["hold"]
                elif "mark" in beat:
                    t += beat.get("gap", 0.0)
                    self.marks[f"{scene['id']}.{beat['mark']}"] = round(t, 3)
            t += scene.get("post", 0.0)
            self.scenes.append(
                {"id": scene["id"], "start": round(start, 3), "duration": round(t, 3), "lines": lines,
                 "gestures": scene.get("gestures", True)}
            )
            start += t
        self.by_scene = {s["id"]: s for s in self.scenes}
        self.total = start

    def resolve(self, expr, scene=None):
        """Scene-local seconds for 12.5, line:s03_05, line:s03_05@end+0.3,
        mark:crack-1, end-2 (end of scene)."""
        if isinstance(expr, (int, float)):
            return float(expr)
        try:
            return float(expr)
        except ValueError:
            pass
        match = re.fullmatch(r"(line|mark|end)(?::([\w.]+))?(@end)?([+-][\d.]+)?", expr.strip())
        if not match:
            raise ValueError(f"bad time expression {expr!r}")
        kind, name, at_end, offset = match.groups()
        offset = float(offset) if offset else 0.0
        if kind == "end":
            return self.by_scene[scene]["duration"] + offset
        if kind == "line":
            line = self.lines[name]
            return (line["end"] if at_end else line["start"]) + offset
        key = name if "." in name else f"{scene}.{name}"
        return self.marks[key] + offset

    def dump(self):
        return {
            "total": round(self.total, 3),
            "scenes": self.scenes,
            "marks": self.marks,
        }


if __name__ == "__main__":
    import sys

    tl = Timeline(sys.argv[1], sys.argv[2])
    for scene in tl.scenes:
        marks = {k.split(".", 1)[1]: v for k, v in tl.marks.items() if k.startswith(scene["id"] + ".")}
        print(f"{scene['id']:8s} start {scene['start']:7.2f} dur {scene['duration']:6.2f}  marks {marks}")
        for line in scene["lines"]:
            print(f"    {line['id']} {line['who']:9s} {line['start']:6.2f}-{line['end']:6.2f}")
    print(f"total {tl.total:.1f} s ({tl.total / 60:.2f} min)")
