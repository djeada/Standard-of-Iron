# Synthesised cue sounds and ambience

Sixty-four of the game's sound effects are not recordings. They are generated from
the recipes in this directory by `make audio-assets`.

## Why generate them

- **Provenance is trivial.** Nothing here is sampled, scraped or licensed, so
  there is no attribution to track and no commercial-use question to answer.
- **They stay small.** All sixty-four come to roughly 640 KiB of Vorbis. Most are
  under 5 KiB, which is mostly the Vorbis header.
- **They stay editable.** "The click is too bright" is a one-line change to a
  recipe followed by a regeneration, not a trip back to a sound library.
- **They stay short.** The cues these replaced were ten-second clips, which is
  a real problem for a sound that fires on every button hover.

## Layout

| File                     | Role                                                                                  |
| ------------------------ | ------------------------------------------------------------------------------------- |
| `dsp.py`                 | Buffers, envelopes, filters and effects. Standard library only.                       |
| `instruments.py`         | Physical bodies: struck wood and bronze, thuds, cloth, gravel, creaks, horns, breath. |
| `cues.py`                | One recipe per cue, plus where the file lands and how loud it may peak.               |
| `synthesize_cues.py`     | Renders recipes to `assets/audio` and encodes with ffmpeg.                            |
| `register_cues.py`       | Adds the manifest tracks and points each cue at its resource.                         |
| `analyse.py`             | Measures rendered files so they can be judged without listening.                      |
| `ambience.py`            | One recipe per looping ambience bed, plus the layers they are built from.             |
| `synthesize_ambience.py` | Renders the beds into `assets/audio/ambience` at the mixer's sample rate.             |

## Regenerating

```sh
make audio-assets                                   # everything
python3 tools/audio_synth/synthesize_cues.py ui.click    # one cue
python3 tools/audio_synth/synthesize_cues.py 'combat.*'  # a family
```

The audio is deterministic: every recipe seeds its own noise, so regenerating
without editing a recipe reproduces the same samples. The _file_ is not quite,
because ffmpeg's Ogg muxer picks a random stream serial, so about two dozen
bytes of container header differ on every encode. Regenerate the cue you are
working on rather than the whole set, or `git checkout` the files you did not
mean to touch -- otherwise a one-cue change arrives as a hundred-file diff.

## Variants

A cue that fires hundreds of times a match becomes a machine gun if it is
always the same clip, so the repeated ones are rendered as two or three takes.
`Recipe.takes` says how many, and `register_cues.py` binds all of them to the
cue; the registry then picks at random and never plays the same take twice in
a row.

A take is not a separate recipe. `dsp.set_variant(n)` applies a global nudge
inside the primitives: every seed is offset, so noise, grains and scatter land
differently, and every pitch is detuned by a couple of percent, so a struck
body rings at a slightly different note the way a real one would. The
structure of the sound is untouched, which is why the takes measure within a
few percent of each other on `analyse.py` while being audibly different
performances.

Give takes to anything the player triggers repeatedly. Leave rare, long or
musical cues at one -- a detuned brass stinger reads as a wrong note rather
than a second take.

## Tuning a sound

The `wanted:` line in `assets/audio/audio_cues.json` is the spec, and the
matching function in `cues.py` is the implementation. Read them side by side.

Two habits keep the recipes honest:

- **Level each layer with `at_db` before mixing.** Normalising the sum lets
  whichever element peaks highest bury the rest -- a resonant creak peaks far
  lower than a struck thud, so mixing raw makes the creak vanish.
- **Check the result with `analyse.py`.** It reports duration, peak, spectral
  centroid and how the centroid moves across the clip, which is enough to catch
  the failures that matter: a dull sound that came out bright, a rising sweep
  that actually falls, or a layer that got trimmed away as silence.

```sh
python3 tools/audio_synth/analyse.py assets/audio/sfx/ui/*.ogg
```

For example `ui.error` should measure far darker than `ui.click`, and
`combat.perfect_guard` far brighter than `combat.block` -- that contrast is the
whole point of those two cues.

## What these are not

They are convincing abstract and foley sounds, not a replacement for a
recordist on the cues that carry real drama. The ones most worth re-recording
later are the crowd and voice-adjacent pieces -- `combat.charge`,
`combat.vanguard_rush`, `build.construction_started` -- where synthesis reads as
"plausible" rather than "real". Everything in the `ui.*`, `state.*` and
`order.*` families is meant to stay synthetic; abstract interface sounds are
what this technique is best at.

## The ambience beds

Twelve of the nineteen looping beds in `assets/audio/ambience` are generated
here, by `make audio-ambience`. They replaced 16 kHz ten-second clips that the
mixer had to resample at load — which manufactured a mirror of their own noise
across 8-16 kHz — and whose energy sat in the 2-6 kHz band the ear finds most
fatiguing. See [AUDIO_MASTERING.md](../../docs/AUDIO_MASTERING.md) for the
measurements.

The beds are rendered at 48 kHz so nothing resamples them, run 18.8 s so the
repeat is less obvious, and are folded tail-into-head so the file loops without
a seam. `ambience.py` holds one recipe per bed over a small set of layers:
`wind`, `water`, `leaves`, `murmur`, `knocks`, `march`, `fire`, `gulls`.

```sh
python3 tools/audio_synth/synthesize_ambience.py                 # every bed
python3 tools/audio_synth/synthesize_ambience.py camp            # matching beds
python3 tools/audio_synth/synthesize_ambience.py --out /tmp/try  # audition first
```

The twelve left here are camps, roads, markets and marching columns — places
defined by people, where a generated murmur reads as a crowd well enough.

### The seven that are not generated

The nature beds — mountain, plains, forest, river, night camp, and the two
weather skies — are cut from public-domain field recordings instead, and live
in [tools/audio_field](../audio_field/README.md). Filtered noise makes a
convincing crowd and an unconvincing hillside: wind is not a noise band, and a
bird is not a sine burst. Those seven are committed rather than rendered, so do
not add recipes for them here — `make audio-ambience` would then overwrite the
recordings on the next build.

### Invariants

Two are held by `AmbienceAssetsTest`, over every bed regardless of which
pipeline made it: each is stored at the mixer's sample rate, and none is louder
in 2-6 kHz than in its 100-800 Hz body. A recipe that breaks either fails the
suite.

Two of the beds are not places but skies. `weather_rain` and `weather_snow` are
layered over whatever bed the biome chose, so they carry only the weather and
never the ground under it -- see
[AUDIO_MASTERING.md](../../docs/AUDIO_MASTERING.md).
