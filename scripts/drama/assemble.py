"""Cut, mix and subtitle a drama episode from its captures.

Runs under the TTS virtualenv (it needs numpy and soundfile):

    ~/.local/share/soi-tts/bin/python scripts/drama/assemble.py EPISODE_DIR \
        --plan DIR --clips DIR --vo DIR --graphics DIR --out FILE.mp4

The picture follows plan/cutlist.json; the sound is laid out here from the
recorded lines, the edit's beds, music and effects, ducked under the voices;
subtitles are burned in and also written as .srt beside the film.
"""

import argparse
import json
import math
import re
import subprocess
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

REPO = Path(__file__).resolve().parents[2]
AUDIO = REPO / "assets" / "audio"
CC0 = REPO / "artifacts" / "drama" / "cc0" / "lib"
FONTS = REPO / "assets" / "fonts"
RATE = 48000
FPS = 24
W, H = 1920, 1080
SCOPE = 816
BAR = (H - SCOPE) // 2
FILM = "noise=alls=3:allf=t,vignette=PI/7"
VO_RMS_DB = -19.0
LEVEL_DB = {"whisper": -5.0, "quiet": -2.5, "normal": 0.0, "raised": 2.0, "shout": 4.0}


def run(cmd):
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


def db(value):
    return 10 ** (value / 20.0)


class Times:
    def __init__(self, timeline, scene):
        self.tl = timeline
        self.scene = scene

    def __call__(self, expr):
        if isinstance(expr, (int, float)):
            return float(expr)
        try:
            return float(expr)
        except ValueError:
            pass
        match = re.fullmatch(r"(line|mark|end)(?::([\w.]+))?(@end)?([+-][\d.]+)?", expr.strip())
        kind, name, at_end, offset = match.groups()
        offset = float(offset) if offset else 0.0
        if kind == "end":
            return self.scene["duration"] + offset
        if kind == "line":
            line = next(l for s in self.tl["scenes"] for l in s["lines"] if l["id"] == name)
            return (line["end"] if at_end else line["start"]) + offset
        key = name if "." in name else f"{self.scene['id']}.{name}"
        return self.tl["marks"][key] + offset


# ---------------------------------------------------------------- picture


def find_clip(clips, shot):
    matches = sorted(clips.glob(f"**/[0-9][0-9]_{shot}.mp4"))
    if not matches:
        raise FileNotFoundError(f"no capture for shot {shot}")
    return matches[-1]


def encode_segment(src, out, frames, offset=None, graphic=False, push=0.0, grade=None):
    common = ["-frames:v", str(frames), "-r", str(FPS), "-an", "-c:v", "libx264", "-preset", "fast",
              "-crf", "15", "-pix_fmt", "yuv420p", str(out)]
    if graphic:
        zoom = (f"scale=-2:{SCOPE},pad={W}:{SCOPE}:(ow-iw)/2:0:color=0x1e160f,"
                f"zoompan=z='1+{push}*on/{max(frames, 1)}':x='iw/2-(iw/zoom/2)':"
                f"y='ih/2-(ih/zoom/2)':d={frames}:s={W}x{SCOPE}:fps={FPS}")
        run(["ffmpeg", "-y", "-loglevel", "error", "-loop", "1", "-i", str(src), "-vf", zoom] + common)
        return
    # Captures run at twice the film rate: averaging each pair is a 180 degree shutter.
    rate = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
                           "stream=r_frame_rate", "-of", "csv=p=0", str(src)], capture_output=True, text=True).stdout
    num, _, den = rate.strip().partition("/")
    double_rate = float(num) / float(den or 1) >= 2 * FPS - 1
    chain = ([f"tmix=frames=2:weights='1 1'"] if double_rate else []) + [f"fps={FPS}", f"scale={W}:{SCOPE}:flags=lanczos"]
    if grade:
        chain.append(grade)
    chain.append(FILM)
    run(["ffmpeg", "-y", "-loglevel", "error", "-ss", f"{offset:.3f}", "-i", str(src),
         "-vf", ",".join(chain)] + common)


