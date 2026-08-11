#!/usr/bin/env python3
"""Re-record one scenario's shots and drop them back into an existing capture.

A promo capture names each clip ``NN_<shot>.mp4`` where ``NN`` is the shot's
index in the *whole* spec, so handing the arena a spec containing a subset
renumbers everything and the clips no longer line up with the manifest. That
makes fixing one act expensive: a two-minute reel is a forty-minute capture at
``supersample: 2``, and retiming a single shot should not cost the other
twenty-seven.

This runs the arena over a temporary spec holding only the shots that name one
scenario, then copies each result back over the clip with the matching *name*
rather than the matching index. Everything else on disk -- the other clips, the
posters, ``shots.json`` -- is left exactly as it was, so the next
``promo-edit.py`` run picks the new footage up with no other change.

    scripts/reshoot-promo-scenario.py trailer_muster

Only the picture is re-recorded. A shot's duration is a property of the spec
rather than of the capture, so replacing a clip cannot desynchronise the
manifest; but if you changed a shot's ``duration`` or ``slow_motion``, re-run
``place-trailer-cues.py`` afterwards so the sound follows the new timeline.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scenario", help="scenario id whose shots to re-record")
    parser.add_argument(
        "--spec",
        type=Path,
        default=ROOT / "tools/arena/promos/trailer.json",
        help="promo spec the capture was made from",
    )
    parser.add_argument(
        "--clips",
        type=Path,
        default=ROOT / "artifacts/promo/trailer",
        help="directory holding the capture to patch",
    )
    parser.add_argument(
        "--scratch",
        type=Path,
        default=ROOT / "artifacts/promo/.reshoot",
        help="working directory for the partial capture",
    )
    parser.add_argument(
        "--arena",
        type=Path,
        default=ROOT / "build/bin/arena_app",
        help="arena binary to record with",
    )
    args = parser.parse_args()

    spec = json.loads(args.spec.read_text())
    subset = [shot for shot in spec["shots"] if shot["scenario"] == args.scenario]
    if not subset:
        print(f"reshoot: no shot uses scenario {args.scenario}", file=sys.stderr)
        return 1
    if not args.clips.is_dir():
        print(f"reshoot: no capture at {args.clips}", file=sys.stderr)
        return 1

    partial = dict(spec)
    partial["id"] = "reshoot"
    partial["shots"] = subset
    partial.pop("sfx", None)
    args.scratch.mkdir(parents=True, exist_ok=True)
    partial_path = args.scratch / "reshoot.json"
    partial_path.write_text(json.dumps(partial, indent=2))

    names = ", ".join(shot["name"] for shot in subset)
    print(f"reshoot: re-recording {len(subset)} shot(s) of {args.scenario}: {names}")

    out = args.scratch / "out"
    if out.exists():
        shutil.rmtree(out)
    result = subprocess.run(
        [str(args.arena), "--promo-spec", str(partial_path), "--promo-out", str(out)],
        cwd=str(args.arena.parent),
        capture_output=True,
        text=True,
    )
    produced = sorted((out / "reshoot").glob("*.mp4"))
    if len(produced) != len(subset):
        print(result.stdout[-4000:], file=sys.stderr)
        print(
            f"reshoot: expected {len(subset)} clip(s), got {len(produced)}",
            file=sys.stderr,
        )
        return 1

    index_of = {shot["name"]: number for number, shot in enumerate(spec["shots"], 1)}
    for clip in produced:
        name = clip.stem.split("_", 1)[1]
        targets = list(args.clips.glob(f"*_{name}.mp4"))
        if len(targets) > 1:
            print(
                f"reshoot: cannot place '{name}': {len(targets)} matching clip(s)",
                file=sys.stderr,
            )
            return 1
        if not targets:

            if name not in index_of:
                print(f"reshoot: '{name}' is not in {args.spec}", file=sys.stderr)
                return 1
            targets = [args.clips / f"{index_of[name]:02d}_{name}.mp4"]
        shutil.copy2(clip, targets[0])
        poster = clip.with_suffix(".png")
        if poster.is_file():
            shutil.copy2(poster, targets[0].with_suffix(".png"))
        print(f"reshoot:   {clip.name} -> {targets[0].name}")

    shutil.rmtree(args.scratch, ignore_errors=True)
    print("reshoot: done; re-run promo-edit.py to cut the reel again")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
