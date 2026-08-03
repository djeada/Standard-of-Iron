# Audio mastering

Every sample the game plays is generated: the music and ambience beds come out
of a model, the voices out of a text-to-speech model, and the interface and
combat cues out of `tools/audio_synth`. Generated masters arrive with problems
that recorded material does not have, and the game used to make them worse
rather than better. This document explains what was wrong, what the fix does,
and where the remaining costs are.

## What was wrong

Three separate things were clipping the signal, and all three were audible as
the "metallic", fatiguing quality the music had.

**1. The decode filter added +2.4 dB and then hard-clipped.** The old
`apply_decode_softening_filter` finished each sample with

```
sample = tanh(sample * drive) / tanh(drive)      // drive = 1 + drive_amount
sample = clamp(sample * output_gain, -1, 1)
```

The intent was a gentle saturator normalised so that full scale maps to full
scale. But dividing by `tanh(drive)` sets the _small signal_ gain to
`drive / tanh(drive)`, which for the music profile is `1.0046 / 0.7640 = 1.315`
— **+2.38 dB of makeup gain on everything**, immediately followed by a hard
clamp. The stage was not softening anything; it was a gain stage into a clipper.

Everything else the filter did was inaudible. Its three EQ bands were scaled by
an `intensity` that the content-adaptive step drove to the same value, `0.306`,
for every music track in the game, which turns the cuts into -0.49 / -0.67 /
-0.34 dB. The high-band compressor's threshold landed 20 dB above the level of
the band it was meant to control. The `warmth_drive` reached the fourth decimal
place.

**2. The source masters already decode above full scale.** Measured through the
game's own decode path, every music file peaks between +0.09 and +1.26 dBFS —
they are streaming-style masters limited to -14 LUFS, and the Vorbis round trip
pushes the reconstructed peaks over 0 dBFS. Combined with (1), a single 90 s
track contained **178,548 hard-clipped samples**.

**3. The mix bus hard-clipped the sum.** `on_audio` summed every music channel
and up to 32 effect slots and finished with a per-sample clamp to ±1.0. Voices
went in at ×2.0 (`VOICE_GAIN`), so a voice line peaking at -0.9 dBFS reached
+5.1 dBFS on the bus. During a battle the clamp was running constantly.

Clipping is broadband odd-harmonic distortion. Generating it at 2–10 kHz on top
of material that already has vocoder artefacts up there is exactly what "hard on
the ears" sounds like.

On top of the clipping, the generated masters have two real defects that no
static EQ can fix: narrow **stationary resonances** (a single bin that stays
6–10 dB above its own spectral neighbourhood for most of the track — a codec or
vocoder artefact, not a note), and wildly inconsistent brightness between
tracks, from `base_management_camp` with 12% of its energy above 8 kHz to
`base_barracks_recruitment` with nothing above 2.4 kHz at all.

## What the fix does

`game/audio/audio_mastering.{h,cpp}` replaces the old filter with a measured,
per-asset mastering pass that runs once at decode. Nothing is baked into the
asset files, so there is no second generation of lossy encoding and no profile
that can go stale when an asset is replaced.

It runs in two parts.

### Analysis — one pass plus 192 sampled spectra

A single pass over the decoded PCM produces the peak, a K-weighted gated
loudness (BS.1770 pre-filter and relative gate; mono, so it is LUFS up to the
channel weighting), the RMS and peak level of the two harshness bands, and
whether the two decoded channels are bit-identical.

The spectral half takes up to 192 windows of 2048 samples spread across the
whole file — not every frame, because the statistics that matter converge long
before that and sampling keeps the cost negligible. For each window it compares
the magnitude spectrum against a ±½-octave smoothed copy of itself. A bin that
sits more than 6 dB above its own neighbourhood is "hot"; a bin that is hot in
more than half the sampled windows, with a mean excess of 5 dB or more, is a
**stationary ridge**.

