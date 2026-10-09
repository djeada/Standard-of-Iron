"""ffmpeg/ffprobe helpers shared by every stage.

Only the standard library is used here, so the timing, caption and rules code
that imports this module runs on a machine with nothing but Python; the
functions that shell out raise :class:`EditError` with ffmpeg's own message
when it is missing or fails.
"""

from __future__ import annotations

import array
import hashlib
import json
import re
import shutil
import subprocess
import wave
from pathlib import Path

from . import EditError

RATE = 48000


def have(tool: str = "ffmpeg") -> bool:
    return shutil.which(tool) is not None


def run(cmd: list[str], what: str = "") -> str:
    """Run a command and return stdout+stderr, or raise with its error tail."""
    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
    except FileNotFoundError as exc:
        raise EditError(f"{cmd[0]} is not installed ({what or 'needed here'})") from exc
    if result.returncode != 0:
        tail = result.stderr.strip().splitlines()[-12:]
        raise EditError(
            f"{what or cmd[0]} failed ({result.returncode}): "
            + " ".join(cmd[:8])
            + " ...\n"
            + "\n".join(tail)
        )
    return result.stdout + result.stderr


def ffmpeg(args: list[str], what: str = "ffmpeg") -> str:
    return run(["ffmpeg", "-hide_banner", "-nostdin", "-y", *args], what)


def probe(path: Path) -> dict:
    """Duration, size, frame rate and stream presence of a media file."""
    out = run(
        [
            "ffprobe",
            "-v",
            "error",
            "-show_entries",
            "format=duration:stream=codec_type,width,height,r_frame_rate,"
            "sample_rate,channels,pix_fmt,duration",
            "-of",
            "json",
            str(path),
        ],
        f"probing {path.name}",
    )
    data = json.loads(out[out.index("{") :])
    info: dict = {
        "duration": float(data.get("format", {}).get("duration", 0.0) or 0.0),
        "has_video": False,
        "has_audio": False,
    }
    for stream in data.get("streams", []):
        if stream.get("codec_type") == "video" and not info["has_video"]:
            num, _, den = stream.get("r_frame_rate", "0/1").partition("/")
            info.update(
                has_video=True,
                width=int(stream.get("width", 0)),
                height=int(stream.get("height", 0)),
                fps=float(num) / float(den or 1) if float(den or 1) else 0.0,
                pix_fmt=stream.get("pix_fmt", ""),
            )
            if stream.get("duration"):
                info["video_duration"] = float(stream["duration"])
        elif stream.get("codec_type") == "audio" and not info["has_audio"]:
            info.update(
                has_audio=True,
                sample_rate=int(stream.get("sample_rate", 0) or 0),
                channels=int(stream.get("channels", 0) or 0),
            )
            if stream.get("duration"):
                info["audio_duration"] = float(stream["duration"])
    return info


def wav_info(path: Path) -> dict:
    """Header facts of a PCM WAV without ffprobe: rate, bits, channels, seconds."""
    try:
        with wave.open(str(path), "rb") as handle:
            frames = handle.getnframes()
            rate = handle.getframerate()
            return {
                "sample_rate": rate,
                "bits": handle.getsampwidth() * 8,
                "channels": handle.getnchannels(),
                "duration": frames / float(rate),
            }
    except (wave.Error, EOFError):
        info = probe(path)
        return {
            "sample_rate": info.get("sample_rate", 0),
            "bits": 0,
            "channels": info.get("channels", 0),
            "duration": info["duration"],
        }


def duration(path: Path) -> float:
    if path.suffix.lower() == ".wav":
        return wav_info(path)["duration"]
    return probe(path)["duration"]


def file_key(path: Path, *extra) -> str:
    """A cache key for a file and the parameters applied to it."""
    stat = path.stat()
    raw = json.dumps([str(path.resolve()), stat.st_size, stat.st_mtime_ns, extra])
    return hashlib.sha1(raw.encode()).hexdigest()[:12]


def data_key(*parts) -> str:
    return hashlib.sha1(
        json.dumps(parts, sort_keys=True, default=str).encode()
    ).hexdigest()[:12]


