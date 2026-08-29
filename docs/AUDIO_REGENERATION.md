# Audio regeneration list

Hand-maintained companion to `AUDIO_WISHLIST.md`, which is generated and only checks that
links are not broken. This file records what is _bad_ or _absent_, what should replace it,
and the prompt to generate it with.

Last measured 29 Aug 2026, after the August import. Method at the bottom.

## Where this stands

The August 2026 batch replaced 38 cues and added 10 that did not exist. Measured against the
CC0 recorded material in the tree, which is the quality bar:

|                                                               | Before  | After   |
| ------------------------------------------------------------- | ------- | ------- |
| Cues measuring as oscillators rather than recordings          | 26      | 2       |
| Spectral flatness of the non-recorded cues (0.34 is the bar)  | 0.024   | 0.141   |
| Dominant partial above its neighbourhood (14.5 dB is the bar) | 26.1 dB | 14.5 dB |
| Share of energy above 6 kHz (0.14 is the bar)                 | 0.023   | 0.144   |

## How to use this

Each row is one cue. Generate the listed number of variants, save them to the exact paths
given, and run `python3 tools/audio_import/import_cues.py --source DIR` after adding an entry
to its `PLAN` table — that handles trimming, levelling and encoding. New cues also need an
entry in `audio_cues.json`, a constant in `game/audio/cue_ids.h` (and its `k_all` count), a
manifest entry, and a call site. All five, or the sound is silently absent.

**ElevenLabs settings:** set the duration explicitly rather than letting it auto-fit, and keep
prompt influence high so the "dry, close, no reverb" clauses are honoured. The game masters at
decode (`game/audio/audio_mastering.cpp`), so deliver dry and unprocessed — no reverb, no
limiting, no normalisation. Anything under half a second must have no reverb tail at all or it
smears when the cue retriggers. Deliver at 48 kHz.

## 1. Still worth replacing

| Files                                                           | Length | Prompt                                                                                                                                                                                  |
| --------------------------------------------------------------- | ------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `sfx/alerts/enemy_spotted_horn.ogg` (`alert.base_under_attack`) | 1.8 s  | Urgent repeated alarm call on a bronze war horn, rough and pressing, close and dry. The current file is the old pitched-down hunting-horn note and has **no energy at all above 6 kHz** |
| `sfx/alerts/reinforcements_arrived.ogg`                         | 2.4 s  | Friendly bronze horns sounding arrival, warm and rising, distant marching feet underneath                                                                                               |
| `sfx/combat/ability_refused.ogg`                                | 0.30 s | Short scraping catch of a blade stopped dead against a shield boss, metallic and damped, no ring. Currently backed up by the shared refusal, so this is optional                        |

`alert.unit_lost`, `alert.objective_failed`, `build.unit_ready` and `order.patrol` still score
high on the tonality measure, but that is the measure mis-firing rather than a defect: a low
mournful drum, a falling brass figure, a bronze bell and a two-note horn are _supposed_ to be
dark and tonal. They were listened for and left alone.

## 2. Missing — still no cue

| Asset to create                          | Length | Prompt                                                                                                   | Wire into                                                                 |
| ---------------------------------------- | ------ | -------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| `sfx/movement/footstep_water_01..04.ogg` | 0.25 s | Single boot stepping into shallow water over gravel, splash with a stone shift underneath, dry and close | rivers are crossable and silent; only grass, stone and run surfaces exist |
| `sfx/economy/spend_tick.ogg`             | 0.40 s | A few coins dropping dully into an empty wooden box, dark and quick, no ring                             | the counterpart to `economy.income`, for a resource being spent           |

## 3. Present but not reachable

`sfx/movement/hooves_walk.ogg` was imported and is deliberately **not** in the manifest. It
needs a mounted-movement state to loop against, and no such hook exists: `move.footstep` is
driven by the first-person commander's step timer, and `horse_gallop_close_pass` is bound to
`combat.hit.cavalry` as an impact. `hooves_gallop` found a home on `combat.charge`; the walk
loop needs someone to add the movement hook first. It shows up under "Files not in the
manifest" in `AUDIO_WISHLIST.md` so it is not forgotten.

## 4. Defects that do not need regenerating

Good recordings with a mechanical fault; trimming and fading fixes these. The August import
cleared the ones it touched — every imported file is trimmed from its loudest transient and
faded at both ends, so it cannot start or end on a step. What remains is in the older CC0
material.

