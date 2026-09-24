# RPG Playability

This document is the permanent contract for direct commander control: what playable means, how the mode is validated, and which runtime rules make the result reliable. `tools/arena/rpg_gate_manifest.json` is the live record of scenario status.

The target is not an RTS camera placed close to a commander. Direct control is expected to provide the control, camera, movement, collision, animation, combat, and frame-pacing quality of a good third-person Souls-like action game while remaining inside Standard of Iron's large RTS world.

The dependency order is strict:

```text
truthful diagnostics
→ authoritative input and presentation
→ stable motor and locomotion
→ collision-safe camera
→ mechanically correct combat
→ readable feedback
→ content, abilities, and spectacle
```

New abilities, enemy types, VFX layers, camera impulses, HUD flourishes, and cinematic behavior do not take priority over movement, camera, or combat correctness. Presentation must not hide a mechanical defect.

## Product contract

"Souls-like" means the following here:

- Camera input is immediate and predictable. The view rotates only from player input, an explicit lock-on rule, or a collision-safety correction.
- Movement has weight without input ambiguity. Starts, stops, reversals, wall slides, slopes, and body contacts remain continuous and repeatable.
- The body, animation, camera anchor, crosshair, hit trace, and hurtbox agree on the commander's presented pose.
- Attacks have explicit startup, active, recovery, buffer, redirect, and cancel windows. Commitment comes from authored rules rather than latency or stuck state.
- Visible weapon contact determines a hit, and one contact applies damage once.
- Guard, perfect guard, dodge invulnerability, posture, and stamina use authored windows with one source of truth.
- Lock-on is stable, overridable, and subordinate to deliberate look input.
- Crowd pressure remains fair: active attackers are budgeted, off-camera pressure is limited, and attacker slots rotate.
- Stable 60 fps frame pacing on declared reference hardware is part of control quality.

## Validation harness

```sh
make rpg-gate
make rpg-gate RPG_GATE_ARGS="--skip-build"
make rpg-gate-baseline

scripts/run-rpg-gates.sh --scenario rpg_locomotion
scripts/run-rpg-gates.sh --enforce-performance
```

The gate builds `arena_app`, `app_tests`, and `arena_tests`, runs the unit-test filters named by the manifest, then runs each manifest scenario sequentially through the real renderer into a fresh artifact directory. The summary reports scenario, owning gate, expectation, repeats passed, completion, verdict, issue codes, frame-time statistics, and artifact path. Any mismatch exits nonzero.

Rendering requires a display; the script defaults `DISPLAY` to `:0`. Full sweeps suspend X11 blanking and DPMS for the duration of the run and restore the original settings on normal exit or interruption. Scenarios are never run concurrently because multiple renderer processes contaminate frame timing and can turn resource starvation into misleading watchdog failures.

### Artifacts

Each scenario writes `report.json`, `run_config.json`, its log, and any captures. `run_config.json` includes commit identity, dirty-worktree state, build type, CPU model, GPU vendor and renderer, GL version, OS, kernel, and viewport size. The run root also contains `gate_run.json` and machine-readable `gate_summary.json`.

Artifacts live under `artifacts/rpg-gates/`. The comparison baseline is `artifacts/rpg-gates/baseline`.

## Scenario manifest

`tools/arena/rpg_gate_manifest.json` is authoritative for the `rpg_*` scenarios included in the gate. `arena_rpg_gate_manifest_test` compares the manifest and scenario registry in both directions, so neither side can silently drift.

| Field          | Meaning                                                          |
| -------------- | ---------------------------------------------------------------- |
| `id`           | Registered scenario id                                           |
| `status`       | `required_green` or `expected_red`                               |
| `gate`         | Playability gate that owns the scenario                          |
| `notes`        | What the scenario proves or why it is intentionally red          |
| `issue_codes`  | Required failure codes for `expected_red`                        |
| `repeats`      | Number of identical runs used for the verdict                    |
| `reproduction` | `deterministic` or `nondeterministic`                            |
| `intermittent` | Whether mixed repeat results are an explicitly declared property |