That test is what separates an artefact from music, but only when there is
enough music for it to separate. A melodic partial moves, so it cannot occupy
one bin for half of a 90-second track; a resonance does not move. Over a
1.5-second chime there is nothing to move, and the test cannot tell the chime's
pitch from a defect — the first version of this pass put notches on 1768, 2354
and 3131 Hz in `objective_complete.ogg`, which is a harmonic series, which is
the sound.

So the search is fenced in three ways. It only looks between 1.5 and 12 kHz.
It only runs on material at least 20 seconds long, which excludes every effect
cue and every 10-second stinger while keeping the music beds the complaint was
about. And it only runs for music, ambience and voice — `Material::Effect` and
`Material::Interface` never get notches at all, because those cues come out of
`tools/audio_synth` and a harsh one is fixed by editing its recipe, not by
carving its harmonics out at load time. Adjacent clusters within 1/6 octave are
merged so that one artefact band becomes one notch rather than four.

On the current library the search fires on exactly two files, both long music
beds, both with the isolated high-frequency clusters that a codec leaves
behind.

Each surviving ridge becomes a peaking cut: 75% of the measured excess above a
3 dB floor, Q from the measured width, and a depth ceiling that rises with
frequency — 3 dB below 3 kHz where music lives, 7 dB above 5 kHz where the
artefacts live.

### Application — a cascade sized by what the analysis found

1. **DC blocker.**
2. **Measured notches**, up to four.
3. **Tonal balance safety net.** The presence (2–5 kHz) and air (6–12 kHz) zones
   are compared, relative to the 200–800 Hz body, against a per-material target
   picked near the bright end of the corpus. The correction can only ever cut,
   never boost, and never by more than 3 dB. On the current library it engages
   on 3 music tracks out of 20 and 6 voice lines out of 31; that is the intent —
   it is a limit on outliers, not a leveller. An earlier version allowed a
   1.5 dB boost for voice and every one of the 31 lines took it, which is a
   blanket EQ wearing a safety net's clothes.
4. **Two-band dynamic harshness control** at 3.4 kHz and 7.2 kHz, thresholds set
   relative to each band's own measured RMS, so it pulls a band down only while
   that band is hot. Skipped entirely when the analysis proves the band's peak
   never reaches the threshold.
5. **Loudness normalisation** to the material's target, measured on the _shaped_
   signal, bounded by an authority window and by the headroom that is left.
6. **Look-ahead limiter** with a hard guarantee, described below.

Interface and effect cues get steps 1, 3, 4 and 6 — never step 2, for the reason
above, and never step 5, because their levels are authored deliberately in
`tools/audio_synth/cues.py` and normalising them would flatten the contrast the
recipes were written to produce.

Interface cues additionally take a **fixed +12 dB makeup** (`Profile::makeup_db`)
in place of step 5. `audio_synth` writes them around -25 dBFS, and effect cues
leave the pass on the -1 dBFS ceiling, so the whole interface bus used to sit
12 dB under everything else: a click was inaudible under music, and a hover blip
at -34 dBFS was inaudible full stop. A single gain for the material moves the bus
without touching the levels inside it, so the contrast step 5 would have
flattened survives exactly — the loudest of the 25 cues lands on the ceiling and
every other cue keeps its authored distance from it. The limiter still runs
after, so the ceiling guarantee below is unaffected.

### The limiter's ceiling guarantee

The offline limiter computes, per 32-sample block, the gain that block needs to
sit under the ceiling, takes a centred sliding minimum of radius `L+1` blocks,
then smooths it with two box filters of radius `L/2`.

Every value the smoother averages lies within `L+1` blocks of the block being
smoothed, and each of those values is already no larger than the gain that block
needs — so their average cannot be larger either, and neither can a linear
interpolation between two neighbouring smoothed values. The ceiling therefore
cannot be exceeded by construction, not merely by a final clamp. A release-rate
cap that only ever lowers the curve preserves the property.

