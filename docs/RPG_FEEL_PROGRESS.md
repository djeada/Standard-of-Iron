# RPG feel implementation evidence

## Slice 1: locomotion ownership and action eligibility (2026-11-12)

Base revision: `78b52197c`, with the earlier camera work still uncommitted.
Evidence root: `artifacts/rpg-feel/locomotion-01/` (local, ignored by Git).
The tracked playback route is `tools/arena/feel/locomotion.action.json`.
This is an initial movement/combat slice, not completion of a plan milestone.

### Reproduced defects and changes

| Defect                                                     | Reproduction                                                                                                        | Owner and correction                                                                                                             |
| ---------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| Lock-on reverses A/D                                       | Compare free and locked movement toward the same target at six headings, with all nine axis pairs                   | `CommanderLocomotion::move_basis`: derive forward toward the target, then derive right with the same handedness as free movement |
| Unaffordable jump still starts                             | Try zero stamina and just below the authored cost, then exact cost and above                                        | `advance_jump`: require the full cost before starting the timer, audio or spending stamina                                       |
| Unaffordable dodge grants protection and cancels an action | Repeat the same cost boundaries; inspect action state, grace time and regeneration delay                            | `try_start_dodge`: require payment eligibility before cancelling attacks or granting dodge grace                                 |
| Blocked dodge publishes full speed                         | Roll into a wide blocked row; compare displacement/dt with reported speed and component velocity on every roll tick | `roll`: preserve the authored roll timeline but publish accepted motor velocity, including zero when blocked                     |

All four new tests failed before the changes; see `tests-before.log`.
The first candidate run passed 264 commander-related tests; see `tests-after.log`.
The RPG gate now includes `CommanderMotorTest.*`, which it previously omitted.
No existing numerical threshold was relaxed.

### Ownership audit for the changed path

Observed controller order, not a proposed replacement engine:

1. `capture_input` consumes the tick snapshot and records held/edge input.
2. `steer_view` updates view/lock steering.
3. `advance_motion` advances jump, defense upkeep, dodge eligibility and motor
   movement, then fall recovery and authored lunge; it commits movement facts.
4. `face_body` owns the controller's body yaw write.
5. `present_tick` publishes the body sample used for presentation.
6. `advance_combat` synchronizes direct-control state, applies guard, activates
   abilities, advances strikes/targeting, and records the trace.

Within locomotion, dodge roll takes precedence over dodge recovery, then ordinary
walk. Jump is decided before dodge in the same tick. The motor resolves permitted
movement; locomotion must not overwrite its result with requested speed. Stamina
eligibility precedes side effects, and accepted actions spend once. The existing
stamina initializer supplies a component when an eligible unit lacks one.

This audit does not establish ownership for every combat/death/reaction writer.
Remaining risks include jump's timed vertical offset and safe-position recovery,
airborne collision, lunge composition, lock-on writing free-look yaw, and differing
RTS/direct-control surface handling. Those remain explicitly open in `todo.md`.

### Verification boundaries

- Tests exercise simulation/locomotion behavior directly, without renderer clocks.
- `before/` and `after/` contain production-game frame captures from the same route.
- `run.json` records revision, platform, CPU and capture settings; patches preserve
  the tested source state. This slice does not certify reference-hardware performance.
- The full Arena sweep, independent simulation/render clock work, complete traversal
  course, sanitizer milestone closure, and a 20-minute acceptance session remain
  outstanding. Do not infer that water, bridges or jumping physics are fixed.

### Gameplay review

The same nine-second production-game fixture was captured before and after the
change; the candidate clip is `after.mp4`. Frame samples cover free strafe, locked
strafe, retreat/dodge, jump, attack and RTS return. A separate real-time X11
keyboard pass exercised Return, D, Tab, D, A, dodge, retreat and RTS return in the
running game; `interactive-input.log` and `live-*.png` record that pass. Damage was
received during the live crowded battle. This short pass is not the planned
20-minute acceptance session and does not validate input latency.

Review found remaining quality gaps: dense soldiers obscure the commander's
silhouette, and lock-on can frame a distant soldier while a nearby opponent is
more visually prominent. Keep crowd readability and target acquisition/framing
open; the movement direction fix is not a claim that lock-on is polished.

## Slice 2: airborne collision and committed jump momentum (2026-11-12)

Base revision: `78b52197c` plus slice 1 and the camera work, all uncommitted.
Evidence root: `artifacts/rpg-feel/jump-01/` (local, ignored by Git).
This slice covers part of milestone 3 (jumping, falling and landing). It does
not replace the timed vertical offset with a physical trajectory.