def speech_segments(
    path: Path, noise_db: float = -42.0, min_silence: float = 0.22
) -> list[tuple[float, float]]:
    """Where a recording has sound, from ffmpeg ``silencedetect``.

    Returns ``(start, end)`` pairs in seconds. A take with no detected silence
    is one segment spanning the file.
    """
    out = ffmpeg(
        [
            "-i",
            str(path),
            "-af",
            f"silencedetect=noise={noise_db}dB:d={min_silence}",
            "-f",
            "null",
            "-",
        ],
        f"silence scan of {path.name}",
    )
    total = duration(path)
    starts = [float(v) for v in re.findall(r"silence_start: (-?[0-9.]+)", out)]
    ends = [float(v) for v in re.findall(r"silence_end: ([0-9.]+)", out)]
    silences = []
    for i, start in enumerate(starts):
        end = ends[i] if i < len(ends) else total
        silences.append((max(0.0, start), min(total, end)))
    segments = []
    cursor = 0.0
    for start, end in silences:
        if start - cursor > 0.02:
            segments.append((round(cursor, 4), round(start, 4)))
        cursor = end
    if total - cursor > 0.02:
        segments.append((round(cursor, 4), round(total, 4)))
    return segments


def loudness(path: Path, stream: str = "0:a") -> dict:
    """EBU R128 integrated loudness, range and true peak of a file's audio."""
    out = ffmpeg(
        [
            "-nostats",
            "-i",
            str(path),
            "-map",
            stream,
            "-af",
            "ebur128=peak=true",
            "-f",
            "null",
            "-",
        ],
        f"loudness of {path.name}",
    )
    summary = out[out.rfind("Summary:") :]

    def grab(pattern: str, default: float) -> float:
        found = re.findall(pattern, summary)
        return float(found[-1]) if found else default

    return {
        "integrated_lufs": grab(r"I:\s+(-?[0-9.]+|-inf) LUFS", -70.0),
        "lra_lu": grab(r"LRA:\s+(-?[0-9.]+) LU", 0.0),
        "true_peak_dbtp": grab(r"Peak:\s+(-?[0-9.]+|-inf) dBFS", -70.0),
    }


def short_term(path: Path) -> list[tuple[float, float]]:
    """``(time, short-term LUFS)`` every 100 ms, from ``ebur128`` frame metadata."""
    out = ffmpeg(
        [
            "-nostats",
            "-i",
            str(path),
            "-af",
            f"asetnsamples=n={RATE // 10}:p=0,ebur128=metadata=1,ametadata=print:key=lavfi.r128.S",
            "-f",
            "null",
            "-",
        ],
        f"short-term loudness of {path.name}",
    )
    times = [float(v) for v in re.findall(r"pts_time:([0-9.]+)", out)]
    values = [
        (-120.0 if "inf" in v else float(v))
        for v in re.findall(r"lavfi\.r128\.S=(-?[0-9.]+|-?inf)", out)
    ]
    return list(zip(times, values, strict=False))


def write_control_wav(path: Path, values: list[float], rate: int) -> None:
    """A mono 16-bit WAV of gain values in [0, 1] (a control signal for ``amultiply``)."""
    samples = array.array(
        "h", (int(round(max(0.0, min(1.0, v)) * 32767)) for v in values)
    )
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(rate)
        handle.writeframes(samples.tobytes())


def luma_series(path: Path, crop: float = 1.0) -> list[float]:
    """Mean luma per frame (0-255), read back from an encoded file."""
    vf = "scale=160:-2,signalstats,metadata=print:key=lavfi.signalstats.YAVG"
    if crop < 1.0:
        vf = f"crop=iw:ih*{crop}," + vf
    out = ffmpeg(["-i", str(path), "-map", "0:v", "-vf", vf, "-f", "null", "-"])
    return [float(v) for v in re.findall(r"YAVG=([0-9.]+)", out)]


def extract_frame(video: Path, at: float, out: Path, width: int | None = None) -> Path:
    vf = ["-vf", f"scale={width}:-2:flags=lanczos"] if width else []
    ffmpeg(
        [
            "-ss",
            f"{max(0.0, at):.3f}",
            "-i",
            str(video),
            "-frames:v",
            "1",
            *vf,
            str(out),
        ],
        f"frame grab at {at:.2f}s",
    )
    return out
