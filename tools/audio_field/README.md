# Recorded audio

Two pipelines live here, both cutting shipped assets out of public-domain and
CC0 recordings rather than generating them:

| Pipeline                       | Recipe        | Builder             | Output                  |
| ------------------------------ | ------------- | ------------------- | ----------------------- |
| Looping ambience beds          | `sources.py`  | `build_beds.py`     | `assets/audio/ambience` |
| Short combat and movement cues | `oneshots.py` | `build_oneshots.py` | `assets/audio/sfx`      |
| Composed battle cues           | `battle.py`   | `build_battle.py`   | `assets/audio/sfx`      |

All three are documented below. `slice.py` is shared: it finds the individual
hits inside a recording, which is what the one-shot builder needs and what the
marching ambience beds are layered from.

The three differ in what they do to a source. `build_beds.py` cuts one window
and loop-seals it. `build_oneshots.py` finds the transients in a performance
and keeps the best few. `build_battle.py` sums several recordings together to
make a sound nobody recorded — there is no field recording of a Punic-war
battlefield, so a volley is a swish fired six times inside a third of a second
and a cavalry charge is three separate gallop performances laid over each other.

## Ambience beds

Thirteen of the game's twenty-one looping ambience beds are cut from
recordings rather than generated. `sources.py` is the recipe — which recording,
which window inside it, and what shaping — and `build_beds.py` turns that into
the `.ogg` files in `assets/audio/ambience`.

| Bed                                             | What it is                             |
| ----------------------------------------------- | -------------------------------------- |
| `alpine_mountain_pass`                          | Tramontane over stone at 2784 m        |
| `mediterranean_plains`                          | Summer meadow, insects over wind       |
| `forest_ambush`                                 | Birds under wind in the canopy         |
| `river_crossing`                                | Shallow river under a bridge           |
| `mountain_camp_night`                           | Crickets over a fire                   |
| `storm`                                         | Steady storm rain, own recording       |
| `weather_rain`                                  | Light steady rain, no thunder          |
| `camp_fire_night`                               | A hearth, close                        |
| `weather_snow`                                  | Blizzard — the wind, not the snow      |
| `battlefield_dry_wind_distant_march_01` / `_02` | Dry wind with a column off to one side |
| `desert_army_march`                             | Carthage moving, under a sandstorm     |
| `roman_road`                                    | A column on paving, wind pulled back   |

## Why these are not generated

The other eight beds are camps, markets and a harbour, and
[tools/audio_synth](../audio_synth/README.md) generates them convincingly: a
crowd really is a filtered murmur and a hammer really is a struck body. Outdoors
that stops being true. Wind is not a noise band with a slow envelope on it, and
a bird is not a sine burst — the generated versions read as _hiss with events
in it_, and no amount of recipe tuning fixed that, because the thing being
imitated has structure the recipe does not model.

The trade is deliberate and it costs something: these carry provenance (recorded in
[THIRD_PARTY_LICENSES.md](../../THIRD_PARTY_LICENSES.md)), they are committed
rather than rebuilt, and rebuilding them needs a network.

## Rebuilding

```sh
make audio-field-ambience                                  # every bed
python3 tools/audio_field/build_beds.py weather_*          # matching beds
python3 tools/audio_field/build_beds.py --list             # sources and licences
python3 tools/audio_field/build_beds.py --cache ~/.soi-ambience   # keep downloads
```

This is **not** part of `make audio-assets`: the originals run to tens of
megabytes each and the build must work offline. Use `--cache` while iterating on
a window, or every run re-downloads them.

## What the build does to a source

1. **Cut** the declared window, downmixed to mono at 48 kHz — the mixer's rate,
   so nothing resamples at load.
2. **Shape** it: high-pass out the mic rumble, low-pass and high-shelf the
   layers that are too bright to sit under combat.
3. **Sum** the layers. A bed is as long as its shortest layer.
4. **Seal the loop** by folding the tail back over the head with the same
   equal-power crossfade the generated beds use, so the file wraps on two
   adjacent samples of the original recording and the seam is exact.
5. **Level** it to −19.3 LUFS behind a limiter holding peaks at −6 dBFS.

Step 5 is where recordings differ most from generated beds. A gust or a cricket
has a far higher crest factor than filtered noise, so gain alone would either
miss the loudness target or eat the headroom that decode-time mastering needs
(see [AUDIO_MASTERING.md](../../docs/AUDIO_MASTERING.md)). Limiting settles
that, and the loop iterates because limiting changes the loudness it was
measuring.

## Choosing a window

The windows in `sources.py` were not chosen by ear. Per-second broadband RMS and
per-second RMS above 2 kHz are measured across the whole recording, and the
window where both stay flattest wins — a passing plane, a voice or one hard gust
moves at least one of them, and any of those becomes glaring once a 22-second
bed has looped four times.

If you change a window, check the result against the two invariants
`AmbienceAssetsTest` holds before committing it:

```sh
./build/bin/app_tests --gtest_filter='AmbienceAssets*'
```

The second invariant — 2-6 kHz must sit at least 3 dB under 100-800 Hz — is the
one a recording fails. Birds, insects and rain all live in exactly that band.
Fix it with the layer's `shelf_db`, and if shelving alone leaves the bed sounding
thin, give it a body layer rather than shelving harder.

### Marching columns

No public-domain library holds a Roman column on the march: field recordings are
of one person walking. The march beds sum the same walk against itself at
offsets that are deliberately not multiples of each other (`ranks` in
`sources.py`), each copy quieter and darker than the last. The ear reads
overlapping unsynchronised footfalls as a body of men; equal spacing would just
sound like one very loud walker. `loop_source` repeats the walk to fill the bed,
because those recordings run ten to sixteen seconds and a bed runs twenty-two.

## One-shot cues

`oneshots.py` describes the short combat and movement cues: which recording,
how many variants, and the shaping. `build_oneshots.py` finds the individual
hits and writes them out.

```sh
python3 tools/audio_field/build_oneshots.py                # every take
python3 tools/audio_field/build_oneshots.py footstep_*     # matching takes
python3 tools/audio_field/build_oneshots.py --dry-run      # detect, write nothing
python3 tools/audio_field/build_oneshots.py --list         # sources and licences
```

The CC0 libraries record a _performance_ — forty sword hits in a row, twenty
footsteps — and a cue needs one hit. `slice.py` gates the envelope with separate
open and close thresholds so a decaying tail does not chatter one hit into five,
then the build ranks the survivors by level and keeps the best few as variants.

`--dry-run` prints how many hits were found and where, which is the fastest way
to tune a gate. Outdoor takes need the tighter `OUTDOOR` settings: their floor
is an ambience rather than silence, so the default long `tail_ms` never sees
enough quiet to close and the whole recording reads as one enormous hit.

Encoding checks itself. Vorbis does not preserve peaks, and a hard metal
transient normalised to −1.9 dBFS can decode _above_ full scale — a click on the
cue that fires most often. The builder measures the decoded file and re-encodes
at lower gain until the decoder agrees.

### What a one-shot must not be

A five-second recording of a whole battle is not an impact. Every `combat.hit.*`
cue fires every 90–260 ms; anything bound to them has to be a single event,
a few hundred milliseconds long, or the game plays overlapping copies of a crowd.
The mass recordings that used to back those cues are still shipped — they moved
to the volley, charge and ambient-state stingers, which fire seconds apart and
genuinely want a crowd.
