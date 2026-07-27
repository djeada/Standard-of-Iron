#!/usr/bin/env python3
"""Compare Arena regression captures against stored baselines.

The lighting Arena scenarios (``lighting_*``) are deterministic fixtures: fixed
camera, locked clock, fixed weather and seed.  That makes their final frame a
usable visual regression signal, but only with a tolerance -- GPU drivers,
filtering and dithering all produce small per-pixel differences that are not
defects.

Two numbers are reported per scenario:

  * ``mean``  - mean absolute per-channel difference, 0-255.  Catches an overall
                shift such as a lighting or exposure regression.
  * ``max_px`` - fraction of pixels differing by more than ``--pixel-threshold``.
                Catches a localised break such as a missing shadow or an
                unlit material, which a mean alone would dilute away.

Both must stay within tolerance for a scenario to pass.

Typical use::

    # record baselines once, from a known-good build
    scripts/compare-arena-captures.py --update --captures artifacts/arena

    # in CI
    scripts/compare-arena-captures.py --captures artifacts/arena

Exit status is non-zero when any scenario regresses or a baseline is missing,
so it can be used directly as a CI gate.
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:  # pragma: no cover - dependency guidance only
    print(
        "error: Pillow is required (pip install pillow)",
        file=sys.stderr,
    )
    raise SystemExit(2)

DEFAULT_FRAME = "final.png"
DEFAULT_BASELINE_DIR = Path("tests/baselines/arena")


def load_rgb(path: Path) -> Image.Image:
    return Image.open(path).convert("RGB")


def compare(candidate: Image.Image, baseline: Image.Image, pixel_threshold: int):
    """Return (mean_abs_diff, fraction_of_pixels_over_threshold, size_mismatch)."""
    if candidate.size != baseline.size:
        return (255.0, 1.0, True)

    cand = candidate.getdata()
    base = baseline.getdata()
    total_pixels = len(cand)
    if total_pixels == 0:
        return (0.0, 0.0, False)

    diff_sum = 0
    over_threshold = 0
    for lhs, rhs in zip(cand, base):
        d0 = abs(lhs[0] - rhs[0])
        d1 = abs(lhs[1] - rhs[1])
        d2 = abs(lhs[2] - rhs[2])
        diff_sum += d0 + d1 + d2
        if max(d0, d1, d2) > pixel_threshold:
            over_threshold += 1

    mean = diff_sum / (total_pixels * 3)
    return (mean, over_threshold / total_pixels, False)


def discover(captures: Path, frame: str) -> dict[str, Path]:
    found: dict[str, Path] = {}
    if not captures.is_dir():
        return found
    for scenario_dir in sorted(p for p in captures.iterdir() if p.is_dir()):
        image = scenario_dir / frame
        if image.is_file():
            found[scenario_dir.name] = image
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "--captures",
        type=Path,
        default=Path("artifacts/arena"),
        help="directory of per-scenario Arena artifact folders",
    )
    parser.add_argument(
        "--baselines",
        type=Path,
        default=DEFAULT_BASELINE_DIR,
        help="directory holding baseline images",
    )
    parser.add_argument("--frame", default=DEFAULT_FRAME, help="frame file to compare")
    parser.add_argument(
        "--prefix",
        default="",
        help="only consider scenarios whose id starts with this prefix",
    )
    parser.add_argument(
        "--mean-tolerance",
        type=float,
        default=2.0,
        help="max allowed mean absolute channel difference (0-255)",
    )
    parser.add_argument(
        "--pixel-threshold",
        type=int,
        default=24,
        help="per-channel difference above which a pixel counts as changed",
    )
    parser.add_argument(
        "--pixel-tolerance",
        type=float,
        default=0.01,
        help="max allowed fraction of changed pixels (0-1)",
    )
    parser.add_argument(
        "--update",
        action="store_true",
        help="write current captures as the new baselines instead of comparing",
    )
    parser.add_argument("--json", type=Path, help="write a machine-readable report here")
    args = parser.parse_args()

    captures = discover(args.captures, args.frame)
    if args.prefix:
        captures = {k: v for k, v in captures.items() if k.startswith(args.prefix)}

    if not captures:
        print(
            f"error: no '{args.frame}' captures under {args.captures}"
            + (f" matching prefix '{args.prefix}'" if args.prefix else ""),
            file=sys.stderr,
        )
        return 2

    if args.update:
        args.baselines.mkdir(parents=True, exist_ok=True)
        for scenario, image in captures.items():
            shutil.copyfile(image, args.baselines / f"{scenario}.png")
        print(f"updated {len(captures)} baseline(s) in {args.baselines}")
        return 0

    results = []
    failures = 0
    missing = 0
    for scenario, image in captures.items():
        baseline_path = args.baselines / f"{scenario}.png"
        if not baseline_path.is_file():
            print(f"MISSING  {scenario}: no baseline at {baseline_path}")
            results.append({"scenario": scenario, "status": "missing"})
            missing += 1
            continue

        mean, changed, size_mismatch = compare(
            load_rgb(image), load_rgb(baseline_path), args.pixel_threshold
        )
        ok = (
            not size_mismatch
            and mean <= args.mean_tolerance
            and changed <= args.pixel_tolerance
        )
        status = "pass" if ok else "fail"
        if not ok:
            failures += 1
        note = " (size mismatch)" if size_mismatch else ""
        print(
            f"{'PASS' if ok else 'FAIL'}     {scenario}: "
            f"mean={mean:.3f} (<= {args.mean_tolerance}) "
            f"changed={changed * 100:.2f}% (<= {args.pixel_tolerance * 100:.2f}%)"
            f"{note}"
        )
        results.append(
            {
                "scenario": scenario,
                "status": status,
                "mean_abs_diff": round(mean, 4),
                "changed_fraction": round(changed, 6),
                "size_mismatch": size_mismatch,
            }
        )

    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(
            json.dumps(
                {
                    "mean_tolerance": args.mean_tolerance,
                    "pixel_threshold": args.pixel_threshold,
                    "pixel_tolerance": args.pixel_tolerance,
                    "results": results,
                },
                indent=2,
            )
        )

    total = len(results)
    print(f"\n{total - failures - missing}/{total} scenario(s) within tolerance")
    if missing:
        print(f"{missing} baseline(s) missing; run with --update to record them")
    return 1 if (failures or missing) else 0


if __name__ == "__main__":
    raise SystemExit(main())
