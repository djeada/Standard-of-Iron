# Audio regeneration list

Hand-maintained companion to `AUDIO_WISHLIST.md`, which is generated and only checks that
links are not broken. This file records what is _bad_ or _absent_, what should replace it,
and the prompt to generate it with.

Measured 29 Aug 2026 across all 170 files in `assets/audio/sfx/`. Method at the bottom.

## How to use this

Each row is one cue. Generate the listed number of variants, save them to the exact paths
given, and the manifest already points at them — no `audio_manifest.json` edit is needed for
a replacement. New cues in section 3 need a manifest entry and a call site as well.

**ElevenLabs settings for everything here:** set the duration explicitly to the length in the
table rather than letting it auto-fit, and keep prompt influence high so the "dry, close, no
reverb" clauses are honoured. The game applies its own mastering at decode
(`game/audio/audio_mastering.cpp`), so deliver these dry and unprocessed — no reverb, no
limiting, no normalisation. Anything under half a second must have **no reverb tail at all**
or it will smear when the cue retriggers.

Deliver at 48 kHz. The bake to 32 kHz mono Vorbis `-q:a 4` happens on import; see
`tools/audio_music/import_music.py` for the encode and level settings.

## 1. Replace first — 28 cues, 47 files

Twenty-six of these measure as oscillators rather than recordings: spectral flatness around **0.024**
against **0.336** for the CC0 recorded material already in the tree, a single partial standing
**26 dB** above its neighbours, and **six times less energy above 6 kHz**. They are dull sine
beeps, and several of them fire on every click. `ui.hover` and `ui.deselect` do not measure
as badly but are listed here anyway: they carry boundary clicks, and the interface family
wants regenerating together so the whole set shares one voice.

### Interface

| Files                                     | Length | Prompt                                                                                                                            |
| ----------------------------------------- | ------ | --------------------------------------------------------------------------------------------------------------------------------- |
| `sfx/ui/click_confirm.ogg`, `_v2`, `_v3`  | 0.12 s | Single firm tap of a wooden stylus on stiff oiled leather, close-miked, tight low-mid thump, no ring, completely dry, no reverb   |
| `sfx/ui/hover_brush.ogg`, `_v2`, `_v3`    | 0.06 s | Very soft brush of a fingertip across dry parchment, faint and airy, extremely short, no pitch, no reverb                         |
| `sfx/ui/select_unit.ogg`, `_v2`, `_v3`    | 0.12 s | Single knuckle knock on a wooden shield boss, dry and close, short woody thud with a faint bronze edge, no reverb                 |
| `sfx/ui/deselect.ogg`, `_v2`              | 0.12 s | Soft brush of a hand lifting off leather, very short, dry, no tone, no reverb                                                     |
| `sfx/ui/toggle_latch.ogg`, `_v2`          | 0.10 s | Small bronze latch snapping shut, tiny metallic click with a very short bright edge, close-miked, completely dry                  |
| `sfx/ui/back_cancel.ogg`, `_v2`           | 0.18 s | Soft wooden lid closing on a small box, one short descending knock, dark and damped, no reverb                                    |
| `sfx/ui/confirm_seal.ogg`, `_v2`          | 0.20 s | Bronze seal pressed firmly into hot wax on a wooden desk, one decisive compressed press with a small metallic edge, dry and close |
| `sfx/ui/error_thud.ogg`, `_v2`            | 0.30 s | Two dull muted thuds of a fist against a thick leather-covered shield, dead and damped, no ring, no reverb                        |
| `sfx/ui/command_accept.ogg`, `_v2`, `_v3` | 0.15 s | Single knock of a spear shaft against a bronze shield rim, short bright ring that dies immediately, dry and close                 |
| `sfx/ui/command_refuse.ogg`, `_v2`        | 0.35 s | Dull heavy thud of a shield dropped flat onto packed earth with a short gritty scrape, dark and damped, no reverb                 |

### Orders

| Files                                       | Length | Prompt                                                                                                                |
| ------------------------------------------- | ------ | --------------------------------------------------------------------------------------------------------------------- |
| `sfx/orders/attack_horn_stab.ogg`           | 0.45 s | Single short blast on a Roman bronze cornu war horn, hard attack, close and dry, cut off abruptly with no reverb tail |
| `sfx/orders/patrol_horn_two_note.ogg`       | 0.80 s | Two calm descending notes on a bronze war horn, unhurried and steady, close and dry, no reverb                        |
| `sfx/orders/stop_drum.ogg`, `_v2`           | 0.25 s | One hit on a taut leather war drum with the hand damping the skin immediately, dry, close, abrupt cut                 |
| `sfx/orders/hold_shields_plant.ogg`, `_v2`  | 0.30 s | Several wooden shields planted down into dirt at once, one short compact thud with grit, dry and close                |
| `sfx/orders/guard_spear_taps.ogg`, `_v2`    | 0.35 s | Two taps of a spear butt on hard packed ground, woody and dry, close, faint dust grit                                 |
| `sfx/orders/formation_standard_planted.ogg` | 0.35 s | Wooden standard pole driven decisively into packed earth, one solid thud with soil compressing, dry and close         |

