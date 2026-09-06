# Gameplay mix review

Issue #1402 adds a runtime hierarchy after source mastering and before the
stereo-linked master limiter. Settings → Audio → Listening Preset selects
Headphones, Speakers (the fresh-install default), or Night. The selection is
persisted separately from the five existing volume sliders; changing a preset
preserves those preferences.

## Gain contract

The gains below multiply the existing master, category, resource and cue gains.
They are shared by live playback and the offline mix-review tool in
`game/audio/gameplay_mix.h`. Volume sliders remain in [0, 1].

| Bus | Trim, approximately | Source loudness policy |
| --- | ---: | --- |
| Music | -6 dB | -15 LUFS target, ±6 dB authority |
| Ambience | -6 dB | -16.5 LUFS target, ±6 dB authority |
| Combat and movement | -8 dB | Trim material above -18 estimated LUFS, never boost quiet variants |
| Voice | -1 dB | -15.5 LUFS target, ±14 dB authority |
| UI and orders | -4 dB | Existing +12 dB interface makeup, preserving authored contrast |
| Construction and economy | -10 dB | Same downward-only effect mastering |
| Weather | -10 dB | Ambience mastering |
| Wildlife and environment | -12 dB | Effect or ambience mastering according to the resource category |
| Alerts and state feedback | -2 dB | Effect mastering; voice resources remain on the voice bus |

Source processing happens on decoded PCM, avoiding lossy asset re-encoding.
Music, voice and ambience keep their existing spectral treatment. Hot effects
can be attenuated by up to 24 dB, but intentionally soft footsteps, distant
impacts and UI hover variants are not raised to a common loudness. The source
meter is an estimate, not a certified EBU R128 meter.

The final gameplay sample ceiling is 0.79 (-2.05 dBFS), including maximum slider
settings. Both the lookahead envelope and the last safety gain are stereo-linked.
This reserves reconstruction headroom; it is not an oversampled true-peak limiter.
Headphones uses safety limiting only. Speakers compresses above 0.35 (-9.1 dBFS)
at 2:1. Night compresses above 0.18 (-14.9 dBFS) at 4:1 and reduces combat,
construction/economy and wildlife by another 3.1 dB. No preset boosts silence or
quiet passages. Existing 3 ms lookahead and 120 ms limiter release are retained.

## Priority and repetition

Any audible voice lowers music, ambience, weather and the three foreground
buses by 1.4 dB. Voice or alert playback at priority 7 or higher requests 4 dB
instead. UI, alerts and voice are exempt from this background duck. The envelope
attacks over 20 ms and releases over 350 ms, applies to sounds already playing,
and is driven by rendered sample time. Muted, failed and finished playback does
not hold the duck. Two information voices share a square-root gain budget.

Combat, economy and wildlife share a density budget: above three simultaneous
voices, their gain is multiplied by `sqrt(3 / count)`. Different cue IDs therefore
cannot bypass buildup control. Night reduces this group further. Existing
per-category caps (16 SFX, 2 voices, 4 ambience), priority eviction, resource
instance limits, cue/resource cooldowns and weighted non-repeating variant
history continue to control admission. These controls keep transient detail;
the final limiter only catches the remaining correlated peaks. An inaudible
positional request is now rejected before it can evict an audible cue.

Positional attenuation is still full inside 14 world units, quadratic to silence
at 90, with pan bounded to ±0.85. Automated checks cover commander height 2 and
RTS heights 40 and 60, left/right symmetry, and near/middle/far ordering. Moving
listeners still use the existing camera and gameplay spatialization path.

## Reproduce the measurements

From the repository root, with the authored assets present:

```sh
cmake --build build --target audio_master_preview
build/bin/audio_master_preview --mix-review --out artifacts/audio-mix
```

The existing preview tool links the same mastering DSP as the game. Its new
review mode uses the runtime `GameplayMix` and `BusLimiter` directly. It writes
nine 8-second stereo WAVs and `mix-review.csv`, returning failure if an output is
non-finite or exceeds the sample ceiling. `--report-only` omits WAVs. The CSV
includes pre/post sample peak, RMS, the native loudness estimate, and minimum
limiter-envelope gain.

The deterministic schedule is an asset-based DSP stress fixture, not a capture
of the game simulation or the backend's admission path:

| Scene | Schedule |
| --- | --- |
| Quiet | Menu music plus Mediterranean ambience, first-install sliders |
| Normal | Same beds plus rain, four aligned sword impacts retriggered every 200 ms; UI, construction and wildlife every 2 s; voice and warning at 3 s; first-install sliders |
| Worst-case | Normal schedule with 16 aligned impacts and all sliders at maximum; bypasses normal cue cooldowns intentionally |

All source paths and onset rules are explicit in `mix_review` in
`tools/audio_master/main.cpp`; no random seed or audio device is involved.
To build this tool without Qt (GCC/Clang on Linux):

```sh
g++ -std=c++20 -O2 -I. -Ithird_party tools/audio_master/main.cpp \
  game/audio/audio_mastering.cpp -pthread -ldl -o /tmp/audio_master_preview
/tmp/audio_master_preview --mix-review --out artifacts/audio-mix
```

Measure each WAV independently with FFmpeg, using the `input_*` results (the
filtered output is discarded):

```sh
ffmpeg -hide_banner -i artifacts/audio-mix/headphones-worst-case.wav \
  -af loudnorm=I=-23:TP=-1:LRA=7:print_format=json -f null -
```

Recorded measurements are in [the DSP CSV](audio-mix-review.csv) and
[the independent EBU measurements](audio-mix-review-ebu.json). They are fixtures
for regression comparison, not promised LUFS values for every map or track.

## Validation and listening review

29 standalone GoogleTest tests passed for mastering, gain hierarchy, envelope
timing/recovery, density, stereo linking, all-preset clipping protection and
spatial rolloff. Backend integration tests additionally cover ducking an already
playing music bed, recovery, and a muted critical voice; these require Qt.
Run the relevant app tests in a configured build:

```sh
QT_QPA_PLATFORM=offscreen build/bin/app_tests \
  --gtest_filter='GameplayMix.*:AudioMastering.*:BusLimiterTest.*:AudioBackendTest.*:AudioBattleLoadTest.*:AudioGameplayScenarioTest.*'
```

For perceptual sign-off, review all three presets at the same device volume:

1. Fresh settings: menu → Rhone mission quiet map → sustained battle →
   victory/defeat → menu. Check music transitions without changing sliders.
2. During battle, trigger a commander message and a base-under-attack warning.
   Confirm intelligibility, audible combat under the message, and gradual return.
3. Hold repeated attacks, run across grass and hard ground, queue construction
   and resource actions. Check variant variety and fatigue over at least 60 s.
4. At RTS camera heights 40 and 60, pan across a nearby fight and out beyond
   90 units; enter commander view and rotate left/right. Check image direction,
   focus and the camera transition. Repeat in mono on ordinary speakers.
5. Keep `SOI_AUDIO_TRACE=1` and `SOI_AUDIO_TRACE_SUMMARY` enabled for these runs;
   compare accepted/drop counts using [the runtime trace guide](AUDIO_RUNTIME_TRACE.md).

The implementation environment lacked Qt 6, so the full game, QML and backend
integration suite could not be built or auditioned here. Those checks and the
perceptual review above remain required before marking the mix release-ready.
