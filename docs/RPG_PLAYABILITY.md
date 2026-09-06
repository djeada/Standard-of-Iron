# RPG Playability

This is the permanent contract for direct commander control: what "playable"
means here, how the gate is run, and how its verdicts are decided. The live
checklist of gates and work packages it used to sit beside has been retired;
the scenario manifest below is the running record.

The target is not "an RTS camera that can get close to a commander". It is the
control, camera, movement, collision, animation, and combat reliability expected
from a good third-person Souls-like action game, while still operating inside
Standard of Iron's large RTS world.

The repair order is mandatory:

```text
truthful diagnostics
  -> one authoritative input/presentation path
  -> stable character motor and locomotion
  -> collision-safe camera
  -> mechanically correct combat
  -> combat feel and restrained feedback
  -> content, abilities, and spectacle
```

No new RPG ability, enemy type, VFX layer, camera impulse, HUD flourish, or
cinematic behaviour is added before the camera/movement gate is green. Existing
polish that obscures a defect is disabled while the underlying system is
repaired.

## Product contract

"Souls-like" means the following here:

- Camera input is immediate and predictable. The game never rotates the view
  without player input, a declared lock-on rule, or a collision safety move.
- Movement has weight but never input ambiguity. Starts, stops, direction
  changes, wall slides, slopes, and body contacts are continuous and repeatable.
- The body, animation, camera anchor, crosshair, hit trace, and hurtbox agree on
  where the commander is on the presented frame.
- Attacks have explicit startup, active, recovery, buffer, and cancel windows.
  Commitment is intentional, not latency or a stuck state.
- A visible weapon contact determines a hit. One contact deals damage once.
- Guard, perfect guard, dodge invulnerability, posture, and stamina have
  authored windows with one source of truth.
- Lock-on is stable, can be overridden, and never fights mouse input.
- Crowds remain fair: active attackers are controlled, off-camera attacks obey a
  readability rule, and the player cannot be indefinitely stagger-locked.
- Stable 60 fps/frame pacing on the declared reference hardware is part of
  control quality, not a later optimization.

## Running the gate

```sh
make rpg-gate                                   # build, tests, every scenario
make rpg-gate RPG_GATE_ARGS="--skip-build"      # reuse the current build
make rpg-gate-baseline                          # write the comparison baseline

scripts/run-rpg-gates.sh --scenario rpg_locomotion
scripts/run-rpg-gates.sh --enforce-performance  # reference hardware only
```

The gate builds `arena_app`, `app_tests`, and `arena_tests`, runs the unit-test
filters named in the manifest, then runs every manifest scenario **one at a
time** through the real renderer into a fresh artifact directory. It prints one
table -- scenario, owning gate, expectation, repeats passed, completion,
verdict, issue codes, p50/p95/max frame time, artifact path -- and exits nonzero
on any mismatch.

Rendering needs a display. The script defaults `DISPLAY` to `:0`.

Scenarios are never run concurrently. Two `arena_app` processes rendering at
once contaminate each other's frame times and can starve one into its watchdog,
which then writes a report with `completed: false` and an empty issue list --
indistinguishable, to a careless reader, from a clean behavioural pass.

### Artifacts

Each run writes, per scenario, `report.json`, `run_config.json` (with the commit,
dirty-worktree flag, and CMake build type merged in by the gate script), the
scenario log, and any captures. The run root also holds `gate_run.json` (commit,
build type, host, platform, fixed fps) and `gate_summary.json` (the machine-
readable form of the printed table).

Artifacts land under `artifacts/rpg-gates/`, which is gitignored. The comparison
baseline is `artifacts/rpg-gates/baseline`.

## The manifest

`tools/arena/rpg_gate_manifest.json` is the authority for which `rpg_*`
scenarios the gate runs. `arena_rpg_gate_manifest_test` compares it against the
scenario registry in both directions, so a new `rpg_*` scenario cannot be added
without appearing in the gate, and a gate entry cannot name a scenario that does
not exist.

Each entry carries:

| field          | meaning                                                       |
| -------------- | ------------------------------------------------------------- |
| `id`           | the registered scenario id                                    |
| `status`       | `required_green` or `expected_red`                            |
| `gate`         | which playability gate owns it                                |
| `notes`        | what it proves, or why it is red                              |
| `issue_codes`  | required on `expected_red`: the codes it currently fails with |
| `repeats`      | how many identical runs decide the verdict (default 1)        |
| `reproduction` | `deterministic` (default) or `nondeterministic`               |
| `intermittent` | `true` if mixed repeat results are a declared property        |

`expected_red` is a ratchet, not a permanent excuse. When a repair makes a
pinned scenario pass, the gate exits 4 and names it; the commit that fixed it
flips the entry to `required_green` in the same change. A threshold is never
relaxed to turn a report green.

## How a verdict is decided

Three axes are kept apart on purpose.

**Completion.** `completed: false` means the run was cut short -- watchdog,
crash, or a missing report. That is never a behavioural result. It is reported
as `INCOMPLETE`, or `TIMEOUT` when a `timeout.txt` names the wall-clock
watchdog, and it always fails the gate.

**Behaviour.** Every issue whose code is not a performance code. This is the
part CI can enforce on any hardware.

**Performance.** `frame_budget_exceeded` and the `performance_*` codes, plus
p50/p95/max frame time. These are hardware-sensitive, so they are always
reported and only enforced under `--enforce-performance` on named reference
hardware.

Repeats exist because a verdict that changes between identical runs is not a
verdict:

| repeats result | `required_green`      | `expected_red`                                         |
| -------------- | --------------------- | ------------------------------------------------------ |
| all pass       | `PASS`                | `FIXED` (exit 4, manifest is stale)                    |
| all fail       | `FAIL` (exit 1)       | `RED(known)`                                           |
| mixed          | `FLAKY-FAIL` (exit 1) | `RED(intermittent)` if declared, else `FLAKY` (exit 6) |

`reproduction: nondeterministic` is an orthogonal admission: the defect is real
but the gate cannot provoke it on demand, because something the scenario does not
control varies between identical runs. Such an entry never flips the gate on its
own; it reports `RED(unreproduced)` when it passes and `RED(known)` when it
fails, and it is listed every run as owing a deterministic reproduction. **Gate 0
is not green while any entry still owes one.**

### Exit codes

| code | meaning                                                               |
| ---- | --------------------------------------------------------------------- |
| 0    | every scenario matched its manifest expectation                       |
| 1    | a `required_green` scenario failed behaviourally                      |
| 2    | the build, a unit-test filter, or an argument was rejected            |
| 3    | a run was incomplete, timed out, or wrote no report                   |
| 4    | an `expected_red` scenario passed every repeat; the manifest is stale |
| 5    | `--enforce-performance` was given and a frame budget was missed       |
| 6    | identical repeats disagreed and the manifest does not declare it      |

## The presentation trace

Every rendered frame of an `rpg_*` Arena scenario carries a `commander` object in
`trace.jsonl`. It is opt-in and Arena-only: `ArenaViewport` calls
`set_presentation_trace_enabled(true)` when it binds a scenario commander, and
nothing in the shipped application turns it on.

The record exists so a failure can be attributed to a stage instead of guessed
at. Its four groups are:

`input` -- monotonic edge sequences for press, release, consumption, drop, and
refusal of the primary attack, guard, dodge, and jump, plus the sampled frame
index, move axes, held state, held duration, raw look delta, and view angles.
Because consumption and drop are counted separately, an edge that reaches no
consumer is visible as a number rather than as a missing swing.

`motor` -- authoritative previous and current pose, desired versus actual
velocity, requested and smoothed speed, the difference between them, grounded
state, whether the step was blocked or slid, the separation push, strike-lunge
distance, jump snap-back distance, and which of the eight displacement sources
moved the body this tick.

