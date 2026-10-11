"""Turn an episode edit into a capture spec and a resolved cut list.

    python3 scripts/drama/plan.py EPISODE_DIR --vo DIR --out DIR [--quality preview|final]

Each scene is one continuous staged performance (an arena scenario). Every
camera the edit cuts to is captured over just the span it is used, plus
handles, so the cut list can take any frame of it.
"""

import argparse
import json
from pathlib import Path

from timeline import Timeline

HANDLE = 0.35

QUALITY = {
    "preview": {"width": 1280, "height": 544, "fps": 24, "supersample": 1},
    "final": {"width": 1920, "height": 816, "fps": 48, "supersample": 1},
    "fast": {"width": 1920, "height": 816, "fps": 24, "supersample": 1},
}

GESTURE_LENGTH = {"talk": 2.6, "point": 2.2, "shrug": 1.8, "nod": 1.4, "crossed": 4.0,
                  "salute": 2.0, "chin": 4.0, "open": 2.0, "read": 3.2, "present": 2.2, "recoil": 2.6}
AUTO_TALK_MIN = 1.6


def camera_keys(cam, tl, scene, offset, span):
    if "track" in cam:
        return {
            "focus": {"mode": "group", "group": cam["track"], "spring": True},
            "eye_space": "focus",
            "look_space": "focus",
            "camera": [{"time": 0, "eye": cam["eye"], "look": cam["look"], "fov": cam["fov"]}],
        }
    keys = []
    for key in cam.get("keys", [dict(cam, at=0.0)]):
        t = tl.resolve(key["at"], scene) - offset
        keys.append({"time": round(min(max(t, 0.0), span), 3), "eye": key["eye"], "look": key["look"], "fov": key["fov"]})
    if len(keys) == 1:
        keys = drift(keys[0], cam, span)
    return {
        "focus": {"mode": "point", "point": [0, 0, 0]},
        "eye_space": "world",
        "look_space": "world",
        "camera": keys,
        "ends": "moving",
    }


def drift(key, cam, span):
    """A static camera never quite holds still: dialogue framings creep in toward
    the subject, wides slide sideways, both at a pace that reads as an operator."""
    eye, look = key["eye"], key["look"]
    dx, dy, dz = (look[i] - eye[i] for i in range(3))
    dist = max(1e-3, (dx * dx + dy * dy + dz * dz) ** 0.5)
    wide = key["fov"] >= 36
    if wide:
        rate = cam.get("drift", 0.03)
        sx, sz = -dz / dist, dx / dist
        end_eye = [eye[0] + sx * rate * span, eye[1], eye[2] + sz * rate * span]
        end_look = [look[0] + sx * rate * span * 0.6, look[1], look[2] + sz * rate * span * 0.6]
    else:
        rate = min(cam.get("push", 0.02), 0.25 * dist / max(span, 1.0))
        end_eye = [eye[i] + (dx, dy, dz)[i] / dist * rate * span for i in range(3)]
        end_look = look
    end = {"time": round(span, 3), "eye": [round(v, 3) for v in end_eye],
           "look": [round(v, 3) for v in end_look], "fov": key["fov"]}
    return [key, end]


def handheld(cam, seed):
    shake = cam.get("shake", 0.0)
    if shake <= 0.0:
        return {}
    return {"handheld": {"degrees": shake, "frequency": 0.2, "seed": seed}}


def gesture_cues(scene_timing, stage_actors):
    """One showcase routine per speaking actor: a gesture on each of their lines,
    never overlapping, idle in between."""
    if not scene_timing.get("gestures", True):
        return {}
    cues = {}
    busy = {}
    for line in scene_timing["lines"]:
        who = line["who"]
        actor = stage_actors.get(who)
        if actor is None or actor.get("routine"):
            continue
        length = line["end"] - line["start"]
        move = line.get("gesture")
        if move is False:
            continue
        if move is None:
            if length < AUTO_TALK_MIN:
                continue
            move = "talk"
        start = max(line["start"] + 0.05, busy.get(who, 0.0))
        natural = GESTURE_LENGTH[move]
        if move == "talk":
            repeats = max(1, round(length / natural))
            each = max(1.6, length / repeats)
            for _ in range(repeats):
                cues.setdefault(who, []).append([round(start, 3), "gesture_talk", round(each, 3)])
                start += each
        else:
            cues.setdefault(who, []).append([round(start, 3), f"gesture_{move}", natural])
            start += natural
        busy[who] = start + 0.1
    return cues


