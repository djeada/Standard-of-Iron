"""A beat-driven bed for short-form reels, and the grid it was built on.

The game's music is orchestral and through-composed: it swells, it rests, and
nothing in it lands on a grid. A short-form reel is cut the other way round --
the picture is cut *to* a beat, and every hit lands on one. Scoring a reel with
a battle track means the cuts fall wherever the story falls, which is why a reel
scored that way reads as a trailer rather than as an edit.

So this synthesises its own bed, with the same standard-library toolkit the cue
sounds use, and hands back the exact grid it laid: nothing has to be detected
because nothing was guessed. `scripts/beat-align.py` quantises a promo spec's
shot lengths onto that grid.

    python3 tools/audio_synth/reel_score.py --bpm 140 --bars 22 \
        --out artifacts/promo/reel_beat.ogg

Half-time is the point: at 140 BPM the kick lands on beats 1 and 3, which reads
as a 70 BPM stride under 140 BPM hats. That is the tempo a slow walk cuts to.
"""

from __future__ import annotations

import argparse
import json
import random
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import dsp

BEATS_PER_BAR = 4


def kick(seed: int) -> list[float]:
    """Sub-heavy 808: a pitch drop with a click on the front of it."""
    body = dsp.sweep(96.0, 41.0, dsp.seconds(0.42), curve=0.35)
    body = dsp.apply(body, dsp.env_ad(len(body), attack=0.002, decay=0.40, curve=2.2))
    click = dsp.bandpass(dsp.noise(dsp.seconds(0.012), seed), 2200.0, 1.4)
    click = dsp.apply(click, dsp.env_perc(len(click), attack=0.0005, curve=3.0))
    return dsp.softclip(dsp.mix(body, dsp.gain_of(click, 0.35)), drive=1.6)


def sub(note_hz: float, length: float, seed: int) -> list[float]:
    """The long tail under a bar: one note, no movement, felt more than heard."""
    tone = dsp.sine(note_hz, dsp.seconds(length))
    tone = dsp.apply(tone, dsp.env_ad(len(tone), attack=0.01, decay=length, curve=1.4))
    return dsp.lowpass(dsp.softclip(tone, drive=1.3), 180.0, 0.8)


def snare(seed: int) -> list[float]:
    """Clap-ish backbeat: two noise bursts a few milliseconds apart."""

    def burst(offset: int, level: float) -> list[float]:
        raw = dsp.bandpass(dsp.noise(dsp.seconds(0.19), seed + offset), 1750.0, 1.1)
        raw = dsp.apply(raw, dsp.env_perc(len(raw), attack=0.001, curve=2.6))
        return dsp.gain_of(raw, level)

    out = burst(0, 1.0)
    out = dsp.place(out, burst(7, 0.55), 0.011)
    body = dsp.modal(dsp.seconds(0.12), [(190.0, 0.06, 0.5), (330.0, 0.04, 0.3)], seed)
    return dsp.mix(out, dsp.gain_of(body, 0.4))


def hat(seed: int, open_hat: bool = False) -> list[float]:
    length = dsp.seconds(0.16 if open_hat else 0.035)
    raw = dsp.highpass(dsp.noise(length, seed), 7200.0, 0.9)
    return dsp.apply(raw, dsp.env_perc(len(raw), attack=0.0004, curve=3.2))


def cowbell(freq: float, seed: int) -> list[float]:
    """The phonk signature: two inharmonic partials, struck and dry."""
    return dsp.modal(
        dsp.seconds(0.30),
        [(freq, 0.10, 1.0), (freq * 1.5, 0.07, 0.7), (freq * 2.42, 0.03, 0.25)],
        seed,
    )


def drone(note_hz: float, length: float, seed: int) -> list[float]:
    """A dark held chord: root, fifth and a detuned octave, filtered down."""
    n = dsp.seconds(length)
    voices = dsp.mix(
        dsp.gain_of(dsp.sine(note_hz, n), 1.0),
        dsp.gain_of(dsp.sine(note_hz * 1.4983, n), 0.55),
        dsp.gain_of(dsp.sine(note_hz * 2.006, n), 0.32),
        dsp.gain_of(dsp.pink(n, seed), 0.05),
    )
    swept = dsp.moving_bandpass(voices, 220.0, 900.0, 0.9)
    return dsp.apply(swept, dsp.env_swell(n, peak=0.35, curve=1.5))