### Reproduced defects and changes

| Defect                                    | Reproduction                                                                                                                                             | Owner and correction                                                                                                                                                     |
| ----------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| A jump passes through walls and buildings | Run at a one-cell wall and jump: the body crossed to z = 2.5 and beyond; in Arena, at a house facade, the airborne body stood inside the house at 1.67 s | `CommanderMotor::advance`: airborne steps now use the same `resolve_ground_step` collision and sliding as walking. `airborne_step`, which accepted any target, is gone   |
| An ordinary landing teleports             | The same Arena jump reported a 0.493 m `jump_recovery` correction in one tick, a 2.49 m render-root jump and a camera eye 0.99 m inside the house        | The last-walkable-position recovery (`recover_from_fall`, `mark_jump_safe_position`) is removed. Collision prevents the body from reaching a position it cannot stand on |
| Air steering matches ground steering      | Releasing the stick braked a 3.75 m/s jump to 0 within a tick; pressing back reversed it within about 0.1 s                                              | `CommanderLocomotion::steer_airborne`: no braking without input; steering is limited to `k_commander_air_acceleration_mps2` (4 m/s², at most 3.8 m/s over a full jump)   |

Obstacle clearance decision: a jump clears no obstacle. Its peak is 0.72 m;
the lowest jump obstacle recorded on the grid is a boulder at 1.2 m. Generic
`Blocked` cells also include water, mountains, building padding, statues and
ruins. The previous tests `JumpForwardBypassesBlockedGroundCells` and the
source-text test `CommanderJumpAllowsAirborneTraversalAcrossGroundObstacles`
asserted pass-through; they now assert the opposite or were reduced to the
RTS-movement invariant. Jumping over low props would need per-prop height
data on the grid and is unsupported.

Both new motor tests failed before the change (`tests-before.log`). After it,
266 commander-related unit tests pass (`tests-after.log`).

### New Arena coverage