`expected_red` is a ratchet, not an exemption. If every repeat passes, the gate reports `FIXED` and exits 4; the same change must promote the scenario to `required_green`. Thresholds are changed only for an explicit design reason, never simply to make a row green.

## Verdicts

Completion, behavior, and performance are separate axes.

`completed: false` means watchdog termination, crash, or missing report. It is `INCOMPLETE`, or `TIMEOUT` when `timeout.txt` identifies the wall-clock watchdog, and it always fails the gate.

Behavior covers every non-performance issue code and is hardware-independent enough for normal CI enforcement.

Performance covers `frame_budget_exceeded`, the `performance_*` codes, and frame-time percentiles. It is always reported and is enforced only with `--enforce-performance` on named reference hardware.

| Repeat result | `required_green` | `expected_red`                                       |
| ------------- | ---------------- | ---------------------------------------------------- |
| All pass      | `PASS`           | `FIXED`                                              |
| All fail      | `FAIL`           | `RED(known)`                                         |
| Mixed         | `FLAKY-FAIL`     | `RED(intermittent)` when declared, otherwise `FLAKY` |

A `nondeterministic` reproduction records a real defect that the scenario cannot provoke on demand. It reports `RED(unreproduced)` on a passing run and `RED(known)` on a failing run, but it does not by itself change the gate result. It remains an explicit debt until a deterministic reproduction exists.

### Exit codes

| Code | Meaning                                                         |
| ---: | --------------------------------------------------------------- |
|    0 | Every scenario matched its manifest expectation                 |
|    1 | A `required_green` scenario failed behaviorally                 |
|    2 | Build, unit-test filter, or argument failure                    |
|    3 | Incomplete run, timeout, or missing report                      |
|    4 | An `expected_red` scenario passed every repeat                  |
|    5 | Enforced performance budget missed                              |
|    6 | Identical repeats disagreed without an intermittent declaration |

## Presentation trace

Every rendered frame of an `rpg_*` Arena scenario carries a `commander` object in `trace.jsonl`. Tracing is Arena-only and opt-in through `ArenaViewport::set_presentation_trace_enabled(true)`.

The trace is designed to attribute a failure to one pipeline stage rather than infer it from the final image.

| Group    | Contents                                                                                                                                                                          |
| -------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `input`  | Edge sequences for press, release, consumption, refusal, and drop; sampled frame; move axes; held state and duration; raw look delta; view angles                                 |
| `motor`  | Authoritative and presented pose, desired and accepted velocity, grounded state, blocked/slide state, separation correction, lunge, jump snap-back, and displacement sources      |
| `camera` | Commander and visual anchor, pivot, unconstrained and resolved eye/target, boom state, obstruction, terrain lift, eye clearance, FOV, yaw/pitch, framing, and framing transitions |
| `combat` | Action phase and time, queue state, guard and perfect-guard state, dodge timing, target ids and slots, hit-confirm sequence, health, and stamina                                  |
| `costs`  | Scoped timing accumulators for motor, targeting, weapon trace, engagement, and camera                                                                                             |

The motor trace also carries the shared movement facts that identify ownership in one line:

| Field                   | Meaning                                                                     |
| ----------------------- | --------------------------------------------------------------------------- |
| `movement_mode`         | `direct_control` or `rts`                                                   |
| `steering_source`       | `DirectControl`, `Route`, or `None` from `MovementFacts::desired.source`    |
| `static_walkable`       | Result from the shared `Walkability` layer                                  |
| `dynamic_push`          | Correction applied by `BodyContactSystem`, with neighbor and overlap counts |
| `accepted_displacement` | Actual displacement compared with requested speed and `dt`                  |

`dynamic_push` comes from shared movement facts rather than a controller-local estimate. A nonzero push in direct control with a steering source other than `DirectControl` is therefore an ownership violation. `separation_push` remains the magnitude used by `CommanderMotorCorrectionWithin`.

