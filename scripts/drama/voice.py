"""Render an episode's dialogue lines with Kokoro.

Runs under the TTS virtualenv, not the system python:

    ~/.local/share/soi-tts/bin/python scripts/drama/voice.py LINES.json --out DIR

Writes one 48 kHz mono WAV per line id plus voice.json (durations, voice, hash).
Unchanged lines are not re-rendered.
"""

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

SAMPLE_RATE = 24000
OUT_RATE = 48000


def apply_pronunciation(text, pronounce):
    for word, phonemes in pronounce.items():
        text = re.sub(rf"\b{re.escape(word)}\b", f"[{word}](/{phonemes}/)", text)
    return text


def line_key(line, cast, pronounce):
    member = cast[line["who"]]
    speed = line.get("speed", 1.0) * member.get("speed", 1.0)
    payload = json.dumps(
        [line["text"], member["voice"], round(speed, 4), pronounce], sort_keys=True
    )
    return hashlib.sha1(payload.encode()).hexdigest()[:16], speed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("lines")
    parser.add_argument("--out", required=True)
    parser.add_argument("--only", nargs="*")
    args = parser.parse_args()

    spec = json.loads(Path(args.lines).read_text())
    cast, pronounce = spec["cast"], spec.get("pronounce", {})
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    manifest_path = out / "voice.json"
    manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}

    import numpy as np
    import soundfile as sf
    from kokoro import KPipeline

    pipelines = {}
    for scene in spec["scenes"]:
        for line in scene["lines"]:
            if args.only and line["id"] not in args.only:
                continue
            key, speed = line_key(line, cast, pronounce)
            wav = out / f"{line['id']}.wav"
            if manifest.get(line["id"], {}).get("key") == key and wav.exists():
                continue
            voice = cast[line["who"]]["voice"]
            lang = voice[0]
            if lang not in pipelines:
                pipelines[lang] = KPipeline(
                    lang_code=lang, repo_id="hexgrad/Kokoro-82M"
                )
            text = apply_pronunciation(line["text"], pronounce)
            chunks = [
                a.numpy() for _, _, a in pipelines[lang](text, voice=voice, speed=speed)
            ]
            audio = np.concatenate(chunks)
            raw = out / f"{line['id']}.raw.wav"
            sf.write(raw, audio, SAMPLE_RATE)
            subprocess.run(
                [
                    "ffmpeg",
                    "-y",
                    "-loglevel",
                    "error",
                    "-i",
                    str(raw),
                    "-af",
                    "silenceremove=start_periods=1:start_threshold=-50dB,"
                    "areverse,silenceremove=start_periods=1:start_threshold=-50dB,areverse,"
                    "adelay=60,apad=pad_dur=0.12",
                    "-ar",
                    str(OUT_RATE),
                    "-ac",
                    "1",
                    "-c:a",
                    "pcm_s24le",
                    str(wav),
                ],
                check=True,
            )
            raw.unlink()
            info = sf.info(wav)
            manifest[line["id"]] = {
                "key": key,
                "who": line["who"],
                "voice": voice,
                "duration": round(info.frames / info.samplerate, 3),
            }
            print(
                f"{line['id']} {line['who']:9s} {manifest[line['id']]['duration']:5.2f}s",
                flush=True,
            )
            manifest_path.write_text(json.dumps(manifest, indent=1, sort_keys=True))
    total = sum(v["duration"] for v in manifest.values())
    print(f"{len(manifest)} lines, {total:.1f} s of dialogue", file=sys.stderr)


if __name__ == "__main__":
    main()