def build_picture(cutlist, clips, graphics, work):
    parts = []
    for scene in cutlist["scenes"]:
        for i, seg in enumerate(scene["segments"]):
            a = round((scene["start"] + seg["from"]) * FPS)
            b = round((scene["start"] + seg["to"]) * FPS)
            if b <= a:
                continue
            out = work / f"seg_{scene['scene']}_{i:02d}.mp4"
            if "graphic" in seg:
                push = 0.0 if seg["graphic"] in ("black", "title", "endcard") else 0.06
                encode_segment(graphics / f"{seg['graphic']}.png", out, b - a, graphic=True, push=push)
            else:
                encode_segment(find_clip(clips, seg["shot"]), out, b - a, offset=seg["clip_offset"],
                               grade=scene.get("grade"))
            parts.append(out)
    listing = work / "parts.txt"
    listing.write_text("".join(f"file '{p.resolve()}'\n" for p in parts))
    picture = work / "picture.mp4"
    run(["ffmpeg", "-y", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", str(listing), "-c", "copy", str(picture)])
    return picture


# ---------------------------------------------------------------- sound


def load_audio(path, rate_factor=1.0, channels=2):
    with tempfile.NamedTemporaryFile(suffix=".wav") as tmp:
        filters = f"asetrate={int(RATE * rate_factor)},aresample={RATE}" if rate_factor != 1.0 else "anull"
        run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(path), "-af", filters, "-ac", str(channels),
             "-ar", str(RATE), tmp.name])
        data, _ = sf.read(tmp.name, dtype="float32", always_2d=True)
    return data


def asset_path(name):
    if name.startswith("cc0/"):
        return CC0 / name[4:]
    return AUDIO / name


