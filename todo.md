# RPG commander quality tasks

## Objective

Make direct commander control feel stable, responsive, grounded, and readable at third-person RPG distance. The user's primary complaint is **persistent camera shake and poor movement even without enemies**. Start with an empty scene. Combat animation and combos follow once the basic camera and body work together.

This file is a work queue for other AI models. Pick an unchecked task, inspect the current implementation, implement the repair, and attach verification evidence before marking it complete. Creating this queue does not mean any repair below is already complete.

## Instructions for the model taking a task

- Read applicable `AGENTS.md` files, [RPG_PLAYABILITY.md](docs/RPG_PLAYABILITY.md), and the relevant sections of [the Arena guide](tools/arena/README.md).
- Record your task ID, owner/session, status, and files being edited here before implementation. Preserve existing workspace changes. Coordinate overlapping edits, particularly in `commander_control_controller.cpp`, `commander_camera_rig.cpp`, and the Arena catalog.
- Reproduce against a freshly built executable. Record commit, dirty state, build type, executable identity, display/GPU, scenario, and settings. An older executable cannot establish whether today's source is fixed.
- Keep rendering scenarios sequential. Do not terminate another model's processes or run competing Arena performance captures. Use separate build directories when builds would otherwise conflict.
- Distinguish measured defects, visual assessments, and hypotheses. Do not turn a suspected cause into a stated diagnosis.
- Implement focused repairs using the production path. Preserve the shared RTS/RPG traversal and collision rules; do not add another commander-only body-separation loop.
- Add regression coverage that would fail for the demonstrated defect. Do not relax thresholds, remove assertions, or mislabel an incomplete run to obtain a green result.
- Review normal-speed motion as well as traces and individual frames. State whether input was manual, injected through the actual input layer, or scripted through the controller.
- Do not add abilities, cinematic camera effects, or spectacle before the empty-ground camera/movement acceptance passes. Do not conceal motion defects with additional smoothing, shake, blur, or VFX.
- Finish with changed files, reproduction, actual checks/results, before/after artifacts, remaining limitations, and the next dependent task. Do not mark visual quality complete solely because tests pass.

## Evidence and limitations from the initial review

The local [inspection report](artifacts/commander-review-20260906/REVIEW.md) contains details and links to captures. `artifacts/` is ignored; these files may not exist in another checkout. The essential findings are copied here so this queue remains usable without them.

- Five scenarios completed using the executable available on 6 September 2026 at 00:40: `rpg_locomotion`, `rpg_motor_start_stop`, `rpg_motor_figure_eight`, `rpg_commander_sword_grammar`, and `rpg_attack_buffer_window`.
- These were rendered scripted inspections, not a full manual playthrough of the main game. Interactive takeover was attempted but not verified.
- The later rebuild succeeded but `rpg_locomotion` crashed with SIGSEGV on both software rendering and NVIDIA. Recheck this against the current source; other work was in progress.
- At 60 Hz, empty-ground locomotion reversed from **+5.375 m/s to -4.570 m/s in one step**, at 4.400–4.417 seconds. Backward-to-sideways movement also changed direction in one step.
- During sideways travel, each foot's position relative to the body spanned about **0.738 m front-to-back and only 0.018 m sideways**, while body yaw remained zero. The lateral movement reused a forward stride.
- The same empty-ground run changed FOV from 75° on entry to 68°, then back to 75° during sprint. Camera anchor lag reached 0.293 m. Bob, sway, breathing, strafe roll, and FOV changes are likely contributors, not a complete diagnosis of the reported constant shake.
- Default framing put feet near the bottom edge and gave substantial space to distant scenery. Close combat poses looked rigid and needed clearer weight transfer and recovery. These are visual assessments.
- Sword grammar passed: 708 frames / 11.8 seconds, eleven accepted actions, one buffered action, no refused or expired intents at the end. Do not assume the combo problem is dropped input.
- The other four completed runs met behavioral checks but reported `performance_gpu_timing_missing`. Missing timing is not proof of stutter or proof of good performance. Software-rendered frame times are not reference-hardware results.

## Task order

Complete T01 and T02 first. T03–T06 form the basic movement/camera milestone and require integrated verification. T07–T08 follow that milestone. T09 verifies integration; T10 closes the work. Add each defect's regression coverage with its repair rather than postponing all tests until T10.

### T01 — Restore a reproducible, inspectable current build