The real-time bus limiter in `bus_limiter.h` is the streaming counterpart: a
3 ms look-ahead delay, a two-block running peak, a soft knee 6 dB below the
ceiling, and an attack fast enough to converge within the look-ahead. The
per-sample clamp in `on_audio` is now a backstop that should never fire.

## Results on the current assets

Measured through the game's own decode path, over all twenty music tracks:

| material  | files | worst peak in | worst peak out | clipped samples in | files clipping | notched | tilted |
| --------- | ----- | ------------- | -------------- | ------------------ | -------------- | ------- | ------ |
| music     | 20    | +1.26 dBFS    | -1.00 dBFS     | 176,454            | 20             | 2       | 3      |
| ambience  | 18    | +1.32 dBFS    | -1.50 dBFS     | 3,908              | 12             | 0       | 1      |
| effect    | 104   | +2.18 dBFS    | -1.00 dBFS     | 17,378             | 26             | 0       | 25     |
| interface | 25    | -12.99 dBFS   | -1.00 dBFS     | 0                  | 0              | 0       | 9      |
| voice     | 31    | -0.75 dBFS    | -1.00 dBFS     | 0                  | 0              | 0       | 6      |

Every one of the twenty music tracks was clipping on decode. Nothing clips now.
Music loudness converges on -15.0 LUFS (17 of 20 land exactly there; -15.8 is
the worst case, where the headroom rule binds first).

`ui.click_confirm` measures -14.35 dBFS in and -2.26 dBFS out with no notches and
no tilt: the +12 dB makeup is the only thing that moved it, and the same +12 dB
moved all 25 cues. Only the loudest, `ui.error_thud_v2`, asks the limiter for
anything at all, and it asks for 0.02 dB.

`VOICE_GAIN` in `audio_system.cpp` dropped from 2.0 to 1.0. It existed because
voices were quiet — they measure -21.9 LUFS — and it paid for that with 6 dB of
bus clipping. Voice material is now normalised to -15.5 LUFS at decode instead,
which lands within half a decibel of the old perceived level without the
distortion.

## Cost

Per asset, measured on a 90 s music track at 48 kHz stereo:

| stage                        | cost    |
| ---------------------------- | ------- |
| Vorbis decode (pre-existing) | ~170 ms |
| mastering analysis           | ~45 ms  |
| mastering application        | ~90 ms  |

The whole 198-file library — decode, analyse and master — costs about 7 seconds
of CPU in total (`make audio-preview AUDIO_PREVIEW_ARGS=--report-only`).

The analysis avoids every scratch buffer: loudness, band levels, peak and
channel-identity all come from one fused pass, and the spectral half reads the
interleaved PCM directly. The spectral half only smooths and takes logarithms inside the band the ridge
search actually reads, which is under half the spectrum. The application
specialises its inner loop on a compile-time channel count, computes the
dynamic-band gains at control rate (every 32 samples, well inside the 4 ms
attack) rather than calling `pow` per sample, and skips any stage the analysis
proves is a no-op — including a dynamic band whose measured peak never reaches
its own threshold.

The largest single win is that **all music, ambience and effect assets are mono
files upmixed to stereo at decode**, so their two channels are bit-identical.
The analysis detects that and the whole chain runs once, on one channel, and the
result is duplicated at the end.

## Loading, memory and the mixer

Three things about the playback path used to cost more than they needed to. All
three are fixed; this section is the map of what the backend now does.

### Decoding never blocks the caller

`MiniaudioBackend::request_track` allocates a track slot, records the id, and
hands the file to a decode worker thread. It does not decode. Registering all
twenty music tracks blocks the calling thread for **0.04 ms**; the worker spends
2.5 s on them in the background. Before, the same work was 7 s of synchronous
decoding on the GUI thread, and a single 90 s track was a ~360 ms stall on every
screen or mission change.