`motor` also carries the four fields that make an RPG-versus-RTS movement
disagreement diagnosable from one line, in the order the pipeline produces them
(issue #1417):

| field                   | what it answers                                                                                     |
| ----------------------- | --------------------------------------------------------------------------------------------------- |
| `movement_mode`         | `direct_control` or `rts` -- which control mode the body was under this tick                        |
| `steering_source`       | `DirectControl`, `Route` or `None`, read back from `MovementFacts::desired.source`                  |
| `static_walkable`       | what the shared `Walkability` layer says about where the body ended up                              |
| `dynamic_push`          | the correction `BodyContactSystem` actually applied, with `dynamic_neighbors` and `dynamic_overlap` |
| `accepted_displacement` | how far the body really moved, against `requested_speed` and `dt`                                   |

`dynamic_push` is read back from the shared facts rather than measured in the
controller, which is the point: if it is ever non-zero while `movement_mode` is
`direct_control` and `steering_source` is not `DirectControl`, something outside
the shared pipeline is moving the commander. `separation_push` is the magnitude
of the same vector and keeps its old meaning for `CommanderMotorCorrectionWithin`.

## Movement is not mode-specific

Direct control produces steering intent. It does not decide where the commander
may go -- `Game::Systems::body_profile_for()`, `Walkability` and
`BodyContactSystem` do, identically for an RTS-ordered commander. The full model
and the reasoning behind it live in
[PATHFINDING_ARCHITECTURE.md](PATHFINDING_ARCHITECTURE.md#the-direct-control-commander-is-a-body-like-any-other);
what matters here is the failure it replaced.

The commander controller used to run its own push-apart against every live
soldier anchor within three metres. It summed the penetration of each anchor and
then clamped the total to 2.4 m/s. In anything denser than a single rank the sum
always exceeded the clamp, so the push saturated on every tick, pointing straight
back out of the crowd -- against a walk speed of 2.7 m/s, a backpedal of 1.9 and
a strafe of 2.3. Measured on a three-deep block of squads the commander advanced
at 0.3-0.4 m/s instead of 2.7, roughly a sevenfold slowdown, and with soldiers on
more sides than that the resultant simply cancelled the step. Nothing reported a
blocker: `motor.blocked` stayed false the whole time, because as far as the motor
was concerned the ground was walkable and the commander was walking on it.

Three properties of the shared layer are what fix it, and none of them are new
behaviour for anything else:

- contact resolves **pairwise against one body per entity**, not against every
  formation anchor, so a squad is one correction rather than eight;
- the per-tick budget is **shared across all pairs** (`k_separation_speed`,
  `k_max_separation_step`), so twenty neighbours cost what one costs;
- a body under way takes its correction **sideways only**, so traffic deflects
  it instead of braking it.

`CommanderSharedTraversalTest` pins the outcome: crossing a dense friendly
formation must cover at least 60% of the ground an unobstructed walk covers in
the same window (it now covers all of it), and a commander standing still inside
his own ranks must not drift more than a metre.

`camera` -- commander position, visual anchor and its lag, pivot, the
unconstrained eye and target, the resolved eye and target, boom length before
and after collision, the raw building blocked fraction, the smoothed occlusion
fraction, the terrain lift applied to the eye, FOV, yaw/pitch and their
velocities, ground height, framing state, and whether framing changed.

`combat` -- action phase, normalized action time, whether an action is running,
queued intent count, guard and perfect-guard state, dodge state/timer/grace,
locked and soft target ids and slots, hit-confirm sequence, hit count, health,
and stamina.

Animation and frame pacing are not duplicated here: the existing `soldiers`
array already carries per-frame animation, visual state, transition counts, and
planted-foot positions, and the frame object already carries `frame_time_ms`
plus the CPU/GPU phase breakdown.

What the trace found on its first run: in `rpg_close_quarters`, 65 of 540 frames
report `motor.blocked` and **zero** report `motor.slid`. The commander does not
graze the wall and slide -- it stops dead and stays stopped. That is finding 4
of the audit expressed as two counters.

## Metric expectations

Six Arena expectation kinds read the commander trace and turn it into pass/fail.
Three of them are attached to every `rpg_mode` scenario automatically, in
`definitions()`, so a new RPG scenario inherits them without remembering to:

| kind                                   | issue codes                                                                                                                            | what it means                                                                                         |
| -------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------- |
| `CommanderInputEdgesAllConsumed`       | `commander_attack_edge_unaccounted`, `commander_attack_edge_dropped`, `commander_dodge_edge_unaccounted`, `commander_input_not_traced` | every press lands in exactly one of consumed or dropped, and drops stay within budget                 |
| `CommanderBoomIsContinuous`            | `commander_boom_discontinuity`, `commander_boom_pumping`                                                                               | the boom may retract instantly but must extend smoothly, and must not alternate under one obstruction |
| `CommanderMotorCorrectionWithin`       | `commander_motor_correction`                                                                                                           | per-tick separation push and jump snap-back stay within budget                                        |
| `NoUncommandedViewRotation`            | `commander_view_rotated_uncommanded`                                                                                                   | the view does not turn without look input, a framing change, or an active lock                        |
| `CommanderSpeedIsContinuous`           | `commander_speed_discontinuity`                                                                                                        | planar speed change per tick, expressed as acceleration                                               |
| `CommanderContactCountAtMost`          | `commander_contact_multiplicity`                                                                                                       | one running action lands no more contacts than it authors                                             |
| `CommanderCameraKeepsCommanderInSight` | `commander_camera_lost_sight`, `commander_camera_not_traced`                                                                           | opaque geometry never stands between the lens and the commander for longer than the allowed run       |

Each one is unit-tested against a synthetic trace in `ArenaCommanderMetricsTest`:
a shaped trace that should fire it, and one that should not. A metric that has
never been seen to fire is not evidence.

Retraction is deliberately not gated. Gate 4.2 requires the camera to "retract
immediately enough to prevent penetration", so a rule that failed on a fast
retraction would forbid the correct behaviour. The first draft of
`CommanderBoomIsContinuous` did gate it, and fired on `rpg_close_quarters`'
scripted 180-degree yaw snap -- a metric being wrong about a camera that was
right.

Two metrics from the plan are not implemented yet and are not silently skipped:
camera penetration depth and clearance need a distance to a surface, and the
only query available today is a zero-radius ray fraction with no contact point.
They arrive with the camera volume query in Gate 4.2. Screen-space anchor jitter
and presentation-pose disagreement need the shared presented pose from Gate 1.2.

## Baseline audit, 2026-08-23

Sequential runs at 60 Hz, Release build, commit `34749a46`. Unit tests: 97
commander camera/controller/latency/mode/regression tests pass. Scenarios: 6 of
11 pass.

| scenario              | gate | state               | evidence                                                                                                                                                          |
| --------------------- | ---- | ------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `rpg_close_quarters`  | 2    | red                 | stopped 2.54 m from the house against a 1.90 m contract; rendered body changed visual state five times in one second (`rpg_approach_blocked`, `pose_oscillation`) |
| `rpg_obstacle_slide`  | 2    | red                 | stopped 2.57 m from the house instead of reaching 1.90 m (`rpg_approach_blocked`, `pose_oscillation`)                                                             |
| `rpg_locomotion`      | 3    | red, load-sensitive | planted foot slid 0.082 m between frames while reported as not walking (`planted_foot_slide`); 1 failure in 5 runs on a loaded box, 0 in 20 on an idle one        |
| `rpg_combo_cadence`   | 5    | red                 | a held attack left a 1.25 s gap against a 1.10 s contract (`rpg_swing_cadence_too_slow`)                                                                          |
| `rpg_defense_contact` | 5    | red                 | no block contact published and protected health lost (`rpg_block_contact_not_observed`, `rpg_health_changed`)                                                     |

The audit's most important result is that the isolated tests are green while the
integrated behaviour is not. More isolated state tests alone will not make the
mode playable; every repair needs a time-series or rendered scenario gate.

A passing `rpg_strike_lunge` run reported a 101.45 ms maximum rendered frame.
The scenario passed because its performance decision is not a strict hitch gate.
Performance numbers are hardware-sensitive, but accepting a visible 100 ms hitch
is not compatible with this mode's target, which is why frame timings are
reported on every row of the table rather than hidden behind a pass.

## Render pose determinism

The renderer's animation clock was never reset when a batch scenario started, so
it carried whatever wall clock had accumulated during window setup and scenario
loading -- measured at 74, 86 and 116 ms across three runs. The simulation, the
commander motor and the camera rig were bit-identical across those runs; only the
rendered pose moved, because every clip was sampled at a different phase.

`Renderer::reset_animation_time()` is now called from `ArenaViewport::load_scenario`
in batch mode (fixed step only, so interactive use is untouched). After it, the
first sampled frame is at animation time 0.016667 in every run and the rendered
pose is bit-identical: `submitted_max_arm_reach` and `submitted_body_up_y` differ
in 0 of 540 frames, against 173 to 529 before.

Two verdicts changed, in opposite directions, and both changes are the point:

- `rpg_locomotion` had an intermittent `planted_foot_slide` -- roughly one run in
  five. It was the phase jitter, not the motor. It now passes 15 of 15 and is
  `required_green`. It proves reproducibility, not that the Gate 3 locomotion
  contract holds.
- `rpg_melee_contact` had been passing on a coin flip. A struck enemy stays in
  `AttackSword` through the whole contact window and goes straight to `Dying`:
  0 of ~1900 enemy soldier samples reach `HitReaction`, against 17 before. The
  commander's own reaction still shows for 72 samples, so the reaction path
  works -- the victim's in-progress attack outranks it. It is pinned
  `expected_red` for Gate 6.

Authored presentation hitches of 33/50/100 ms were built while hunting this and
do not reproduce it; `rpg_locomotion_hitch` stays green. Ruling long frames out
is what pointed at the clock.

## Authored presentation hitches

`ArenaScenarioDefinition::presentation_hitches` is a list of
`{at_seconds, frame_ms}`. In batch mode the viewport substitutes that frame time
for the fixed step, once per entry, the first sampled frame at or after
`at_seconds`. A hitch therefore reaches the simulation, the controller, and the
camera rig exactly as a real long frame would, and appears in the trace as a
large `motor.dt`.

It exists so a hitch-only defect can be reproduced on every run rather than
waited for on a busy machine. `rpg_locomotion_hitch` is the working example;
Gate 4's `rpg_camera_hitch` uses the same field.

## Input ownership, Gate 1.1

One physical press used to enter the world twice.
`CommanderViewModel::primary_action_down()` recorded the edge on the controller
and then immediately called `primary_action()` against the world, while the
controller kept `m_primary_press_pending` set so the next tick pushed and
consumed the same press through the intent queue. The guard release had the
same shape: `secondary_action_up()` wrote `guard->active = false` straight into
the world, outside any tick.

Both direct calls are gone, and `primary_action` is private so the GUI cannot
reach it again. `release_guard` had no callers left and is deleted; the tick
already owns `guard->active` and `m_guard_was_active` unconditionally, and the
perfect-guard window it used to zero cannot fire while `active` is false.

Removing a second producer changes behaviour, so it needs saying plainly:

- **The queued path was sending an empty swing.** `update_impl` pushed
  `intent.swing = body->steered_intent` and `primary_action` passed
  `has_swing = pending != nullptr`, which is always true. On the first press of
  a life there is no `CommanderBodyControlComponent` yet, so the packet claimed
  a default `MeleeIntent` -- a left slash -- and
  `CombatActionService::request_attack` skipped its family-aware
  `resolve_melee_intent` fallback entirely. A spear commander slashed instead of
  thrusting. That never showed in play because the GUI's direct call ran first
  and won the race; deleting it exposed it. `CombatActionIntent` now carries
  `has_swing`, set only when a steered intent actually exists.
- **Two controller tests were pinning the bypass.** They called
  `primary_action` directly, so they saw a world where the tick's targeting had
  never run. Through the real tick a strike does acquire a soft target, and a
  drawn bow deliberately does not acquire a melee target at all -- the `shooting`
  branch zeroes it. Both now assert the shipped contract and say why.
- **Edges vanished unaccounted in two places.** The flag-rally branch cleared
  four pending presses with no consumed/refused/dropped accounting, and
  `reset()` cleared `m_input` but left `m_primary_press_pending` set, so a press
  survived mode exit and fired on re-entry. `discard_pending_input_edges()` owns
  both.
- **Focus loss never released held movement.** `CommanderInputLayer.qml`'s
  `release_actions()` released the two mouse buttons and dropped its `held_keys`
  map without a matching `key_up`, so the commander walked on. Nothing watched
  the window's `active` or `menu_visible` either. Everything now goes through
  one `release_all_input()`, which the arena's focus-out and interactive-exit
  paths also use instead of assigning a fresh `InputState{}`.
- **The arena's scripted attack pressed the world directly.** `rpg_primary_attack`
  is now a real press edge held for three ticks -- long enough to survive a busy
  tick, short enough to stay under the 0.08 s auto-repeat cooldown. The gate's
  own counters confirm it: every scripted press in the twelve scenarios reports
  as consumed exactly once with zero dropped, where before those presses were
  invisible to the edge counters entirely.

### The packet

`CommanderFrameIntent` was supposed to be the render-to-simulation packet. It
had **no production consumer at all** -- nothing outside the controller and its
tests ever read a field of it -- while the simulation tick read the same
`CommanderControlController::InputState` the GUI thread was writing. That is a
real race, not a tidiness complaint: `GameEngine::update` runs on the
SoISimulation thread and QML input events arrive on the GUI thread, so
`m_input.dodge_requested = true` from a keypress could land on either side of
the tick's `= false`, and the dodge was simply gone.

`CommanderInputSnapshot` is the packet now.
`CommanderControlController::take_input_snapshot()` runs once per tick, under
`m_input_mutex`: it copies the nine held booleans and **moves** the six edge
latches out of the producer. Exactly one tick can therefore see a given press.
Every simulation-side read goes through `m_tick_input`; `InputState` is written
only by the input handlers, each of which now takes the same mutex.

Two details are worth keeping in mind when changing this code:

- **A press the body cannot act on is carried deliberately.** It used to survive
  by accident, because `m_primary_press_pending` was shared state the tick
  simply did not clear. Draining it into a snapshot broke that, silently and
  without accounting. `m_carried_primary_press` is the explicit replacement, and
  `AHeldPressSurvivesTicksTheBodyCannotAct` fails without it -- a press held
  through a dodge roll is otherwise lost.
- **View angles are deliberately not under the mutex.** `m_view_yaw` and
  `m_view_pitch` are read and written in about forty places across the input,
  camera and simulation paths, including the lock-on spring and the Q/E turn
  keys, so locking them would mean locking the camera path. A racing read of an
  aligned float yields either the old or the new angle; both are valid camera
  angles, and neither loses an input. The latches are where loss was possible,
  and they are the ones that are guarded.

## The shared presented pose, Gate 1.2

The simulation ticks at 60 Hz; the display does not. Before this change the
camera read `TransformComponent` live on every presented frame and the renderer
built its model matrix from the same stepwise value, so at 144 Hz the commander
held still for two or three frames and then jumped, and the camera's own anchor
filter -- smoothing toward a target that only moves on ticks -- produced a
_different framing distance_ depending on the display rate. Measured over one
scripted second of running, against the 60 Hz result:

| presentation | presented-pose drift | anchor lag behind the body |
| ------------ | -------------------- | -------------------------- |
| 30 Hz        | 0.027 m              | 0.177 m                    |
| 60 Hz        | 0 m                  | 0.205 m                    |
| 120 Hz       | 0 m                  | 0.205 m                    |
| 144 Hz       | 0.053 m              | 0.195 m                    |

At a 3.4 m boom and a 75 degree field of view, 2.7 cm is roughly 11 px at
1920x1080 -- well past the 1.5 px the plan asks for.

`CommanderPresentationSampleComponent` now carries the previous and current
authoritative pose, the tick duration and a tick sequence number, and
`resolve_presentation_pose()` in `game/core/component.h` turns that plus an age
into the pose to present. The controller uses it for the camera anchor and
`UnitRenderCache::update_model_matrix` uses it for the body, so the two cannot
drift apart: each resets its own age when the sequence changes and adds the same
presented delta.

Three things are worth knowing before touching it:

- **It is commander-owned, not `MotionPresentationComponent`.** That component
  already holds a previous/current pair for every unit, which looks like exactly
  the right source until you follow the frame order:
  `begin_motion_presentation_frame` latches `previous_*` at the top of
  `World::update`, and both `GameEngine` and the arena run the commander tick
  _before_ `World::update`. For the commander that pair brackets nothing --
  previous equals current.
- **The presented pose lags the authoritative one by up to one tick.** That is
  the price of interpolating rather than extrapolating, and it is paid by the
  body and the camera together, which is the point. Extrapolation is capped at
  half a tick and only reachable when a presented frame is shorter than a tick;
  rendering _slower_ than the simulation clamps to the newest sample rather than
  overshooting. Nothing reads the presented pose back -- simulation, collision
  and contact still use `TransformComponent`.
- **Locked-step runs are unaffected.** The arena renders one frame per
  simulation step, so the age always reaches exactly one tick and the resolver
  returns the authoritative sample unchanged. Camera anchor against rendered
  root measures 0.000000 m across all 540 frames of `rpg_locomotion`, before and
  after.

## Person-scale geometry, Gate 2

`rpg_close_quarters` and `rpg_obstacle_slide` were both pinned red as motor
defects. Neither was one. `BuildingCollisionRegistry` keeps one size per
building type, and it is the **navigation** footprint -- the box that keeps whole
RTS formations clear of a structure. Measured against the meshes the game
actually draws (`building_preview --bounds`):

| type        | navigation footprint | drawn (roman) | ratio |
| ----------- | -------------------- | ------------- | ----- |
| home        | 4.3 x 4.4            | 2.36 x 2.42   | 1.8x  |
| marketplace | 5.5 x 5.5            | 2.80 x 2.80   | 2.0x  |
| temple      | 6.2 x 4.4            | 3.14 x 2.20   | 2.0x  |
| barracks    | 4.0 x 4.0            | 8.65 x 4.20   | 0.46x |

A person-sized commander was therefore stopped roughly a metre short of every
facade -- 2.54 m from a house he was contractually required to reach at 1.90 m.
The envelope was unreachable, not missed. The `pose_oscillation` that came with
it was the same thing seen from the animation side: the commander kept walking
into a wall that was not there.

`get_building_body()` is the second, person-scale extent, taken from the drawn
mesh and carried on `BuildingFootprint` as `body_center_*` / `body_width` /
`body_depth`, rotated with the building's facing at registration. Every
person-scale query uses it: the commander's body collision, the camera's
obstruction ray, bow aim and melee line of sight. Nothing about navigation
changed -- the footprint, the grid padding and every RTS path are untouched.

`BuildingBodyFootprintTest` rebuilds each building's geometry with a recording
submitter and asserts the body covers the drawn span without padding past it, so
the table cannot drift from the art the way the first one did. It also asserts
the two tables stay _different_, because them matching would mean one had been
edited by mistake.

Two things this measurement turned up that are **not** fixed here:

- **The barracks navigation footprint is less than half its drawn size.** RTS
  units can path through the drawn building. That is an RTS pathing question,
  not an RPG one, and changing it moves formation spacing and city layout.
- **`motor.slid` was zero throughout, before and after.** Neither scenario was
  ever a slide failure. Whoever takes on the swept capsule in 2.2 should not
  read these two greens as evidence that wall slide works; it has not been
  exercised yet.

## What the camera clearance metric found

Gate 0.3 deferred camera penetration and clearance because "today's only query
is a zero-radius ray fraction that returns no contact point". The Gate 2
person-scale geometry supplied the missing piece:
`nearest_building_body_clearance()` returns a signed horizontal distance to the
nearest building body, negative inside, and the camera trace carries it as
`eye_clearance` on every frame.

Its first run failed `rpg_close_quarters` on a defect that had been unreachable
until the commander could get close to a wall. At the scripted 0 -> 180 degree
yaw snap at 2.72 s the boom retracts from 3.426 m to 0.754 m in a single frame
-- correctly, that is the immediate retraction Gate 4.2 asks for -- and the eye
still ends up **0.339 m inside the house**, for 14 frames. A zero-radius ray can
stop a boom at a surface; it cannot hold a camera volume clear of one.

The scenario is pinned `expected_red` against gate 4 with that number in its
notes. Worth being precise about what changed: its Gate 2 clause is fixed and
stays fixed. This is a second, different defect that the first fix made visible,
which is the same shape as `rpg_melee_contact` becoming reproducible once the
animation clock stopped drifting.

## Where Gate 3 actually starts

`rpg_motor_start_stop` and `rpg_motor_figure_eight` are new with the Gate 2 motor
work, and both failed `planted_foot_slide` on their first run -- 0.091 m and
0.107 m of planted foot travel in a single frame. `rpg_locomotion` had never
caught this because its script has no abrupt halt.

The obvious reading is that the gait snaps from Walk to Idle. It does not.
Driving `resolve_humanoid_locomotion_sample()` through a stop directly:

```
   t    speed    blend  presence    phase   state
0.167    0.064   0.5267   1.0000   0.2737   Walk
0.183    0.000   0.4584   0.8703   0.2929   Idle
0.200    0.000   0.3990   0.7575   0.3120   Idle
0.217    0.000   0.3472   0.6592   0.3312   Idle
```

`locomotion_blend` fades smoothly and `locomotion_presence` fades behind it. The
defect is that while they fade, `gait_running` stays true, so **the cycle phase
keeps advancing at full rate** -- 0.019 of a cycle per frame -- under a body that
has already stopped. The planted foot keeps sweeping at roughly 40% stride
amplitude across stationary ground, and the blend is pulling that same foot
toward the idle base at the same time. Both land in one frame.

`rpg_motor_start_stop` reproduces it on every run, at exactly 2.05 s.
`rpg_motor_figure_eight` reported it once on a loaded machine and then passed
five standalone runs and every gate run afterwards with no code change between,
so it is pinned **green** with three repeats rather than red: one
unreproducible observation does not justify a pin, and `NoPlantedFootSliding`
only compares frames sampled within 0.05 s of each other, which makes it
sensitive to how busy the machine is.

### Four hypotheses, three refuted by measurement

Adding `foot_l_world` / `foot_r_world` to the soldier trace made the transition
measurable frame by frame. Ground-relative foot heights across the stop:

```
   t  visual      lY      rY  l_step
2.017 Walk    0.0902  0.0171  0.0014
2.033 Walk    0.1030  0.0170  0.0694
2.050 Idle    0.0887  0.0159  0.0909   <- reported
2.067 Idle    0.0166  0.0166  0.2299   <- the real snap
2.083 Idle    0.0166  0.0166  0.0000
```

At full run speed the feet alternate correctly -- one at 0.017 while the other
is at 0.195, crossing over cleanly. The damage is entirely in the last few
frames of deceleration, and the largest single jump is **0.23 m**, one frame
after the reported one, when both feet land on the idle rest height of 0.0166
and stay frozen there.

What was tried and did not work, so nobody repeats it:

1. **Freeze the phase during fade-out.** Made it worse, 0.091 -> 0.117 m: it
   strands the foot at the stride extreme so the blend has further to drag it.
2. **Settle the phase to the next foot-plant and hold the blend until it gets
   there.** Worse again, 0.128 m, and the pose still cut to idle two frames
   after the flip -- proof the settle never reached what was submitted.
3. **Keep `sample.state` at the moving state while `residual_gait` holds.** No
   effect at all, byte-identical to baseline. The reason is
   `animation_runtime.cpp`: `state.gait.state = gait_state` takes the state from
   `resolve_pose(inputs.anim).motion_state` and throws the manifest's sample
   state away.
4. **Feed `sample.state` through to `gait.state` as well, so the poser keeps
   applying the locomotion pose through the fade.** Also byte-identical. Forcing
   `gait.state = Run` unconditionally proves the path is live -- foot heights do
   change -- and the snap still happens, so `is_moving` gating is _not_
   sufficient on its own. Whatever collapses the pose is inside
   `locomotion_blend` on the real path, which the synthetic probe of
   `resolve_humanoid_locomotion_sample()` showed fading smoothly.

`render/humanoid/runtime/poser.cpp` guards the whole locomotion pose behind
`if (is_moving)`, and that is a real weakness -- but attempt 4 shows it is not
the whole story, because removing the gate does not remove the snap.

### What the gait instrument shows

`locomotion_blend`, `locomotion_presence`, `cycle_phase` and `persistent_valid`
now ride `SoldierAnimationDebugSample` out to the Arena trace, beside the foot
positions. Read at the stop:

```
   t  visual   blend  presence   phase  persist
2.033 Walk    0.2857    1.0000  0.7278     True
2.050 Idle    0.2486    0.8703  0.7422     True
2.067 Idle    0.0000    0.0000  0.3507     True
2.083 Idle    0.0000    0.0000  0.3610     True
```

This kills the remaining guesses and leaves one contradiction:

- The gait does **not** decay. `locomotion_presence` goes 0.8703 -> 0.0000 in a
  single frame, and `locomotion_blend` 0.2486 -> 0.0000 with it. With
  `locomotion_blend_tau` at 0.12 s and a 16.7 ms frame, `smooth_towards` can
  only take 0.8703 to 0.757. Zero in one step is not reachable.
- `cycle_phase` **jumps backwards**, 0.7422 -> 0.3507, which is what
  `resolve_humanoid_locomotion_sample` produces from an uninitialised previous
  state (the phase is then recomputed from `sample_time`).
- And yet `persistent_valid` -- `previous.initialized` as the real path sees it
  -- is **True on every one of those frames**, and every field including
  `locomotion_presence` is written back in the persist block.

Those three facts cannot all hold for a single call. Two candidate mechanisms
were checked against the same trace and **both ruled out**:

- **The time-rewind guard.** `resolve_humanoid_locomotion_sample` wipes its
  _local_ copy of `previous` when `sample_time < previous.last_sample_time`,
  which would explain everything -- `persistent_valid` is read before that wipe,
  so it would still report True. But `sample_time` in the trace is monotonic at
  exactly 0.016670 s per frame across the transition. The guard does not fire.
- **The phase override.** `resolve_humanoid_locomotion_phase_override` forces
  phase 0.5, and only for a bow-ready idle. This commander carries a sword, and
  the observed phase is 0.3507.

A third mechanism was then checked and also ruled out: a stale
`previous.last_sample_time` would make the smoothing delta large enough to
collapse presence in one step. Exposing it shows the delta is exactly one frame
throughout:

```
   t  visual  sample_t  prev_last     delta  presence   phase
2.050 Idle     2.05000    2.03333   0.01667    0.8703  0.7422
2.067 Idle     2.06667    2.05000   0.01667    0.0000  0.3507
```

`prev_last` advancing to 2.05000 also proves the persist block _did_ run on the
previous frame, so `previous.locomotion_presence` must be 0.8703 and
`smooth_towards(0.8703, 0.0, 0.01667, 0.12)` must be 0.757.

Every input to that call is now measured and every one of them is normal. The
code as written cannot produce the recorded output, which means **the recorded
gait does not come from the call that `build_humanoid_locomotion_state`
returns**. `locomotion_override.active` rewrites six `gait` fields but not
`locomotion_presence` or `locomotion_blend`, so it is not that one either.
There is a write path still unfound, and the strongest remaining lead is an
ownership seam rather than a solver bug.

`HumanoidAnimationStateComponent` lives **only on the render snapshot entity**,
never on the authoritative one. `World::publish_render_snapshot` reuses snapshot
entities while `render_entity_signature` is unchanged, and when the signature
does change it can destroy and recreate the entity -- taking the animation state
with it. `scene_walk.cpp` bridges that with `transfer_render_runtime_state`, but
only when the snapshot pointer changed between frames, and only for ids in
`current.render_unit_ids()`.

That predicts exactly the observed signature: the gait persists smoothly while
the commander walks, because the entity signature is stable, and is wiped on the
one frame the movement state flips, because that is what changes the signature.
It also explains why every input to the solver measures normal -- the solver is
fine; its `previous` is a fresh component on a fresh entity.

That check was run. Logging the identity of the persistent state the solver is
handed, frame by frame:

```
   t  visual    address  persist  presence   phase
1.983 Walk     4869728     True    1.0000  0.6837
2.000 Walk     4973584     True    1.0000  0.6987
2.017 Walk     4869728     True    1.0000  0.7134
2.033 Walk     4973584     True    1.0000  0.7278
2.050 Idle     4869728     True    0.8703  0.7422
2.067 Idle     4869728     True    0.0000  0.3507   <- same buffer twice
2.083 Idle     4973584     True    0.0000  0.3610
```

**There are two `HumanoidAnimationStateComponent` instances, not one**, and the
renderer alternates between them every frame -- one per render-snapshot buffer.
The gait's continuity therefore depends entirely on
`transfer_render_runtime_state` copying state across on every swap. And at the
failure frame the alternation **breaks**: 2.050 and 2.067 are served by the same
buffer, so `m_render_world_snapshot.get() == render_snapshot.get()` and the
transfer is skipped.

This is an ownership bug at the render-snapshot seam, not a locomotion bug. It
also explains why nothing in the solver reads wrong: the solver is handed a
state object that is simply not the one it last wrote to.
`World::publish_render_snapshot` allocates a **brand-new** `World` whenever its
next buffer is still referenced (`buffer.use_count() > 1`), and a new World
means new entities and a fresh animation state.

Two consequences to weigh before fixing it:

- It affects **every humanoid** whose movement state changes, not just the
  commander. Soldiers halting after a march have the same discontinuity; nobody
  noticed because `NoPlantedFootSliding` is only attached to a handful of
  scenarios.
- The fix belongs in snapshot buffer lifetime or in making the runtime-state
  transfer independent of pointer identity. It does **not** belong in
  `locomotion_manifest.cpp`, which is where all six failed attempts went.

Six edits were attempted against this defect across the manifest, the animation
runtime and the poser. Four are documented above; the last two, feeding
`sample.state` through to `gait.state`, were also inert. All are reverted. The
instrumentation is what survived, and it is what makes the remaining question a
single lookup rather than a seventh guess.

That changes every humanoid in the game, not just the commander, so it is the
first real piece of Gate 3 rather than a tail of Gate 2.

## The timeouts were the screensaver

Three consecutive full sweeps reported one scenario as INCOMPLETE on a
wall-clock watchdog, and it was a **different scenario each time** --
`rpg_close_quarters`, then `rpg_locomotion` run 2/3, then
`rpg_motor_figure_eight` run 2/3. Raising `--watchdog-multiplier` to 20 did not
help, and the machine was idle (load 0.4).

Running the suspected scenario three times back to back by hand finished in
about 12 s each, every time. That ruled out the scenario and ruled out repeats.
What actually correlated was elapsed time: `xset q` shows blanking at 600 s and
DPMS standby at 600 s, and a full sweep runs longer than ten minutes. A blanked
display does not slow the presented frame loop, it stalls it, so the watchdog
fires on whatever happens to be running at the ten minute mark.

`scripts/run-rpg-gates.sh` now suspends blanking for the sweep and restores the
original timeouts on exit, including on interrupt. The sweep that had failed
three times in a row came back clean: 10 green, 5 known-red, 0 incomplete.

Worth remembering when reading any Arena result: an INCOMPLETE is not a
behavioural result, and a defect that moves between scenarios run to run is
almost never in the scenarios.

## Two CI breaks on main, 24 Aug 2026

Neither came from the RPG work; both were reproduced from the job logs.

**Windows, `error C3861: 'glTexImage3D': identifier not found`.**
`allocate_shadow_array` in `render/gl/backend.cpp` is a free function, so it
called the _global_ `glTexImage3D`. That is OpenGL 1.2, and the Windows SDK's
`gl.h` only declares 1.1 -- which is why it compiled on Linux and macOS and
failed only under MSVC. It takes the Backend's `QOpenGLFunctions_3_3_Core` now.
Anything GL 1.2 or newer must go through Qt's function table, never the global
name.

**macOS, exit 10 from the renderer self-test.** The log says it plainly:

```
02:17:16.854  PASS - gameplay OpenGL frame rendered and presented
02:17:16.917  FAIL - no gameplay frame was presented within 30 seconds
```

PASS, then FAIL 63 ms later. `QGuiApplication::exit(0)` asks the loop to return;
it does not stop pending events, so the 30 s timeout still fired and its
`exit(10)` won. Both self-tests latch on a shared flag now, so whichever
outcome happens first is the one that sticks.

## Camera collision, Gate 4.2

The eye reached 0.339 m inside a house at the scripted 180 degree yaw snap. Two
causes, both needed fixing:

- `first_building_intersection_fraction` is a **zero-radius ray**. It can stop a
  boom at a surface; it cannot hold a camera volume clear of one.
  `first_building_body_intersection_fraction` sweeps a camera radius by growing
  each building body by that radius before the slab test, and returns 0 when the
  start is already inside.
- Even retracted, the occlusion clamp floors the boom at 22% of its length. With
  the commander 0.34 m from a facade that still put the eye through the wall, so
  the resolved eye is now depenetrated out of any body it overlaps, along the
  shallower axis.

`commander_boom_pumping` then fired on `rpg_obstacle_slide`, and it was right to
look: the reversals were 0.2 mm. A reversal below 5 mm is numerical noise, not
pumping -- at a 3.4 m boom and a 75 degree field of view that is under a pixel
at 1080p -- so the metric now needs both steps to clear that floor.

## Camera collision against the rest of the world, Gate 4.2

The first pass at Gate 4.2 was correct and incomplete in two separate ways, both
of which the issue "camera clips into buildings and obstacles" names directly.

**It only knew about buildings.** `BuildingCollisionRegistry` holds houses,
walls, towers and the authored structure obstacles -- not the 200-270 world props
a campaign map carries, and not the scatter trees the sim adopted when props
started blocking navigation. So a commander walking a treeline had the lens
inside trunks, ruins and tents with nothing reporting it. `camera_obstruction`
answers the same three questions -- boom clear fraction, body clearance,
depenetration -- over buildings **and** every solid world prop, from an index
rebuilt off `TerrainService::world_props_revision` rather than per frame.

Props are not buildings, and the difference is vertical. A building is taller
than the boom ever reaches, so it blocks at any height; a prop blocks only up to
`world_prop_occluder_height`, its model height carried through the same scale
chain as its ground extents. The eye sits about 2.6 m above the commander's
ground, so this is what decides that ruins (5.9 m) and a pine trunk (6.4 m) pull
the lens in while a boulder (0.84 m), an iron ore seam (0.86 m) and a fallen dead
tree (0.78 m) do not. Tree canopies are deliberately excluded: the trunk is what
the simulation calls solid, and colliding with foliage would pump the boom on
every step through a wood.

**Depenetration could put the lens on the far side of the wall.** The resolved
eye used to be pushed out of any body it overlapped along the shallower axis,
unconditionally. That is the right move for a point that has to end up outside
something; it is the wrong move for a camera, because "outside" includes the face
away from the commander. With the commander in a temple footprint the old code
retracted to the 22% floor, was still inside, and pushed 2.6 m out to the nearest
temple wall -- with the commander behind it. The boom is resolved by shortening
now, floored at a metric 0.55 m instead of a fraction, and the sideways push
survives only for the one case shortening cannot fix -- a pivot that is itself
buried -- where it may no longer increase the planar distance to the commander.
`ABuriedPivotNeverPushesTheLensAwayFromTheBody` pins it: with the commander
inside a temple footprint the resolved eye stays within 0.75 m of him, where the
old rule's escape distance was the half-depth of whichever face was nearest.

**Terrain was cleared under the eye, not along the boom.** Lifting the eye to
`ground + 0.55` at its own position keeps the lens out of a hill and puts the
crest between it and the commander. Lifting the eye by `L` raises the boom point
at `t` by `L * t`, so the ground is sampled along the boom and the lift solved
for the worst sample; the whole sight line clears the ground, which is the thing
the player actually needs.

The bound on that lift is the part worth keeping. The first version solved it
unbounded up to a 4 m cap, and `rpg_camera_hill_bank` -- written for this issue
-- caught what that does: against a 5 m bank a boom's length behind him, the
solve saturated the cap for half a second and put the eye at `y = 7.86` with the
commander standing at `y = 1.34`. That is a helicopter shot, not a chase camera,
and it fails the same "keep him in sight" contract it was trying to satisfy.
Metres of lift is the wrong answer to a steep bank; a shorter boom is. The lift
is capped at 2.5 m now -- enough to ride ordinary rolling ground -- and past
that the ground feeds the _same shortening path_ the bodies use: the boom is
sampled once against the highest sight line a bounded lift can reach, and where
it crosses caps the fraction. A tight bank pulls the lens in instead of
launching it, which is what "tight spaces should reduce camera distance
gracefully" asks for. On the bank the boom now works between 4.86 and 0.92 m
with the lift peaking at 2.48 m, and the commander is never behind the crest.

Read the crossing, not the sample index. The first version snapped the limit
back to the last clear sample, which quantised the boom target to eighths and
turned a smooth walk past the bank into ten `commander_boom_pumping` reversals
-- the same scenario catching the fix for its own first failure. Interpolating
the crossing between the two bracketing samples takes it to one.

**Release had to move from a rate to a speed.** `m_occlusion_fraction` retracts
instantly and eases back out at 18/s, which is fine while an obstruction
approaches and clears gradually -- the commander walking toward and away from a
facade, which is all `rpg_close_quarters` ever asked for. Walking the lens
_past_ the corner of a body is different: the clear fraction steps from about a
half to 1.0 between two frames, and an 18/s follow on that step is 0.48 m of
boom in one frame on a 3.4 m boom. That is the pop `CommanderBoomIsContinuous`
exists to catch, and adding discrete props to the collision set is what made it
reachable. Extension is now capped at 6 m/s on top of the ease; retraction is
still immediate.

Three scenarios cover it, all the same shot -- the commander sidesteps with the
view held across his path, so the boom trails through whatever is there:

| scenario                   | what it drives the lens through                         | boom, rest -> worst |
| -------------------------- | ------------------------------------------------------- | ------------------- |
| `rpg_camera_prop_gauntlet` | ruins, a pine, a boulder, a fallen dead tree and a tent | 3.44 -> 1.80 m      |
| `rpg_camera_wall_pocket`   | a nine-segment wall run ending at a house               | 3.44 -> 2.37 m      |
| `rpg_camera_hill_bank`     | the flank of a 5 m bank                                 | 4.86 -> 0.92 m      |

All three run zero frames with geometry between the lens and the commander, and
the gauntlet's minimum eye clearance is 0.285 m against a 0.10 m contract.

The boulder and the log in the gauntlet are not filler: they are the half of the
contract that says the camera must _not_ react to geometry it is already two
metres above. Measured, with the eye riding 2.62 m over the commander:

| prop      | top    | boom while the lens is over it |
| --------- | ------ | ------------------------------ |
| ruins     | 5.92 m | 2.54 m                         |
| pine      | 6.39 m | 2.48 m                         |
| tent      | 3.04 m | 1.80 m                         |
| boulder   | 0.84 m | 3.41 m -- untouched            |
| dead tree | 0.78 m | 3.42 m -- untouched            |

A collision model without the height lid would have fired on the bottom two and
pumped the boom across every rocky map in the game.

## The gait was being advanced twice per frame

After six refuted hypotheses, the instrument that finally isolated it was
`previous_locomotion_presence` -- the value `resolve_humanoid_locomotion_sample`
receives, traced beside the value it produces. It showed the persistent state
arriving with **0.0**, the default, when `0.87032` had been written the frame
before.

That combination -- `initialized == true` with a zeroed presence -- can only be
produced by a call whose own `previous` was uninitialised. In other words there
was a _second_ call per frame that the trace never showed, and it was
overwriting the first one's work with a fresh fade.

The fix is idempotence, not ownership: a locomotion sample whose
`sample_time` has not advanced past `previous.last_sample_time` no longer writes
persistent state. Preparing the same frame twice is now a no-op instead of a
second tick of decay.

What it bought, measured on `rpg_motor_start_stop`:

```
             before              after
presence     0.8703 -> 0.0000    0.8703 -> 0.7575 -> 0.6592 -> 0.5738
foot step    0.2299 m            0.0622 -> 0.0477 -> 0.0366
```

`rpg_motor_figure_eight` had been intermittently red on the same defect -- once
per two or three sweeps, depending on whether a transition happened to land on a
planted foot. It now passes three repeats per sweep and standalone, and is
pinned green.

Two smaller things came out of the same work:

- **The plant window admitted swinging feet.** `k_planted_foot_height` was
  0.12 m. Measured, a genuinely planted foot sits at 0.015-0.021 m and a
  swinging one at 0.05-0.19 m, so the old window counted normal swing travel as
  sliding. It is 0.05 m now -- still more than twice the observed contact
  height, and it still catches the remaining real defect, which is a foot at
  0.0208 m moving 0.133 m.
- **A lifted foot may move any distance**, and that is deliberate:
  `RenderProbeAcceptsAFootLiftedOffTheGround` encodes it. A height-independent
  foot-teleport check was tried and removed for contradicting it.

`rpg_motor_start_stop` stays red on 0.133 m at the second stop -- a run
backwards into a halt. The gait fades cleanly through it now, so what is left is
a one-frame discontinuity elsewhere; the next suspect is
`resolve_humanoid_locomotion_variation`, which scales `walk_speed_multiplier` by
1.25 from a raw `running` bool rather than the smoothed `run_presence`, and so
steps the gait's spatial scale by 25% on the frame a run ends.

## The sword slice, Gate 5

The first slice is one on-foot sword commander, one sword enemy, and the verbs
move, camera, lock, light attack, guard, and dodge. Four defects were found and
repaired here; each is named with the measurement that found it.

### One press is one attack

`update_impl` used to build an attack intent from either a press edge **or** an
`auto_repeat` branch that fired whenever the attack button was held, the queue
was empty and no animation was running. That is melee auto-repeat, and it is the
contract Gate 5.1 removes: holding the button now requests exactly one attack,
the same as tapping it. `rpg_one_press_one_attack` presses three times and then
holds the button for 1.6 s, and asserts exactly four accepted actions.

The removal made two unit tests fail, and they were right to: they set
`controller.input().primary_action = true` directly and never produced a press
edge. A held flag with no edge behind it is not an attack request. Both now go
through `primary_action_down()`/`primary_action_up()`.

### The input buffer is observable

`CombatIntentQueueComponent` buffered presses for 0.45 s and reported only the
last outcome. It now buffers for the authored 0.15 s and counts every outcome
class -- accepted, buffered, refused, expired and overflow -- so a press that
never becomes a swing is a number instead of a silent loss. `Expired` is a new
outcome; `expire_stale_intents` records it when it drops an entry, and `push`
counts an overflow when a fourth press arrives inside the window.

`rpg_attack_buffer_window` replaces `rpg_combo_cadence`, which measured the
cadence of a _held_ button and therefore tested exactly the auto-repeat contract
that is now gone. The replacement presses deliberately: one clean press, one
late enough in the previous swing that the buffer carries it into the next, and
one so early that the buffer has to let it expire.

### Contacts resolve onto the hurt body

`rpg_defense_contact` had been red since the baseline audit with "no block
contact published and protected health lost". The guard was not broken. The
incoming contact point was a point on the _attacker's blade_: for an overhead
swing, 3.05 m in world space, 2.04 m above the commander's feet, which is above
his head. The guard test measured that point against a 0.60 m plate at chest
height and correctly concluded it was nowhere near the guard.

Two repairs, both in the direction Gate 5.3 asks for:

1. `hurt_body_contact_point()` clamps the blade's closest point onto the
   target's hurt body -- a capsule from 0.30 m to 1.70 m with the body radius --
   so the contact reported to damage, reaction, sparks and the guard is a point
   on the body that was actually hit.
2. Ordinary blocking is the authored guard arc (`frontal_arc_dot`, plus vertical
   coverage and the guarded side of the body). The 0.60 m plate test is what a
   _perfect_ guard has to meet, which is what makes the perfect window a skill
   check rather than an accident of where the blade tip happened to be.

### Line of sight was measured against the navigation box

Writing `rpg_lock_occlusion_death_cycle` produced a scenario where lock-on
refused all three enemies, including one standing four metres to the side of the
house. `has_clear_building_los` read `building.width`/`building.depth` -- the RTS
navigation footprint -- so a `home` drawn 2.36 x 2.42 m cast a 4.3 x 4.4 m shadow
over every person-scale sight line: lock acquisition, melee line of sight and bow
aim. It reads the person-scale body now, the same repair Gate 2 made for the body
collider and the camera boom, and the only reason it survived that pass is that
nothing measured it.

### Lock-on holds at close range and yields to the hand

Three additions, each with a number behind it:

- Inside 0.75 m the lock stops solving facing; from 0.75 m to 2 m its authority
  tapers in; and the spring can never turn the view faster than 220 deg/s. A
  target pinned inside the commander's own footprint used to swing the view 148
  degrees in half a second. It moves under one degree now.
- Cycling is screen-space: the first lock takes the target nearest the view
  centre, each cycle steps to the next target to the right, and the rightmost
  wraps to the leftmost. It used to step through a list sorted by distance from
  the centre, which is not an order a player can predict.
- A look input during lock suspends the spring for 0.35 s, so manual look steers
  without unlocking.

What is deliberately not done here: the lock still reaches the camera by writing
`m_view_yaw`, the player's stored free-look yaw. Gate 4.1 forbids that and owns
the fix -- a camera-owned framing yaw with the player's free-look preserved
underneath it.

### Defence feedback follows a resolved contact

`PerfectGuard` was published when the player raised the guard and `DodgeSuccess`
when the player pressed dodge. Neither event meant anything had been defended.
`RpgHealthComponent` now counts resolved contacts by outcome -- blocked, perfect,
dodged, damaging, guard-broken -- at the single point where damage resolves, and
the controller publishes feedback from a change in those counters. Pinned by
`DefenceFeedbackFollowsAResolvedContactNotARequest` and
`GuardFeedbackWaitsForAContactToBlock`.

### Stamina became a real constraint

A light attack costs 12 stamina, regeneration is 10/s and a swing takes about
0.9 s, so the pool never moved: the commander could swing forever. Regeneration
now pauses for 0.75 s after any spend (`StaminaComponent::spend`), which makes
the pool the constraint the Gate 5.4 contract assumes.
`rpg_stamina_refusal_and_recovery` presses until the pool cannot pay, checks the
refusal is counted, waits, and swings again. The Arena commander also gets a
stamina component now -- it had none, so its trace reported `-1`.

### Guard and dodge windows are authored in one place

`game/systems/combat_actions/commander_defense_timeline.h` holds the guard raise,
perfect window, release and break-recovery times and the dodge startup,
i-frame interval, roll and recovery times. `CommanderDefenseWindowsTest` drives
the boundary matrix Gate 5.5 asks for -- a contact one tick inside the perfect
window, one tick after it, and well after it; i-frames at 30, 60 and 120 Hz --
frame-exact, which a rendered scenario cannot be.

## Feedback that reads, Gate 6

### The hit pause no longer stops the player's timeline

A contact set `is_hit_paused` on the attacker, and `process_combat_state` used
that flag to skip the authored action timeline entirely. The measured effect on
the player's own swing was a 0.22 s freeze of `normalized_action_time` at 0.408,
which delays every window that follows: cancel, exit-safe, and therefore the next
accepted input. The pause is now presentation-local for a player-controlled
commander (0.045 s, and it never gates the timeline); other units keep the
original punch.

### A struck soldier reads, and the instrument can see it

`rpg_melee_contact` was pinned red on `no_visible_hit_reaction` with the note
that the victim's in-progress attack outranked the reaction. Two separate things
were wrong.

The scenario gave the six-soldier enemy 500 health, so every commander strike
was lethal: the "missing reaction" was a death animation, correctly outranking a
flinch. With 4000 health the victim survives to react.

The second is real and deliberate: `instance_prepare` converts a hit reaction
into a _swing recoil_ when the victim is mid-swing, so a contact does not cut a
swing in half. Nothing reported that, so the diagnostics said "Attack" and the
Arena concluded no reaction had been shown. `is_swing_recoiling` and
`hit_reaction_tilt_degrees` now ride every soldier sample, and
`HitReactionObserved` accepts a recoil that carries real tilt. The struck
soldier recoils 5.77 degrees for 17 frames per contact.

### Accessibility gates

`Game::Accessibility::CommanderInput` holds look sensitivity X and Y, invert Y,
camera impulse on/off, head bob on/off, an FOV scale, and hold-versus-toggle
guard; `UiPreferences` persists all seven and publishes them. The camera rig
already scaled bob and impact kick by `camera_motion_scale`; it now also honours
the individual switches, and run/dodge FOV kicks scale with reduced motion.
`CommanderAccessibilityTest` proves each knob changes behaviour rather than only
being stored.

## Performance and lifecycle, Gate 7

### Prewarm is separated from the budget

The first 0.75 s of every scenario is a prewarm window. Its frames are excluded
from p50/p95/p99/max and reported separately as `prewarm_frames` and
`prewarm_max_ms`, because a 306 ms first frame is shader and asset compilation,
not a gameplay hitch. `PrewarmFramesAreReportedButNotBudgeted` pins the split.

`run_config.json` now names the machine the numbers came from -- CPU model, GPU
vendor and renderer, GL version, OS, kernel and viewport size -- so a report can
be read against the hardware it was taken on.

`scripts/rpg_gate_report.py` evaluates the Gate 7.1 contract on every run and
prints it, and fails the gate on it only under `--enforce-performance`: p95 <=
16.67 ms, p99 <= 20.0 ms, post-prewarm max <= 33.3 ms, GPU timing present, and a
marked prewarm window. A frame budget with no GPU timing at all is reported as
`performance_gpu_timing_missing`.

These numbers are worthless on a loaded box. Check `uptime` first: a run taken
while another build is using twenty cores reported a p50 four times its quiet
value.

### Soak and lifecycle

`CommanderLifecycleSoakTest` covers the parts of Gate 7.2 that do not need a
window: a hundred enter/exit cycles restoring every commander flag and input
state, a ten-minute duel at 60 Hz where every input edge stays accounted for and
the buffer never overflows, and a pause injected at forty different points
inside an action, each of which has to leave no action running two seconds later.

The ten-minute duel is deliberately headless. Long unattended GPU renders have
hard-crashed this machine twice, and a soak that measures input accounting does
not need pixels.

## There is no roll pose to check the i-frames against

Gate 5.4 asks for the hurtbox and i-frame window to be verified against the
rendered roll rather than a timer. It cannot be written yet: through the whole
dodge the commander is submitted as `Run`, a run cycle played fast, and his root
height never leaves 1.006 m. The dodge is a translation with a locomotion clip on
top; there is no roll in the manifest.

The numbers the pose has to satisfy when it is authored: the roll is 0.22 s and
the i-frames are its first 0.12 s, so a visible tuck has to land inside the first
half or the invulnerability will not read.

## Two rules the crowd was missing

The engagement ring's attacker budget was already deterministic --
`deterministic_unit_roll(entity_id, epoch)`, no RNG state -- and two identical
worlds now prove it. What it lacked was fairness.

At most one presser may come from outside the 100-degree arc the player can see;
before that, a ring could fill entirely with attackers behind him.

And a presser's `-0.55` incumbency bonus becomes a `+2.4` penalty after three
seconds in the slot, so the ring rotates. `ACrowdCannotHoldEveryBearingForever`
found that ten attackers pinned the same four on the commander for the whole ten
seconds it watched, which is the chained pressure Gate 5.4 forbids.

## A committed strike no longer follows the camera

`body_must_face_view` included `attack_animation_active`, so every tick of a
running swing assigned `m_body_yaw = m_view_yaw`. A 150-degree mouse flick during
the active frames turned the body 150 degrees with it, which is what Gate 5.1
means by "raw camera yaw rotating a committed body arbitrarily through its
strike".

The authored allowance already existed and nothing read it:
`melee_interruption_at` publishes `redirect_authority` per phase -- 1.0 in
windup, 0.35 in the early strike, 0.0 once committed. The body yaw now follows
the view at that authority, so a swing still starts where the player is looking
and stops obeying the mouse once the blade is out.

## The second freeze was the whole simulation clock

`CommanderViewModel::time_effect_scale` multiplied `GameEngine::simulate`'s time
scale by **0.04** for the first half of a 0.10-0.18 s window every time the
commander landed a hit. Not the commander's animation: the whole match --
every unit, every projectile, the mission timers -- at four per cent speed. It
is the "near-global freeze" Gate 6.3 names, and it is deleted: the timer, the
`hit_stop_duration` field on `CommanderUpdateEffects`, and the scale.
`LandingAHitNeverSlowsTheSimulationClock` fails if it comes back.

The per-entity hit pause documented above was the other half. Both were found
the same way -- by asking what a contact does to the _clock_, not to the pose.

## Per-frame budgets have to scale with the frame

Running the gate at `--fps 30` failed six scenarios; four of them were the
metrics rather than the game. `planted_foot_slide`, `pelvis_snap`,
`commander_motor_correction` and `commander_boom_discontinuity` are all budgets
authored as "so many metres (or degrees) between frames" at 60 Hz, so a doubled
step trips them by construction. They are multiplied by `frame_time * 60`, never
below 1, so a 60 Hz run measures exactly what it measured before, a 30 Hz run
measures the same physical rate, and an authored 100 ms hitch frame is judged
against what a 100 ms frame can legitimately cover.

That took 30 Hz from six failures to two, and the two left are real:
`rpg_stamina_refusal_and_recovery` accepts nine swings at 30 Hz and ten at 120
against eleven at 60, so the pool never reaches a refusal; and
`rpg_close_quarters` trips the `fullscreen_flash` image check on its scripted
180-degree yaw snap, because consecutive frames are twice as far apart.

The clause this was meant to satisfy -- "behaviour is identical; only
presentation sampling changes" -- is still owed, because the Arena's `--fps` is
the batch loop's fixed _simulation_ step. Decoupling render frames from
simulation steps in the batch loop is the missing piece.

## What the RPG systems actually cost

Five scoped accumulators -- motor, targeting, weapon trace, engagement, camera --
sample and reset once per tick, ride the commander trace as `costs`, and are
reported as `rpg_cost_p95_ms` beside `simulation_p95_ms`.

First measurement, `rpg_skirmish_three_attackers` at 60 Hz: the simulation tick
is 0.19 ms at p95, and the entire RPG slice inside it is 0.019 ms -- engagement
0.021, camera 0.010, weapon trace 0.004, targeting 0.002, motor below the
timer's resolution. Whatever a 13 ms frame is spent on, it is not these.

## Where the gate stands after the sword slice

21 scenarios: 19 green, 2 known-red, 0 unexpected failures, 0 undeclared flakes,
0 incomplete. Both reds predate this work -- `rpg_motor_start_stop` is Gate 3's
planted-foot slide, and `rpg_escort_crowd` regressed at commit `35d87259` with a
1.4-1.6 m render-root jump on three enemy soldiers at 0.23 s, reproduced on a
pristine checkout of that commit before it was pinned.

Every unit suite is green: 4322 gtest cases across nine binaries plus 439 QML
cases, `scripts/run-tests.sh` exit 0.

An ASan/UBSan sweep (`RelWithDebInfo`, `-DENABLE_SANITIZER=address,undefined`,
`ASAN_OPTIONS=detect_leaks=0`) of `app_tests`, `combat_balance_tests`,
`arena_tests` and `persistence_tests` reports zero address findings and one
undefined-behaviour finding: a signed integer overflow in the Arena city plot
hash, predating this work and fixed in passing. `AudioCueCatalogTest` fails only
in the sanitized tree, whose `bin/assets` copy is six days stale -- see
[[bin-assets-is-a-copy-not-a-symlink]].

Two repository checks are red at HEAD and were red before this work:
`check-ambient-instances` (one ambient lookup in `app/commander` against a budget
of zero, introduced by `35d87259`) and `content_validator` (a troop layout
reference). Neither is touched by the sword slice; both were confirmed by
stashing the change and re-running.

## Working rules

- Every bug fix starts with a failing deterministic regression or a new Arena
  expectation that reproduces the defect as a time series.
- Every behaviour-changing commit names the gate it makes green and includes
  before/after trace evidence.
- Never relax a threshold merely to turn a report green. A threshold change
  needs an explicit design reason and captured before/after footage or trace.
- Keep work packages narrow. Camera collision, combat balance, and effects do
  not travel in one change.
- Preserve deterministic simulation and replay behaviour. Presentation-only
  interpolation may not feed authoritative combat or navigation queries.
- Run ASan/UBSan and the full normal suite before closing a milestone.

## The swing ran slower than it was authored, 2 Sep 2026

Reading `commander.combat` in `trace.jsonl` for `rpg_one_press_one_attack`: a light
slash authored at `duration_seconds = 0.72` ran for 0.90 s, and the normalized time
advanced 0.0185 per frame instead of 0.0231. The reason sits in
`resolve_melee_intent`: `swing_speed = 0.80 + aim_rate * 0.55`, and
`CombatActionService` divides the authored duration by it. A keyboard or controller
player produces no aim rate during a press, so every light ran at 80% of its authored
pace, and `k_heavy_maximum_swing_speed = 0.72` put heavies at 72% on top of timelines
that are already the longest in the table (overhead 0.88 s became 1.22 s, the finisher
1.15 s became 1.60 s). "A swing takes about 0.9 s" earlier in this document was a
measurement of that scaling, not of the design.

The definitions are the design. The solver now starts at 1.0 and clamps to
[0.85, 1.35], so a plain press plays the authored timeline and a swipe can still
quicken it; the heavy cap is 1.0 so heavy weight comes from the authored durations and
stagger, not from a second multiplier. The tired-swing penalty is unchanged.

Measured after the change, same scenario: slash 0.70-0.72 s, held-combo links still
0.02 s apart, press-to-swing latency still 0-17 ms, and every accepted press still
accounted for. Two scripts were authored against the slow cadence and are retuned in
the same change: `rpg_one_press_one_attack` allows five links in the 1.6 s hold (the
authored cadence fits five), and `rpg_attack_buffer_window` presses at 1.02 s and
3.75 s so the late press still lands inside the 0.15 s buffer before recovery start and
the early press still expires.

Two things the same reading found that are not defects: the render's "Attack" visual
outlives `action_running` by `exit_blend_duration` (0.10 s) -- that is the blend into
idle, not a freeze -- and the 3000 deg/s view yaw in `rpg_commander_sword_grammar`
during the dive is the harness (`track_rpg_aim` re-aims at the enemy centroid every
frame while the dive carries the camera past it), not the camera.

## The walk cycle outlived the stop

`rpg_motor_start_stop` at the walk halt: the motor's smoothed speed decays with a
0.06 s time constant, but the presented state stayed `Walk` until the speed was
exactly zero, 0.23 s after the release, because `has_component_velocity` compared the
motor's published velocity against a 0.01 m/s epsilon. The gait cycled at full stride
under a body doing 0.3 m/s -- the foot slide every stop ended with.

Direct control now has a gait floor, `CommanderComponent::k_direct_control_gait_floor_speed`
(0.60 m/s). Three inputs to the presented state had to honour it, and the first two
alone changed nothing, which is worth knowing before anyone repeats it: the published
motor velocity (`has_component_velocity`), the per-tick displacement test
(`displaced`), and the controller's `fpv_motion_requested`, which counted a smoothed
speed above 0.05 m/s as a request and so kept "moving" true through the whole
coast-down. With all three gated, the walk halt presents `Idle` at 1.90 s (0.08 s after
the release, speed 0.54 m/s) instead of 2.05 s, and `locomotion_presence` fades from
there while the motor finishes braking. Starts are unchanged because a held input
counts as moving regardless of speed. Other units keep the epsilon.

## Strafing slides the feet, and the fix is a clip, not code

Projecting each foot's world velocity onto the travel direction in
`rpg_motor_diagonal` (the commander always faces the view, so the axes are body-
relative): walking forward the trailing foot moves at 0.64 m/s against a 2.69 m/s
root and the leading foot at 4.54 -- a foot is planted. Backpedalling plants one at
-0.03 m/s through the reverse gait. Both pure strafes read 2.28 / 2.34 m/s on both
feet against a 2.31 m/s root: the walk cycle pumps forward and back while the body
translates sideways, so the feet slide the whole way. Diagonals land in between
(1.15 / 4.67).

`NoPlantedFootSliding` cannot see it, because it only compares feet while the root
is planted; slip during locomotion is unmeasured by the gate. Rotating the stride
toward the travel direction in `resolve_locomotion_foot` was tried and is inert at
runtime: humanoid bone palettes come from baked `.bpat` clips and the stride math
only runs in `bpat_baker`. The fix is authored strafe (and ideally 8-way) locomotion
clips selected by `travel_alignment` / `turn_amount`, which is Gate 3 content work
and was not started here. Until then, lock-on circling is the move that looks worst.

`rpg_motor_start_stop`, `rpg_locomotion`, `rpg_motor_figure_eight`,
`rpg_motor_diagonal`, `rpg_locomotion_hitch` and `rpg_close_quarters` pass with it.

## Play-through findings, 2 Sep 2026

A scripted play session (Xephyr + XTEST, `hold_the_sallow_ford`) entered direct
control from a live match and walked the menus around it. Xephyr renders through
llvmpipe at under one frame per second, so every claim below was confirmed from a
log trace or an offscreen QML render, not from a single screenshot. What it found,
and what changed:

- **Main menu keyboard died after any sub-screen.** Closing Campaign, Missions,
  Skirmish, Save, Load, Settings, Objectives or Help with the mouse set
  `menu_visible = true` while it already was, so `MainMenu.onVisibleChanged` never
  fired and nothing held focus: Enter, Esc and the arrows were dead until the player
  clicked. `Main.qml` now routes every return through `return_to_main_menu()`, which
  forces focus back onto the menu.
- **RTS cursor hints leaked into direct control.** The crosshair sat on the barracks
  and the HUD read "No ground under the cursor". `OrdersViewModel::refresh_context_intent`
  clears the intent while the commander is active.
- **The commander bar clipped its own orders.** `Metrics.commanderBottomBarMinHeight`
  was 96 px; the compact panel needs about 140 px, so "Place Rally" and "Command Aura"
  were cut off at every resolution. The bar is now 148-176 px (still under the RTS
  bar, which `tst_design_tokens` pins). Measured with a throwaway
  `design_system_qml_tests` TestCase that renders `HUDBottomCommander` against a fake
  `game` object; that is the fast loop for HUD layout.
- **Paused in direct control, Space could not resume.** Space is dodge in the commander
  context, `rts.pause` is RTS-only, and the overlay still said "Press Space to resume".
  `GameView.Keys.onPressed` now lets the pause binding resume from any context while
  the game is paused, and the overlay names the bound key.
- **Enemy waves arrived without warmed creature assets.** The template prewarm only
  warmed nations that already had a unit on the field, so a headless wave force
  (Carthage in the Roman ford mission) hit 565 `creature_submit_miss` criticals and
  drew nothing until something else loaded it. The runner now also warms every nation
  in `NationRegistry::player_nation_assignments()`. Zero criticals after the fix.
- **"Carthaginian Empire has been defeated" between waves.** `PlayerDefeatWatcher`
  announced any side with no living unit; wave forces have none between waves.
  `GameEngine` now passes `MissionWaveRuntime::owner_has_unspawned_waves` as a
  `still_expected` predicate, with a regression in `player_defeat_watcher_test`.
- **The defeat toast printed `qrc:/assets/visuals/icons/defeated.png`.**
  `IronNotification.icon` is a glyph string drawn in a `Text`; `Main.qml` handed it an
  image path. It now passes a glyph.
- **Raw nation id in the commander panel.** "roman_republic" under the commander's
  name; the panel now uses `Design.FactionTheme.nameFor`.
- **Mission orders spoken by a stranger.** `hold_the_sallow_ford` and
  `the_timber_levy` voiced their messages as `roman_legion_organizer` while the map
  fields `roman_field_commander`; the setup coordinator warned about it every launch.
  Both missions now speak through the commander the player controls.

Still open from the same session: the autosave progress card (with a Cancel button)
pops over the battlefield mid-fight; moving the pointer to a top-bar button swings the
commander camera because mouse-look keeps tracking; and the empty production panel's
"No Barracks" headline reads as "you have none" when it means "none selected". `C`
(camera mode) and `X` (weapon stance) both work, but only the stance shows feedback
("BOW · X" / "BLADE · X" over the ability tiles); the camera toggle is silent.

## Related

- `docs/CAMERA_CONTROLS.md` -- the RTS camera this mode borrows from.
- `docs/COMBAT_SYSTEM.md` -- the combat systems the RPG slice drives.
- `tools/arena/README.md` -- the Arena harness, scenario schema, and artifacts.