- `CommanderJumpLandsClean` (threshold = required landings): a landing is at least
  0.3 s airborne followed by a grounded frame; no airborne or landing frame may
  report `static_walkable = false`. `ArenaCommanderMetricsTest` covers a clean
  jump, no jump, a four-frame hop, an airborne penetration, and grounded
  penetration (not the jump's).
- `rpg_jump_into_facade` (gate 2, required green, three repeats): a jump straight
  at a house, a diagonal jump that slides along its facade, and a jump away from
  it. With the old motor and recovery restored it fails with five issues
  (`gate-jump-baseline/`); with this change it passes 3/3.

### Verification

- Full RPG gate: 34 of 34 scenarios match the manifest, and all unit-test filters
  pass (`gate-full/`, `gate-full.log`). Frame times are software-measured on an
  i7-12700KF with an RTX 5060 (driver 590.48.01) and are not certified.
  One `rpg_camera_prop_gauntlet` run had a 171.5 ms post-prewarm frame.
- `--capture-interval 0.1` caused the new scenario to time out after capturing all
  80 frames (`gate-jump/`). The uncaptured run completes; capture cost remains an
  open harness issue.
- Full suites: `arena_tests` 234/234; `app_tests` 1282 passed and 3 failed
  (`app-tests-full.log`). The failures are `CampaignPlaythroughTest` Rhône,
  Trebia and Cannae. They are unchanged with HEAD's commander sources, and the
  harness never enters direct control. At `4274fa350`, Rhône wins (10:29), so
  `78b52197c` regressed it. Trebia and Cannae fail there with the numbers from
  CI's 9 Nov Extended Validation run. These failures remain open.
- Production-game film of `tools/arena/feel/locomotion.action.json`:
  `film/` and `after.mp4`. The jump at 5.1–6.1 s still leaves the ground and lands.

### Still open in milestone 3

The vertical motion is still a timed parabola (`jump_height_offset`), not a
simulated trajectory. Collision, damage and support queries use the ground
position. Jump states are timer phases rather than explicit takeoff, apex and
landing states. Landing strength does not follow impact speed. Coyote time,
buffering, walking off a ledge, fall damage, airborne hits, mode switches in the
air, bridges and water are untested. No real-time play session covered jumping
against obstacles.

## Slice 3: combat resources survive control handoffs (2026-11-12)

Continues the shared worktree after the other agent's jump slice; none of its
motor/collision changes were reverted. Evidence root:
`artifacts/rpg-feel/mode-resources-01/`.

Four new `CommanderModeResourcesTest` cases reproduced these problems before
implementation (`tests-before.log`):

- First entry raised a partially spent 100-point stamina pool to a full 180,
  granting resources simply by changing control.
- Re-entry cleared the 0.75-second regeneration delay from combat spending.
- Exit cleared guard-break recovery, removing a combat penalty.
- Guard-break countdown belonged to controller upkeep rather than world time,
  so preserving it alone would leave it frozen in RTS mode.

The mode coordinator now preserves current stamina, recovery delay and guard-break
time. Existing capacity/regeneration tuning remains unchanged; this is not a
balance retune. `CommanderSystem` advances guard-break recovery in either mode and
its scheduler access declaration includes the guard component. Controller upkeep
and rally handling no longer advance that timer. Active/perfect guard still clears
on exit because those protections depend on held input.

The tests cover first entry, 100 consecutive handoffs, health/posture/ability
cooldown preservation, actual stamina-system recovery, and guard refusal until
the sole world-owned recovery timer expires. The new suite is included in the RPG
gate. `mode_resources.action.json` adds a repeatable production-game route with
spending and rapid handoffs; exact penalty timing is asserted by tests, not by
reading the HUD bar.

The rebuilt candidate passes all 270 commander-related tests (`tests-after.log`).
The eight-second production-game route completed before and after the fix;
`before.mp4`/`after.mp4` and binary hashes preserve the comparison. At the same
late-route frame, the fixed HUD shows the retained stamina cost. This is visual
integration evidence, not a numerical timing assertion. `tests-stale-binary.log` is a rerun
of the old executable while that rebuild was underway; it is not a candidate
result. The earlier jump slice's full gate and known campaign failures remain
separate evidence and are not claimed as validation of this change.

### Completed validation (2026-11-13)

- Build: game, app tests, Arena, Arena tests and combat-balance tests completed.
- Commander-related unit tests: **270/270** (`tests-after.log`).
- Arena manifest/metrics/scenario tests: **73/73** (`arena-tests.log`).
- Combat mode tests: **174/174** (`combat-tests.log`).
- All **34/34 RPG scenarios** matched their behavioral expectations, including
  repeated locomotion and the inherited jump-into-facade scenario (`gate-full/`,
  `gate-full.log`). The scenario run used `--skip-build --skip-tests`; unit filters
  were executed separately against the rebuilt binaries as recorded above.
- Performance is **not accepted**. The gate reports several frame-time budget
  misses; compilation overlapped its early portion and the desktop had other
  active applications. Keep those reports, then repeat on controlled reference
  hardware rather than adjusting thresholds or claiming a performance pass.
- Full normal-suite/sanitizer closure and the planned 20-minute unscripted session
  remain outstanding. The previously documented campaign failures were not rerun
  in this slice and are not attributed to these resource changes.

- Real-time production-game run: all **17 actions executed**, fixture coverage
  **passed**, no unmet coverage (`realtime.json`, `realtime.log`). This was a
  scripted real-time check, not the outstanding unscripted acceptance session.
- That run initially exposed missing coverage instrumentation for `select_id`:
  `OrdersViewModel::select_unit_by_id` performed selection without recording the
  event required by the fixture. It now records the event when dispatching to the
  input controller, consistent with the other selection paths. The initial failed
  report is retained as `realtime-missing-coverage.json`; the rerun passed without
  removing or relaxing the coverage requirement. This telemetry-only change was
  rebuilt and checked through the real-time route after the behavioral gate.

## Slice 4 — shared ford movement rules (2026-11-13)

The shared worktree's jump and resource changes were preserved. Inspection found
that RTS route following applies `FordRules::speed_multiplier` and `can_run`, but
commander walking ignored both. The baseline production-game ford capture shows
sprinting through shallow water, and both new water behavior tests fail before
the change (`artifacts/rpg-feel/wading-01/tests-before.log`).

`CommanderLocomotion` now consumes the existing WadingComponent/FordRules contract:
walking uses authored water speed and residual chill, sprint is suppressed while
wading, and held sprint resumes on dry ground. Dodge travel and recovery use the
same multiplier while retaining their existing duration, invulnerability window
and upfront action cost. Rolls no longer publish a sprint request, which previously
could charge running stamina in addition to the dodge cost. FordSystem remains the
owner of water detection and chill; no controller-side water detector was added.

Three regressions cover water/chill speed and sprint at 30/60/120 Hz; wet versus dry
dodge travel, timeline and sprint requests; and actual terrain/navigation crossings
at those rates. The terrain test traverses the ford and dry bridge, checks surface
height each tick, and verifies the adjacent deep-water lane remains blocked.
It does not establish all bridge approach angles or multi-layer support behavior.

Validation so far: **310/310 app tests** (273 commander-related plus 37 terrain
service tests), **10/10 shared FordTest checks**. The first post-change dodge check
sampled ordinary walking after recovery, making its distance ratio invalid; the
final test measures the roll and recovery before walking resumes. Both logs are
retained. The initial game link was interrupted (exit 143); its empty output was
not used for candidate playback. A separate game rebuild and visual/keyboard
verification follow below.

Limitations: authored ford speed is not yet a continuously depth-dependent drag
model; jump support is still the existing timed-offset model. Bridge reverse and
diagonal approaches, underwater/deck ambiguity, slope transitions, deeper-water
feedback, and wading combat/animation polish remain open. This slice does not
close terrain acceptance, performance, the full suite or sanitizer milestones.

## Slice 5 — jump/dodge input buffer (2026-11-13, not built or run)

A jump or dodge pressed just before landing or during roll recovery was refused
with the error cue, so chaining a roll or jumping again needed frame-exact timing.
Presses inside the last 0.20 s of a jump or roll-plus-recovery are now carried by
`CommanderInputPort` (`carry_jump_edge`/`carry_dodge_edge`) and resolved by the
unchanged locomotion rules on the first free tick. `CommanderLocomotion::frees_within`
decides the window; the stage functions in the controller decide to carry. Edge
accounting is preserved: merged and abandoned presses count as refused.

Tests added in `commander_presentation_trace_test.cpp`:
`AJumpPressedJustBeforeLandingIsHeldThenTaken` and
`ADodgePressedDuringRecoveryChainsOneRoll`.

**Unverified:** written without compiling at the user's request. Nothing here has
been built, and no test, Arena scenario or playback has been run against it.

## Slice 6 — handoff and reset hygiene (2026-11-13, not built or run)

A read-through of the controller's owners for state that outlives a reset or a
control handoff. Each item below was found by inspection, not reproduced in play.

- **Event counters adopted, not replayed.** `CommanderDefence`, `CommanderTargeting`
  and `CommanderStrike` each detect events by comparing a counter on the body
  with a remembered value. `reset()` did not resynchronise them, and the
  controller outlives missions, saves and commanders (contact counters restore to
  zero on load). The first tick with a body whose totals differed published a
  guard/dodge/block/guard-break feedback event or a camera impact kick for
  something that had not just happened. The first look after a reset now adopts
  the body's totals silently.
- **Stale dive state.** Entry and exit cleared `jump_active` but left
  `airborne_velocity` and `dive_attack_active`. `tick_jump_timer` reads a set dive
  flag as a dive in progress and cuts the next jump to a short hop.
- **One definition of what a handoff clears.** `commander_mode_coordinator.cpp`
  carried two copies each of the target, action and body-flag clears.
  `settle_body_on_handoff`, `clear_target_state` and `clear_idle_action` replace
  them; the early returns inside the action block are gone.
- **Controller reset completeness.** `reset()` now also clears the lunge carry,
  the cached tick aim ray (a zero-dt entry tick reused the previous session's
  ray), the held-attack restart delay, the lock spring/manual-override state and
  the look history, so a stale previous view is not read as aim movement.
- **Lock clearing has one path.** Five hand-written copies in
  `CommanderTargeting` became `drop_lock()` (lock only) and `release_lock()`
  (lock and soft target), with the same behaviour at each call site.
- Removed two no-op `set_pitch(pitch())` calls; the pitch is clamped at every write.

Test added: `AHandoffLeavesNoAirborneStateToCutTheNextJump` in
`commander_mode_transition_test.cpp`.

Seen and left alone, because moving them needs a build and the test suites:
ability cooldowns, posture and the punish window are still ticked by the
controller in direct control and by `CommanderSystem` otherwise, split on
`fpv_controlled`. Between a rally request clearing that flag and
`flag_rally_in_progress` being set, both tick. Guard-break recovery already moved
to `CommanderSystem` alone; these should follow it. `reset_commander_input` also
resets the camera rig mid-mode on a rally, which is likely a visible camera pop.

**Unverified:** written without compiling at the user's request.

## Slice 7 — Souls-style body rules (2026-11-13, not built or run)

Three rules that decide how the body answers the player. All are design changes,
not bug fixes, and none has been seen running.

**Free movement faces travel; a fight squares up.** The body used to turn to the
camera whenever it moved, so every direction but forward was a strafe or a
backpedal with a speed penalty, and an idle body rotated after the camera past
50 degrees. `CommanderBodyFacing` now follows one precedence list (documented in
its header): committed strike, then the view while squared up, then the heading
of the movement input, then nothing. Squared up means attacking, guarding,
drawing a bow or locked on (`CommanderControlController::squared_up`); only then
do the strafe/backpedal speed scales apply. Moving freely he turns at 720 deg/s
and runs at full speed any way the player points; standing, he keeps his facing
while the camera orbits. A roll turns the body to the roll direction even when
locked on, and a jump keeps the facing it left the ground with. Attack still
snaps the body to the view, as before.

**The roll is front-loaded.** Same 0.36 s, same distance, same i-frames, but the
speed runs from 1.6x the mean down to 0.4x instead of a flat 6.5 m/s into a dead
stop. Distance is integrated per tick, so it does not vary with tick rate.

**A stagger takes the body away.** Hits already put a `StaggerComponent` on the
commander (flinch when punished, guard break, knockdown), but it only blocked
new attacks: a guard-broken commander could still run, roll with i-frames, jump
and re-guard. While it runs, movement input is ignored, guard drops, and roll and
jump are refused; a roll or jump pressed in the last 0.20 s is held for recovery.

Tests: body-facing cases in `commander_stage_owners_test.cpp` rewritten for the
new rules (the turn-in-place case is gone with the behaviour);
`AStaggeredBodyCannotRollOrJumpOutOfTheHit` added to `commander_motor_test.cpp`.

Must be checked in play before it is kept:

- locomotion clips while turning (the body now sweeps through strafe angles on a
  reversal) and the roll tumble now that the body faces the roll;
- attacking while running toward the camera (the snap to the view is a half turn);
- Arena scenarios that measure strafe or backpedal speed without a lock, guard or
  attack held, which now see full speed;
- stagger length against enemy attack cadence, so a guard break is not a stun-lock.

Found and not changed: `CommanderLunge` writes target-assist yaw straight to the
transform, and `face_body` overwrites it from its own stored yaw later in the same
tick. The assist therefore only bends the lunge direction for that tick; the
authored `target_assist.maximum_turn_degrees` never turns the body. Enabling it
changes how often swings connect, so it needs tuning with the game running.

## Integration of slices 4–7 (2026-11-13)

The other agent's input-buffer, reset/handoff and body-rule work arrived while
slice 4 was being played. It was preserved and built as a combined candidate.
Evidence is separate: `artifacts/rpg-feel/integration-07/`. The earlier 310-test
water result and `wading-01/before.mp4` / `after.mp4` cover the pre-integration
water change only. The 13-second clips use the same fixture, preserved as
`wading-01/traversal-filmed.map.json`; an initial capture with an empty victory
condition ended immediately and is explicitly kept under `invalid-empty-victory`.

The first combined unit run passed 313/315. Both failures were old assertions
requiring idle auto-turn after a wide camera sweep. They were replaced with the
new explicit contract: camera orbit never rotates the idle body; beginning an
attack still faces the view immediately. A further test measures the front-loaded
roll at 30/60/120 Hz and with mixed 8/33/50/100 ms steps, dry and wet: total distance
must remain 2.34 m (times the water modifier), with falling speed before recovery.
All 316 tests then passed, alongside 73 Arena manifest/metrics/scenario checks.

The rendered gate found a real integration problem in `rpg_defense_contact`:
the new body-facing rule turned a backward dodge through 180 degrees during the
renderer's world-axis somersault. The projected pelvis-yaw measurement jumped
138.4 degrees at 2.267 s while root yaw moved from 84 to 96 degrees and root-up
crossed through horizontal. That projected angle is not a measurement of a
138-degree physical teleport, but it exposes an undesirable combined twist.
The failure frame and full trace are retained. The initial gate sweep was stopped
before rebuilding; its unfinished locomotion reports are not candidate failures.

`face_body` now preserves takeoff facing during a roll/recovery, while the renderer
continues to roll about the requested travel axis. Free ground movement still
faces travel, and the shaped roll speed, distance and defensive timing remain.
`BackwardRollKeepsTakeoffFacingWhileTheCameraTurns` protects this boundary.
No visual-stability thresholds were loosened. This supersedes slice 7's unverified
proposal to turn the body during the roll.

Real keyboard session before that facing correction: selected the commander,
entered RPG mode, sprinted into the ford, rolled in water, crossed to the north
bank, reversed freely, returned through the ford, approached and crossed the
bridge, reversed across the deck, rolled sideways on it and returned to RTS.
Screenshots and timed XTest inputs are saved as `live-*.png` and
`interactive-input.jsonl`. Feet stayed on the bridge and the body faced free reverse
travel. The map's default wildlife spawned wolves during this longer session;
their damage is visible in the evidence. The final focused map explicitly disables
wildlife, and `traversal-played.map.json` preserves the map actually played.
A final-map forward crossing film is `ford.mp4`. This is not the outstanding
20-minute uninterrupted acceptance session, and no performance pass is claimed.

After the roll-facing correction, **317/317 commander/terrain tests** and
**174/174 combat-mode tests** pass. The corrected rendered defense sequence passes
without captures. At 2.250, 2.267 and 2.283 seconds its root yaw stays 0 degrees and
pelvis yaw stays 89.97 degrees, removing the previous compound twist. Captures in
`roll-review/` show the corrected pose but that capture-enabled run timed out after
60 images; it is **incomplete**, not a pass. This matches the earlier capture
watchdog limitation documented in the jump slice. The uncaptured full rerun uses
`gate-final/`; `gate-full/` is the interrupted first candidate and must not be used
as final acceptance. Builds and exact source/binary hashes are retained in the
integration directory; all results still concern the dirty shared worktree.

### Final combined validation

- **317/317 commander/terrain app tests**, **73/73 Arena checks**, and **174/174
  combat-mode tests** passed on the corrected candidate.
- **34/34 RPG scenarios passed**, including the manifest's repeated locomotion and
  jump cases, with **zero unexpected failures, flakes or incomplete runs**.
  Command: `scripts/run-rpg-gates.sh --skip-build --skip-tests --artifact-dir
artifacts/rpg-feel/integration-07/gate-final`. Unit filters were run separately.
  The final reports contain no issue codes, including frame-budget issues. This is
  still not the named-reference-hardware performance acceptance session.
- Added the body-facing, input-port, look and shared-traversal suites to the gate's
  permanent app-test filter. They were already included in the broad 317-test run;
  the manifest's five checks passed after this filter-only change.
- Source hashes were unchanged across the corrected gate run. `run.json` and
  `candidate.patch` identify the final dirty worktree and executable hashes.
  `before-roll-facing.run.json` identifies the earlier integrated build used for
  the keyboard session and `ford.mp4`; those captures do not claim to show the
  subsequent roll-facing correction. The corrected roll is in `roll-review/` and
  is numerically verified by the completed `gate-final/` defense sequence.
- Full normal-suite/sanitizer closure, independent simulation/display clocks,
  capture-watchdog repair, sustained combat tuning, physical jump/support rules,
  slope/step animation and user feel review remain outstanding. The roll still
  uses the existing procedural whole-body tumble, not a finished authored roll clip.
- Final corrected production-game real-time route: all **7/7 actions executed**,
  selection coverage passed, no unmet coverage (`realtime.json`, `realtime.log`).
  This is 15 measured seconds of scripted ford traversal on the wildlife-disabled
  map, not an unscripted acceptance session or a numerical assertion of water speed.

## Slice 8 — animation overlay polish (code only, not built or played)

Requested constraint: no recompilation. These changes come from source inspection;
none of the earlier green test/gate results validates this new rendering code.
The other agent's wildlife work and all prior commander changes were preserved.

- Moved upper-body overlay continuity out of `instance_prepare.cpp` into
  `humanoid_transition_continuity.{h,cpp}`, with explicit per-soldier memory.
  Replaced the Euler-integrated, rate-clamped spring with exponential weight
  response. The response is 80 ms for ambient/water/carry overlays and 35 ms for
  active combat poses. These are candidate tuning values, not measured acceptance.
- Looping clip phases now use circular distance, using the existing baked clip
  loop metadata. Crossing phase 0.99 -> 0.01 no longer looks like a discontinuity
  that fades the arms out. Real phase jumps and clip/archetype/variant changes
  still retire the outgoing pose before introducing the incoming one, consuming
  leftover frame time so a low frame rate does not insert an extra empty frame.
- Full-body guard, attack, reaction, jump, roll and death can clear an obsolete
  outgoing overlay when the new pose has no overlay. A departing water/carry pose
  must not linger over a shield raise or flinch. Seeks and long cull gaps reset
  presentation memory; shadow/read-only passes do not advance it.
- Wading no longer overrides guard/release, formed holds, or scripted ambient
  poses such as the jump. Its upper-body cycle follows the accepted leg gait phase
  instead of a separate wall-clock oscillator, keeping starts/stops coordinated.

Eight regression tests were added (not run): loop wrap, frame-rate-independent
blend weight, immediate reaction takeover, non-mutating preview/repeated frame,
seek/cull reset, same clip with a different archetype, gait-synchronised wading,
and guard/jump priority both stationary and moving. Only source review,
changed-line formatting and `git diff --check` were performed.

Next verification when compilation is allowed:

```sh
cmake --build build --target render_tests standard_of_iron arena_app -j4
build/bin/render_tests --gtest_filter='HumanoidOverlayContinuity.*:HumanoidTransitionContinuity.*:HumanoidPrepare.*'
scripts/run-rpg-gates.sh --skip-build --scenario rpg_defense_contact --scenario rpg_locomotion --scenario rpg_attack_buffer_window
```

Play water entry/exit, walking guard raise/release, hits interrupting wading,
repeated walk/run/attack transitions, jump/roll interruption, camera cull/re-entry,
and replay seeking. Inspect the blend pace at 30/60/120 display Hz separately from
simulation timestep checks. New roll/jump clips, landing compression and foot IK
remain open; this slice does not claim that the whole animation milestone is done.

## Slice 8 verification and correction (2026-11-14)

Slice 8 was compiled and run for the first time. Its eight new overlay tests
passed, but the full render suite exposed a real regression the earlier gates
never saw: `HumanoidMotionQuality.MeleeHitsRecoveryAndRetargetingStayVisuallyContinuous`
reported **53 violations** (hand speeds up to 85 m/s and 15,000 deg/s at 120 Hz,
rising with frame rate, i.e. one-frame snaps). The pull request's CI failed on
the same test. The pre-slice-8 overlay code passes it with zero violations.

Causes, found by tracing overlay weights frame by frame:

- The one-frame `clear_immediately` when a reaction takes over dropped an
  attack overlay from 0.88 weight to zero: the arms jumped to the reaction pose.
- The 35 ms combat response moved 60% of an arm swap in one 30 Hz frame at
  attack start, after a reaction and on an authored phase wrap.
- Faster fades alone are not enough. A swing blended at mid weight against a
  different full-body pose spins the hand (3,700 deg/s sustained over 8 frames
  at every frame rate). Linear 0.12 s, 0.16 s and 0.25 s ramps still left 31,
  15 and 10 violations; only the original 2.1/s rate-limited spring passes.

Correction (`a8935424e`): overlay weights move at most 2.1/s through the damped
spring again. Kept from slice 8: circular phase distance for looping clips (the
wading cycle no longer drops the arms at its wrap), seek and cull resets,
preview passes that do not advance state, gait-synchronised wading arms and the
guard/jump priority over wading. The takeover is kept only for ambient overlays:
a water or carry pose leaves within 0.12 s when a full-body reaction takes over,
still spread over several frames at 30 Hz; a combat overlay keeps the ordinary
rate. The overlay unit tests now assert those rules, including that a combat
pose is not cut short and that an archetype swap happens at near-zero weight.

Validation: full `render_tests` **2042/2042**; motion-quality test 0 violations;
`rpg_defense_contact`, `rpg_locomotion` (x3) and `rpg_attack_buffer_window` pass
(`artifacts/rpg-feel/overlay-09/gate`). Wading in real-time play is still unreviewed.

## Slice 9: one owner for commander combat timers (2026-11-14)

Ability cooldowns, posture recovery and the punish window were run down by the
direct-control controller while the player drove the commander and by
`CommanderSystem` otherwise, split on `fpv_controlled`. Found by inspection:
nothing ran an _AI_ commander's punish window down, so a vanguard rush or shield
bash left it punishable (22-35% bonus damage, extra stagger) for the rest of the
battle; during a rally request both owners ticked. `CommanderSystem` now advances
all of them for every commander in either mode (a raised guard still slows
posture recovery, 8/s instead of 18/s). `CommanderAbilities` keeps no cooldown
copies; it reads and writes the component. `CommanderDefence::advance_upkeep`
only handles input-owned guard state. Switching control no longer clears the
punish window: like guard-break recovery, an opening the enemy earned survives
a handoff. Tests: `CombatTimersHaveOneSimulationOwnerInBothModes`,
`AnUncontrolledCommandersOpeningCloses`, the extended 100-handoff debt test and
the rewritten `CooldownsRunDownOnceInEitherControlMode`.

## Slice 10: a perfect guard opens the attacker (2026-11-14)

`punish_window_remaining` meant "this body is open" in the damage rules, but a
perfect guard set it on the _defender_. The parrier's next ordinary hit flinched
them, their counter on the turned attacker earned no bonus, and the HUD showed
PUNISH. A perfect guard now grants `riposte_window_remaining` (1 s) against
`riposte_target_id`: the next hit on that attacker is a punishing one (bonus
damage and a heavy stagger) and spends the riposte; hits on anyone else are
ordinary. The HUD's PUNISH callout and crosshair read the riposte; being open
after a guard break keeps the GUARD BROKEN callout. Tests:
`APerfectGuardDoesNotExposeTheDefenderToTheNextHit`,
`ARiposteMakesOneHitOnTheTurnedAttackerPunishing`, `ARiposteExpiresWithItsTarget`
and updated perfect-guard expectations.

## Slice 11: strike target assist has one yaw writer (2026-11-14)

Decision for the open slice 7 question: the authored target assist turns the
body. Before, `CommanderLunge` turned the transform toward the target and
`face_body` overwrote it in the same tick, while `drive_lunge` pulled the body
straight at any target, with no cone check. Measured with the new tests on the
old code: against a target 20 degrees off the view the body faced 0 degrees
while travelling at 20 degrees (a sideways slide), and a target 73 degrees off
dragged the body 0.51 m sideways.

Now the lunge never writes yaw. During the wind-up (before the first weapon-trace
frame) it bends the swing's aim from the view toward its target, only if the
target is inside the action's authored cone, by at most its
`maximum_turn_degrees`, at a rate that completes the bend by the commit frame;
the bend is then frozen. `CommanderBodyFacing` aims the body at view plus that
offset for every strike rule (snap, square-up, authority-limited redirect). The
lunge travels where the body faces and is pulled onto the target only when the
target lies inside the swing's cone. Tests:
`ASwingBendsTowardAnOffAxisTargetAndFacesItsTravel` and
`ATargetOutsideTheSwingConeNeitherTurnsNorDragsIt`; both fail on the old code.

Because this changes how often swings connect, the whole RPG gate was rerun on
the candidate: **34/34 scenarios** matched their expectations, including melee
contact, whiff recovery, three attackers, duel standoff, strike lunge and all
weapon grammars (`artifacts/rpg-feel/assist-11/gate`). Frame times are reported,
not enforced: one 34.9 ms frame in `rpg_friendly_livestock` overlapped a
formatter run on the same machine. Hit rates against live opponents still need
play review.

## Slice 12: a rally does not cut the camera (2026-11-14)

Starting placement, confirming a rally, rallying in place and regaining control
after a rally all called the controller's full `reset()`, which also reset the
camera rig (framing, shoulder, FOV, smoothing). The view stays on the commander
throughout, so each step re-framed the camera. The rally paths now call
`release_for_rally()`: input and action state are dropped, the camera rig, look
and presented pose carry on. Entering and leaving direct control still reset
everything. Test: `ARallyHandoffDoesNotCutTheCamera`.

## Pull request CI (2026-11-14)

- The motion-quality failure is the slice 8 regression above.
- Entity-object access ratchet: the shared-ford, stagger and guard-recovery
  changes had added five `get_component` calls. The controller now resolves the
  wading component once per tick through the registry and passes it in
  `MotionTickInput`; stagger and guard use `World::try_get`.
- Translation coverage (failed on main since #1627): the catalogues still held
  the removed "Go to Barracks"; two preview strings in `Main.qml` were not
  wrapped. All eight catalogues are complete again.

## Arena captures on the scenario clock (2026-11-14)

`--capture-interval` grabbed and PNG-encoded a frame on a wall-clock `QTimer`
between fixed-step ticks. Each capture stalled the simulation, so a 3.3 s
scenario captured 60 frames and then hit the wall-clock watchdog; the frames
also fell at different simulated moments on every run, so baseline and
candidate captures could not be compared frame by frame. The batch runner now
captures right after the rendered frame that reaches each multiple of the
interval on the scenario clock (queued, never inside `paintGL`), and extends the
watchdog by the time spent capturing. `rpg_defense_contact --capture-interval
0.05` now completes and passes with 65 frames; uncaptured runs are unchanged
(Arena tests 234/234). The frames confirm the corrected roll keeps its takeoff
facing through the tumble (`artifacts/rpg-feel/capture-13/`).

The production-game combat route `tools/arena/feel/combat.action.json` (duel
mission) was filmed as well. It never reached an enemy: the troops streaming
past the commander are his own, so the refused lock-on and "no target in reach"
are correct. At `--film-fps 30` the fixed QML animation step plays HUD
animations at half speed, so toasts last twice as long in the film; use
`--film-fps 60` when judging HUD timing.