There is no pending-play queue, because the slot index exists the moment the
track is registered. `play()` can reference a track that has not finished
decoding — the mixer simply produces nothing for that channel until the slot's
pointer becomes non-null, then starts it. Music that is played immediately after
being registered begins a fraction of a second late instead of failing.

One-shot effects behave differently on purpose: `play_sound` drops a cue whose
track is not decoded yet rather than firing it late, because a late one-shot is
worse than a missing one. To keep that window small the worker runs two queues —
effects, interface cues and voices first, music and ambience beds last — so the
short assets a mission needs immediately are ready long before the beds finish.

A decode that fails is logged and leaves the slot empty; the resource stays
registered and silently plays nothing, where before the loader would have
dropped it at load time.

### Mono sources are stored once

Every music, ambience and effect asset in the game is a mono file that the
decoder upmixes to stereo, so both channels are bit-identical. The mastering
analysis already detects that, and the decoder now stores one channel instead of
two: **34.6 MB down to 17.3 MB** for a 90 s music track. The mixer branches on the
stored channel count and writes the same sample to both outputs, so the stereo
path costs one load per frame instead of two.

### The mixer takes no locks

`on_audio` used to take a `QMutex` shared with every control call — a priority
inversion waiting to happen on the one thread in the process that must never
block. The mixing state (channels, effect slots, master volume) is now owned
exclusively by the audio thread, and control threads reach it through a
fixed-capacity lock-free command ring (`audio_commands.h`). Commands carry track
_indices_, never strings, so nothing allocates.

Track lifetime is handled without reference counting: `unload` removes the id
from the registry, posts a `ReleaseTrack` command, and waits for the mixer to
report that it has processed it before freeing the storage. Since the id is gone
first, no later command can reference the slot.

Queries answer from published atomics rather than shared state:
`any_channel_playing` and `channel_playing` read a bitmask the mixer stores each
callback, and `is_sound_active` scans one atomic per effect slot.

Two consequences worth knowing. The bus limiter adds its 3 ms of look-ahead as
output latency, so a change takes one look-ahead to appear in the output — tests
that assert silence after a stop need to render twice. And `initialize` takes an
`open_device` flag: passing `false` brings up the mixer and the decode worker
without a playback device, which is how the backend tests drive `on_audio`
directly and deterministically.

### What is left

Music is still fully decoded into RAM — 17.3 MB per 90 s track, and a mission
can hold six. Streaming would remove that, and the mastering chain is streamable
except for the analysis, which needs a first pass over the file, and the
limiter's look-ahead.

## Promo videos

`scripts/promo-edit.py` scores the arena's captured shots with a music track, and
that track goes through this chain first: it invokes
`build/bin/audio_master_preview --render`, which links the same
`audio_mastering.cpp` the game uses, so a promo cannot be scored with audio that
differs from what a player hears. If the tool is not built the edit warns and
falls back to the raw track rather than failing.

Delivery then needs headroom rather than another normaliser. The mastered track
is already on its loudness target with a -1 dBFS ceiling, so the edit adds only a
safety limiter at -4 dBFS. That sounds low until you measure what AAC does to
dense material: at the old `loudnorm=I=-14:TP=-1.5` setting the _decoded_ short
peaked at +1.47 dBFS with 176 clipped samples, and at -1.5 dBFS sample peak it
still decoded at +0.69 dBFS. At -4 dBFS the decoded short peaks at -0.86 dBFS
with nothing clipped, and measures 0.44 dB quieter than not limiting at all.

## Auditioning a change

`tools/audio_master` builds a preview binary that links the same
`audio_mastering.cpp` the game uses, so it cannot drift from what you will hear:

```sh
make audio-preview                       # every asset, into artifacts/audio_preview
make audio-preview AUDIO_PREVIEW_ARGS="assets/audio/music/menu/*.ogg"
```

It writes `<name>.before.wav` and `<name>.after.wav` next to a printed report of
what the analysis measured and what the chain decided to do.