### Construction

| Files                                      | Length | Prompt                                                                                                              |
| ------------------------------------------ | ------ | ------------------------------------------------------------------------------------------------------------------- |
| `sfx/build/placement_confirmed.ogg`, `_v2` | 0.25 s | One hammer blow driving a wooden stake into soil, solid woody knock with earth compressing around it, dry and close |
| `sfx/build/placement_rejected.ogg`, `_v2`  | 0.25 s | Dry refusing thunk of wood striking wood and stopping dead, heavily damped, no ring, no reverb                      |
| `sfx/build/unit_ready_bell.ogg`            | 1.5 s  | Single low bronze bell struck once in an open courtyard, warm fundamental, natural decay, no added reverb           |

### Alerts

| Files                                         | Length | Prompt                                                                                                                                                                            |
| --------------------------------------------- | ------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `sfx/alerts/unit_lost.ogg`                    | 1.0 s  | One low mournful tap on a large leather war drum, dark and soft, natural decay, no reverb                                                                                         |
| `sfx/alerts/commander_message.ogg`            | 1.4 s  | Muted horn call from a distant camp, close-mouthed and dark, followed by the dry wooden turn of a wax writing tablet. Announces a speaker — must not sound like a victory fanfare |
| `sfx/alerts/objective_failed.ogg`             | 1.8 s  | Short falling three-note figure on low bronze horns, dark and heavy, dry and close, no reverb tail                                                                                |
| `sfx/alerts/enemy_spotted_horn.ogg`           | 1.8 s  | Urgent repeated alarm call on a bronze war horn, rough and pressing, close and dry                                                                                                |
| `sfx/alerts/population_limit_horn.ogg`        | 1.4 s  | Short flat two-note bronze horn call that ends unresolved, dry and close                                                                                                          |
| `sfx/alerts/enemy_reinforcements_warning.ogg` | 2.4 s  | Enemy war horns answering from a distance across open ground, low and threatening, some air between them and the listener, no music                                               |
| `sfx/alerts/low_resources_click.ogg`          | 0.5 s  | Small dry wooden click of an empty counting tally, hollow and final, close, no reverb                                                                                             |

The five alert horns above are the pitched-down hunting-horn note. They measure **zero energy
above 6 kHz**, which is why they sound muffled rather than urgent.

### Game state

| Files                  | Length | Prompt                                                                                            |
| ---------------------- | ------ | ------------------------------------------------------------------------------------------------- |
| `sfx/state/pause.ogg`  | 0.40 s | War drum hit immediately damped by a hand, the sound of stopping, falling in pitch, dry and close |
| `sfx/state/resume.ogg` | 0.40 s | Soft war drum pickup, one rising damped hit that leads forward, dry and close                     |

These two are the worst-measuring files in the whole set — a single partial standing 53–55 dB
above its neighbours, which is to say a sine wave.

## 2. Replace when convenient — 7 cues, 10 files

| Files                                   | Length | Prompt                                                                                                  |
| --------------------------------------- | ------ | ------------------------------------------------------------------------------------------------------- |
| `sfx/alerts/objective_complete.ogg`     | 1.8 s  | Short confident rising three-note figure on ancient bronze horns, dry and close, no reverb tail         |
| `sfx/build/construction_complete.ogg`   | 2.2 s  | Final heavy timber beam settling into place, followed by one short satisfied bronze note, dry and close |
| `sfx/build/construction_started.ogg`    | 1.0 s  | Wooden scaffolding poles set down and lashed together, rope tightening on timber, dry and close         |
| `sfx/combat/ability_refused.ogg`        | 0.30 s | Short scraping catch of a blade stopped dead against a shield boss, metallic and damped, no ring        |
| `sfx/alerts/reinforcements_arrived.ogg` | 2.4 s  | Friendly bronze horns sounding arrival, warm and rising, distant marching feet underneath               |

## 3. Missing entirely — no cue exists yet

These need a new cue in `assets/audio/audio_cues.json`, a constant in `game/audio/cue_ids.h`,
a manifest entry, and a call site. All four, or the sound is silently absent.