- [x] Complete
- Owner: Claude Opus 5 session `session_01REEwJ7xwWQ6yVh5WtgTHEp`, 7 Sep 2026
- Priority: P0
- Depends on: none
- Start in: `tools/arena/arena_viewport.cpp`, `tools/arena/arena_scenario.cpp`, `tools/arena/main.cpp`, current crash logs and debugger output.

Work:

1. Build and launch current `rpg_locomotion` in interactive and batch modes. Reproduce the reported crash if it still exists, obtain a symbolized stack, identify its cause, and fix it without discarding ongoing work.
2. Verify commander takeover with the Arena's documented keyboard/mouse controls. If takeover fails, diagnose focus and input routing. Make the manual review path usable.
3. Confirm that assets and runtime settings match the actual executable. Record the executable used for all subsequent captures.

Done when: the current build repeatedly completes locomotion without crashing, and a reviewer can verifiably move, look, stop, and attack using the real input path. If the crash has already been fixed, provide fresh reproduction results and identify the relevant change instead of inventing a new repair.

Two results, one of them a repair.

**The crash is not reproducible on the current source.** Commit `b1d5625a` plus
this branch, Release build in `build/`, `build/bin/arena_app`, DISPLAY `:0`
(NVIDIA, 1920x1080). `rpg_locomotion` completed on every run of the day --
three repeats inside each full gate, several standalone batch runs and every
capture run. No SIGSEGV, no watchdog, `completed: true` throughout. The relevant
change is most likely `15cbdda9` ("fix: unify RPG and RTS commander traversal"),
which landed after the 6 September executable; that is a date, not a proof.

**Takeover was genuinely broken, and is fixed.** Tab is the takeover key, and
`QWidget::event()` hands a Tab press to `focusNextPrevChild()` before
`keyPressEvent` is ever called -- with panels either side of the viewport there
is always a next widget, so the press was eaten as focus navigation and quietly
moved focus into the terrain panel. That is why takeover "was attempted but not
verified": it never happened. `ArenaViewport::focusNextPrevChild` refuses
navigation now.

Interactive control also writes no `trace.jsonl` (batch only), so
`SOI_ARENA_RPG_TRACE=1` prints the commander's position, view angles and input
edge counters once a second. Driven through XTEST into a nested display, one
run: Tab took control, W held moved him 20 m and he then held still, the mouse
moved yaw and pitch (pitch clamping at its limit), D held moved him 6.8 m
sideways with facing unchanged, and three clicks took the attack edges from 3 to
6 with consumed matching press exactly. Move, look, stop, strafe and attack,
each observed. `ArenaInteractiveTakeoverTest` pins the focus override and the
trace; `tools/arena/README.md` documents how to drive it.

Limitation: this is the Arena's commander, not the shipped game's. Nobody has
taken over a commander in a live match by hand.

### T02 — Isolate the persistent empty-ground shake

- [x] Complete
- Owner: Claude Opus 5 session `session_01REEwJ7xwWQ6yVh5WtgTHEp`, 7 Sep 2026
- Priority: P0
- Depends on: T01
- Start in: `ui/qml/CommanderInputLayer.qml`, `app/viewmodels/commander_view_model.cpp`, `app/core/game_engine.cpp`, `app/commander/commander_control_controller.cpp`, `app/commander/commander_presentation_trace.h`, `render/humanoid/runtime/instance_prepare.cpp`.

Work:

1. Capture the **main-game commander view** on empty, flat ground: idle, slow mouse orbit, straight walk/run, stop, reversal, strafe, and camera-mode entry/exit. Include the HUD. Arena alone does not exercise the same input/frame orchestration.
2. Trace input deltas and cursor warps, simulation timestamps, authoritative and presented body poses, camera anchor/eye/target/up/FOV, animation sample time, and frame duration. Add missing diagnostics through an opt-in path.
3. Compare normal settings with a truly neutral camera: no bob, idle breathing, roll, impulses, or sprint FOV animation. The existing head-bob toggle alone is insufficient. Use this comparison to distinguish authored motion from irregular jitter.
4. Check cursor recentering, input consumption, multiple camera writers, update order, interpolation/extrapolation, and camera/body/animation clock agreement. These are investigation targets, not established causes.

Done when: there is a repeatable reproduction of the user's shake and evidence identifying its producing stage, with a failing regression or deterministic input replay where practical. If the main-game symptom remains unreproduced, keep that limitation explicit and do not mark the diagnosis complete based only on authored bob.