Animation data stays in the existing `soldiers` samples, including visual state, transition counts, gait diagnostics, reaction data, and planted-foot positions. Frame objects carry `frame_time_ms` and the CPU/GPU phase breakdown.

## Standard expectations

Arena expectations convert traces into explicit pass/fail rules. The standard commander checks include:

| Expectation                            | What it enforces                                                                                  |
| -------------------------------------- | ------------------------------------------------------------------------------------------------- |
| `CommanderInputEdgesAllConsumed`       | Every relevant press is consumed or explicitly dropped, and drops remain within budget            |
| `CommanderBoomIsContinuous`            | Retraction may be immediate; extension must remain smooth and must not pump under one obstruction |
| `CommanderMotorCorrectionWithin`       | Separation correction and jump snap-back remain inside their per-frame budgets                    |
| `NoUncommandedViewRotation`            | The view changes only from look input, framing, or an active lock                                 |
| `CommanderSpeedIsContinuous`           | Planar velocity changes remain inside acceleration/deceleration budgets                           |
| `CommanderContactCountAtMost`          | One running action produces no more contacts than authored                                        |
| `CommanderCameraKeepsCommanderInSight` | Opaque geometry cannot remain between the lens and commander beyond the allowed run               |

Each expectation has synthetic positive and negative coverage in `ArenaCommanderMetricsTest`. Retraction itself is not treated as a discontinuity because collision safety requires the camera to shorten immediately when necessary. Tiny boom reversals below 5 mm are numerical noise rather than pumping.

Per-frame geometric budgets scale by `frame_time * 60`, with a minimum scale of 1, so the same physical-rate limit applies to ordinary frames and authored hitch frames.

## Input ownership

Physical input has one authoritative path from the GUI into the simulation tick. Input handlers only record state and edges; they do not execute combat or guard state directly against the world.

`CommanderInputSnapshot` is the render-to-simulation packet. `CommanderControlController::take_input_snapshot()` runs once per tick under `m_input_mutex`, copies the nine held booleans, and moves the six edge latches out of the producer. Simulation-side code reads `m_tick_input`; `InputState` is written only by input handlers using the same mutex.

`primary_action` is private to the controller path. A press with no immediately actionable body state is carried explicitly in `m_carried_primary_press`, so a held press can survive a dodge or another temporary inability to act without becoming an unaccounted edge.

View angles are intentionally outside the input mutex. `m_view_yaw` and `m_view_pitch` participate in input, camera, lock-on, and simulation paths; locking them would put the camera path behind the input mutex. Edge latches, where loss is possible, remain synchronized.

Focus loss, menu transitions, Arena focus-out, and interactive exit all route through `release_all_input()`. This releases held movement and mouse actions rather than dropping a local key map while leaving simulation state held.

Arena scripted attacks use real press edges. Interactive takeover keeps the viewport as the raw-key owner by making `ArenaViewport::focusNextPrevChild` refuse Tab focus navigation. With `SOI_ARENA_RPG_TRACE=1`, interactive mode logs commander position, view angles, and input-edge counters once per second so manual takeover remains observable.

## Presented pose

The simulation ticks at 60 Hz while presentation can run at a different rate. `CommanderPresentationSampleComponent` carries the authoritative previous and current pose, tick duration, tick sequence, and the resolved presentation sample used by both camera and renderer.

Presentation age is measured from the previous sample's tick. On a new sequence, the resolver subtracts the tick that just ended rather than resetting the clock. A frame on a simulation tick reads the authoritative pose at alpha 1; a frame between ticks may extrapolate into the current tick with alpha in `[1, 2)`, subject to the resolver's cap.

The camera is the presentation authority for the resolved commander pose. It publishes `presented_position`, `presented_yaw`, and `presented_valid` on the sample, and the renderer reads that exact pose. `instance_prepare` builds `unit_base` from the published presentation pose rather than independently aging another clock or reading raw `TransformComponent`.

`Renderer::reset_animation_time()` runs when a batch scenario is loaded under fixed-step execution, so the first clip sample is deterministic across repeated runs.