def render(
    bpm: float, bars: int, root_hz: float, seed: int
) -> tuple[list[float], dict]:
    """Lay the bed and return it with the grid, in seconds."""
    beat = 60.0 / bpm
    bar = beat * BEATS_PER_BAR
    total = bar * bars
    rng = random.Random(seed)

    track = dsp.silence(dsp.seconds(total + 1.2))

    intro_bars = 2
    hole_bar = intro_bars + int((bars - intro_bars) * 0.62)

    progression = [1.0, 1.0, 1.1892, 0.8909]
    accents: list[float] = []

    for bar_index in range(bars):
        at = bar_index * bar
        note = root_hz * progression[bar_index % len(progression)]
        track = dsp.place(
            track, dsp.gain_of(drone(note, bar, seed + bar_index), 0.16), at
        )
        if bar_index >= intro_bars:
            accents.append(round(at, 4))

        playing = bar_index >= intro_bars and bar_index != hole_bar
        if not playing:
            for eighth in range(BEATS_PER_BAR * 2):
                if eighth % 2 == 0:
                    track = dsp.place(
                        track,
                        dsp.gain_of(hat(seed + eighth + bar_index * 31), 0.10),
                        at + eighth * beat / 2.0,
                    )
            continue

        track = dsp.place(
            track, dsp.gain_of(sub(note * 0.5, bar * 0.9, seed), 0.55), at
        )
        for kick_beat in (0.0, 2.0, 2.75):
            track = dsp.place(
                track, dsp.gain_of(kick(seed + bar_index), 0.95), at + kick_beat * beat
            )
        for snare_beat in (1.0, 3.0):
            track = dsp.place(
                track,
                dsp.gain_of(snare(seed + bar_index * 7), 0.50),
                at + snare_beat * beat,
            )
        for sixteenth in range(BEATS_PER_BAR * 4):
            when = at + sixteenth * beat / 4.0
            if sixteenth % 2 == 0:
                level = 0.16 if sixteenth % 4 == 0 else 0.11
                track = dsp.place(
                    track, dsp.gain_of(hat(seed + sixteenth), level), when
                )
            elif rng.random() < 0.22:
                track = dsp.place(
                    track, dsp.gain_of(hat(seed + sixteenth + 3), 0.07), when
                )
        if bar_index % 2 == 1:
            for bell_beat in (1.5, 3.5):
                track = dsp.place(
                    track,
                    dsp.gain_of(cowbell(root_hz * 12.0, seed + bar_index), 0.13),
                    at + bell_beat * beat,
                )

    track = dsp.softclip(track, drive=1.15)
    track = dsp.fade_out(dsp.normalize(track, -1.2), 0.6)
    grid = {
        "bpm": bpm,
        "first_beat": 0.0,
        "beat_seconds": beat,
        "bar_seconds": bar,
        "beats_per_bar": BEATS_PER_BAR,
        "duration_seconds": round(len(track) / dsp.SR, 3),
        "drums_in": round(intro_bars * bar, 3),
        "hole": round(hole_bar * bar, 3),
        "bar_starts": accents,
    }
    return track, grid


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bpm", type=float, default=140.0)
    parser.add_argument("--bars", type=int, default=22)
    parser.add_argument(
        "--root", type=float, default=43.65, help="root note in Hz (F1)"
    )
    parser.add_argument("--seed", type=int, default=1337)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()

    track, grid = render(args.bpm, args.bars, args.root, args.seed)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as work:
        wav = Path(work) / "reel.wav"
        dsp.write_wav(wav, track)
        subprocess.run(
            [
                "ffmpeg",
                "-v",
                "error",
                "-y",
                "-i",
                str(wav),
                "-c:a",
                "libvorbis",
                "-q:a",
                "5",
                str(args.out),
            ],
            check=True,
        )
    grid_path = args.out.with_suffix(".grid.json")
    grid_path.write_text(json.dumps(grid, indent=2) + "\n")
    print(
        f"wrote {args.out} ({grid['duration_seconds']}s at {args.bpm} BPM) "
        f"and {grid_path.name}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
