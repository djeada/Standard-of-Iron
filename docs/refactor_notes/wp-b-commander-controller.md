# WP-B: commander_control_controller.cpp split

`app/commander/commander_control_controller.cpp` (2,850 lines, `update_impl` 1,145
lines) held input capture, mouse look, locomotion, strike and combo handling,
lock-on, defence feedback, presentation sampling, camera feeding and tracing
behind one class with about eighty members. It is now an orchestrator over
stage owners that each keep only their own state.

## Recorded before the split: stage order and early exits of `update_impl`

Order, with the exits marked:

1. Resolve the commander (`controlled_commander`). **Exit false** when it is
   missing, dead, foreign or lacks a `CommanderComponent`.
2. Fetch `TransformComponent` / `UnitComponent`. **Exit false** when missing.
3. Input capture: `take_input_snapshot`, fold in the carried primary press,
   clear the carry, `exchange_recorded_input` (replay overrides the tick input
   and the view yaw; otherwise the recorder logs the tick input).
4. Ensure a `MovementComponent`.
5. Rally hold: `flag_rally_in_progress && !fpv_controlled`. Advances ability
   cooldowns, zeroes fpv motion, releases input edges and held buttons, zeroes
   speeds and guard, syncs view yaw to the body, publishes a presentation
   sample, updates the camera when one was passed. **Exit true.**
6. `movement->stop()`, rest position.
7. Lock-on yaw spring (`update_lock_on_yaw`), then Q/E turn keys when unlocked,
   wrap view yaw.
8. Dodge FOV kick decay.
9. Jump: resume an authored airborne action, decide, count the edge, cue,
   timer, `CommanderComponent` jump fields.
10. Punish window and posture decay, guard timers, re-arm on release.
11. Move vector (lock orbit replaces it when a lock is valid).
12. Dodge start (edge counted, cue, stamina, invulnerability).
13. Motion: jump safe position, roll, recover or walk through `CommanderMotor`.
14. Dodge pose fields on `CommanderComponent`.
15. Jump fall recovery (teleport back to the last walkable position).
16. Strike lunge (through the motor, after everything else moved the body).
17. Commit speeds, accepted-speed smoothing, axes, running.
18. Body yaw (`transform->rotation.y`), publish presentation sample, clamp
    pitch, update the camera when one was passed.
19. `sync_commander_aim`, fpv motion publish, manual-move engagement, stamina
    `run_requested`, traced stamina sampled here (before guard drain).
20. Guard raise/lower and stamina drain.
21. Ability request consumption, cooldown advance, activation.
22. Combo/hold timers, held restart delay.
23. Intent queue open + expiry, dodge grace decay.
24. Held-continuation and pressed intents (`body_can_take_input` gate), press
    edge settlement (carry when held, drop when not).
25. Dispatch the front intent through `CombatActionService::request_attack`.
    **Exit false** if the commander vanished (`primary_action` returned false).
26. `advance_melee_control`, remember previous view.
27. Hit-count observation and camera kick.
28. Aim candidate (bow raycast or primary target scan).
29. Defence feedback publish.
30. `RpgCommanderTargetComponent` publish, hit-confirm camera kick, recent-hit
    timer decay.
31. Latency probe: simulation response.
32. Trace (only when enabled).
33. **Return true.**

The stage order is preserved exactly. `update_impl` now reads this list.

## New homes