`ArenaScenarioDefinition::presentation_hitches` supplies `{at_seconds, frame_ms}` entries that replace one fixed-step frame at or after the authored time. This feeds the long frame through simulation, controller, camera, and trace exactly like a real hitch.

`CommanderPresentationPoseTest.PresentedSpeedIsUniformAtEveryDisplayRate` and `UnitRenderCacheTest.ThePresentedStepIsUniformAtANonMultipleDisplayRate` cover non-multiple display rates including 72, 100, 144, and 165 Hz.

## Shared movement

Direct control produces steering intent; it does not own collision policy. `Game::Systems::body_profile_for()`, `Walkability`, and `BodyContactSystem` decide where the commander may move in the same shared layer used by RTS-controlled bodies.

Dynamic contact has three important properties:

- Contact resolves pairwise against one body per entity rather than every formation anchor.
- All pairs share the per-tick correction budget defined by `k_separation_speed` and `k_max_separation_step`.
- A body that is already moving receives its contact correction sideways, so traffic deflects motion rather than simply braking it.

`CommanderSharedTraversalTest` requires dense friendly traversal to retain at least 60% of unobstructed progress and requires an idle commander inside friendly ranks to remain within one metre of the starting point.

## Commander motor

The direct-control motor stores planar velocity as a vector, so a direction reversal cannot preserve scalar speed while flipping direction in one tick.

`k_commander_ground_acceleration_mps2` is 30 m/s² and `k_commander_ground_deceleration_mps2` is 36 m/s². A full reversal passes continuously through zero. A blocked step adopts zero accepted velocity, and a slide adopts the accepted slide velocity, preventing stored perpendicular speed from reappearing when contact ends.

`fpv_motion_requested` follows smoothed accepted speed rather than the raw `motor.blocked` flag, because tiny repeated facade contacts can legitimately alternate the blocked bit while the presented gait should remain stable.

The direct-control movement-state floor is `CommanderComponent::k_direct_control_gait_floor_speed` at 0.60 m/s. Published component velocity, per-tick displacement, and `fpv_motion_requested` all honor that floor during coast-down, allowing locomotion presence to fade while the motor finishes braking.

The direct-control sprint target is exactly 2.5 times `unit->speed` through `k_fpv_walk_speed_scale` and the run multiplier. Movement continuity thresholds therefore include explicit headroom rather than relying on asymptotic approach to top speed.

## Person-scale geometry

RTS navigation footprints and person-scale collision bodies are separate concepts.

`get_building_body()` provides the body extent used by the commander, camera obstruction, bow aim, melee line of sight, and other person-scale queries. `BuildingFootprint` carries `body_center_*`, `body_width`, and `body_depth`, rotated with building facing at registration. RTS navigation footprint, grid padding, and route behavior remain separate.

`BuildingBodyFootprintTest` rebuilds building geometry through a recording submitter and requires the person-scale body to cover the rendered span without unnecessary padding. It also requires the person body and navigation footprint to remain distinct.

The barracks navigation footprint is smaller than its rendered geometry; that is an RTS pathing issue and is not corrected by the RPG person-scale body.

## Locomotion continuity

Persistent gait state is idempotent for a repeated sample time. `resolve_humanoid_locomotion_sample()` does not advance or overwrite persistent state when `sample_time` has not moved beyond `previous.last_sample_time`, so preparing one frame more than once cannot produce an extra decay step.

`SoldierAnimationDebugSample` exposes `locomotion_blend`, `locomotion_presence`, `cycle_phase`, `persistent_valid`, previous presence, sample time, and foot positions. These values make a stop or transition diagnosable from one trace rather than from visual inference.

`k_planted_foot_height` is 0.05 m. A foot above that contact window may move freely; a grounded foot is subject to the sliding expectation.

Direct control switches the presented gait to idle once accepted speed falls through the 0.60 m/s gait floor while `locomotion_presence` eases out. A held movement input still counts as movement during startup.

### Side-step clips

Direct-control strafing uses authored clips rather than rotating procedural stride math at runtime. The baked set is:

- `walk_strafe_left`
- `walk_strafe_right`
- `run_strafe_left`
- `run_strafe_right`

`resolve_locomotion_crossfade` selects them from the lateral share of travel and blends them against the fore/aft clip. `resolve_humanoid_locomotion_pose` lays the stride on the travel axis in body space, widens the stance along that axis, leans the torso into the step, and reduces fore/aft arm swing and torso twist.

`turn_amount` and `travel_lateral` remain separate. Ordered RTS units normally turn toward travel; a direct-control commander may sustain a facing/travel mismatch as an intentional side-step. Strafe clips are therefore offered only when `VisualMovementState::facing_independent_of_travel` is set from `MotionPresentationSource::DirectControl`.

`foot_turn_scale` remains a turn mechanic and is not applied as a sustained strafe modifier.

A full eight-way authored locomotion set and a dedicated metric for foot slip during locomotion remain open content/validation work.

## Camera collision and obstruction

`camera_obstruction` resolves boom clearance, body clearance, and depenetration against buildings and every solid world prop. Prop obstruction data is indexed from `TerrainService::world_props_revision` rather than rebuilt every frame.

Buildings are treated as vertically blocking across the chase-camera range. Props use `world_prop_occluder_height`, including model scaling, so tall ruins, tents, and trunks can retract the camera while low boulders, ore seams, and fallen logs below the eye do not. Tree canopies are intentionally excluded; the solid trunk is the collision object.

The boom shortens before any sideways escape. Its minimum resolved length is 0.55 m rather than a fixed fraction of nominal boom length. Sideways depenetration is reserved for a pivot that is itself buried and may not increase planar distance from the commander. `ABuriedPivotNeverPushesTheLensAwayFromTheBody` requires the resolved eye to remain within 0.75 m of a commander inside a temple footprint.

Terrain clearance is solved along the entire boom rather than only beneath the final eye. The lift required to clear ordinary terrain is capped at 2.5 m; steeper terrain shortens the boom instead of turning the chase camera into a high aerial shot. Terrain intersection uses an interpolated crossing between samples rather than snapping to the last clear sample.

Obstruction retraction is immediate. Extension is eased and capped at 6 m/s. Eye depenetration leaves a 0.14 m margin rather than resting exactly on the surface.

`nearest_building_body_clearance()` and the shared obstruction layer expose signed eye clearance in the camera trace. `rpg_camera_prop_gauntlet`, `rpg_camera_wall_pocket`, and `rpg_camera_hill_bank` cover props, wall pockets, and steep terrain while requiring the commander to remain visible.

## Camera motion and framing

The camera seeds its FOV from the active framing state on first update, avoiding an entry zoom caused solely by initialization.

Idle breathing, bob, sway, strafe roll, sprint FOV, and impact impulse all obey accessibility motion controls. Breathing depends on commander stillness and is disabled with head motion. Bob, sway, and strafe roll use restrained amplitudes, and strafe roll follows the head-bob setting.

Anchor lag is radial rather than per axis, so diagonal travel does not produce a larger trailing distance than axis-aligned travel at the same speed. The normal sprint follow settles below the teleport clamp.

`camera_motion_scale = 0` is a complete motion-free camera: no bob, breathing, roll, sprint FOV change, impulse, or entry settle. `CommanderCameraRig.TheNeutralCameraIsCompletelyStill` holds eye, FOV, and horizon to `1e-4` across idle and running samples.

Framing uses `look_drop` to keep the commander inside the viewport. The ordinary framings place the feet around normalized screen y 0.83–0.89 and the head around 0.51–0.59. The drop is applied after lock-on target blending so duel framing does not dilute it. Bow aim intentionally uses zero drop because the reticle is the camera axis.

`LookingUpAndDownKeepsTheCommanderOnScreen` covers the practical combat pitch band from -60° to +10°.

## Lock-on

Lock-on selection and cycling are screen-space operations. Initial lock chooses the target nearest the view center. Cycling steps to the next target to the right and wraps from the rightmost target to the leftmost.

