#!/usr/bin/env python3
"""Conform the trailer picture from arena shot clips.

Reads the edit decision list in ``cut.json`` and turns the high-frame-rate
arena clips into the finished, graded, matted picture:

1. each event is trimmed out of its clip at the capture rate;
2. motion blur is integrated from the capture's sub-frames (``shutter`` of
   ``subframes`` samples per delivered frame, i.e. a 216-degree shutter for 3
   of 5) and the clip is decimated to the delivery rate;
3. the event's look is applied (filmic curve, split toning, saturation,
   halation, vignette, grain) inside the 2.39:1 scope extraction, and the
   matte is padded back to 1920x1080;
4. events are joined: hard cuts by default, and ``dissolve`` or ``dip`` only
   where an event asks for one.

Each event is rendered to its own near-lossless intermediate in ``--work`` and
only re-rendered when its inputs change, so a re-cut costs seconds.

Usage::

    scripts/trailer/conform.py --cut tools/arena/promos/cinematic/cut.json \\
        --clips artifacts/trailer/clips --work artifacts/trailer/work \\
        --out artifacts/trailer/picture.mov
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))

import titles  # noqa: E402

WIDTH = 1920
HEIGHT = 1080


def run(cmd: list[str]) -> None:
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(" ".join(cmd) + "\n" + result.stderr[-4000:])
        raise SystemExit(f"ffmpeg failed ({result.returncode})")


def probe_frames(path: Path) -> tuple[int, float]:
    out = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "v:0", "-count_packets",
         "-show_entries", "stream=nb_read_packets,r_frame_rate", "-of", "json", str(path)],
        capture_output=True, text=True, check=True).stdout
    stream = json.loads(out)["streams"][0]
    num, den = stream["r_frame_rate"].split("/")
    return int(stream["nb_read_packets"]), float(num) / float(den)


def find_clip(clips: Path, ref: str) -> Path:
    folder, _, name = ref.partition("/")
    matches = sorted((clips / folder).glob(f"[0-9][0-9]_{name}.mp4"))
    if not matches:
        raise SystemExit(f"no clip for '{ref}' under {clips / folder}")
    return matches[0]


def curve_points(points: list[list[float]]) -> str:
    return " ".join(f"{x:.3f}/{y:.3f}" for x, y in points)


def look_filter(look: dict, scope_h: int) -> str:
    """Compile a look description into an ffmpeg chain (active picture only)."""
    stages: list[str] = []
    exposure = float(look.get("exposure", 0.0))
    if exposure:
        stages.append(f"exposure=exposure={exposure:.3f}")
    temperature = look.get("temperature")
    if temperature:
        stages.append(f"colortemperature=temperature={int(temperature)}:mix={look.get('temperature_mix', 0.6)}")
    balance = look.get("balance")
    if balance:
        keys = []
        for zone, prefix in (("shadows", "s"), ("mids", "m"), ("highlights", "h")):
            rgb = balance.get(zone)
            if rgb:
                keys += [f"r{prefix}={rgb[0]:.3f}", f"g{prefix}={rgb[1]:.3f}", f"b{prefix}={rgb[2]:.3f}"]
        if keys:
            stages.append("colorbalance=" + ":".join(keys) + ":pl=1")
    curve = look.get("curve")
    if curve:
        stages.append(f"curves=master='{curve_points(curve)}'")
    for channel in ("red", "green", "blue"):
        if look.get(f"curve_{channel}"):
            stages.append(f"curves={channel}='{curve_points(look[f'curve_{channel}'])}'")
    sat = float(look.get("saturation", 1.0))
    gamma = float(look.get("gamma", 1.0))
    contrast = float(look.get("contrast", 1.0))
    brightness = float(look.get("brightness", 0.0))
    chain = ",".join(stages) if stages else "null"

    halation = float(look.get("halation", 0.0))
    if halation > 0:
        threshold = float(look.get("halation_threshold", 0.72))
        tint = look.get("halation_tint", [1.0, 0.55, 0.35])
        glow = (
            f"curves=master='0/0 {threshold:.2f}/0 1/1',"
            f"colorchannelmixer=rr={tint[0]}:gg={tint[1]}:bb={tint[2]},"
            f"gblur=sigma={look.get('halation_radius', 18)},format=gbrpf32le"
        )
        # blend posterises 16-bit planar RGB; screen the glow in float.
        chain = (
            f"{chain},split[hbase][hsrc];[hsrc]{glow}[hglow];[hbase]format=gbrpf32le[hfloat];"
            f"[hfloat][hglow]blend=all_mode=screen:all_opacity={halation:.3f},format=gbrp16le"
        )
    # eq, vignette, unsharp and noise act on plane 0 as luma, which in planar RGB
    # is green; everything after this point runs in YUV.
    chain += ",format=yuv444p16le"
    if sat != 1.0 or gamma != 1.0 or contrast != 1.0 or brightness != 0.0:
        chain += (f",eq=saturation={sat:.3f}:gamma={gamma:.3f}:contrast={contrast:.3f}"
                  f":brightness={brightness:.3f}")
    vignette = float(look.get("vignette", 0.0))
    if vignette > 0:
        chain += f",vignette=angle={vignette:.3f}:mode=forward"
    sharpen = float(look.get("sharpen", 0.0))
    if sharpen > 0:
        chain += f",unsharp=5:5:{sharpen:.2f}:5:5:0"
    grain = int(look.get("grain", 0))
    if grain > 0:
        chain += f",noise=c0s={grain}:c0f=t+u"
    return chain


def event_hash(event: dict, look: dict, clip: Path | None, settings: dict) -> str:
    h = hashlib.sha1()
    h.update(json.dumps([event, look, settings], sort_keys=True).encode())
    if clip is not None:
        stat = clip.stat()
        h.update(f"{clip}:{stat.st_size}:{stat.st_mtime_ns}".encode())
    h.update(Path(__file__).read_bytes())
    return h.hexdigest()[:16]


def render_event(index: int, event: dict, cut: dict, clips: Path, work: Path) -> Path:
    fps = int(cut["fps"])
    sub = int(cut.get("subframes", 5))
    shutter = int(cut.get("shutter", 3))
    scope = float(cut.get("scope", 2.39))
    scope_h = int(round(WIDTH / scope / 2)) * 2
    looks = cut.get("looks", {})
    look = dict(looks.get(event.get("look", ""), {}))
    look.update(event.get("look_adjust", {}))
    duration = float(event["dur"])
    head = float(event.get("_head", 0.0))
    tail = float(event.get("_tail", 0.0))
    total = head + duration + tail
    settings = {"fps": fps, "sub": sub, "shutter": shutter, "scope": scope, "head": head, "tail": tail}

    if "card" in event:
        clip = None
    else:
        clip = find_clip(clips, event["clip"])
    key = event_hash(event, look, clip, settings)
    out = work / f"e{index:03d}_{key}.mov"
    if out.exists():
        return out
    for stale in work.glob(f"e{index:03d}_*.mov"):
        stale.unlink()

    encode = ["-c:v", "prores_ks", "-profile:v", "3", "-pix_fmt", "yuv422p10le",
              "-r", str(fps), "-an"]

    if clip is None:
        png_dir = work / f"card{index:03d}_{key}"
        titles.render_card(event["card"], png_dir, total, fps, WIDTH, HEIGHT, scope_h,
                           head=head)
        run(["ffmpeg", "-y", "-v", "error", "-framerate", str(fps), "-i",
             str(png_dir / "f%05d.png"), *encode, str(out)])
        return out

    frames, clip_fps = probe_frames(clip)
    speed = float(event.get("speed", 1.0))
    start = float(event.get("in", 0.0)) - head * speed
    if start < -1e-3:
        raise SystemExit(f"event {index} ({event.get('clip')}) starts before its clip "
                         f"(in {event.get('in', 0)} - head {head})")
    start = max(0.0, start)
    need = start + total * speed
    available = frames / clip_fps
    if need > available + 1e-3:
        raise SystemExit(f"event {index} ({event['clip']}) needs {need:.2f}s of a "
                         f"{available:.2f}s clip")
    capture_sub = int(round(clip_fps / fps))
    blur = []
    if capture_sub > 1:
        take = max(1, min(capture_sub, round(shutter * capture_sub / sub)))
        blur = [f"tmix=frames={take}", f"framestep={capture_sub}"]
    zoom = event.get("reframe", {})
    z = float(zoom.get("zoom", 1.0))
    cx = float(zoom.get("x", 0.5))
    cy = float(zoom.get("y", 0.5))
    crop_w = int(round(WIDTH / z / 2)) * 2
    crop_h = int(round(scope_h / z / 2)) * 2
    x0 = int(round(min(max(cx * WIDTH - crop_w / 2, 0), WIDTH - crop_w)))
    y0 = int(round(min(max(cy * HEIGHT - crop_h / 2, 0), HEIGHT - crop_h)))
    flip = ["hflip"] if event.get("flip") else []
    pre = [
        f"trim=start={start:.4f}:duration={total * speed:.4f}",
        "setpts=PTS-STARTPTS",
    ]
    if abs(speed - 1.0) > 1e-3:
        pre.append(f"setpts=PTS/{speed:.4f}")
    graph = ",".join(
        pre
        + blur
        + [f"setpts=N/{fps}/TB", "format=gbrp16le"]
        + flip
        + [f"crop={crop_w}:{crop_h}:{x0}:{y0}", f"scale={WIDTH}:{scope_h}:flags=lanczos"]
    )
    graph += "," + look_filter(look, scope_h)
    fade_in = float(event.get("fade_in", 0.0))
    fade_out = float(event.get("fade_out", 0.0))
    if fade_in > 0:
        graph += f",fade=t=in:st={head:.3f}:d={fade_in:.3f}"
    if fade_out > 0:
        graph += f",fade=t=out:st={head + duration - fade_out:.3f}:d={fade_out:.3f}"
    graph += f",pad={WIDTH}:{HEIGHT}:0:{(HEIGHT - scope_h) // 2}:black,format=yuv422p10le"
    graph += f",trim=end_frame={int(round(total * fps))}"
    run(["ffmpeg", "-y", "-v", "error", "-i", str(clip), "-filter_complex",
         f"[0:v]{graph}[v]", "-map", "[v]", *encode, str(out)])
    return out


def plan(cut: dict) -> list[dict]:
    """Attach dissolve handles: a blended join needs overlap on both sides."""
    events = [dict(e) for e in cut["events"]]
    for i, event in enumerate(events):
        join = event.get("join", "cut")
        if i > 0 and join in ("dissolve", "dip"):
            d = float(event.get("join_dur", 0.5))
            if join == "dissolve":
                events[i - 1]["_tail"] = d / 2
                event["_head"] = d / 2
    return events


def assemble(parts: list[Path], events: list[dict], cut: dict, out: Path) -> float:
    fps = int(cut["fps"])
    inputs: list[str] = []
    for p in parts:
        inputs += ["-i", str(p)]
    chains = []
    labels = []
    for i, event in enumerate(events):
        chains.append(f"[{i}:v]settb=AVTB,setpts=PTS-STARTPTS[p{i}]")
        labels.append(f"p{i}")
    stage = labels[0]
    cut_point = float(events[0]["dur"])
    for i in range(1, len(events)):
        event = events[i]
        if event.get("join", "cut") == "dissolve":
            d = float(event.get("join_dur", 0.5))
            chains.append(f"[{stage}][p{i}]xfade=transition=fade:duration={d:.4f}:"
                          f"offset={cut_point - d / 2:.4f}[j{i}]")
        else:
            chains.append(f"[{stage}][p{i}]concat=n=2:v=1:a=0[j{i}]")
        cut_point += float(event["dur"])
        stage = f"j{i}"
    total = cut_point
    graph = ";".join(chains)
    run(["ffmpeg", "-y", "-v", "error", *inputs, "-filter_complex", graph, "-map",
         f"[{stage}]", "-c:v", "prores_ks", "-profile:v", "3", "-pix_fmt", "yuv422p10le",
         "-r", str(fps), str(out)])
    return total


def timeline(cut: dict) -> list[dict]:
    """Start/end of every event on the finished picture (for the sound edit)."""
    rows = []
    t = 0.0
    for event in cut["events"]:
        rows.append({"name": event.get("name") or event.get("clip") or "card",
                     "start": round(t, 4), "end": round(t + float(event["dur"]), 4)})
        t += float(event["dur"])
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--cut", type=Path, required=True)
    parser.add_argument("--clips", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--only", type=int, nargs="*", help="render just these events")
    args = parser.parse_args()

    cut = json.loads(args.cut.read_text())
    args.work.mkdir(parents=True, exist_ok=True)
    events = plan(cut)
    parts = []
    for index, event in enumerate(events):
        if args.only and index not in args.only:
            continue
        part = render_event(index, event, cut, args.clips, args.work)
        parts.append(part)
        print(f"  {index:02d} {event.get('name') or event.get('clip') or 'card':28s} "
              f"{float(event['dur']):5.2f}s  {part.name}", flush=True)
    if args.only:
        return 0
    total = assemble(parts, events, cut, args.out)
    (args.out.with_suffix(".timeline.json")).write_text(json.dumps(timeline(cut), indent=1))
    print(f"picture: {args.out} ({total:.2f}s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
