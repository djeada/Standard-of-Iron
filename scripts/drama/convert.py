"""Give every take its character's voice: Chatterbox VC converts the timbre of a
directed take to the character's reference while keeping its delivery.

Runs under the Chatterbox virtualenv (~/.local/share/soi-cbx):

    python scripts/drama/convert.py LINES.json --work DIR
"""

import argparse
import json
from pathlib import Path

import soundfile as sf


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("lines")
    parser.add_argument("--work", required=True)
    args = parser.parse_args()
    spec = json.loads(Path(args.lines).read_text())
    work = Path(args.work)
    out_dir = work / "vc"
    out_dir.mkdir(parents=True, exist_ok=True)

    from chatterbox.vc import ChatterboxVC

    vc = ChatterboxVC.from_pretrained("cuda")
    who_of = {line["id"]: line["who"] for s in spec["scenes"] for line in s["lines"]}
    for take in sorted((work / "takes").glob("*_t*.wav")):
        line_id = take.stem.rsplit("_t", 1)[0]
        if line_id not in who_of:
            continue
        out = out_dir / take.name
        reference = work / "refs" / f"{who_of[line_id]}.wav"
        if out.exists() and out.stat().st_mtime >= max(
            take.stat().st_mtime, reference.stat().st_mtime
        ):
            continue
        wav = vc.generate(str(take), target_voice_path=str(reference))
        sf.write(out, wav.squeeze().cpu().numpy(), vc.sr)
        print("converted", take.name, flush=True)


if __name__ == "__main__":
    main()