| Asset to create                           | Length             | Prompt                                                                                                                                   | Wire into                                                                                                                                          |
| ----------------------------------------- | ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| `sfx/undead/skeletons_rise_01..03.ogg`    | 2.5 s              | Dry bones and rusted armour shifting and grinding up out of earth and stone, a heavy stone lid scraping aside, hollow and dead, no music | `undead_awakening_system.cpp` — currently borrows `combat.hit.generic`                                                                             |
| `sfx/economy/resource_deposit_01..03.ogg` | 0.35 s             | Heavy grain sacks and wicker baskets set down onto packed earth, dull soft thump with a shift of grain inside, dry and close             | `resource_delivery_system.cpp` — currently fires `build.placement_confirmed`, a construction stake, on every drop-off                              |
| `sfx/economy/capture_gained.ogg`          | 1.8 s              | Cloth standard struck and re-raised, wooden pole thudding into ground, one bronze note over it, dry and close                            | `capture_system.cpp` — currently fires the reinforcements horn                                                                                     |
| `sfx/economy/capture_lost.ogg`            | 1.8 s              | Standard torn down, cloth ripping and a wooden pole falling onto earth, one dull broken bronze note                                      | as above                                                                                                                                           |
| `sfx/economy/income_tick.ogg`             | 0.40 s             | Small handful of bronze coins and grain tipped into a wooden bowl, light and quick, dry and close                                        | `EconomyFeedbackEvent` — no audio subscriber exists at all                                                                                         |
| `sfx/economy/spend_tick.ogg`              | 0.40 s             | A few coins dropping dully into an empty wooden box, dark and quick, no ring                                                             | as above                                                                                                                                           |
| `sfx/combat/stagger_01..03.ogg`           | 0.50 s             | Soldier in armour losing his footing, bronze scale and leather rattling with a scuffed step in dirt, dry and close                       | `combat_status_effect_system.cpp` — stagger and knockdown are silent, including the stagger that cancels the commander's swing                     |
| `ambience/building_fire_loop.ogg`         | 8 s, seamless loop | Large timber building burning steadily, crackling with occasional beam cracks and collapses, no ignition whoosh, no start transient      | `combat_system/structure_fire.cpp` — buildings burn silently                                                                                       |
| `sfx/movement/footstep_water_01..04.ogg`  | 0.25 s             | Single boot stepping into shallow water over gravel, splash with a stone shift underneath, dry and close                                 | rivers are crossable and currently silent; only grass, stone and run surfaces exist                                                                |
| `sfx/movement/hooves_walk.ogg`            | 0.8 s, loop        | Shod horse hooves at a walk on a packed dirt road, steady and even, close and dry                                                        | mounted movement has no sound; `horse_gallop_close_pass` is bound to `combat.hit.cavalry`, so it is a hit sound                                    |
| `sfx/movement/hooves_trot.ogg`            | 0.7 s, loop        | Shod horse hooves at a trot on packed dirt, steady rhythm, close and dry                                                                 | as above                                                                                                                                           |
| `sfx/movement/hooves_gallop.ogg`          | 0.6 s, loop        | Shod horse hooves at a full gallop on packed dirt, driving and heavy, close and dry                                                      | as above                                                                                                                                           |
| `sfx/combat/elephant_trumpet_charge.ogg`  | 2.5 s              | War elephant trumpeting on the charge, huge and aggressive, heavy footfalls underneath                                                   | `elephant_special_processor.cpp` — the existing charge and panic recordings are bound only to `combat.hit.elephant`, so they play as impact sounds |
| `sfx/combat/elephant_trumpet_panic.ogg`   | 2.5 s              | War elephant panicking, broken high-pitched distressed trumpeting, uneven and frightened                                                 | as above                                                                                                                                           |
| `sfx/wildlife/sheep_bleat_01..03.ogg`     | 1.2 s              | Single sheep bleating in an open field, close and natural, no reverb                                                                     | `wildlife_system.cpp` — sheep have a renderer and no voice                                                                                         |
| `sfx/wildlife/bird_chirp_01..03.ogg`      | 2.0 s              | Small countryside birds chirping briefly, sparse and natural, open air                                                                   | as above                                                                                                                                           |
| `sfx/wildlife/horse_idle_01..03.ogg`      | 1.2 s              | Horse snorting and whinnying softly while standing still, close and natural                                                              | as above                                                                                                                                           |

## 4. Defects that do not need regenerating

Good recordings with a mechanical fault. Trimming and fading fixes these; a new render is not
needed.

**Waveform discontinuity at a file boundary — an audible click.** The sample at the boundary
sits above −40 dB relative to peak, so playback starts or ends on a step. Fix with a 2–5 ms
fade at the affected end.