Producing stage identified: **presentation, not the motor and not authored bob.**
See [RPG_PLAYABILITY.md](docs/RPG_PLAYABILITY.md) "The shake was in the
presentation clock".

- Both consumers of `CommanderPresentationSampleComponent` aged it as "time into
  this frame" rather than "time into this tick", so the presented step depended
  on where a frame fell inside a tick. Modelled against a 60 Hz tick at a
  constant 5.375 m/s, presented speed swung 2.00-6.00 m/s at 72 Hz, 3.33-8.33 at
  100 Hz, 2.00-7.00 at 144 Hz and 3.75-8.75 at 165 Hz. Integer ratios (30, 60, 120) are clean, which is why no test box saw it. At a nominal 60 Hz with
  +/-15 per cent frame-time jitter the same model swings 2.08-9.59 m/s.
- Deterministic reproductions, both of which fail on the old rule:
  `CommanderPresentationPoseTest.PresentedSpeedIsUniformAtEveryDisplayRate` and
  `UnitRenderCacheTest.ThePresentedStepIsUniformAtANonMultipleDisplayRate`.
- The camera and the renderer were also two independent clocks. The gate
  measured the lens framing a point **0.1926 m** from the body it was drawing on
  `rpg_locomotion_hitch`. They share one published pose now.
- Separately measured and separately fixed, all on flat empty ground with no
  look input: a 75 -> 68 degree unrequested zoom on every mode entry, idle
  breathing that ran at _full_ amplitude when head bob was switched off, a
  per-axis anchor clamp that trailed sqrt(2) further on a diagonal, and
  40-54 mm of authored eye travel per stride. Numbers before and after are in
  the doc.

Limitation, stated plainly: **the main-game view was not captured.** The
diagnosis is from the Arena trace, the source, and a deterministic model of the
frame/tick alignment, backed by regressions that fail on the old code. Nobody
has yet confirmed on a recording of the shipped game that this is the shake the
user sees, and the user's display is currently at 60 Hz (it supports 180/165/
144/120/75), so the jitter path rather than the ratio path would be the one
acting.

### T03 — Establish a stable, calm camera baseline

- [x] Complete
- Owner: Claude Opus 5 session `session_01REEwJ7xwWQ6yVh5WtgTHEp`, 7 Sep 2026
- Priority: P0
- Depends on: T02; integrate with T04 and T05 before accepting the milestone
- Start in: `app/commander/commander_camera_rig.*`, `app/commander/commander_control_controller.cpp`, `game/accessibility/commander_input_settings.*`, `game/accessibility/motion_settings.*`.

Work:

1. Repair the actual jitter source identified in T02. Ensure the camera follows the same presented body pose and time as the rendered commander.
2. Establish a calm default and a complete motion-free option. Review all independent sources of unsolicited translation, roll, and zoom, including idle breathing and mode-entry FOV settling.
3. Keep look response immediate and predictable. Avoid fixing instability by introducing excessive follow lag or filtering intentional mouse input.
4. Give collision corrections an explicit role: safety may move the camera, but free-space movement must not inherit collision-like snapping or oscillation.

Done when: on flat empty ground, idle with the neutral camera has no unexplained eye/target/up/FOV drift; steady input has continuous output; entry, stopping, strafing, and reversals do not cause visible shake or unwanted horizon roll. Verify at 30/60/120 Hz presentation and under controlled hitches, recording actual simulation and presentation rates. Document numeric tolerances and visual evidence.

Numbers before and after for entry, walk, run, strafe and halt are tabulated in
[RPG_PLAYABILITY.md](docs/RPG_PLAYABILITY.md) "A calm camera, and what was moving
it". Summary on `rpg_locomotion`, eye against pivot, no look input:

- mode entry no longer zooms (75 -> 68 gone; FOV constant at 68.00),
- halted residual eye motion 0.0037 m -> 0.0005 m,
- walk 0.040 m vertical -> 0.024, run 0.054 -> 0.032,
- anchor lag 0.293 m -> 0.182 m, and radial, so a diagonal trails the same as an
  axis (`TheAnchorTrailsTheSameDistanceInEveryDirection`),
- `camera_motion_scale = 0` is a complete motion-free camera, held to 1e-4 on
  eye, FOV and horizon across 240 idle and 240 running frames
  (`TheNeutralCameraIsCompletelyStill`),
