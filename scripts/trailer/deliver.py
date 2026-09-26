#!/usr/bin/env python3
"""Encode the delivery MP4 and check it before anyone uploads it.

H.264 High, 1920x1080, the picture's own frame rate, BT.709 flags, AAC-LC 320k
48 kHz stereo, faststart. The encode is two-pass at a high target bitrate with
``-tune grain`` so the film grain in the grade survives compression.

Checks run on the *encoded* file (a failed check leaves the file at
``<name>.rejected.mp4`` and exits non-zero):

* picture and sound durations agree to within one frame;
* integrated loudness is within 1 LU of the mix report and the true peak
  of the decoded AAC stays at or under -1 dBTP;
* no frame inside the picture (outside intentional fades to black, listed in
  ``--allow-black``) is black, and no run of more than ``--max-freeze`` frames is
  frozen;
* no luminance flash series that would trip WCAG 2.3.1 (three flashes/second).
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


def run(cmd: list[str]) -> str:
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stderr[-3000:])
        raise SystemExit(f"command failed: {' '.join(cmd[:4])} ...")
    return result.stdout + result.stderr


def duration(path: Path, stream: str) -> float:
    out = run(["ffprobe", "-v", "error", "-select_streams", stream, "-show_entries",
               "stream=duration", "-of", "csv=p=0", str(path)])
    return float(out.strip().splitlines()[0])


def loudness(path: Path) -> tuple[float, float]:
    out = run(["ffmpeg", "-hide_banner", "-nostats", "-i", str(path), "-map", "0:a",
               "-af", "ebur128=peak=true", "-f", "null", "-"])
    integrated = float(re.findall(r"I:\s+(-?[0-9.]+) LUFS", out)[-1])
    peak = float(re.findall(r"Peak:\s+(-?[0-9.]+) dBFS", out)[-1])
    return integrated, peak


def luma_series(path: Path) -> list[float]:
    out = run(["ffmpeg", "-hide_banner", "-i", str(path), "-map", "0:v", "-vf",
               "crop=iw:ih*0.74,scale=160:-2,signalstats,metadata=print:key=lavfi.signalstats.YAVG",
               "-f", "null", "-"])
    return [float(v) for v in re.findall(r"YAVG=([0-9.]+)", out)]


def freeze_runs(path: Path, max_frames: int, fps: float) -> list[tuple[float, float]]:
    out = run(["ffmpeg", "-hide_banner", "-i", str(path), "-map", "0:v", "-vf",
               f"freezedetect=n=0.0008:d={max_frames / fps:.3f}", "-f", "null", "-"])
    starts = [float(v) for v in re.findall(r"freeze_start: ([0-9.]+)", out)]
    ends = [float(v) for v in re.findall(r"freeze_end: ([0-9.]+)", out)]
    return list(zip(starts, ends + [None] * (len(starts) - len(ends))))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--picture", type=Path, required=True)
    parser.add_argument("--mix", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--bitrate", default="24M")
    parser.add_argument("--allow-black", default="",
                        help="comma list of start-end seconds where black is intended")
    parser.add_argument("--allow-freeze", default="",
                        help="comma list of start-end seconds where stillness is intended")
    parser.add_argument("--max-freeze", type=int, default=12)
    parser.add_argument("--cut", type=Path,
                        help="cut.json: its cards, dips and fades count as intended black")
    args = parser.parse_args()
    if args.cut:
        spans = []
        t = 0.0
        events = json.loads(args.cut.read_text())["events"]
        for i, event in enumerate(events):
            d = float(event["dur"])
            if "card" in event:
                spans.append(f"{t:.3f}-{t + d:.3f}")
            fade_in = float(event.get("fade_in", 0.0))
            fade_out = float(event.get("fade_out", 0.0))
            if event.get("join") == "dip":
                fade_in = max(fade_in, float(event.get("join_dur", 0.5)) / 2)
            if i + 1 < len(events) and events[i + 1].get("join") == "dip":
                fade_out = max(fade_out, float(events[i + 1].get("join_dur", 0.5)) / 2)
            if fade_in:
                spans.append(f"{t:.3f}-{t + fade_in:.3f}")
            if fade_out:
                spans.append(f"{t + d - fade_out:.3f}-{t + d:.3f}")
            t += d
        joined = ",".join(spans)
        args.allow_black = ",".join(filter(None, [args.allow_black, joined]))
        args.allow_freeze = ",".join(filter(None, [args.allow_freeze, joined]))

    fps_text = run(["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
                    "stream=r_frame_rate", "-of", "csv=p=0", str(args.picture)]).strip()
    num, den = fps_text.split("/")
    fps = float(num) / float(den)
    tmp = args.out.with_suffix(".tmp.mp4")
    passlog = str(args.out.with_suffix(".x264"))
    common = ["-i", str(args.picture), "-i", str(args.mix), "-map", "0:v", "-map", "1:a",
              "-c:v", "libx264", "-preset", "slow", "-tune", "grain", "-profile:v", "high",
              "-level", "4.2", "-pix_fmt", "yuv420p", "-b:v", args.bitrate,
              "-maxrate", "40M", "-bufsize", "60M", "-g", str(int(round(fps * 2))),
              "-color_primaries", "bt709", "-color_trc", "bt709", "-colorspace", "bt709",
              "-vf", "scale=out_color_matrix=bt709:out_range=tv"]
    run(["ffmpeg", "-y", "-hide_banner", *common, "-pass", "1", "-passlogfile", passlog,
         "-an", "-f", "mp4", "/dev/null"])
    run(["ffmpeg", "-y", "-hide_banner", *common, "-pass", "2", "-passlogfile", passlog,
         "-c:a", "aac", "-b:a", "320k", "-ar", "48000", "-ac", "2", "-shortest",
         "-movflags", "+faststart", str(tmp)])

    problems: list[str] = []
    vdur = duration(tmp, "v:0")
    adur = duration(tmp, "a:0")
    if abs(vdur - adur) > 1.5 / fps:
        problems.append(f"picture {vdur:.3f}s and sound {adur:.3f}s differ")

    report = json.loads(args.mix.with_suffix(".report.json").read_text())
    integrated, peak = loudness(tmp)
    if abs(integrated - report["integrated_lufs"]) > 1.0:
        problems.append(f"delivered loudness {integrated} LUFS vs mix {report['integrated_lufs']}")
    if peak > -0.9:
        problems.append(f"delivered true peak {peak} dBTP over -1")

    def inside(t: float, spans: str) -> bool:
        for span in filter(None, spans.split(",")):
            a, b = (float(x) for x in span.split("-"))
            if a - 0.05 <= t <= b + 0.05:
                return True
        return False

    luma = luma_series(tmp)
    blacks = [i / fps for i, y in enumerate(luma) if y < 17.5]
    stray = [t for t in blacks if not inside(t, args.allow_black)]
    if stray:
        problems.append(f"{len(stray)} black frame(s), first at {stray[0]:.2f}s")
    jumps = [abs(luma[i] - luma[i - 1]) for i in range(1, len(luma))]
    window = int(round(fps))
    for i in range(0, max(1, len(jumps) - window)):
        if sum(1 for j in jumps[i:i + window] if j > 20) > 3:
            problems.append(f"flash series near {i / fps:.2f}s")
            break
    for start, end in freeze_runs(tmp, args.max_freeze, fps):
        if not inside(start, args.allow_black) and not inside(start, args.allow_freeze):
            problems.append(f"frozen picture from {start:.2f}s to {end}")

    summary = {"file": str(args.out), "seconds": round(vdur, 3), "fps": fps,
               "integrated_lufs": integrated, "true_peak_dbtp": peak,
               "bytes": tmp.stat().st_size,
               "video_mbps": round(tmp.stat().st_size * 8 / vdur / 1e6, 1),
               "problems": problems}
    args.out.with_suffix(".qc.json").write_text(json.dumps(summary, indent=1))
    if problems:
        rejected = args.out.with_suffix(".rejected.mp4")
        tmp.replace(rejected)
        if args.out.exists():
            args.out.unlink()
        print(json.dumps(summary, indent=1))
        return 1
    tmp.replace(args.out)
    print(json.dumps(summary, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
