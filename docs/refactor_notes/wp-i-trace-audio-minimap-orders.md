# WP-I: movement trace analysis, audio backend and mastering, minimap generator, order handlers, tutorial and victory

Part of the #1559 readability refactor. Everything below is a move: behaviour, report text, processing order,
rendered pixels and DSP numerics are unchanged, and each claim of "unchanged" was measured (see Verification).

## movement_trace_analysis (1,238 lines, `analyze_troops` 533 lines, 28 finding kinds)

`analyze_troops` was one loop body holding roughly twenty independent state machines that all shared one
`EntityWalkState`. The state is now owned by the detector that uses it:

| Unit                                                                                                                                              | Owns                                                                                          | Finding kinds                                                                 |
| ------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------- |
| `StallDetector`, `RouteRegressionDetector` (`movement_trace_progress_detectors.cpp`)                                                              | stall/turning/launch timers, minimum remaining arclength                                      | ProgressStall, RouteRegression                                                |
| `ObstructionDetector`, `BodyContactDetector`                                                                                                      | recovering/blocked timers, rejected-step streak, penetration and overlap timers               | ObstructionNotEscalated, BlockedStepStreak, CollisionPenetration, BodyOverlap |
| `RouteChurnDetector`, `StarvationDetector`                                                                                                        | repaths and waypoint regressions per order, active-seconds                                    | RepathChurn, WaypointRegression, Starvation                                   |
| `HeadingDetector`, `DirectionReversalDetector`, `GaitDetector`, `ArrivalDetector`, `LayoutDetector` (`movement_trace_presentation_detectors.cpp`) | previous yaw/angular speed, reversal window, gait timers, arrival state, traversal mode dwell | the remaining kinds                                                           |

`movement_trace_troop_analysis.cpp` is the walker: it groups samples per entity, builds a read-only `SampleFrame`
(dt, accepted speed, active, new order, locomotion) and calls the detectors in the original order. The order is load
bearing because findings are appended in emission order and `format_movement_findings` prints them that way.
`on_new_order()` reproduces exactly the resets the old `new_order` block did; note `regression_seconds` is _not_
reset by a new order (only by a route revision change), which the old code also did.

`movement_trace_soldier_analysis.cpp` holds the per-slot walk (anchor agreement, anchor jump), `movement_trace_format.cpp`
the text reports, `movement_trace_digest.cpp` the digest and the JSONL directory loader. `movement_trace_analysis.cpp`
keeps the public entry points, `FindingSink` and the kind names. The internal seams are in
`movement_trace_analysis_detail.h` and `movement_trace_troop_detectors.h`; nothing outside `game/core` includes them.

`IndefiniteActiveOrder` is a declared kind that no code ever emits; that was true before and is left alone.

## miniaudio_backend (1,204 lines)

The backend keeps what has to stay together for the real-time contract: device lifecycle, the command ring, channel and
sound-effect mixer state and `on_audio`. Only the decode side moved:

- `TrackStore` (`track_store.*`): slot registry, the `DecodedTrack` table the audio thread reads with an acquire load,
  and PCM residency accounting (resident, peak, budget, overruns).
- `TrackDecoder` (`track_decoder.*`): file to mastered 16-bit PCM, and the per-id mastering-analysis cache.
- `DecodeWorker` (`decode_worker.*`): the decode thread, urgent and bulk queues, pending slots, deferred loops.
  It reaches the mixer only through a `std::function` that submits `AudioCommand`s; it holds no backend pointer.

`miniaudio_config.h` holds the `MA_*` feature macros so the implementation translation unit (still
`miniaudio_backend.cpp`, which defines `MINIAUDIO_IMPLEMENTATION` and pulls in `stb_vorbis`) and `track_decoder.cpp`
(header-only use of `ma_decoder`) see the same configuration.

`apply_command` (a 127-line switch) is now a dispatcher over `apply_play`, `apply_stop`, `apply_stop_all`,
`apply_play_sound`, `apply_set_sound_volume` and `apply_release_track`, and `on_audio` (219 lines) is split into
`measure_effect_load`, `mix_music_channels`, `mix_sound_effects` and the two `render_*_run` inner loops. They are
private members of the same class, so the audio thread still reads the same members with the same memory orders; the
per-sample arithmetic is byte-for-byte the old expressions. `BlockGains` is a stack array exactly as before (no
allocation, no locks added on the audio thread).

One deliberate simplification: the old `release_slot` also removed the slot's deferred loop under the decode mutex while
holding the registry mutex (registry then decode lock). `finish_job`, its only caller, removes the same deferred loop
one statement later under the decode mutex, so the extra removal was redundant and the lock nesting is gone.

## audio_mastering (1,183 lines): spectrum analysis split

The FFT, octave smoothing, resonance-ridge clustering and notch selection (about 330 lines) form a self-contained
stage that produces the `spectral_frames`, zone levels and `notches` of `Analysis`. It moved to
`audio_spectrum_analysis.cpp` (added to the `soi_audio_mastering` target, so `audio_master_preview` still links exactly
the code the game runs). `PI`, `SILENCE` and `to_db` moved to `audio_dsp_math.h` because both files need them.
The biquad, loudness meter, dynamic bands, limiter and `apply` chain stay together in `audio_mastering.cpp`: they share
`Biquad`/`BiquadState`, run in one order and were judged one cohesive pipeline. It is 841 lines with a longest
function of 69 (`profile_for`, a preset table).