Facing authority is suppressed inside 0.75 m, tapers in from 0.75 m to 2 m, and is capped at 220°/s. Manual look suspends the lock spring for 0.35 s without dropping the target.

Person-scale building bodies are used for lock acquisition, melee line of sight, and bow aim.

The lock still writes `m_view_yaw`, which is also the player's stored free-look yaw. Separating lock framing yaw from persistent free-look yaw remains open work.

## Combat input and buffering

One physical press requests one melee attack. Holding the primary attack button does not auto-repeat.

`CombatIntentQueueComponent` uses an authored 0.15 s buffer and counts every outcome class: accepted, buffered, refused, expired, and overflow. `expire_stale_intents` records expiration, and `push` records overflow when the queue exceeds its supported pending capacity.

`rpg_attack_buffer_window` exercises a clean press, a press late enough to carry into the next action, and a press early enough to expire before the legal window.

Plain attacks play at their authored timeline speed. `resolve_melee_intent` starts from a `swing_speed` of 1.0 and clamps it to `[0.85, 1.35]`; look/swipe input may accelerate a light attack within that range. Heavy attacks cap at 1.0, so their weight comes from authored duration and stagger rather than an additional slowdown multiplier. The tired-swing penalty remains independent.

The render-side attack visual may outlive `action_running` by `exit_blend_duration` of 0.10 s; that interval is the blend into the base stance rather than part of the mechanical action.

## Strike direction and commitment

`melee_interruption_at` publishes phase-specific `redirect_authority`: full authority in windup, reduced authority in the early strike, and zero authority once the strike is committed. Body yaw follows view yaw only by that authored amount, so the player can aim the startup without rotating an active committed blade arbitrarily through the camera.

A committed action therefore owns its direction while still allowing explicit redirect windows.

## Contact, guard, and damage

`hurt_body_contact_point()` clamps the blade's closest point onto the target hurt body, represented as a capsule from 0.30 m to 1.70 m plus body radius. Damage, reaction, sparks, and guard resolution therefore share a contact point on the body that was actually hit.

Ordinary blocking uses the authored guard arc through `frontal_arc_dot`, vertical coverage, and guarded side. Perfect guard additionally requires the 0.60 m plate test during the authored perfect window.

`game/systems/combat_actions/commander_defense_timeline.h` is the source of truth for guard raise, perfect window, release, guard-break recovery, dodge startup, dodge i-frame interval, roll duration, and recovery. `CommanderDefenseWindowsTest` covers exact boundary behavior at 30, 60, and 120 Hz.

`RpgHealthComponent` counts resolved contacts by outcome: blocked, perfect, dodged, damaging, and guard-broken. Feedback is emitted from changes in those resolved counters rather than from the input request itself.

## Stamina

A light attack costs 12 stamina and regeneration is 10 stamina per second. Any spend pauses regeneration for 0.75 s through `StaminaComponent::spend`, so repeated attacks eventually produce a real refusal and recovery cycle.

Arena commanders carry a stamina component and expose the current value in the combat trace. `rpg_stamina_refusal_and_recovery` drives expenditure to refusal, waits for recovery, and verifies a later attack can be accepted.

## Dodge presentation

Dodge mechanics have authored timing, but the rendered dodge still lacks a dedicated roll pose. The commander is presented with locomotion animation over the dodge translation, so there is no visual tuck against which the i-frame interval can be validated.

The authored roll lasts 0.22 s and the i-frame interval occupies the first 0.12 s. A dedicated roll pose must place the visible tuck inside that first portion of the action.

## Crowd pressure

Attacker selection is deterministic through `deterministic_unit_roll(entity_id, epoch)` and uses an explicit fairness policy.

At most one active presser may come from outside the 100° visible arc. An incumbent presser receives a `+2.4` selection penalty after three seconds in the slot, replacing its normal `-0.55` incumbency preference and forcing the ring to rotate rather than pinning the same attackers indefinitely.

## Combat feedback