| File                      | Owns                                                                                                                                                                               | State moved out of the controller                                                                                        |
| ------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| `commander_input_port`    | Producer side (`key_down`, `request_dodge`...), the mutex-guarded held state, edge counters, per-tick snapshot, carried primary press, replay exchange, rally/release edge discard | held input, pending press, requested dodge direction, snapshot sequence, carried press, tick snapshot, edge trace, mutex |
| `commander_look`          | View yaw/pitch, previous view, mouse recentre/poll, frame-intent sampling                                                                                                          | view yaw/pitch, previous view, mouse centre/last/warp/recentring, intent sample, frame intent                            |
| `commander_locomotion`    | Jump, dodge start, roll/recover/walk on `CommanderMotor`, fall recovery, speed smoothing, move basis with lock orbit                                                               | speeds, planar velocity, axes, dodge state/timer/direction/kick, jump timer/safety/follow-up                             |
| `commander_body_facing`   | Body yaw follows the view (turn-in-place, travel turn, redirect authority)                                                                                                         | body yaw, valid flag, turning-in-place flag                                                                              |
| `commander_strike`        | Hold timers, held continuation and pressed intents, dispatch through `CombatActionService`, melee control advance, hit observation                                                 | combo timer, held duration, restart delay, observed hit count                                                            |
| `commander_lunge`         | Authored strike lunge and target steering, all through `CommanderMotor`                                                                                                            | carry sequence/requested/delivered                                                                                       |
| `commander_targeting`     | Lock-on cycle, lock yaw spring, primary target selection order, bow aim candidate, `RpgCommanderTargetComponent` publish                                                           | locked/soft/primary ids and slots, lock lost timer, spring yaw and override timer, observed hit-confirm sequence         |
| `commander_primary_scan`  | Stateless scan of eligible soldiers (reach, cone, line of sight, score)                                                                                                            | (none)                                                                                                                   |
| `commander_defence`       | Guard raise/lower and drain, posture/punish/guard upkeep, dodge grace, resolved-defence feedback                                                                                   | guard-was-active, observed contact counters                                                                              |
| `commander_presentation`  | Presentation sample publish, pose interpolation clock, camera input assembly, footstep stride cue                                                                                  | pose, snap flag, clock                                                                                                   |
| `commander_tick_trace`    | Trace record from typed views                                                                                                                                                      | (none; writes the caller's trace)                                                                                        |
| `commander_entity_access` | `controlled_commander`, `body_allows_now`, `cancel_current_attack`, `CommanderHandles`                                                                                             | (none)                                                                                                                   |
| `commander_heading.h`     | Yaw wrap and shortest-delta in degrees                                                                                                                                             | (none)                                                                                                                   |

The controller keeps: probe/feedback pointers, `CommanderMotor`,
`CommanderCameraRig`, `CommanderAbilities`, the trace toggle and value, and the
public API (which is unchanged; `DodgeState` and the ground acceleration
constants moved to `commander_locomotion.h`, still visible through the
controller header).

## Sizes

| File                                      | Before |                                                                               After |
| ----------------------------------------- | -----: | ----------------------------------------------------------------------------------: |
| `commander_control_controller.cpp`        |  2,850 |                                        805 (public forwarders plus stage functions) |
| `CommanderControlController::update_impl` |  1,145 |                                                                                  36 |
| Longest controller stage function         |        |                                                                 42 (`record_trace`) |
| New files                                 |        | 12 `.cpp` between 63 and 582 lines; longest function 59 (`CommanderLunge::advance`) |

## Invariants

- **Sim vs presentation time.** `update_simulation` runs on the sim tick and
  never touches the camera; `update` runs both. `update_camera_presentation`
  advances only `CommanderPresentation::advance_pose` and the camera rig, from
  the last published sample. The sample is published exactly once per sim tick
  (including the rally hold), before the camera reads it.
- **Replay.** `exchange_recorded` runs directly after the snapshot is taken and
  is the only place that overwrites the tick input and view yaw; a recorded
  tick wins over live input. Nothing reads live input after that within the
  tick. The recorder logs the input before turn keys or the lock spring change
  the yaw.
- **Edge consumption.** Each edge is consumed by exactly one stage and counted
  once: jump and dodge by the locomotion stage (`EdgeOutcome`), abilities by the
  ability stage, primary/heavy/special by the strike stage. An unconsumed primary
  press is carried one tick while the button is held, dropped otherwise
  (`settle_primary_press`). `release_all`, the rally hold and the mode exit
  discard every pending edge and count them as dropped/refused.
- **Mode entry/exit and reset.** `release_all_input` clears input, move axes
  and speed, held duration and mouse capture state. `reset` additionally snaps
  the presentation pose, resets look/camera/locomotion/lock/guard/strike state,
  and does not touch the motor, view yaw, lock spring or the observed
  contact counters (same as before).
- **Ordering that is load-bearing.** The lock spring reads the un-consumed
  dodge edge (before the dodge is decided). Traced stamina is sampled before
  the guard drain. Lunge runs after motion so it sees the post-roll dodge
  state. The trace look deltas are `view - previous` after the previous view was
  updated, so they are zero (kept as it was).
- **Only the motor moves the commander.** All translation still goes through
  `CommanderMotor`; the controller-source guard test now scans all stage files.

## Dropped as dead

`m_primary_scan_cooldown` and `m_last_move_direction` were written but never
read; they are gone. `CommanderAbilities::activate`'s `rescan_primary_target`
result therefore has no consumer in the controller.

## Behaviour

Unchanged. Code was moved verbatim into stage functions; the only expression
changes are `desired_direction` capture (walk uses the normalised move stored
in `MotorReport::move`, as the old local was) and the lock position lookup,
which the camera and the orbit shared as two identical copies and now share as
`CommanderTargeting::locked_position`.

## Tests

- `tests/core/commander_stage_owners_test.cpp` covers the state owners that can
  now be built without a world: `CommanderInputPort` (carry and drop of an
  unconsumed press, consume, release-all discard counts, jump/dodge outcomes),
  `CommanderBodyFacing` (snap, threshold, turn rate, travel) and `CommanderLook`
  (wrap, clamp, frame-intent delta).
- `commander_control_regression_test.cpp` greps the controller and stage
  sources through `commander_control_source()`, so moving a guarded line
  between stage files keeps the guard; new stage files must be added to that
  list.
- The entity-object access ratchet counts `get_component<>` per directory
  (`app/commander` budget 120, now 119). Stages take the already fetched
  `CommanderHandles`, the action pointer or the position instead of re-fetching.