def looped(data, length, xfade=1.0):
    if len(data) >= length:
        return data[:length].copy()
    fade = min(int(xfade * RATE), len(data) // 4)
    ramp = np.linspace(0.0, 1.0, fade, dtype=np.float32)[:, None]
    body = data.copy()
    out = np.zeros((length, data.shape[1]), dtype=np.float32)
    pos = 0
    while pos < length:
        chunk = body[: length - pos].copy()
        if pos > 0:
            n = min(fade, len(chunk))
            chunk[:n] *= ramp[:n]
            out[pos : pos + n] *= 1.0 - ramp[:n]
        out[pos : pos + len(chunk)] += chunk
        pos += len(body) - fade
    return out


def fades(data, fade_in, fade_out):
    n = len(data)
    a = min(int(fade_in * RATE), n)
    b = min(int(fade_out * RATE), n)
    if a:
        data[:a] *= np.linspace(0.0, 1.0, a, dtype=np.float32)[:, None]
    if b:
        data[n - b :] *= np.linspace(1.0, 0.0, b, dtype=np.float32)[:, None]
    return data


def pan(data, position):
    if not position:
        return data
    left = np.float32(np.cos((position + 1) * np.pi / 4) * np.sqrt(2))
    right = np.float32(np.sin((position + 1) * np.pi / 4) * np.sqrt(2))
    return data * np.array([left, right], dtype=np.float32)


def place(bus, data, at):
    start = int(round(at * RATE))
    if start < 0:
        data = data[-start:]
        start = 0
    end = min(len(bus), start + len(data))
    if end > start:
        bus[start:end] += data[: end - start]


def voice_room(data, room):
    """A little air around the voices so they sit in the place rather than in a booth."""
    if not room:
        return data
    delay, decay, mix = {"night": (0.045, 0.32, 0.16), "tent": (0.018, 0.25, 0.12),
                         "open": (0.06, 0.2, 0.08), "camp": (0.035, 0.28, 0.12)}[room]
    out = np.concatenate([data, np.zeros((int(0.6 * RATE), 2), dtype=np.float32)])
    wet = np.zeros_like(out)
    for k, offset in enumerate((delay, delay * 1.7, delay * 2.9, delay * 4.3)):
        shift = int(offset * RATE)
        gain = decay ** (k + 1)
        side = np.array([1.0, 0.85] if k % 2 == 0 else [0.85, 1.0], dtype=np.float32)
        wet[shift : shift + len(data)] += data * gain * side
    return out + wet * mix / decay


def speech_envelope(length, intervals, attack=0.25, release=0.6):
    env = np.zeros(length, dtype=np.float32)
    for a, b in intervals:
        env[int(a * RATE) : int(b * RATE)] = 1.0
    out = np.zeros_like(env)
    level = 0.0
    up = 1.0 / (attack * RATE)
    down = 1.0 / (release * RATE)
    for i in range(0, length, 240):
        target = env[i : i + 240].max(initial=0.0)
        level = min(target, level + up * 240) if target > level else max(target, level - down * 240)
        out[i : i + 240] = level
    return out


def build_mix(edit, timeline, lines, vo, work):
    level = {l["id"]: l.get("level", "normal") for sc in (lines or {}).get("scenes", []) for l in sc["lines"]}
    total = timeline["total"] + 1.0
    n = int(total * RATE)
    voices = np.zeros((n, 2), dtype=np.float32)
    beds = np.zeros((n, 2), dtype=np.float32)
    music = np.zeros((n, 2), dtype=np.float32)
    effects = np.zeros((n, 2), dtype=np.float32)
    intervals = []
    scenes = {s["id"]: s for s in timeline["scenes"]}
    scenes_room = {spec["id"]: spec.get("room") for spec in edit["scenes"]}
    for scene in timeline["scenes"]:
        for line in scene["lines"]:
            data = load_audio(vo / f"{line['id']}.wav", channels=1)
            rms = math.sqrt(float(np.mean(data[np.abs(data) > 0.01] ** 2) or 1e-9))
            target = VO_RMS_DB + LEVEL_DB.get(level.get(line["id"], "normal"), 0.0)
            data = np.repeat(data * (db(target) / max(rms, 1e-6)), 2, axis=1)
            data = voice_room(data, scenes_room.get(scene["id"]))
            at = scene["start"] + line["start"]
            place(voices, data, at)
            intervals.append((at, at + len(data) / RATE))
    cache = {}

    def asset(name, rate=1.0):
        # Every asset is brought to the dialogue's level first, so an edit's dB
        # values read as "this far under the voices" whatever the file's mastering.
        key = (name, rate)
        if key not in cache:
            data = load_audio(asset_path(name), rate)
            loud = data[np.abs(data) > 0.003]
            rms = math.sqrt(float(np.mean(loud**2))) if len(loud) else 1.0
            cache[key] = data * (db(VO_RMS_DB) / max(rms, 1e-6))
        return cache[key]

    for spec in edit["scenes"]:
        scene = scenes[spec["id"]]
        times = Times(timeline, scene)
        base = scene["start"]
        sound = spec.get("sound", {})
        for bed in sound.get("bed", []):
            a = times(bed.get("from", 0.0))
            b = times(bed.get("until", "end"))
            data = pan(looped(asset(bed["file"]), int((b - a) * RATE)) * db(bed.get("db", -20)), bed.get("pan"))
            fades(data, bed.get("fade_in", 0.8), bed.get("fade_out", 0.8))
            place(beds, data, base + a)
        for cue in sound.get("music", []):
            a = times(cue.get("from", 0.0))
            b = times(cue.get("to", "end"))
            src = asset(cue["file"])
            offset = int(cue.get("offset", 0.0) * RATE)
            data = looped(src[offset:], int((b - a) * RATE)) * db(cue.get("db", -20))
            fades(data, cue.get("fade_in", 0.3), cue.get("fade_out", 1.0))
            place(music, data, base + a)
        for sfx in sound.get("sfx", []):
            gain = db(sfx.get("db", -14))
            if "every" in sfx:
                t, end, k = times(sfx["from"]), times(sfx.get("to", "end")), 0
                while t < end:
                    data = pan(asset(sfx["file"].format(n=k % sfx.get("variants", 4) + 1)) * gain, sfx.get("pan"))
                    place(effects, data, base + t)
                    t += sfx["every"]
                    k += 1
                continue
            data = pan(asset(sfx["file"], sfx.get("rate", 1.0)).copy() * gain, sfx.get("pan"))
            if "dur" in sfx:
                data = fades(data[: int(sfx["dur"] * RATE)].copy(), 0.0, 0.3)
            place(effects, data, base + times(sfx["at"]))
    duck = speech_envelope(n, intervals)[:, None]
    mix = voices + beds * (1.0 - 0.45 * duck) + music * (1.0 - 0.55 * duck) + effects
    raw = work / "mix_raw.wav"
    sf.write(raw, mix, RATE, subtype="PCM_24")
    # One static gain to -16 LUFS plus a peak limiter: single-pass loudnorm is
    # dynamic and would lift the quiet beds between lines up to dialogue level.
    probe = subprocess.run(["ffmpeg", "-hide_banner", "-i", str(raw), "-af", "ebur128", "-f", "null", "-"],
                           capture_output=True, text=True).stderr
    integrated = float(re.findall(r"I:\s+(-?[\d.]+) LUFS", probe)[-1])
    out = work / "mix.wav"
    run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(raw), "-af",
         f"volume={-16.0 - integrated:.2f}dB,alimiter=limit=0.70:level=false", "-ar", str(RATE), str(out)])
    return out


# ---------------------------------------------------------------- subtitles


def chunks(text, limit=42):
    """Caption-sized pieces: whole sentences when short, else clauses, else word runs."""
    out = []
    for sentence in re.split(r"(?<=[.!?])\s+", text.strip()):
        if len(sentence) <= limit:
            out.append(sentence)
            continue
        for clause in re.split(r"(?<=[,;:])\s+", sentence):
            words, current = clause.split(), ""
            for word in words:
                if current and len(current) + 1 + len(word) > limit:
                    out.append(current)
                    current = word
                else:
                    current = f"{current} {word}".strip()
            if current:
                out.append(current)
    merged = []
    for piece in out:
        if merged and (len(merged[-1]) < 12 or len(piece) < 12) and len(merged[-1]) + 1 + len(piece) <= limit + 8:
            merged[-1] = f"{merged[-1]} {piece}"
        else:
            merged.append(piece)
    return merged