| File                                   | Boundary level           |
| -------------------------------------- | ------------------------ |
| `sfx/build/construction_complete.ogg`  | head -34 dB              |
| `sfx/build/placement_confirmed_v2.ogg` | head -37 dB              |
| `sfx/build/placement_rejected_v2.ogg`  | head -37 dB              |
| `sfx/build/unit_queued.ogg`            | head -27 dB, tail -25 dB |
| `sfx/build/unit_queued_v2.ogg`         | head -33 dB              |
| `sfx/build/unit_ready_bell.ogg`        | head -37 dB              |
| `sfx/combat/arrow_impact_02.ogg`       | head -37 dB              |
| `sfx/combat/shield_bash_v2.ogg`        | head -28 dB              |
| `sfx/combat/sword_hit_02.ogg`          | head -39 dB              |
| `sfx/combat/sword_hit_03.ogg`          | head -40 dB              |
| `sfx/combat/sword_hit_04.ogg`          | head -14 dB, tail -35 dB |
| `sfx/movement/footstep_grass_01.ogg`   | head -40 dB              |
| `sfx/movement/footstep_grass_03.ogg`   | head -38 dB              |
| `sfx/movement/footstep_run_01.ogg`     | head -29 dB              |
| `sfx/movement/footstep_run_02.ogg`     | head -40 dB              |
| `sfx/movement/footstep_run_03.ogg`     | head -33 dB              |
| `sfx/movement/footstep_run_04.ogg`     | head -37 dB              |
| `sfx/movement/footstep_stone_01.ogg`   | head -32 dB              |
| `sfx/movement/footstep_stone_02.ogg`   | head -30 dB              |
| `sfx/movement/footstep_stone_04.ogg`   | head -21 dB, tail -35 dB |
| `sfx/orders/guard_spear_taps.ogg`      | head -35 dB              |
| `sfx/orders/move_kit_shuffle.ogg`      | tail -29 dB              |
| `sfx/orders/move_kit_shuffle_v3.ogg`   | tail -37 dB              |
| `sfx/orders/run_kit_rattle_v2.ogg`     | head -33 dB              |
| `sfx/orders/stop_drum.ogg`             | head -39 dB              |
| `sfx/orders/stop_drum_v2.ogg`          | head -37 dB              |
| `sfx/state/load_complete.ogg`          | head -37 dB              |
| `sfx/state/pause.ogg`                  | head -38 dB              |
| `sfx/state/speed_notch.ogg`            | head -31 dB              |
| `sfx/ui/back_cancel.ogg`               | head -30 dB              |
| `sfx/ui/click_confirm.ogg`             | head -35 dB              |
| `sfx/ui/click_confirm_v3.ogg`          | head -40 dB              |
| `sfx/ui/confirm_seal.ogg`              | head -35 dB              |
| `sfx/ui/confirm_seal_v2.ogg`           | head -32 dB              |
| `sfx/ui/error_thud.ogg`                | head -39 dB              |
| `sfx/ui/hover_brush.ogg`               | tail -35 dB              |
| `sfx/ui/hover_brush_v2.ogg`            | tail -34 dB              |
| `sfx/ui/hover_brush_v3.ogg`            | head -40 dB, tail -37 dB |
| `sfx/ui/notification.ogg`              | head -40 dB              |
| `sfx/ui/select_group.ogg`              | head -32 dB              |
| `sfx/ui/select_unit.ogg`               | head -34 dB              |
| `sfx/ui/select_unit_v3.ogg`            | head -33 dB              |
| `sfx/ui/tab_slide.ogg`                 | tail -38 dB              |
| `sfx/ui/toggle_latch.ogg`              | head -28 dB              |
| `sfx/ui/toggle_latch_v2.ogg`           | head -37 dB              |

**Dead lead-in.** These are fired by a game event, so silence before the sound is latency
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

Each file is decoded to mono at the mixer's 32 kHz and measured for:

- **spectral flatness** — geometric over arithmetic mean of the mean magnitude spectrum.
  1.0 is noise, near 0 is a pure tone. The recorded impacts already in the tree sit at 0.34.
- **partial prominence** — how far the loudest bin stands above the median of its
  neighbourhood. A real transient spreads energy across the spectrum; an oscillator puts it
  in one bin.
- **high-frequency fraction** — share of energy above 6 kHz. A cue with none sounds muffled
  no matter how it is mixed.
- **spectral flux** — how much the spectrum changes frame to frame. Static means synthetic.
- **boundary sample level** and **onset delay**, for the mechanical faults in section 4.

The first four combine into the ordering used in sections 1 and 2. Thresholds were set
against the CC0 recorded material in the tree, which is the quality bar.

A high noise floor on a crowd or battle bed is the recording rather than a fault, so floor
measurements are deliberately not used to condemn anything here.