def write_gestures(edit, tl, stages):
    for scene in edit["scenes"]:
        if "stage" not in scene:
            continue
        stage_path = stages / f"{edit['episode']}_{scene['stage']}.stage.json"
        stage = json.loads(stage_path.read_text())
        retimed = False
        for step in stage.get("steps", []):
            if "at" in step:
                t = round(tl.resolve(step["at"], scene["id"]), 3)
                retimed |= step.get("t") != t
                step["t"] = t
        for actor in stage.get("actors", []):
            if "routine_at" not in actor:
                continue
            routine, clock = [], 0.0
            for move, start, end in actor["routine_at"]:
                a = tl.resolve(start, scene["id"])
                b = tl.resolve(end, scene["id"]) if isinstance(end, str) else a + end
                if a > clock + 0.01:
                    routine.append(f"wait:{a - clock:.3f}")
                routine.append(f"{move}:{b - a:.3f}")
                clock = b
            if actor.get("routine") != routine:
                actor["routine"], actor["routine_loop"] = routine, False
                retimed = True
        if retimed:
            stage_path.write_text(json.dumps(stage, indent=1) + "\n")
        actors = {a["name"]: a for a in stage.get("actors", [])}
        cues = gesture_cues(tl.by_scene[scene["id"]], actors)
        for who, extra in stage.get("extra_gestures", {}).items():
            for at, move in extra:
                t = tl.resolve(at, scene["id"])
                cues.setdefault(who, []).append([round(t, 3), f"gesture_{move}", GESTURE_LENGTH[move]])
            cues[who].sort()
            kept = []
            for cue in cues[who]:
                if kept and cue[0] < kept[-1][0] + kept[-1][2]:
                    if "gesture_talk" in kept[-1][1]:
                        kept[-1][2] = round(max(0.4, cue[0] - kept[-1][0] - 0.05), 3)
                    else:
                        continue
                kept.append(cue)
            cues[who] = kept
        out = stage_path.with_name(stage_path.name.replace(".stage.json", ".gestures.json"))
        if cues:
            out.write_text(json.dumps(cues, indent=1) + "\n")
        elif out.exists():
            out.unlink()


def plan(episode_dir, vo_dir, quality):
    edit = json.loads((episode_dir / "edit.json").read_text())
    tl = Timeline(episode_dir / "beats.json", vo_dir / "voice.json")
    shots, cutlist = [], []
    for scene in edit["scenes"]:
        sid = scene["id"]
        timing = tl.by_scene[sid]
        duration = timing["duration"]
        if "graphic" in scene:
            cutlist.append({"scene": sid, "start": timing["start"], "duration": duration,
                            "segments": [{"graphic": scene["graphic"], "from": 0.0, "to": duration}]})
            continue
        cuts = []
        for cut in scene["cuts"]:
            at = max(0.0, tl.resolve(cut["at"], sid))
            cuts.append(dict(cut, at=round(at, 3)))
        cuts.sort(key=lambda c: c["at"])
        segments = []
        for i, cut in enumerate(cuts):
            end = cuts[i + 1]["at"] if i + 1 < len(cuts) else duration
            if end - cut["at"] < 0.05:
                continue
            segment = {"from": cut["at"], "to": round(end, 3)}
            segment.update({k: cut[k] for k in ("cam", "graphic") if k in cut})
            segments.append(segment)
        ranges = {}
        for seg in segments:
            if "cam" in seg:
                lo, hi = ranges.get(seg["cam"], (seg["from"], seg["to"]))
                ranges[seg["cam"]] = (min(lo, seg["from"]), max(hi, seg["to"]))
        for cam_name, (lo, hi) in ranges.items():
            start = max(0.0, lo - HANDLE)
            span = min(duration + 1.0, hi + HANDLE) - start
            cam = scene["cameras"][cam_name]
            shot = {
                "name": f"{sid}_{cam_name}",
                "scenario": edit["stage_prefix"] + scene["stage"],
                "start": round(start, 3),
                "duration": round(span, 3),
                "rig": "free",
                "ground_clearance": 0.25,
                "near": 0.05,
            }
            shot.update(camera_keys(cam, tl, sid, start, span))
            shot.update(handheld(cam, len(shots) + 7))
            if "lighting" in scene:
                shot["lighting"] = scene["lighting"]
            shots.append(shot)
            for seg in segments:
                if seg.get("cam") == cam_name:
                    seg["shot"] = shot["name"]
                    seg["clip_offset"] = round(seg["from"] - start, 3)
        cutlist.append({"scene": sid, "start": timing["start"], "duration": duration,
                        "grade": scene.get("grade"), "segments": segments})
    spec = {
        "id": f"dead_company_{edit['episode']}",
        "title": edit["title"],
        "audio": False,
        "record_music": False,
        "gameplay_ui": False,
        "motion_limits": {
            "yaw_degrees_per_second": 720,
            "pitch_degrees_per_second": 720,
            "fov_degrees_per_second": 720,
            "minimum_clip_seconds": 0.05,
            "mean_clip_seconds": 0.05,
        },
        "shots": shots,
    }
    spec.update(QUALITY[quality])
    return spec, {"episode": edit["episode"], "total": round(tl.total, 3), "scenes": cutlist}, tl


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("episode_dir")
    parser.add_argument("--vo", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--quality", default="preview", choices=sorted(QUALITY))
    parser.add_argument("--stages", default="tools/arena/promos/dead_company/stages")
    args = parser.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    spec, cutlist, tl = plan(Path(args.episode_dir), Path(args.vo), args.quality)
    edit = json.loads((Path(args.episode_dir) / "edit.json").read_text())
    write_gestures(edit, tl, Path(args.stages))
    (out / "spec.json").write_text(json.dumps(spec, indent=1))
    (out / "cutlist.json").write_text(json.dumps(cutlist, indent=1))
    (out / "timeline.json").write_text(json.dumps(tl.dump(), indent=1))
    seconds = sum(s["duration"] for s in spec["shots"])
    print(f"{len(spec['shots'])} shots, {seconds:.0f} s to capture; episode {tl.total:.1f} s")


if __name__ == "__main__":
    main()