## minimap_generator (1,112 lines)

`MinimapProjection` (`minimap_projection.h`) is the world to pixel mapping, built once per `generate` from the
existing `world_to_pixel` in `minimap_utils.h` (the generator had a private copy of the same formula).
`minimap_palette.h` holds the ink/parchment palette and `hash_coords`. `minimap_water_layer.cpp` owns lakes, river
stroke construction, pools and the four stroke passes. `minimap_decor.cpp` owns parchment, terrain base, terrain
symbols, compass rose, border and vignette. Roads, bridges, zones, props and structures stay in the generator.
The existing runtime layers (`unit_layer`, `camera_viewport_layer`, `minimap_fog_compositor`) draw dynamic overlays on
top of this baked image; they share no painting code with it, so there was nothing to reuse there.
`draw_road_segment` and `terrain_feature_color` were already unused private helpers; left as found.

## command_dispatcher (991 lines)

The file was a `std::visit` plus thirty-odd `apply_*` handlers in an anonymous namespace. The handlers are now in
three files by who they order, declared in `command_handlers.h` (namespace `Game::Command::handlers`):
`command_unit_orders.cpp` (move, stop, hold, guard, run, patrol, formation, commander ability, squads),
`command_worker_orders.cpp` (construction, harvest, deliver, repair, dismantle, wall plans, placement, auto gather) and
`command_alliance_orders.cpp` (trade, tribute, calls, appeal answers). Helpers used by one file stayed private to it.
`commander_control_regression_test` greps the hold handler's spelling and now reads `command_unit_orders.cpp`.

## tutorial_director (964 lines)

`step_id_name`, `step_title`, `step_body`, `step_objective` and `focus_target_name` are static and stateless; they moved
to `tutorial_step_text.cpp`. `hint_for` and `focus_for` only read the step and the observation and moved to
`tutorial_step_coaching.cpp`; `hint_for` is a 50-line switch over per-step helper functions. They are still
`TutorialDirector::` member definitions (and the helpers call `TutorialDirector::tr`), so the translation context
stays `TutorialDirector`. Checked with a scratch `lupdate` over the old file and the new three: identical 64
context/source pairs.

## victory_service (952 lines)

The rule data model moved to `victory_rules.h`; `build_rule_set_from_config` (map config to rule set, pure) to
`victory_rule_builder.cpp`; `refresh_rule_metadata` became `analyze_rule_traits` in `victory_rule_traits.cpp`, which
returns a `VictoryRuleTraits` value the service stores as `m_traits` (twelve loose members before). The evaluation
and progress code still needs the service's registries, elapsed time and last world summary, so it stays.
`victory_service.cpp` is 715 lines; `summarize_world` (76) is the longest function.

## Reviewed, no extraction: commander_view_model (784 lines)

Almost every method is a one-to-fifteen line forwarder that takes a frame lock, builds a request for
`CommanderModeCoordinator` or the commander controller, and applies the returned effects. The logic already lives in
those owners. The rally placement methods (about 170 lines) mutate the same `m_rally_preview` and cursor mode as the mode
transitions, and eight regression tests pin spellings in this file. The controller it forwards to is being reshaped by
another work package in the same change set, so cutting a second seam here now would collide with it.

## Not done, and why

- `dispatch` is a 70-line `if constexpr` table with one arm per payload type; it is a routing table, not logic.
- `TutorialDirector::step_body` (93 lines) and `focus_for` (85) are text/table switches.
- `summarize_world` and the victory rule checks stay in `VictoryService` because they read its state.

## Verification

- Movement analysis: scratch program (random multi-entity, multi-session traces, both jittered and steady, 12 seeds,
  every finding kind except the never-emitted one) printed summary, findings, timelines, digest, worst entity/soldier
  and per-kind counts through the old and new library: identical output (md5 equal, 450k lines).
- Minimap: 19 shipped maps x 3 camera yaws x 2 structure bakes x 2 sizes = 228 images hashed (MD5 of the ARGB
  bytes) with the old generator source and the new one: identical.
- Mastering: 45 signals (3 sample rates, 3 lengths, 4 signal kinds, all 5 materials) through `analyse`, `apply` and
  `restore`, FNV over every output sample and report field: identical hash, notches exercised.
- Backend: ten generated tracks through `request_track`/`play`/`play_sound`/`unload`, 400 `on_audio` blocks, FNV over
  every output sample plus residency counters: identical between the old and new backend.
- `simulation_tests`: 827 of 827 pass on a build containing all of the above (includes MovementTrace, MovementAnalysis,
  VictoryService, Command* and Dismantle/Food/Production/AutoGather suites).
- `campaign_tests` tutorial suites (21) and `combat_balance_tests` hold/guard/squad/defensive-layout suites (81) pass.
- Audio, mastering, loop seam, resampler, ambience assets, minimap generator and minimap utils tests (80) pass when
  compiled outside `app_tests` against the fresh libraries; `commander_control_regression_test` passes 37 of 38, the
  one failure reads `game_engine.cpp` and belongs to the app work package.
- `bash scripts/check-headless-replay.sh bot_skirmish 20 7`: digest 16297127701076550023, replay verified.
- Not run: `movement_quality_gate_test` (ai_tests) and the full `app_tests`, because other packages' in-progress edits
  stopped `ai_tests` and `app_tests` from linking at the time.