def stamp(t, srt=False):
    h, rem = divmod(max(t, 0.0), 3600)
    m, s = divmod(rem, 60)
    if srt:
        return f"{int(h):02d}:{int(m):02d}:{int(s):02d},{int(round((s % 1) * 1000)):03d}"
    return f"{int(h)}:{int(m):02d}:{s:05.2f}"


def build_subtitles(edit, timeline, lines, work, out_srt):
    text = {l["id"]: l["text"] for sc in lines["scenes"] for l in sc["lines"]}
    events = []
    for scene in timeline["scenes"]:
        for line in scene["lines"]:
            a = scene["start"] + line["start"]
            b = scene["start"] + line["end"] + 0.25
            pieces = chunks(text[line["id"]])
            total = sum(len(piece) for piece in pieces) or 1
            span = b - a
            cursor = a
            for piece in pieces:
                length = span * len(piece) / total
                events.append([cursor, cursor + max(length, 0.8), piece])
                cursor += length
    events.sort()
    for i in range(len(events) - 1):
        events[i][1] = min(events[i][1], events[i + 1][0] - 0.04)
    header = f"""[Script Info]
ScriptType: v4.00+
PlayResX: {W}
PlayResY: {H}
WrapStyle: 2

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Line,Open Sans Semibold,52,&H00FFFFFF,&H00FFFFFF,&H00000000,&H78000000,0,0,0,0,100,100,0.5,0,3,7,0,2,160,160,36,1
Style: Caption,Standard Iron Display,46,&H00C8E7F4,&H00C8E7F4,&H00101010,&H64000000,1,0,0,0,100,100,6,0,1,1.6,0.8,1,120,120,176,1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
"""
    body, srt = [], []
    for k, (a, b, t) in enumerate(events, 1):
        body.append(f"Dialogue: 0,{stamp(a)},{stamp(b)},Line,,0,0,0,,{t}")
        srt.append(f"{k}\n{stamp(a, True)} --> {stamp(b, True)}\n{t}\n")
    scenes = {s["id"]: s for s in timeline["scenes"]}
    for spec in edit["scenes"]:
        for cap in spec.get("captions", []):
            scene = scenes[spec["id"]]
            times = Times(timeline, scene)
            a, b = scene["start"] + times(cap["from"]), scene["start"] + times(cap["to"])
            body.append(f"Dialogue: 1,{stamp(a)},{stamp(b)},Caption,,0,0,0,,{{\\fad(600,600)}}{cap['text'].replace(chr(10), '\\N')}")
    ass = work / "subs.ass"
    ass.write_text(header + "\n".join(body) + "\n")
    out_srt.write_text("\n".join(srt))
    return ass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("episode_dir")
    parser.add_argument("--plan", required=True)
    parser.add_argument("--clips", required=True)
    parser.add_argument("--vo", required=True)
    parser.add_argument("--graphics", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--work")
    parser.add_argument("--reuse-picture", action="store_true")
    args = parser.parse_args()
    episode = Path(args.episode_dir)
    plan = Path(args.plan)
    out = Path(args.out)
    work = Path(args.work or out.parent / "work")
    work.mkdir(parents=True, exist_ok=True)
    edit = json.loads((episode / "edit.json").read_text())
    lines = json.loads((episode / "lines.json").read_text())
    timeline = json.loads((plan / "timeline.json").read_text())
    cutlist = json.loads((plan / "cutlist.json").read_text())
    picture = work / "picture.mp4"
    if not (args.reuse_picture and picture.exists()):
        picture = build_picture(cutlist, Path(args.clips), Path(args.graphics), work)
    mix = build_mix(edit, timeline, lines, Path(args.vo), work)
    ass = build_subtitles(edit, timeline, lines, work, out.with_suffix(".en.srt"))
    run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(picture), "-i", str(mix),
         "-vf", f"pad={W}:{H}:0:{BAR}:black,ass={ass}:fontsdir={FONTS}", "-map", "0:v", "-map", "1:a",
         "-c:v", "libx264", "-preset", "medium", "-crf", "18", "-pix_fmt", "yuv420p",
         "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart", str(out)])
    for _ in range(3):
        probe = subprocess.run(["ffmpeg", "-hide_banner", "-i", str(out), "-af", "ebur128=peak=true",
                                "-f", "null", "-"], capture_output=True, text=True).stderr
        peak = float(re.findall(r"Peak:\s+(-?[\d.]+) dBFS", probe)[-1])
        if peak <= -1.0:
            break
        fixed = out.with_suffix(".tp.mp4")
        run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(out), "-c:v", "copy", "-af",
             f"volume={-1.3 - peak:.2f}dB", "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart", str(fixed)])
        fixed.replace(out)
    print(f"wrote {out} (true peak {peak:.2f} dBTP)")


if __name__ == "__main__":
    main()