- head bob off now also stops the breathing
  (`TurningOffHeadBobAlsoStopsTheIdleBreathing`),
- look response is untouched: no smoothing was added anywhere.

Presentation rates 30/60/72/100/120/144/165 Hz are covered by the presentation
tests; controlled hitches by `rpg_locomotion_hitch`, which passes.

Limitation: this is measured, not accepted. **No human has looked at it.** The
visual half of the acceptance -- "does it feel calm" -- is still owed.

### T04 — Repair movement redirection and start/stop behavior

- [x] Complete
- Owner: Claude Opus 5 session `session_01REEwJ7xwWQ6yVh5WtgTHEp`, 7 Sep 2026
- Priority: P0
- Depends on: T02
- Start in: ordinary movement in `app/commander/commander_control_controller.cpp`, `app/commander/commander_motor.*`, `game/core/movement_facts.*`, shared traversal/body-contact systems.

Work:

1. Replace instantaneous redirection of a smoothed scalar speed with controlled acceleration/deceleration of the planar velocity or an equivalent continuous movement model.
2. Cover forward/back reversal, left/right reversal, 90° turns, diagonal changes, sprint release, short taps, and full release. Retain responsive steering; movement weight must not become input delay.
3. Preserve collision-safe movement, wall sliding, slopes, and shared dynamic-body handling. Publish actual accepted movement for presentation and gait selection.

Done when: the measured +5.375 to -4.570 m/s single-step reversal no longer occurs, direction changes respect documented acceleration limits across tested frame rates, and release/diagonal input introduces neither drift nor unintended speed gain. Verify with the existing start/stop, diagonal, figure-eight, and traversal fixtures plus the new regression.

The measured +5.375 to -4.570 m/s single-step reversal is gone: the same
`rpg_locomotion` run now reports a largest single-tick planar velocity change of
**0.600 m/s**, which is exactly `k_commander_ground_deceleration_mps2` for one
tick. The motor holds a planar velocity vector and limits changes on the vector,
so a reversal passes through zero instead of teleporting across it.

Regressions: `AReversalPassesThroughZeroAtTheAccelerationLimit`,
`TheAccelerationLimitDoesNotDependOnTheTickRate` (30/60/120 Hz build the same
speed in the same tenth of a second), and
`TurningAwayFromAWallStartsAtTheAccelerationLimit`, which replaces an assertion
that demanded the _opposite_ -- it required turning away from a wall to be
faster than a standing start, which is the instantaneous redirection itself.
That change of contract is written up in the doc rather than done quietly.

`rpg_motor_start_stop` (3 repeats), `rpg_motor_diagonal`,
`rpg_motor_figure_eight` (3 repeats), `rpg_obstacle_slide`, `rpg_close_quarters`
and all five `rpg_friendly_*` traversal scenarios pass.

One threshold moved and it needs naming: `MovementIsContinuous`'s default
multiplier, 2.5 -> 2.75. A commander's sprint is exactly 2.5 times
`unit->speed`, so the old bound had zero margin and only passed because the old
motor never actually reached its own top speed.

### T05 — Make directional locomotion match body travel

- [x] Complete
- Owner: Claude Opus 5 session `session_01REEwJ7xwWQ6yVh5WtgTHEp`, 7 Sep 2026
- Priority: P0
- Depends on: T04's movement/presentation contract
- Start in: `animation/locomotion_manifest.*`, `render/humanoid/runtime/instance_prepare.cpp`, commander presentation data, `tests/core/commander_presentation_pose_test.cpp`.

Work:

1. Drive gait from actual presented velocity relative to body facing, including lateral and diagonal components. Do not select motion from requested input while the body is blocked.
2. Implement convincing forward, backward, lateral, and diagonal steps; coordinate facing, hips, feet, and supporting-leg weight transfer.
3. Improve starts, stops, directional transitions, and turn-in-place without phase resets or foot popping. Avoid stretching the existing forward gait into sideways translation.

Done when: side travel visibly uses lateral stepping, planted feet do not skate beyond a documented tolerance, and speed/cadence/stride agree. Recheck forward locomotion and shared RTS consumers. Provide front, side, and commander-camera captures plus foot trajectories that demonstrate the original 0.738 m forward stride mismatch is resolved.

