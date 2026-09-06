# Audio mastering

For the current gameplay bus trims, effect loudness guard, listening presets and
reproducible final-mix diagnostics, see [Gameplay mix review](GAMEPLAY_AUDIO_MIX.md).
The measurements below document the earlier source-mastering pass.

Almost every sample the game plays is generated: the music comes out of a model,
the voices out of a text-to-speech model, and the interface and combat cues out
of `tools/audio_synth`. Generated masters arrive with problems that recorded
material does not have, and the game used to make them worse rather than better.
This document explains what was wrong, what the fix does, and where the
remaining costs are.

The exceptions are the seven nature ambience beds, which are cut from
public-domain field recordings by `tools/audio_field` — see
[that directory's README](../tools/audio_field/README.md) for why, and for the
levelling they need that a generated bed does not.

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

### Beds are decoded at their own rate and resampled with a real filter

The loudest artefact in the game was not in any asset: it was manufactured at
load time. Every ambience bed shipped at **16 kHz**, and the decoder was asked to
produce 48 kHz, so miniaudio resampled with its stock linear interpolator behind
a fourth-order IIR low-pass whose cutoff sits exactly at the source Nyquist.
Linear interpolation mirrors the source's content around its own sampling
frequency, and a 24 dB/octave filter starting at 8 kHz cannot remove an image
that begins at 8 kHz.

Measured on the mix the game actually produced, against the level of its own
1-4 kHz band:

| Band       | 16 kHz source, decoded by the engine | The same file through `soxr` |
| ---------- | ------------------------------------ | ---------------------------- |
| 8.2-10 kHz | **-26.5 dB**                         | -85.9 dB                     |
| 10-12 kHz  | -34.2 dB                             | -85.9 dB                     |
| 12-14 kHz  | -48.8 dB                             | -85.9 dB                     |

A 16 kHz file cannot contain anything above 8 kHz, so every decibel in those
rows was invented by the resampler: a mirror image of the bed's own noise,
continuous, sitting in the band the ear is most sensitive to, under every map
for the whole mission. Raising the IIR to eighth order — the highest miniaudio
allows — moved the worst band by only 4 dB, because the problem is the shape of
the filter, not its order.

`game/audio/resampler.{h,cpp}` decodes at the file's native rate and converts
with a polyphase FIR: a Kaiser-windowed sinc designed for a 90 dB stopband and a
transition band 10% of the passband, which comes to 115 taps per phase for both
ratios the game ships (16 kHz and 32 kHz into 48 kHz). The same measurement
afterwards reads **-69.0 dB** at 8.2-10 kHz — 42 dB of fabricated hiss removed —
and the real 6-7.8 kHz band comes back 5 dB louder, because the old IIR had been
eating content as well as failing to stop images.

It costs 39 ms for a ten-second bed and 286 ms for a sixty-second music track,
on the decode worker, behind the loading screen.

`ResamplerTest.UpsamplingDoesNotFabricateContentAboveTheSourceNyquist` measures
the imaging against a linear interpolator written into the test, so reverting to
interpolation fails rather than quietly passing.

### The beds themselves are generated, not sampled

Fixing the resampler stopped the game inventing hiss, but the beds were still
16 kHz ten-second clips, and three of them measured _louder_ in 2-6 kHz than in
their own 100-800 Hz body — `alpine_mountain_pass` by +2.0 dB. Several decoded
above full scale (`mediterranean_plains` clipped 1132 samples) and every one of
them had a step discontinuity at its loop point.

`tools/audio_synth/ambience.py` replaced all eighteen with generated beds, for
the reasons the cue sounds are generated: nothing sampled or licensed, a bed is
retuned by editing a recipe, and they can be produced at the rate the mixer runs
at. `make audio-ambience` renders them.

Each bed is layers of noise, a filter and a slow contour — wind, water,
foliage, murmur, work sounds, a distant tread, fire — mixed per recipe, shaped
by a house curve that removes the rumble below 45 Hz and the hiss above 3.2 kHz,
then folded tail-into-head so the file loops without a seam.

That generalised further than it should have. Camps, roads and markets survive
being generated because a crowd really is a filtered murmur, but the outdoor
beds did not: filtered noise makes a hillside that reads as hiss with events in
it. The seven nature beds are now cut from public-domain field recordings by
`tools/audio_field` instead, holding the same two invariants and the same
loudness target. The remaining twelve are still generated here.

|                          | before                         | after                             |
| ------------------------ | ------------------------------ | --------------------------------- |
| Sample rate              | 16 kHz                         | 48 kHz, so nothing resamples them |
| Length                   | 10 s                           | 18.8 s                            |
| 2-6 kHz against the body | +2.0 to -20.3 dB               | **-5.9 to -15.4 dB**, never above |
| Clipped samples          | up to 1132                     | **0**                             |
| Loop wrap step           | up to 0.67, to 590x the median | within normal signal variation    |
| Limiter action at load   | 2-4 dB                         | 0 dB                              |

`AmbienceAssetsTest` holds both properties on the shipped files: every bed is
stored at the mixer's rate, and no bed is louder in 2-6 kHz than in its body.

### Weather is a layer, not a bed per sky

A rainy forest is the forest plus rain. Authoring one bed per biome and sky
would be eighteen times three assets, so weather rides over whatever bed the
biome chose: `ambient.weather_rain` and `ambient.weather_snow` loop on their own
effect slot alongside the biome bed.

`App::Core::WeatherAudio` follows `RainManager`, which already runs the weather
cycle and publishes an intensity that has been faded in and out, and rides that
intensity so the sound arrives and leaves with the thing on screen. Snow had no
sound at all before this: the only weather query in the ambience selection asked
for rain, and snow maps fell through to their biome bed in silence.

Two things had to change underneath for a layer to work at all.

**A looping sound can now have its level changed while it plays.** The mixer's
effect slots carried a fixed volume, so the only way to follow an intensity was
to stop and restart. `AudioCommand::SetSoundVolume` ramps an effect's volume the
way `SetVolume` already ramped a music channel's, and
`AudioSystem::set_playing_sound_volume` reaches it.

**A looping sound whose start was dropped no longer holds its slot forever.**
`play_sound` drops a cue whose track is still decoding, but the sound was still
recorded as active, and `cleanup_inactive_sounds_locked` deliberately never
pruned loops. With `max_instances: 1` on the bed, that one dropped start
blocked every later attempt for the rest of the session -- silently, because the
rejection happens before the backend is reached. Cleanup now prunes a loop the
mixer is not running, after a grace period long enough to cover the gap between
asking for a sound and the mixer publishing that it is running. The weather
layer also waits for the mixer to confirm it is playing rather than assuming it,
and preloads its bed when the mission's weather is configured.

### Loudness is matched within a category, not across them

A player should not have to reach for the volume between one unit answering and
the next. Measured through the game's own decode path, output loudness by
category:

| Category  | Before  | After      |
| --------- | ------- | ---------- |
| Ambience  | 3.8 LU  | **0.1 LU** |
| Music     | 1.2 LU  | 1.2 LU     |
| Voice     | 7.8 LU  | **2.2 LU** |
| Effects   | 7.9 LU  | 7.9 LU     |
| Interface | 17.9 LU | 17.9 LU    |

Voice was the defect. Every voice asset is the same kind of thing -- a unit
answering an order -- so they have to land together, and the profile does ask for
that. But the quietest shipped lines decode near -27 LUFS, 11.8 dB from the
-15.5 target, and `loudness_authority_db` was the generic 6 dB. They were
clamped six decibels short and played five to six decibels under the rest of the
cast, which is exactly the drop you hear moving between units. Their peaks sat
at -10 dBFS, so the headroom to correct them had been there all along. Voice
authority is now 14 dB.

The effect and interface spreads are **not** defects and are deliberately left
alone: those profiles set `normalise_loudness = false` because the level of a
cue is a design decision. A pointer hover at -30 LUFS _should_ be far quieter
than an error at -12, and an arrow flyby quieter than a reinforcements horn.
Flattening them would make the interface shout.

Category volumes the player sets in the options menu are applied after all of
this, so turning music down still turns music down.

### Loops are sealed, so the wrap is not a click

Every music and ambience track is looped — `AudioSystem::play_music` hardcodes
the loop flag, and `apply_mission_ambience` starts its bed with `loop = true` —
and the mixer loops by setting the read position back to zero. The generated
masters do not end where they begin, so that assignment was a step
discontinuity spliced into the output: a broadband click.

It was loudest and most frequent on ambience, because the beds are ten seconds
long and one plays on every map. Measured through the decode path, the wrap step
was 0.10 to 0.57 of full scale against a median sample-to-sample step of 0.005
to 0.05 — 10x to 100x the surrounding signal, every ten seconds, for the whole
mission. `music.combat.cavalry_flank_fast` was 0.57 and
`music.stinger.mission_failed_distant_horn` 0.51.

`game/audio/loop_seam.{h,cpp}` folds the last 120 ms of a bed into its first
120 ms with an **equal-power** crossfade and reports the shorter length that now
wraps cleanly; the decoder truncates the buffer to that length. The mixer keeps
wrapping at `frames` and knows nothing about any of this. Across the shipped
assets the wrap step drops to 0.0006–0.034, which is at or below the material's
own median step — an ordinary sample transition rather than an edge.

Two choices in there are load-bearing:

- **Equal power, not linear.** Beds are noise-like, so the two halves of the
  crossfade are uncorrelated; linear weights sum to 0.707 in the middle and
  would trade the click for a 3 dB dip once per loop.
  `TheCrossfadeHoldsItsLevelThroughTheWrap` is the guard.
- **After mastering, not before.** Sealing first leaves the mastering filters'
  start-up transient at frame 0, which is itself a discontinuity at the wrap —
  on a near-DC test signal it reproduced the entire original step. Sealing last
  folds steady-state tail over that transient and covers it. The cost is that
  the crossfade can sum above the mastering ceiling; measured on the shipped
  beds the fade region peaks at most **+0.19 dB** above the rest of the track,
  so nothing needs re-limiting.

`AudioBackendTest.ALoopingBedWrapsWithoutAClick` renders a bed that ends a
quarter cycle short through the real mixer, crosses the wrap twice, and fails if
any step outside the attack is more than 12x the median.

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
