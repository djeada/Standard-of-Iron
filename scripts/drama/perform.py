"""Directed takes: Qwen3-TTS VoiceDesign performs each line as written in its
`direct` field, and designs one neutral reference per character.

Runs under the Qwen TTS virtualenv (~/.local/share/soi-qwentts):

    python scripts/drama/perform.py LINES.json --work DIR [--takes 3] [--only ID ...]

Writes DIR/refs/<who>.wav and DIR/takes/<id>_t<n>.wav, plus DIR/takes.json
recording the cache key of every line so unchanged lines are skipped.
"""

import argparse
import hashlib
import json
from pathlib import Path

import soundfile as sf
import torch

REFERENCE_TEXT = (
    "Right. I'll tell you exactly what happened, and you won't believe a word of it. "
    "Nobody ever does. It started with the ledgers, as these things always do."
)


def key_for(*parts):
    return hashlib.sha1(json.dumps(parts).encode()).hexdigest()[:16]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("lines")
    parser.add_argument("--work", required=True)
    parser.add_argument("--takes", type=int, default=3)
    parser.add_argument("--only", nargs="*")
    args = parser.parse_args()

    spec = json.loads(Path(args.lines).read_text())
    voices = spec["voices"]
    work = Path(args.work)
    (work / "refs").mkdir(parents=True, exist_ok=True)
    (work / "takes").mkdir(parents=True, exist_ok=True)
    state_path = work / "takes.json"
    state = json.loads(state_path.read_text()) if state_path.exists() else {}

    from qwen_tts import Qwen3TTSModel

    model = Qwen3TTSModel.from_pretrained(
        "Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign",
        device_map="cuda:0",
        dtype=torch.bfloat16,
        attn_implementation="sdpa",
    )

    for who, description in voices.items():
        ref = work / "refs" / f"{who}.wav"
        ref_key = key_for(description, REFERENCE_TEXT)
        if state.get(f"ref:{who}") == ref_key and ref.exists():
            continue
        wavs, sr = model.generate_voice_design(
            text=REFERENCE_TEXT,
            language="English",
            instruct=f"{description} Delivery: relaxed, natural, conversational.",
        )
        sf.write(ref, wavs[0], sr)
        state[f"ref:{who}"] = ref_key
        state_path.write_text(json.dumps(state, indent=1))
        print("reference", who, flush=True)

    for scene in spec["scenes"]:
        for line in scene["lines"]:
            if args.only and line["id"] not in args.only:
                continue
            description = voices[line["who"]]
            instruct = f"{description} Delivery: {line.get('direct', 'Natural.')}"
            line_key = key_for(line["text"], instruct, args.takes)
            if state.get(line["id"]) == line_key:
                continue
            for n in range(args.takes):
                wavs, sr = model.generate_voice_design(
                    text=line["text"], language="English", instruct=instruct
                )
                sf.write(work / "takes" / f"{line['id']}_t{n}.wav", wavs[0], sr)
            state[line["id"]] = line_key
            state_path.write_text(json.dumps(state, indent=1))
            print("performed", line["id"], flush=True)


if __name__ == "__main__":
    main()
