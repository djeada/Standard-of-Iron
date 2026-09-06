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

- [ ] Complete
- Owner: unassigned
- Priority: P0
- Depends on: none
- Start in: `tools/arena/arena_viewport.cpp`, `tools/arena/arena_scenario.cpp`, `tools/arena/main.cpp`, current crash logs and debugger output.

Work:

1. Build and launch current `rpg_locomotion` in interactive and batch modes. Reproduce the reported crash if it still exists, obtain a symbolized stack, identify its cause, and fix it without discarding ongoing work.
2. Verify commander takeover with the Arena's documented keyboard/mouse controls. If takeover fails, diagnose focus and input routing. Make the manual review path usable.
3. Confirm that assets and runtime settings match the actual executable. Record the executable used for all subsequent captures.

Done when: the current build repeatedly completes locomotion without crashing, and a reviewer can verifiably move, look, stop, and attack using the real input path. If the crash has already been fixed, provide fresh reproduction results and identify the relevant change instead of inventing a new repair.

### T02 — Isolate the persistent empty-ground shake

- [ ] Complete
- Owner: unassigned
- Priority: P0
- Depends on: T01
- Start in: `ui/qml/CommanderInputLayer.qml`, `app/viewmodels/commander_view_model.cpp`, `app/core/game_engine.cpp`, `app/commander/commander_control_controller.cpp`, `app/commander/commander_presentation_trace.h`, `render/humanoid/runtime/instance_prepare.cpp`.

Work:

1. Capture the **main-game commander view** on empty, flat ground: idle, slow mouse orbit, straight walk/run, stop, reversal, strafe, and camera-mode entry/exit. Include the HUD. Arena alone does not exercise the same input/frame orchestration.
2. Trace input deltas and cursor warps, simulation timestamps, authoritative and presented body poses, camera anchor/eye/target/up/FOV, animation sample time, and frame duration. Add missing diagnostics through an opt-in path.
3. Compare normal settings with a truly neutral camera: no bob, idle breathing, roll, impulses, or sprint FOV animation. The existing head-bob toggle alone is insufficient. Use this comparison to distinguish authored motion from irregular jitter.
4. Check cursor recentering, input consumption, multiple camera writers, update order, interpolation/extrapolation, and camera/body/animation clock agreement. These are investigation targets, not established causes.

Done when: there is a repeatable reproduction of the user's shake and evidence identifying its producing stage, with a failing regression or deterministic input replay where practical. If the main-game symptom remains unreproduced, keep that limitation explicit and do not mark the diagnosis complete based only on authored bob.

### T03 — Establish a stable, calm camera baseline

- [ ] Complete
- Owner: unassigned
- Priority: P0
- Depends on: T02; integrate with T04 and T05 before accepting the milestone
- Start in: `app/commander/commander_camera_rig.*`, `app/commander/commander_control_controller.cpp`, `game/accessibility/commander_input_settings.*`, `game/accessibility/motion_settings.*`.

Work:

1. Repair the actual jitter source identified in T02. Ensure the camera follows the same presented body pose and time as the rendered commander.
2. Establish a calm default and a complete motion-free option. Review all independent sources of unsolicited translation, roll, and zoom, including idle breathing and mode-entry FOV settling.
3. Keep look response immediate and predictable. Avoid fixing instability by introducing excessive follow lag or filtering intentional mouse input.
4. Give collision corrections an explicit role: safety may move the camera, but free-space movement must not inherit collision-like snapping or oscillation.

Done when: on flat empty ground, idle with the neutral camera has no unexplained eye/target/up/FOV drift; steady input has continuous output; entry, stopping, strafing, and reversals do not cause visible shake or unwanted horizon roll. Verify at 30/60/120 Hz presentation and under controlled hitches, recording actual simulation and presentation rates. Document numeric tolerances and visual evidence.

### T04 — Repair movement redirection and start/stop behavior

- [ ] Complete
- Owner: unassigned
- Priority: P0
- Depends on: T02
- Start in: ordinary movement in `app/commander/commander_control_controller.cpp`, `app/commander/commander_motor.*`, `game/core/movement_facts.*`, shared traversal/body-contact systems.

Work:

1. Replace instantaneous redirection of a smoothed scalar speed with controlled acceleration/deceleration of the planar velocity or an equivalent continuous movement model.
2. Cover forward/back reversal, left/right reversal, 90° turns, diagonal changes, sprint release, short taps, and full release. Retain responsive steering; movement weight must not become input delay.
3. Preserve collision-safe movement, wall sliding, slopes, and shared dynamic-body handling. Publish actual accepted movement for presentation and gait selection.

Done when: the measured +5.375 to -4.570 m/s single-step reversal no longer occurs, direction changes respect documented acceleration limits across tested frame rates, and release/diagonal input introduces neither drift nor unintended speed gain. Verify with the existing start/stop, diagonal, figure-eight, and traversal fixtures plus the new regression.

### T05 — Make directional locomotion match body travel

- [ ] Complete
- Owner: unassigned
- Priority: P0
- Depends on: T04's movement/presentation contract
- Start in: `animation/locomotion_manifest.*`, `render/humanoid/runtime/instance_prepare.cpp`, commander presentation data, `tests/core/commander_presentation_pose_test.cpp`.

Work:

1. Drive gait from actual presented velocity relative to body facing, including lateral and diagonal components. Do not select motion from requested input while the body is blocked.
2. Implement convincing forward, backward, lateral, and diagonal steps; coordinate facing, hips, feet, and supporting-leg weight transfer.
3. Improve starts, stops, directional transitions, and turn-in-place without phase resets or foot popping. Avoid stretching the existing forward gait into sideways translation.