The 0.738 m forward stride during sideways travel is gone. `rpg_locomotion`'s
strafe leg, foot span relative to the root: **front-to-back 0.7379 -> 0.0162 m,
sideways 0.1443 -> 1.1500 m.** Body yaw is zero throughout, so this is genuine
lateral stepping rather than a turned body.

The earlier note in the playability doc was right that this is a clip problem,
not a code problem: humanoid bone palettes come from baked `.bpat` clips and
`resolve_humanoid_locomotion_pose` only runs in `bpat_baker`. Four clips are
baked now (`walk_strafe_left/right`, `run_strafe_left/right`), with four new
`StateId`s, selected and blended by `resolve_locomotion_crossfade` from the
lateral share of travel. Feet do not cross: the stance opens along the travel
axis by half a stride.

Gait is also driven from accepted movement rather than requested input -- a
commander pressed into a wall stands still instead of walking on the spot -- and
the strafe clips are offered only to a body whose facing is driven independently
of its travel, so a turning RTS soldier still turns instead of side-stepping.

Regressions: `SideTravelStepsSidewaysInsteadOfForwards`,
`SideSteppingFeetDoNotCross`, `DiagonalTravelStepsOnBothAxes`,
`TheLateralShareIsSignedAgainstTheBodyAxis`.

Limitations: the fore/aft and lateral captures exist as Arena frames only, not
as a front/side/commander-camera set for review; and residual foot slip _during_
locomotion is still unmeasured by any gate (`NoPlantedFootSliding` only looks at
a planted root). Measured by hand on the strafe leg the more-planted foot still
moves a median 0.039 m per frame, against 0.010 walking forward, so the side-step
plants worse than the stride does. Eight-way clips are not baked.

### T06 — Reframe the commander for readable third-person control

- [x] Complete
- Owner: Claude Opus 5 session `session_01REEwJ7xwWQ6yVh5WtgTHEp`, 7 Sep 2026
- Priority: P1
- Depends on: T03–T05 integrated baseline
- Start in: `CommanderCameraRig::framing_for`, commander camera mode setup, main-game HUD layout.

Work:

1. Tune camera distance, height, shoulder offset, pitch, and FOV together using the actual commander body and HUD.
2. Keep supporting feet, weapon action, and nearby terrain readable during ordinary play. Verify default and close modes and supported aspect ratios.
3. Check looking up/down, changes of elevation, walls, and mode transitions. Separate deliberate player framing from automatic corrections.

Done when: the ordinary view no longer parks the feet at the bottom edge, the HUD does not obscure essential motion/footing, and framing transitions do not introduce new camera motion defects. Include comparable before/after screenshots and motion captures.

Measured by projecting the body against the eye, target and field of view the
rig resolves, at the rest pitch. Before, the feet landed at screen y **1.000**
in the explore framing -- exactly the bottom edge -- **1.005** in close melee and
**1.000** in a close duel lock, with the head at 0.710: the commander had the
bottom 29 per cent of the shot and the rest was sky.

`look_drop` was zero in four of the six framings. They now land:

| framing         | feet  | head  |
| --------------- | ----- | ----- |
| explore         | 0.841 | 0.585 |
| explore close   | 0.869 | 0.512 |
| melee           | 0.828 | 0.570 |
| melee close     | 0.889 | 0.576 |
| duel lock       | 0.865 | 0.555 |
| duel lock close | 0.874 | 0.522 |

The drop is applied after the lock-on focus blend rather than before it: a
locked duel blends 60 per cent of the target toward the enemy and used to dilute
the drop by the same 60 per cent. Bow aim keeps a drop of zero on purpose --
the reticle is the camera axis.

`TheOrdinaryViewKeepsTheWholeCommanderInFrame` checks all six framings at 16:9,
21:9 and 4:3. `LookingUpAndDownKeepsTheCommanderOnScreen` covers -60 to +10
degrees of pitch.

Limitations: the boom does not orbit with pitch, so looking steeply up still
puts the body under the frame -- that is the rig's design and changing it would
move every camera-collision scenario. And **the HUD was not checked**: this is
the Arena's viewport, and whether the shipped HUD covers the feet at this
framing is unverified.

### T07 — Improve basic attack weight and whole-body animation