**Boundary clicks.** The sample at the file boundary sits above −40 dB relative to peak,
so playback starts or ends on a step. Fix with a 2–5 ms fade at the affected end.

| File                                 | Boundary level           |
| ------------------------------------ | ------------------------ |
| `sfx/build/unit_queued.ogg`          | head -27 dB, tail -25 dB |
| `sfx/build/unit_queued_v2.ogg`       | head -33 dB              |
| `sfx/combat/arrow_impact_02.ogg`     | head -37 dB              |
| `sfx/combat/shield_bash_v2.ogg`      | head -28 dB              |
| `sfx/combat/sword_hit_02.ogg`        | head -39 dB              |
| `sfx/combat/sword_hit_03.ogg`        | head -40 dB              |
| `sfx/combat/sword_hit_04.ogg`        | head -14 dB, tail -35 dB |
| `sfx/movement/footstep_grass_01.ogg` | head -40 dB              |
| `sfx/movement/footstep_grass_03.ogg` | head -38 dB              |
| `sfx/movement/footstep_run_01.ogg`   | head -29 dB              |
| `sfx/movement/footstep_run_02.ogg`   | head -40 dB              |
| `sfx/movement/footstep_run_03.ogg`   | head -33 dB              |
| `sfx/movement/footstep_run_04.ogg`   | head -37 dB              |
| `sfx/movement/footstep_stone_01.ogg` | head -32 dB              |
| `sfx/movement/footstep_stone_02.ogg` | head -30 dB              |
| `sfx/movement/footstep_stone_04.ogg` | head -21 dB, tail -35 dB |
| `sfx/orders/attack_horn_stab.ogg`    | tail -33 dB              |
| `sfx/orders/move_kit_shuffle.ogg`    | tail -29 dB              |
| `sfx/orders/move_kit_shuffle_v3.ogg` | tail -37 dB              |
| `sfx/orders/run_kit_rattle_v2.ogg`   | head -33 dB              |
| `sfx/state/load_complete.ogg`        | head -37 dB              |
| `sfx/state/speed_notch.ogg`          | head -31 dB              |
| `sfx/ui/notification.ogg`            | head -40 dB              |
| `sfx/ui/select_group.ogg`            | head -32 dB              |
| `sfx/ui/tab_slide.ogg`               | tail -38 dB              |

**Dead lead-in.** Triggered by a game event, so silence before the sound is latency
the player feels as the game reacting late. Trim to the onset.

| File                                         | Silence before onset |
| -------------------------------------------- | -------------------- |
| `sfx/combat/arrows_overhead_ambience.ogg`    | **1290 ms**          |
| `sfx/combat/spearmen_formation_advance.ogg`  | **660 ms**           |
| `sfx/combat/aftermath_battlefield.ogg`       | **460 ms**           |
| `sfx/combat/battlefield_distant_mass_01.ogg` | **340 ms**           |
| `sfx/combat/arrows_overhead_dark.ogg`        | **300 ms**           |
| `sfx/build/gate_open.ogg`                    | **230 ms**           |
| `sfx/build/gate_close.ogg`                   | **210 ms**           |
| `sfx/combat/roman_cavalry_charge.ogg`        | **210 ms**           |
| `sfx/combat/battlefield_crowd_chaos.ogg`     | **170 ms**           |
| `sfx/combat/battlefield_distant_mass_02.ogg` | **170 ms**           |
| `sfx/combat/army_march_dirt_mass.ogg`        | **140 ms**           |
| `sfx/combat/bow_loose_heavy_v2.ogg`          | **90 ms**            |
| `sfx/wildlife/wolf_pack_attack.ogg`          | **70 ms**            |

## Method

Each file is decoded to mono and measured for **spectral flatness** (geometric over
arithmetic mean of the mean magnitude spectrum: 1.0 is noise, near 0 is a pure tone),
**partial prominence** (how far the loudest bin stands above the median of its
neighbourhood — a real transient spreads energy, an oscillator concentrates it),
**high-frequency fraction** above 6 kHz, **spectral flux** frame to frame, and the
**boundary sample level** and **onset delay** for the mechanical faults above.

The first four combine into the ordering used in section 1. Deliberately dark and tonal
cues — bells, low drums, falling brass — score high without being defective, so the
ordering is a starting point for listening rather than a verdict.

A high noise floor on a crowd or battle bed is the recording rather than a fault, so floor
measurements are not used to condemn anything here.