Player-controlled hit pause is presentation-local at 0.045 s and never stops the mechanical action timeline. Other units may retain their stronger per-entity presentation pause.

A commander hit never changes the global simulation time scale. Unit updates, projectiles, mission timers, and the rest of the match continue at normal simulation time.

Soldier trace samples expose `is_swing_recoiling` and `hit_reaction_tilt_degrees`. A victim already committed to a swing can show an authored swing recoil rather than having the action cut in half, and `HitReactionObserved` accepts that recoil when it carries real visible tilt.

## Sword presentation

Sword locomotion and idle stances author both hand placement and grip orientation. `HumanoidHeldPoseSample` carries `blade_direction` and `offhand_axis`, and clips whose ground stance orients those grips rebuild the humanoid attachment frames during baking.

`carry_sword_and_shield` keeps the upright blade in front of and clear of the torso. Spear, caster, and stave profiles retain their own socket orientation paths.

Authored sword attack keys lift a moving foot according to horizontal foot speed along the same Hermite path used for translation. This lets a travelling foot leave the floor instead of skating across it. `AuthoredSwordStepsLeaveTheGroundInsteadOfSkating` covers all five RPG sword clips.

Action-to-action combo links preserve the outgoing clip at its cut phase and blend it under the incoming clip for 0.14 s. The link is keyed on the resolved clip and is read from incoming inputs before selection, so the fade begins on the same frame as the transition.

Idle-to-action and action-to-idle arm seams still move roughly 0.19–0.21 m in one frame. Those transitions belong to the base-stance/action blend policy and remain separate work.

## Accessibility

`Game::Accessibility::CommanderInput` contains look sensitivity X/Y, invert Y, camera impulse, head bob, FOV scale, and hold-versus-toggle guard. `UiPreferences` persists and publishes all seven.

The camera rig applies the individual motion switches in addition to `camera_motion_scale`. Run and dodge FOV effects scale with reduced-motion settings. `CommanderAccessibilityTest` verifies behavior changes, not merely stored preference values.

## Performance

The first 0.75 s of every scenario is a prewarm window. Those frames are reported separately as `prewarm_frames` and `prewarm_max_ms` and are excluded from gameplay p50/p95/p99/max budgets.

`scripts/rpg_gate_report.py` evaluates the reference performance contract on every run:

- p95 ≤ 16.67 ms
- p99 ≤ 20.0 ms
- post-prewarm max ≤ 33.3 ms
- GPU timing present
- prewarm window marked

The values are always reported and become gate failures only under `--enforce-performance`.

Five scoped accumulators measure motor, targeting, weapon trace, engagement, and camera once per tick and publish them as `costs` in the commander trace. `rpg_skirmish_three_attackers` measures the full RPG slice at roughly 0.019 ms against a roughly 0.19 ms simulation p95 on the recorded reference run; those numbers are diagnostic and must be interpreted with the hardware metadata in `run_config.json`.

Per-frame metric budgets scale with frame duration. The Arena `--fps` option still changes the batch loop's fixed simulation step rather than only presentation sampling, so strict cross-rate equivalence remains incomplete until render sampling is decoupled from simulation stepping.

## Lifecycle

`CommanderLifecycleSoakTest` covers repeated mode entry/exit, input accounting, long combat, and pause/resume cleanup.

The suite performs one hundred enter/exit cycles and requires every commander flag and input state to restore correctly. It runs a ten-minute headless duel at 60 Hz with every input edge accounted for and no buffer overflow. It also injects pause at forty points inside an action and requires no action to remain running two seconds later.

## UI and integration

Direct-control UI paths follow the same ownership rules as the gameplay systems.

`Main.qml::return_to_main_menu()` restores focus whenever a sub-screen returns to the main menu, preserving keyboard navigation.

`OrdersViewModel::refresh_context_intent` clears RTS cursor intent while commander mode is active, preventing ground/order hints from leaking into the crosshair state.