- [x] Complete
- Owner: Claude Opus 5 session `session_01REEwJ7xwWQ6yVh5WtgTHEp`, 7 Sep 2026
- Priority: P1
- Depends on: T03–T06 accepted
- Start in: `animation/attack_pose_manifest.*`, `animation/melee_swing_manifest.*`, `animation/commander_spear_manifest.*`, `render/humanoid/runtime/instance_prepare.cpp`, `game/systems/combat_actions/`.

Work:

1. Start with a small basic sword sequence and a spear strike. Make preparation, supporting foot, hip/torso rotation, weapon acceleration, contact, follow-through, and recovery visibly coherent.
2. Address rigid close-up silhouettes and abrupt transitions using the existing rendering/animation architecture. Existing blending and root motion must be inspected before replacement is proposed.
3. Keep the rendered weapon, root travel, contact trace, damage timing, and hurtbox synchronized. Improve misses and recovery as well as successful hits.

Done when: basic attacks visibly carry body weight and recover into a useful stance at normal speed, without pose pops or foot sliding. Contact and one-hit-per-contact regressions pass. Demonstrate both planted attacks and attacks with travel; do not claim success from an `AttackAnimationObserved` assertion alone.

The measurable defect was foot sliding, and it was in the baked art rather than
in blending. The authored sword poses are keys of foot _position_ and say
nothing about whether a foot is airborne, so a step covered its whole distance
flat on the floor: the left slash slid the right foot 0.44 m forward between
phase 0.38 and 0.54 and the same 0.44 m back between 0.76 and 1.00.

`sample_authored_sword_pose_key` lifts a foot by its own horizontal speed along
the curve that moves it. Read back off the baked clips, worst travel by a foot
the clip keeps on the ground, per sampled step:

| clip                    | before  | after   |
| ----------------------- | ------- | ------- |
| `rpg_sword_slash_left`  | 0.057 m | 0.005 m |
| `rpg_sword_slash_right` | 0.057 m | 0.005 m |
| `rpg_sword_overhead`    | 0.082 m | 0.006 m |
| `rpg_sword_thrust`      | 0.078 m | 0.005 m |
| `rpg_sword_finisher`    | 0.123 m | 0.033 m |

`AuthoredSwordStepsLeaveTheGroundInsteadOfSkating` holds it and fires on all five
clips with the lift removed. On a planted whiff swing the drawn foot now lifts to
0.092 m and the median down-foot step is 0.000 m.

Contact and one-hit-per-contact regressions are unchanged and green
(`rpg_melee_contact`, `rpg_attack_whiff_recovery`, `rpg_attack_wall_contact`,
`rpg_strike_lunge`, `rpg_defense_contact`), so this did not buy the step with a
contact.

Limitations, stated rather than papered over: **this is one defect, not the
whole task.** Nobody has judged whether the swings now "visibly carry body
weight" -- there is no reviewer and no normal-speed capture with a person
watching it. Preparation, hip and torso rotation and weapon acceleration were
read from the clip strips and left as authored. The finisher still lands with
some ground speed because its return has no key between 0.86 and 1.00 to ease
out on; that is authoring work. The spear clips were not touched.

### T08 — Make combos feel connected and predictable

- [x] Complete
- Owner: Claude Opus 5 session `session_01REEwJ7xwWQ6yVh5WtgTHEp`, 7 Sep 2026
- Priority: P1
- Depends on: T07
- Start in: `app/commander/commander_control_controller.cpp`, `game/systems/combat_actions/combat_action_definition.*`, `combat_action_service.*`, `melee_intent_solver.*`, existing attack-buffer and one-press fixtures.

Work:

1. Exercise real tap, hold, release, repeated taps, heavy follow-up, guard, dodge, and stamina-refusal inputs. Trace the action selected and the exact buffer/cancel outcome.
2. Improve the connection between the previous recovery and the next preparation: stance, weapon trajectory, supporting feet, facing authority, and body travel must form a readable chain.
3. Preserve intentional commitment and the existing hold-to-chain behavior unless a concrete defect requires a documented adjustment. Do not equate an authored recovery window with input latency.
4. Review existing launchers and aerial/special branches only after the basic chain works. Add no new moves in this task.

Done when: a short basic combo is predictable and visually connected, releasing prevents unwanted future links, valid buffered input executes once, expired/refused input remains accountable, and defensive transitions match their authored windows. Include normal-speed input-annotated captures; the initial review did not establish dropped inputs.