Done when: side travel visibly uses lateral stepping, planted feet do not skate beyond a documented tolerance, and speed/cadence/stride agree. Recheck forward locomotion and shared RTS consumers. Provide front, side, and commander-camera captures plus foot trajectories that demonstrate the original 0.738 m forward stride mismatch is resolved.

### T06 — Reframe the commander for readable third-person control

- [ ] Complete
- Owner: unassigned
- Priority: P1
- Depends on: T03–T05 integrated baseline
- Start in: `CommanderCameraRig::framing_for`, commander camera mode setup, main-game HUD layout.

Work:

1. Tune camera distance, height, shoulder offset, pitch, and FOV together using the actual commander body and HUD.
2. Keep supporting feet, weapon action, and nearby terrain readable during ordinary play. Verify default and close modes and supported aspect ratios.
3. Check looking up/down, changes of elevation, walls, and mode transitions. Separate deliberate player framing from automatic corrections.

Done when: the ordinary view no longer parks the feet at the bottom edge, the HUD does not obscure essential motion/footing, and framing transitions do not introduce new camera motion defects. Include comparable before/after screenshots and motion captures.

### T07 — Improve basic attack weight and whole-body animation

- [ ] Complete
- Owner: unassigned
- Priority: P1
- Depends on: T03–T06 accepted
- Start in: `animation/attack_pose_manifest.*`, `animation/melee_swing_manifest.*`, `animation/commander_spear_manifest.*`, `render/humanoid/runtime/instance_prepare.cpp`, `game/systems/combat_actions/`.

Work:

1. Start with a small basic sword sequence and a spear strike. Make preparation, supporting foot, hip/torso rotation, weapon acceleration, contact, follow-through, and recovery visibly coherent.
2. Address rigid close-up silhouettes and abrupt transitions using the existing rendering/animation architecture. Existing blending and root motion must be inspected before replacement is proposed.
3. Keep the rendered weapon, root travel, contact trace, damage timing, and hurtbox synchronized. Improve misses and recovery as well as successful hits.

Done when: basic attacks visibly carry body weight and recover into a useful stance at normal speed, without pose pops or foot sliding. Contact and one-hit-per-contact regressions pass. Demonstrate both planted attacks and attacks with travel; do not claim success from an `AttackAnimationObserved` assertion alone.

### T08 — Make combos feel connected and predictable

- [ ] Complete
- Owner: unassigned
- Priority: P1
- Depends on: T07
- Start in: `app/commander/commander_control_controller.cpp`, `game/systems/combat_actions/combat_action_definition.*`, `combat_action_service.*`, `melee_intent_solver.*`, existing attack-buffer and one-press fixtures.

Work:

1. Exercise real tap, hold, release, repeated taps, heavy follow-up, guard, dodge, and stamina-refusal inputs. Trace the action selected and the exact buffer/cancel outcome.
2. Improve the connection between the previous recovery and the next preparation: stance, weapon trajectory, supporting feet, facing authority, and body travel must form a readable chain.
3. Preserve intentional commitment and the existing hold-to-chain behavior unless a concrete defect requires a documented adjustment. Do not equate an authored recovery window with input latency.
4. Review existing launchers and aerial/special branches only after the basic chain works. Add no new moves in this task.

Done when: a short basic combo is predictable and visually connected, releasing prevents unwanted future links, valid buffered input executes once, expired/refused input remains accountable, and defensive transitions match their authored windows. Include normal-speed input-annotated captures; the initial review did not establish dropped inputs.

### T09 — Validate the repaired baseline in the surrounding game

- [ ] Complete
- Owner: unassigned
- Priority: P1
- Depends on: T06 and T08
- Start in: Arena RPG scenario catalog/manifest, `game/systems/camera_obstruction.*`, commander lock-on and engagement code, main-game commander mode.

Work:

1. Recheck terrain, walls, narrow spaces, friendly ranks/workers/livestock, and movement through crowds using the shared traversal path.
2. Recheck lock-on, target loss, enemy pressure, guard, dodge, bow aim, and camera-mode transitions for new framing or control conflicts.
3. Verify that fixes to empty-ground control survive these conditions; enemy behavior is a secondary integration concern, not a substitute explanation for the baseline shake.

Done when: collision safety, existing combat rules, and shared RTS behavior remain correct, and added context does not restore camera oscillation or directional skating. Use the current scenario list; the older executable lacked `rpg_camera_wall_pocket`.

### T10 — Close with regression, performance, and visual evidence

- [ ] Complete
- Owner: unassigned
- Priority: P1
- Depends on: T01–T09
- Start in: `tests/core/commander_*`, `tests/tools/arena_commander_metrics_test.cpp`, `tools/arena/rpg_gate_manifest.json`, `scripts/run-rpg-gates.sh`, `docs/RPG_PLAYABILITY.md`.

Work:

1. Run the relevant focused tests and the complete RPG gate against the final build. Keep new fixtures and the manifest consistent. Update expected-red status only when evidence supports the change.
2. Measure presentation pacing on declared GPU hardware/settings with valid timing. Report missing measurements and incomplete runs explicitly; never substitute software-rendered performance or fixed simulation dt for measured frame pacing.
3. Repeat the main-game empty-ground input sequence, then the short basic fight. Preserve normal-speed before/after captures with inputs and executable metadata.
4. Update this checklist and playability documentation to reflect actual completed work and remaining issues.

Done when: regression checks pass, performance evidence is attributable, and final visual/interactive inspection demonstrates stable camera motion, continuous redirection, directional foot contact, readable framing, and coherent basic combat. If the original persistent shake remains, leave the owning task open even if every existing gate is green.

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
