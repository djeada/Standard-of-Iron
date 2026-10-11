"""Choose the best take of every line and write the episode's recordings.

Runs under the Qwen TTS virtualenv (faster-whisper, speechbrain):

    python scripts/drama/cast.py LINES.json --work DIR --out VO_DIR

A take is heard back by speech recognition and rejected if it doesn't say the
line. Among the rest, the one whose voice is closest to the character's
reference wins, with livelier pitch as the tie-breaker. The winner is trimmed,
resampled to 48 kHz mono and recorded in VO_DIR/voice.json with its scores.
"""

import argparse
import json
import re
import subprocess
from pathlib import Path

import numpy as np
import soundfile as sf
import torch
import torchaudio

OUT_RATE = 48000


def words(text):
    return re.sub(r"[^a-z0-9' ]", " ", text.lower()).split()


def wer(ref, hyp):
    r, h = words(ref), words(hyp)
    d = np.zeros((len(r) + 1, len(h) + 1), dtype=int)
    d[:, 0] = range(len(r) + 1)
    d[0, :] = range(len(h) + 1)
    for i in range(1, len(r) + 1):
        for j in range(1, len(h) + 1):
            d[i, j] = min(d[i - 1, j] + 1, d[i, j - 1] + 1, d[i - 1, j - 1] + (r[i - 1] != h[j - 1]))
    return d[len(r), len(h)] / max(len(r), 1)


def load(path):
    data, sr = sf.read(path, dtype="float32")
    if data.ndim > 1:
        data = data.mean(axis=1)
    return data, sr


def pitch_spread(data, sr):
    hop, win, f0 = int(0.02 * sr), int(0.04 * sr), []
    lo, hi = int(sr / 400), int(sr / 60)
    for i in range(0, len(data) - win, hop):
        s = data[i : i + win]
        if np.sqrt(np.mean(s**2)) < 0.02:
            continue
        s = s - s.mean()
        ac = np.correlate(s, s, "full")[win - 1 :]
        k = lo + int(np.argmax(ac[lo:hi]))
        if ac[k] > 0.4 * ac[0]:
            f0.append(sr / k)
    if len(f0) < 5:
        return 0.0
    return float(np.log(np.percentile(f0, 90) / np.percentile(f0, 10)))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("lines")
    parser.add_argument("--work", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    spec = json.loads(Path(args.lines).read_text())
    work, out = Path(args.work), Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    from faster_whisper import WhisperModel
    from speechbrain.inference.speaker import EncoderClassifier

    asr = WhisperModel("small.en", device="cpu", compute_type="int8")
    encoder = EncoderClassifier.from_hparams(
        source="speechbrain/spkrec-ecapa-voxceleb", savedir=str(work / "ecapa")
    )

    def embed(data, sr):
        w = torchaudio.functional.resample(torch.tensor(data)[None], sr, 16000)
        e = encoder.encode_batch(w).squeeze().detach().cpu().numpy()
        return e / np.linalg.norm(e)

    references = {}
    manifest_path = out / "voice.json"
    manifest = {}
    for scene in spec["scenes"]:
        for line in scene["lines"]:
            who = line["who"]
            if who not in references:
                references[who] = embed(*load(work / "refs" / f"{who}.wav"))
            best = None
            for take in sorted((work / "vc").glob(f"{line['id']}_t*.wav")):
                data, sr = load(take)
                audio16 = torchaudio.functional.resample(torch.tensor(data)[None], sr, 16000)[0].numpy()
                segments, _ = asr.transcribe(audio16, beam_size=1, language="en")
                heard = " ".join(s.text for s in segments)
                score = {
                    "take": take.stem,
                    "heard": heard.strip(),
                    "wer": round(wer(line["text"], heard), 3),
                    "similarity": round(float(embed(data, sr) @ references[who]), 3),
                    "spread": round(pitch_spread(data, sr), 3),
                }
                score["rank"] = score["similarity"] + 0.5 * score["spread"] - 2.0 * max(0.0, score["wer"] - 0.1)
                if best is None or score["rank"] > best["rank"]:
                    best = score
            if best is None:
                continue
            final = out / f"{line['id']}.wav"
            subprocess.run(
                ["ffmpeg", "-y", "-loglevel", "error", "-i", str(work / "vc" / f"{best['take']}.wav"), "-af",
                 "silenceremove=start_periods=1:start_threshold=-45dB,areverse,"
                 "silenceremove=start_periods=1:start_threshold=-45dB,areverse,adelay=40,apad=pad_dur=0.1",
                 "-ar", str(OUT_RATE), "-ac", "1", "-c:a", "pcm_s24le", str(final)],
                check=True,
            )
            info = sf.info(final)
            best["duration"] = round(info.frames / info.samplerate, 3)
            best["who"] = who
            manifest[line["id"]] = best
            flag = "  CHECK" if best["wer"] > 0.2 else ""
            print(f"{line['id']} {who:9s} wer {best['wer']:.2f} sim {best['similarity']:.2f} "
                  f"spread {best['spread']:.2f}{flag}", flush=True)
            manifest_path.write_text(json.dumps(manifest, indent=1, sort_keys=True))


if __name__ == "__main__":
    main()