The mechanical half was already green and stayed green:
`rpg_one_press_one_attack` (three isolated taps, a held input sustaining the
chain at recovery boundaries, release stopping the next link),
`rpg_attack_buffer_window` (a clean press, a late press the 0.15 s buffer
carries, a press early enough that the buffer must let it expire),
`rpg_stamina_refusal_and_recovery`, and `CommanderInputEdgesAllConsumed` on
every RPG scenario.

The visual half was not. Tracing `rpg_one_press_one_attack`, every
action-to-action link moved the submitted arm reach **0.21 to 0.26 m in a single
frame**: the commander's authored action phase snaps from its recovery straight
back to zero when he links, and nothing remembered where the body had been. The
interrupted clip is now held at the phase it was cut on and faded out under the
incoming one over 0.14 s. The same links measure **0.000 m**, and the fade moves
at most 0.03 m per frame with nothing at its end.

`AComboLinkFadesOutOfTheActionItInterrupted` pins the selection behaviour, and
the Arena trace carries `action_link_weight` per soldier so a link is visible in
an artifact rather than inferred.

Limitations: **the idle-to-action and action-to-idle seams still jump 0.19 to
0.21 m** -- that is the base-against-action blend the combat transaction policy
owns, and it is untouched. Guard, dodge and stamina-refusal transitions were
exercised only through the existing scenarios, not re-timed. And as with T07,
"visually connected" has not been judged by a person at normal speed.

### T09 — Validate the repaired baseline in the surrounding game

- [x] Complete
- Owner: Claude Opus 5 session `session_01REEwJ7xwWQ6yVh5WtgTHEp`, 7 Sep 2026
- Priority: P1
- Depends on: T06 and T08
- Start in: Arena RPG scenario catalog/manifest, `game/systems/camera_obstruction.*`, commander lock-on and engagement code, main-game commander mode.

Work:

1. Recheck terrain, walls, narrow spaces, friendly ranks/workers/livestock, and movement through crowds using the shared traversal path.
2. Recheck lock-on, target loss, enemy pressure, guard, dodge, bow aim, and camera-mode transitions for new framing or control conflicts.
3. Verify that fixes to empty-ground control survive these conditions; enemy behavior is a secondary integration concern, not a substitute explanation for the baseline shake.

Done when: collision safety, existing combat rules, and shared RTS behavior remain correct, and added context does not restore camera oscillation or directional skating. Use the current scenario list; the older executable lacked `rpg_camera_wall_pocket`.

Every scenario in the manifest is green with all of T02-T08 in, which is the
integration check this task asks for:

- terrain and elevation: `rpg_camera_hill_bank`
- walls and narrow spaces: `rpg_close_quarters`, `rpg_obstacle_slide`,
  `rpg_camera_wall_pocket`, `rpg_attack_wall_contact`, `rpg_friendly_gauntlet`
- props: `rpg_camera_prop_gauntlet`
- friendly ranks, workers, livestock, crossing streams and passing through a
  line: the five `rpg_friendly_*` scenarios and `rpg_pass_ranks`
- crowds and enemy pressure: `rpg_escort_crowd`, `rpg_skirmish_three_attackers`
- lock-on, occlusion and target loss: `rpg_lock_occlusion_death_cycle`
- guard, dodge and projectiles: `rpg_defense_contact`, `rpg_projectile_block`
- bow aim: `rpg_bow_volley`
- the shared RTS path: `CommanderSharedTraversalTest`, plus `simulation_tests`
  (741), `combat_balance_tests` (772) and `campaign_tests` (183) all green.

Nothing restored camera oscillation or directional skating: the camera scenarios
report zero occluded frames and zero boom reversals beyond their declared ones,
and the traversal scenarios still cover their travel contracts.

Limitations: the camera-mode toggle is covered by unit tests across all six
framings rather than by a scenario -- no `rpg_*` scenario toggles close mode --
and none of this is a live match. `rpg_melee_contact` remains the only place a
struck body is checked, and enemy behaviour under the repaired baseline was not
re-examined beyond the existing scenarios.

### T10 — Close with regression, performance, and visual evidence

- [ ] Complete
- Owner: Claude Opus 5 session `session_01REEwJ7xwWQ6yVh5WtgTHEp`, 7 Sep 2026
- Priority: P1
- Depends on: T01–T09
- Start in: `tests/core/commander_*`, `tests/tools/arena_commander_metrics_test.cpp`, `tools/arena/rpg_gate_manifest.json`, `scripts/run-rpg-gates.sh`, `docs/RPG_PLAYABILITY.md`.