Direct control has no bottom bar. The pointer is captured, so the commander bar's buttons cannot be reached in combat and its readouts repeat the combat overlay; `HUD.qml` collapses the bottom panel to zero height while `commander_rpg_mode` is set, and it returns only while a rally destination is chosen with a free cursor. `Metrics.commanderBottomBarMinHeight` sizes that placement bar, below the RTS bar height, and is covered by design-token/QML layout tests. The chase framing therefore uses the whole viewport, and the commander's feet are no longer hidden behind a panel.

`RpgFpvOverlay.qml` owns everything direct control shows:

- The ability row carries Special, Rush, Wind, Aura (only when the commander has one) and Rally. Keycaps read `InputBindings.display_shortcut_for`, so a rebound key is never misreported.
- The weapon chip names the current weapon and its swap key.
- The combo pips sit under the reticle.
- A short toast confirms camera-mode and weapon changes.
- The stamina bar flashes "WINDED" when `last_input_outcome` reports `InsufficientStamina`.
- A controls strip, built from the live bindings, fades in on entry and out after a few seconds.

`sync_attack_range_rings` publishes no RTS reach rings while direct control is active. The controlled commander stays selected, and a bow stance's ring otherwise lies across the chase view as a line on the horizon.

When the match reaches a verdict, `GameEngine` leaves commander mode on the GUI thread. Otherwise the captured, recentred pointer could never reach the verdict's buttons.

The autosave and save-progress card moves to the top centre in direct control, clear of the vitals and ability plates.

When the game is paused, the pause binding may resume from any input context even though Space is also a commander dodge binding. The pause overlay names the actual bound key.

Scenario/template prewarm includes every nation in `NationRegistry::player_nation_assignments()`, including forces that have no spawned unit yet.

`PlayerDefeatWatcher` receives `MissionWaveRuntime::owner_has_unspawned_waves` as a `still_expected` predicate, so a wave owner is not declared defeated between scheduled waves.

Notifications pass glyphs to `IronNotification.icon`, and commander faction labels use `Design.FactionTheme.nameFor`.

Mission speech for `hold_the_sallow_ford` and `the_timber_levy` uses the field commander controlled by the player.

The following integration items remain open: moving the pointer to a top-bar control can continue feeding mouse-look, and the empty production panel says "No Barracks" when it means no barracks is selected.

## Working rules

- Every behavioral defect gets a deterministic regression or Arena expectation that reproduces it as a time series whenever the system allows deterministic reproduction.
- Every behavior-changing change includes trace or rendered evidence that demonstrates the resulting contract.
- Thresholds change only for an explicit design reason.
- Camera collision, combat balance, animation, and effects remain narrow work packages rather than being bundled into unrelated changes.
- Deterministic simulation and replay behavior take priority over presentation convenience. Presentation interpolation never feeds authoritative combat or navigation queries.
- ASan/UBSan and the complete normal test suite are part of milestone closure.
- The live scenario manifest, not a prose snapshot in this document, defines current green/red gate status.

## Current known limitations

- Dodge has mechanical timing but no dedicated roll pose, so i-frame timing cannot yet be validated against the rendered tuck.
- Lock-on framing still writes the stored free-look yaw instead of using a separate camera-owned framing yaw.
- Locomotion has four authored lateral clips rather than a complete eight-way set, and slip during locomotion does not yet have a dedicated gate metric.
- Idle-to-action and action-to-idle sword seams still have a visible arm-position discontinuity owned by the base/action blend.
- Arena `--fps` changes simulation timestep, so it cannot yet prove identical behavior under different presentation sampling rates.
- The barracks RTS navigation footprint is smaller than its rendered geometry; the RPG person-scale body does not change RTS pathing.
- Two HUD/integration issues remain: mouse-look over top-bar controls and the empty-production wording.

## Related

- `docs/CAMERA_CONTROLS.md` — RTS camera behavior shared with direct control.
- `docs/COMBAT_SYSTEM.md` — combat systems driven by the RPG slice.
- `PATHFINDING_ARCHITECTURE.md` — shared movement and direct-control body rules.
- `tools/arena/README.md` — Arena harness, scenario schema, traces, and artifacts.