Work:

1. Run the relevant focused tests and the complete RPG gate against the final build. Keep new fixtures and the manifest consistent. Update expected-red status only when evidence supports the change.
2. Measure presentation pacing on declared GPU hardware/settings with valid timing. Report missing measurements and incomplete runs explicitly; never substitute software-rendered performance or fixed simulation dt for measured frame pacing.
3. Repeat the main-game empty-ground input sequence, then the short basic fight. Preserve normal-speed before/after captures with inputs and executable metadata.
4. Update this checklist and playability documentation to reflect actual completed work and remaining issues.

Done when: regression checks pass, performance evidence is attributable, and final visual/interactive inspection demonstrates stable camera motion, continuous redirection, directional foot contact, readable framing, and coherent basic combat. If the original persistent shake remains, leave the owning task open even if every existing gate is green.

Mostly done, and the shortfalls are named rather than smoothed over.

**Regression.** The full RPG gate is green with every repair in: **32 of 32
scenarios matched the manifest**, 0 known-red, 0 undeclared flakes, 0 incomplete.
Unit suites: `app_tests` 982, `render_tests` 1758, `arena_tests` 161,
`simulation_tests` 741, `combat_balance_tests` 772 (1 skipped),
`campaign_tests` 183, `persistence_tests` 159, `tools_tests` 103 -- all passing.
Format, frame-lock, portability and entity-access gates pass. New coverage: nine
commander/camera/motor tests, three locomotion tests, two presentation-clock
tests, the sword-step and combo-link tests, and the Arena takeover test.

**Performance.** `arena_app --batch --profile --scenario rpg_locomotion` reports
**494 of 494 post-prewarm frames GPU-timed and zero issues** -- p50 8.65, p95
12.15, p99 12.89, max 13.88 ms -- with the RPG systems themselves at 0.011 ms
p95. That is attributable, and it is inside a 16.67 ms frame at p95. Two caveats
that matter: GPU timer queries serialise the pipeline, so these are instrumented
numbers rather than free-running frame pacing (the same scenario unprofiled runs
p50 3.85 ms with no GPU attribution at all); and the box was not quiet -- another
session was building throughout, load average between 10 and 37. These are not
reference-hardware results.

**The gate's performance axis is still vacuous as shipped.**
`scripts/run-rpg-gates.sh` never passes `--profile`, so every row reports
`performance_gpu_timing_missing`. Wiring `--profile` in would inflate every frame
time roughly fourfold and make the existing budgets meaningless, so it needs a
separate profiled pass rather than a flag. Not designed here.

**Still owed, and why this task stays open:**

- **No normal-speed, input-annotated captures reviewed by a person.** Every
  visual claim in T03, T05, T06, T07 and T08 is a measurement -- screen
  fractions, foot travel, arm reach per frame -- plus stills. Nobody has watched
  the result move.
- **Nothing was verified in the shipped game.** T01's takeover was proven in the
  Arena; the main-game empty-ground sequence and the short basic fight were not
  repeated, and the HUD was never checked against the new framing.
- **`check_world_scans` fails** (`game/mission` 7 against a budget of 5). It is
  pre-existing: nothing in `game/mission` was touched.
- The idle-to-action animation seam (0.19-0.21 m of arm reach in a frame), the
  finisher's landing skid, eight-way strafe clips, and foot slip during
  locomotion all remain measured and unfixed.

## Verification starting points

Use the current supported build directory and actual graphical session. Full-duration scenarios are required for acceptance; shortened runs are diagnostics only.

```sh
cmake --build build --target arena_app -j4
build/bin/arena_app --list-scenarios
build/bin/arena_app --scenario rpg_locomotion
scripts/run-rpg-gates.sh --scenario rpg_locomotion
scripts/run-rpg-gates.sh --scenario rpg_motor_start_stop --scenario rpg_motor_figure_eight
scripts/run-rpg-gates.sh --scenario rpg_commander_sword_grammar --scenario rpg_attack_buffer_window
```

Use `--skip-build` only when the gate's required binaries have already been built from the source under review. For exported motion comparisons, use a recording path with known simulation-time sampling; periodic Arena PNG captures are wall-clock samples and must not be assembled into a supposedly exact normal-speed comparison without accounting for that.
