# RPG mode recovery: Souls-like playability before content

This is the active recovery plan for direct commander control. The target is not
"an RTS camera that can get close to a commander." The target is the control,
camera, movement, collision, animation, and combat reliability expected from a
good third-person Souls-like action game, while still operating inside Standard
of Iron's large RTS world.

The order is mandatory:

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
cinematic behavior should be added before the camera/movement gate is green.
Existing polish that obscures a defect should be disabled while the underlying
system is repaired.

## Baseline audit (2026-08-23)

### Reproduced results

The following checks were run from a clean worktree against the current local
build. Arena cases were run **sequentially** at 60 Hz so concurrent GPU work did
not contaminate their reports.

```sh
build/bin/app_tests \
  --gtest_filter='CommanderCameraRig*:CommanderControlControllerTest.*:CommanderDirectControlLatencyTest.*:CommanderModeTransitionTest.*' \
  --gtest_brief=1

# Then run each registered rpg_* Arena scenario separately:
build/bin/arena_app --batch --scenario <scenario> --fps 60 \
  --capture-interval 0 --artifact-dir <fresh-directory>
```

- All **66** targeted camera/controller/latency/mode unit tests passed.
- Only **6 of 11** rendered RPG Arena scenarios passed.
- `rpg_close_quarters` failed because the commander stopped 2.54 m from the
  house when the contract required 1.90 m, and the rendered body changed visual
  state five times in one second.
- `rpg_obstacle_slide` failed because the commander stopped 2.57 m from the
  house instead of reaching 1.90 m.
- `rpg_locomotion` failed because a planted foot moved 0.084 m between frames
  while the commander was reported as not walking.
- `rpg_combo_cadence` failed because a held attack left a 1.25 s gap where the
  existing contract permits at most 1.10 s.
- `rpg_defense_contact` failed because no block contact was published and the
  commander lost health in a protected block/dodge sequence.
- A passing `rpg_strike_lunge` run reported a 101.45 ms maximum rendered frame.
  The current scenario passed because its performance decision is not a strict
  hitch gate. Performance numbers are hardware-sensitive, but accepting a
  visible 100 ms hitch is not compatible with this mode's target.

This is the most important audit result: the isolated tests are green while the
integrated behavior is not. More isolated state tests alone will not make the
mode playable. Every repair below needs a time-series or rendered scenario gate.

### Confirmed code-level findings

1. **One attack press currently enters through two paths.**
   `CommanderViewModel::primary_action_down()` calls
   `CommanderControlController::primary_action_down()` and then immediately
   calls `primary_action()` against the world. The controller also leaves
   `m_primary_press_pending` set so the simulation queue can process that press
   again. Input edges need one owner.

2. **`CommanderFrameIntent` is sampled but is not the simulation's input.**
   `GameEngine::update_presentation()` samples it, but the simulation continues
   to read the controller's mutable `m_input` fields directly. The frame intent
   currently provides diagnostics/accessors rather than a render-to-simulation
   contract. Fast press/release edges can therefore be lost or handled through
   a different path than held state.

3. **The controlled body and camera do not share one presentation pose.**
   Simulation runs at 60 Hz. The renderer submits the latest snapped transform,
   while `CommanderCameraRig` independently chases that transform through a
   bounded visual anchor on every presentation frame. At high refresh rates the
   body can advance in simulation-sized steps relative to a separately eased
   camera. `MotionPresentationComponent::previous_*` is used to classify gait,
   not to interpolate the rendered transform.

4. **The character motor is a collection of direct transform edits.**
   Walking, dodge, strike lunge, body separation, and jump recovery all mutate
   `TransformComponent` inside `CommanderControlController`. Ordinary collision
   tests a destination point against grid state and building footprints, then
   tries the X and Z axes separately. This explains premature blocking, abrupt
   diagonal axis loss, corner sticking, and corrections that animation has to
   disguise.

5. **Jump is presentation lift plus permissive planar travel, not a physical
   traversal state.** While airborne, planar motion bypasses ground collision;
   landing can restore the last walkable position. That can cross geometry and
   then snap back. Disable jump for the first playable slice or implement it in
   the same motor/collision solver as every other displacement.

6. **Camera collision is a zero-radius 2D building ray.**
   `first_building_intersection_fraction()` checks only X/Z slabs from
   `BuildingCollisionRegistry::get_all_buildings()`. It has no camera radius,
   near-plane volume, Y/height test, surface normal, terrain sweep, or authored
   obstacle/prop coverage. Treating a footprint as an infinite vertical column
   can pull the camera in while it is above a roof; treating the camera as a ray
   can let the near plane and lens enter a wall at edges and corners.

7. **Terrain protection checks only the final eye point.** The camera raises
   `eye_desired.y` over the terrain below that endpoint. A ridge between pivot
   and eye is not swept, so the boom can cross terrain before the endpoint is
   corrected.

8. **Camera collision changes the eye after the free-look target has already
   been derived.** When obstruction retracts the eye, the old target remains.
   The view axis and therefore bow/crosshair ray can pitch or yaw during camera
   collision even without mouse input.

9. **Several members named `*_smooth` are assigned the final desired value.**
   `m_eye_smooth` and `m_target_smooth` do not themselves implement a camera
   spring. Smoothing is spread across the anchor, framing, focus, FOV, and
   occlusion fraction, making overshoot and derivative continuity hard to
   reason about or test.

10. **Polish is active before the base rig is trustworthy.** The current camera
    layers locomotion bob, breathing, strafe lean, run FOV, impact dolly/FOV,
    threat-side bias, focus pull, dodge FOV, and dodge roll tilt. Each adds
    movement to a camera already missing complete collision and presentation
    interpolation.

11. **Free exploration and lock-on orientation are conflated.** Outside a lock,
    the commander's body yaw is still forced to view yaw every simulation tick.
    In lock-on, `update_lock_on_yaw()` changes the view yaw itself. A Souls-like
    contract needs distinct modes: free camera with the body rotating toward
    movement, and locked combat with the body facing the target while movement
    becomes radial/tangential.

12. **Combat movement bypasses the motor contract because there is no motor
    contract.** Strike lunge, dodge, rush, separation push, and ordinary walking
    use different movement rules. A hit can therefore feel correct in its
    authored clip while its world displacement clips, sticks, or snaps.

13. **Melee hold-to-auto-repeat is not a Souls-like input contract.** The
    controller scans a held primary button and creates another intent whenever
    the current animation is no longer active. For the target experience, one
    press should mean one committed light attack, another press should buffer a
    deliberate combo, and hold should be reserved for a defined heavy/charge or
    bow draw. The current `rpg_combo_cadence` scenario should be replaced after
    this contract is changed, not loosened until it passes.

14. **Feedback reports success before success occurs.** Raising guard publishes
    `PlayerFeedbackType::PerfectGuard`, and starting a dodge publishes
    `DodgeSuccess`; neither event proves a contact was actually blocked or
    avoided. Feedback must be emitted from resolved contact outcomes.

15. **Hit stop scales the whole simulation almost to zero.** A landed commander
    strike currently applies 0.10-0.18 s of global time scaling that begins at 0.04.
    That can make the entire RTS battle feel sticky and compounds input/camera
    pacing. Any later hit pause should be short, presentation-local where
    possible, and added only after contact timing is correct.

16. **Existing camera gates are too shallow.** The main occlusion test only
    asserts that one building causes the camera Z position to move closer. The
    Arena has lens-gap/body-culling contracts but no camera-pose trace and no
    assertions for penetration, clearance, corner behavior, terrain, camera
    velocity, uncommanded rotation, or render/simulation jitter.

17. **The controller owns too many domains.** Input capture, mouse warping,
    character movement, collision, jump, dodge, ability activation, targeting,
    attack buffering, animation-facing state, audio, feedback, and camera all
    live in `commander_control_controller.cpp`. Repair work must extract narrow,
    testable systems rather than keep adding flags to the same update function.

Relevant implementation areas:

- `app/viewmodels/commander_view_model.cpp`
- `app/commander/commander_control_controller.{h,cpp}`
- `app/commander/commander_camera_rig.{h,cpp}`
- `app/commander/commander_frame_intent.h`
- `game/systems/building_line_of_sight.{h,cpp}`
- `game/systems/building_collision_registry.{h,cpp}`
- `game/core/world.cpp` and `game/core/component.h`
- `game/systems/combat_actions/`
- `game/systems/rpg_combat_system/`
- `render/scene_walk.cpp`
- `tools/arena/arena_scenario*.{h,cpp}`

## Non-negotiable product contract

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
- Guard, perfect guard, dodge invulnerability, posture, and stamina have authored
  windows with one source of truth.
- Lock-on is stable, can be overridden, and never fights mouse input.
- Crowds remain fair: active attackers are controlled, off-camera attacks obey a
  readability rule, and the player cannot be indefinitely stagger-locked.
- Stable 60 fps/frame pacing on the declared reference hardware is part of
  control quality, not a later optimization.

## Agent execution rules

The permanent contract now lives in [`docs/RPG_PLAYABILITY.md`](docs/RPG_PLAYABILITY.md):
the product contract, how the gate is run, how a verdict is decided, and the
2026-08-23 baseline audit. `todo.md` stays the live checklist.

- [x] Create `docs/RPG_PLAYABILITY.md` from the product contract and gates below;
      keep `todo.md` as the live checklist and link the permanent document here.
- [x] Add `scripts/run-rpg-gates.sh` (and a `make rpg-gate` wrapper) that builds
      required targets, runs unit/integration tests, then runs every required RPG
      Arena case **sequentially** into a fresh artifact directory.
- [x] Make the gate script print one compact table: scenario, completed, passed,
      issue codes, p50, p95, max, and artifact path; exit nonzero on any failure.
- [x] Save the initial sequential reports as the comparison baseline outside
      tracked source artifacts; record the commit and build type in `run_config`.
      `make rpg-gate-baseline` writes `artifacts/rpg-gates/baseline` (gitignored);
      commit, dirty flag and CMake build type are merged into every
      `run_config.json` and into `gate_run.json`.
- [ ] Every bug fix starts with a failing deterministic regression or a new Arena
      expectation that reproduces the defect as a time series.
- [ ] Every behavior-changing commit names the gate it makes green and includes
      before/after trace evidence in its commit or PR description.
- [ ] Never relax a threshold merely to turn a report green. A threshold change
      needs an explicit design reason and captured before/after footage/trace.
- [ ] Keep work packages narrow. Do not mix camera collision, combat balance, and
      effects in one change.
- [ ] Preserve deterministic simulation/replay behavior. Presentation-only
      interpolation may not feed authoritative combat or navigation queries.
- [ ] Run ASan/UBSan and the full normal suite before closing a milestone.

## Gate 0: make failures and telemetry truthful

This gate precedes behavioral fixes. It prevents agents from tuning by eye while
the important data is absent.

### 0.1 Reproduce current failures in CI-capable form

- [x] Pin the five current failures as expected-red tests on a recovery branch:
      close-quarters clearance, obstacle slide, planted-foot stability, deliberate
      combo timing, and protected defense contact. All five are `expected_red` in
      `tools/arena/rpg_gate_manifest.json` with the issue codes they fail with.
- [x] Add a manifest listing all required RPG scenarios so adding a new `rpg_*`
      case without adding it to the gate fails a test.
      `ArenaRpgGateManifestTest` compares manifest and registry in both directions.
- [x] Separate behavioral pass/fail from hardware performance reporting. CI can
      enforce deterministic behavior; frame budgets run on named reference
      hardware with prewarming. `frame_budget_exceeded` and `performance_*` are
      reported on every row and enforced only under `--enforce-performance`.
      Prewarming is still owed; it belongs with the Gate 7 performance contract.
- [x] Add a strict `completed == true` assertion so watchdog exits cannot produce
      an empty-issue report that looks like a normal behavioral failure. A
      watchdog exit reports as `TIMEOUT` and names the `timeout.txt` that proves
      it; anything else incomplete reports as `INCOMPLETE`. Both exit 3.
- [x] Build authored presentation-hitch injection so a hitch-only defect can be
      reproduced on demand. `ArenaScenarioDefinition::presentation_hitches` lists
      `{at_seconds, frame_ms}`; the batch loop substitutes that frame time for the
      fixed step exactly once per entry. `rpg_locomotion_hitch` exercises it with
      33/50/100/100 ms frames, confirmed in the trace. Gate 4's `rpg_camera_hitch`
      builds on the same field.
- [x] Make the rendered pose a deterministic function of the simulation at a
      fixed step. **The renderer's animation clock was never reset when a batch
      scenario started**, so it carried 74-116 ms of window-setup wall clock and
      every run sampled every clip at a different phase. The simulation, the
      motor and the camera rig were already bit-identical; only the pose moved.
      `Renderer::reset_animation_time()` is now called at batch scenario load,
      and the rendered pose is bit-identical across runs
      (`submitted_max_arm_reach` and `submitted_body_up_y` differ in 0 of 540
      frames, against 173-529 before).
- [x] `rpg_locomotion` is `required_green` again and passes 15 of 15. Its
      planted-foot slide was the sampling artefact above, not a motor defect.
      This proves reproducibility, not that the Gate 3 locomotion contract holds:
      gait is still selected from requested input rather than presented velocity.
- [ ] `rpg_melee_contact` is now pinned `expected_red` on
      `no_visible_hit_reaction`, owned by Gate 6. Removing the phase jitter turned
      a coin flip into a stable failure: a struck enemy stays in `AttackSword`
      through the whole contact window and goes straight to `Dying`, with 0 of
      ~1900 enemy soldier samples in `HitReaction` against 17 before. The
      commander's own reaction still shows for 72 samples, so the reaction path
      works -- the victim's in-progress attack outranks it. The threshold was not
      relaxed to keep the scenario green.

### 0.2 Add a presentation trace

Opt-in trace records (Arena diagnostics only) are emitted as a `commander` object
on every `trace.jsonl` frame of an `rpg_*` scenario. `App::Core::CommanderPresentationTrace`
is the schema; `ArenaViewport` enables it when it binds a scenario commander and
the shipped application never does. Schema and first findings are in
[`docs/RPG_PLAYABILITY.md`](docs/RPG_PLAYABILITY.md#the-presentation-trace).

- [x] Input: event sequence, press/release edge counters, sampled frame index,
      simulation-consumed sequence, move axes, raw look delta, yaw, and pitch.
      Consumption, drop and refusal are counted separately per verb, so an edge
      that reaches no consumer is a number rather than a missing swing.
- [x] Motor: authoritative previous/current pose, desired and actual velocity,
      grounded state, blocked/slid flags, separation push, lunge distance,
      snap-back distance, and displacement source
      (walk/airborne/dodge-roll/dodge-recover/strike-lunge/body-separation/jump-recovery).
- [x] Motor: presented pose. Unblocked by Gate 1.2 -- `presented_position`,
      `presented_yaw`, `presentation_alpha` and `presentation_extrapolated` ride
      the motor trace beside the authoritative pose.
- [ ] Motor, still owed: ground normal, collision normal/id/type, sweep fraction
      and depenetration distance. Those are outputs of the swept collider Gate
      2.2 builds; today's collision is a point/grid test with no normals to
      report.
- [x] Animation: already carried by the existing `soldiers` array -- selected
      animation and visual state, attack phase, transition count, and each
      planted foot's world position. Not duplicated into the commander record.
- [x] Camera: pivot, unconstrained eye/target, resolved eye/target, boom length
      before and after collision, blocked fraction, occlusion fraction, terrain
      lift, framing state and change, yaw/pitch and their velocities.
- [x] Camera clearance: `eye_clearance` on every traced frame, signed, negative
      when the eye is inside a building body.
- [ ] Camera, still owed: collision id/type/normal (needs the volume query from
      Gate 4.2), and the commander's screen-space anchor.
- [x] Combat: action phase and normalized time, queued intent count, guard and
      perfect-guard state, dodge state/timer/grace, locked and soft target
      ids/slots, hit-confirm sequence, hit count, health, and stamina.
- [ ] Combat, still owed: queue result/reason, authored window bounds, weapon
      trace start/end, contact time, damage result, guard result, i-frame result,
      and hit reaction. These arrive with the Gate 5 action timeline.
- [x] Frame pacing: presented-frame dt and CPU/GPU frame time are already on each
      trace frame (`frame_time_ms`, the `cpu_ms`/`gpu_ms` breakdowns).
- [x] Frame pacing: interpolation alpha. `presentation_alpha` is on every
      commander frame, along with whether the pose was extrapolated.
- [ ] Frame pacing, still owed: simulation ticks consumed, snapshot age and
      prewarm state. Prewarm belongs with Gate 7.1.

### 0.3 Add metric expectations, not screenshot-only checks

Six reusable expectation kinds now read the commander trace. Every `rpg_*`
scenario gets the input-edge, boom-continuity and motor-correction ones
automatically (`add_commander_control_metrics`, applied in `definitions()` to any
scenario with `rpg_mode`). Each is unit-tested from a synthetic trace in
`ArenaCommanderMetricsTest`, so the metric is proved to fire before it is
trusted on a scenario.

- [x] `CommanderBoomIsContinuous` -- `commander_boom_discontinuity` when the boom
      _extends_ more than the allowance in one frame, and `commander_boom_pumping`
      when it reverses direction repeatedly under one live obstruction. Immediate
      retraction is deliberately allowed: Gate 4.2 requires it to prevent
      penetration, so gating it would forbid the correct behaviour. The first
      draft did gate it and fired on `rpg_close_quarters`' scripted 180-degree
      yaw snap; that was the metric being wrong, not the camera.
- [x] `NoUncommandedViewRotation` -- yaw/pitch that moves with no look delta, no
      framing change, and no active lock.
- [x] `CommanderMotorCorrectionWithin` -- per-tick separation push and jump
      snap-back against a budget, named with the displacement source.
- [x] `CommanderSpeedIsContinuous` -- planar speed change per tick expressed as
      acceleration.
- [x] `CommanderInputEdgesAllConsumed` -- presses must equal consumed plus
      dropped, dropped must stay within budget, and dodge requests must resolve
      to consumed or refused. An empty trace reports
      `commander_input_not_traced` rather than passing silently.
- [x] `CommanderContactCountAtMost` -- contact multiplicity per running action.
- [x] Maximum camera penetration and minimum camera clearance. Unblocked early
      by the Gate 2 person-scale geometry: `nearest_building_body_clearance()`
      returns a signed horizontal distance to the nearest building body, the
      camera trace carries it as `eye_clearance`, and
      `CommanderCameraClearanceAtLeast` is attached to every `rpg_mode`
      scenario. It found a real defect on its first run -- see
      `rpg_close_quarters`. Still owed with the Gate 4.2 volume query: the
      contact id, type and normal, which a distance function cannot report.
- [x] Presentation-pose disagreement. `CommanderPresentedPoseAgrees` compares
      the point the camera framed against the body that was drawn, on every
      `rpg_mode` scenario, at 0.01 m. It found a real defect immediately: the two
      agree to **0.0000 m** in all ten scenarios that never swing a weapon, and
      diverge by a constant **0.2144 m** in the five melee ones -- the same value
      in four of them, so a fixed engagement offset rather than jitter. That is
      Gate 3's clip-root-versus-world-root item, now with a number.
- [ ] Screen-space anchor jitter. Still needs a projection; the pose half is
      covered by the metric above, which is stricter than a pixel budget.
- [x] Keep captures for human review, but never accept a camera/movement repair
      because two sparse screenshots look plausible. The gate defaults to
      `--capture-interval 0`; captures are opt-in and never decide a verdict.

**Gate 0 passes when:** the gate command reliably reports the current five
failures, traces contain enough data to identify the responsible stage, repeated
runs produce the same behavioral verdict, and a watchdog/incomplete run is
unambiguously red.

Status: **green.** Every pinned failure reproduces on every run with its issue
codes recorded; the trace identifies the responsible stage for each; identical
repeats give an identical verdict, including the rendered pose; and an incomplete
or watchdog run reports as `INCOMPLETE` or `TIMEOUT` and always exits nonzero.

The last open item closed by fixing the harness rather than the thresholds:
resetting the renderer's animation clock at batch scenario start made the
rendered pose reproducible, which turned one intermittent failure into a pass
(`rpg_locomotion`) and one intermittent pass into a stable pinned failure
(`rpg_melee_contact`). Two metric expectations from 0.3 remain unimplemented and
are named in place rather than skipped; both need data that later gates produce.

## Gate 1: one authoritative input and presentation path

### 1.1 Input ownership

- [x] Remove the direct world attack call from
      `CommanderViewModel::primary_action_down()`. The GUI layer only records an
      input edge; simulation is the sole action consumer.
      `CommanderControlController::primary_action` is now private, so the GUI
      cannot reach it again; `release_guard` had the same shape on the guard
      release and is deleted. Reproduced first by
      `CommanderViewModelInputTest.APressAloneDoesNotReachTheWorld` and
      `ReleasingGuardDoesNotWriteTheWorldOutsideATick`.
- [x] Replace one-frame booleans for dodge/jump/attack presses with monotonic edge
      sequence numbers or a bounded event queue so press+release between ticks is
      not lost. The sequence numbers landed with the Gate 0.2 trace; what was
      still missing is that every edge resolves. The flag-rally branch cleared
      four pending presses with no accounting, and `reset()` left
      `m_primary_press_pending` set, so a queued press survived mode exit and
      fired on re-entry. `take_input_snapshot()` and `discard_input_edges()` now own both.
- [x] Make `CommanderFrameIntent` (or a renamed `CommanderInputSnapshot`) the
      authoritative render-to-simulation packet. Consume each edge exactly once;
      copy held state every tick. `CommanderFrameIntent` turned out to have no
      production consumer at all -- an unused mirror -- while the tick read the
      same `InputState` the GUI thread was writing. `CommanderInputSnapshot` is
      the packet now: `take_input_snapshot()` copies held state and _moves_ the
      six edge latches out under `m_input_mutex`, so exactly one tick can see a
      press, and every simulation-side read goes through `m_tick_input`. A press
      the body could not act on is carried explicitly rather than surviving by
      accident in shared state.
- [x] Keep raw mouse accumulation on the presentation path so camera response is
      not delayed by the simulation tick. Publish the resulting view angles to
      simulation as intent; never have simulation and presentation both integrate
      the same raw delta. Already true and already pinned:
      `sample_frame_intent` calls `poll_mouse_look` from
      `GameEngine`'s presented frame, the simulation tick reads the resulting
      angles and never a raw delta, `CommanderControlRegressionTest` asserts
      `update_control_mode` does not poll, and
      `MouseLookChangesPresentedCameraWithinOneRenderedFrame` measures the
      budget at 30, 60, 120 and 144 Hz.
- [x] Reset all held inputs and edge queues on focus loss, mouse-capture loss,
      menu open, mode exit, commander death, loading, and window deactivation.
      `release_all_input()` is the single entry point. `CommanderInputLayer.qml`
      called it from nowhere: `release_actions()` released the two mouse buttons
      and dropped its `held_keys` map on the floor without a matching `key_up`,
      so the commander kept walking after focus loss, and nothing at all watched
      the window's `active` or `menu_visible`. Both are wired now.
- [x] Remove the second interactive input implementation in the Arena or route it
      through the same input packet API as the shipped application. Routed: the
      scripted `rpg_primary_attack` hook pressed the world directly and is now a
      real press edge held for three ticks, the two raw `InputState{}` wipes on
      focus-out and interactive exit go through `release_all_input()`, and
      `set_rpg_move_input` drives `key_down`/`key_up` instead of assigning the
      movement booleans behind the input mutex's back.

### 1.2 Shared presented commander pose

- [x] Store previous/current authoritative commander pose plus tick timestamps in
      the render snapshot. `CommanderPresentationSampleComponent` carries
      previous/current position and yaw, the tick duration and a tick sequence
      number, and rides `copy_presentation_snapshot_components` into the
      snapshot. It is commander-owned rather than reusing
      `MotionPresentationComponent`, because `begin_motion_presentation_frame`
      latches its `previous_*` at the top of `World::update`, which the
      commander tick has already run before -- so for the commander that pair
      brackets nothing.
- [x] Resolve one `CommanderPresentationPose` per rendered frame from those
      samples and the simulation clock. Feed that exact pose to both humanoid
      submission and `CommanderCameraRig`. One `resolve_presentation_pose()` in
      `game/core/component.h` serves both: the controller calls it for
      `inputs.commander_position`, and `UnitRenderCache::update_model_matrix`
      calls it for the body. Both age their own copy from the shared tick
      sequence, so they cannot disagree.
- [x] Use interpolation under normal cadence and tightly bounded extrapolation
      only when the next 60 Hz snapshot is late. Never extrapolate combat contact
      or feed a presentation pose back into simulation. Extrapolation is capped
      at half a tick and is only reachable when the presented frame is _shorter_
      than the tick; rendering slower than the simulation clamps to the newest
      sample instead of overshooting. Nothing reads the presented pose back:
      simulation, collision and contact all still use `TransformComponent`.
- [x] Snap/reset interpolation for explicit teleports, spawn, load, mode entry,
      death, and corrections tagged above the teleport threshold. A step over
      2 m sets `snap`, and `reset()` -- which mode entry, mode exit, focus loss
      and commander death all go through -- requests one for the next tick.
- [x] Remove the camera's independent chase of the raw simulation transform once
      the shared presentation pose exists.

### 1.3 Input/presentation tests

- [x] `OnePhysicalAttackPressProducesOneConsumedAttackEdge`.
- [x] `PressAndReleaseBetweenSimulationTicksIsConsumedOnce`.
- [x] `FocusLossClearsEveryHeldCommanderAction`. The C++ half pins the contract;
      the QML half that calls it is pinned by
      `CommanderInputLayerReleasesEveryHeldInputAtOnce`, because no gtest can
      reach a `Connections` block.
- [x] `MouseLookChangesPresentedCameraWithinOneRenderedFrame` at 30/60/120/144
      presentation Hz over a 60 Hz simulation.
- [x] `BodyAndCameraUseTheSameInterpolatedAnchor` with a screen-space residual
      below 1.5 px at 1920x1080 during steady straight travel. Landed as an
      exact-agreement pair instead of a pixel budget, which is stricter and does
      not need a projection: `TheCameraFramesThePresentedPoseNotTheRawTransform`
      pins the camera to the resolved pose, and
      `ModelMatrixFollowsTheCommanderPresentationPose` pins the body to the same
      resolver. The arena confirms it end to end -- camera anchor against
      rendered root is 0.000000 m across all 540 frames of `rpg_locomotion`.
- [x] `PresentationPoseIsFrameRateInvariant`: after the same scripted second,
      30/60/120/144 Hz presented positions agree within 0.03 m and yaw within
      0.25 degrees.

**Gate 1 passes when:** every input edge has exactly one producer and consumer,
camera look responds within one presented frame, movement/action response occurs
within one simulation tick, and the rendered body no longer steps relative to
its camera anchor.

**Status: green.** All four clauses hold and each is pinned by a test that fails
without its fix.

- One producer, one consumer. `primary_action` is private, the GUI's two direct
  world writes are gone, and `take_input_snapshot()` moves each edge out of the
  producer under a mutex so exactly one tick can see it. Every discard is
  counted: `CommanderInputEdgesAllConsumed` is attached to all twelve `rpg_*`
  scenarios and the gate reports every scripted press consumed once, zero
  dropped.
- Camera look within one presented frame:
  `MouseLookChangesPresentedCameraWithinOneRenderedFrame` at 30, 60, 120 and
  144 Hz over a 60 Hz simulation.
- Movement and action within one simulation tick:
  `LocomotionInputIsHonouredWithinOneSimulationTick` and
  `AttackStartsWithinTheDirectControlBudget`, plus
  `AHeldPressSurvivesTicksTheBodyCannotAct` for the case where the body refuses.
- Body and camera anchor: both resolve the same
  `CommanderPresentationSampleComponent` through one
  `resolve_presentation_pose()`. The arena measures camera anchor against
  rendered root at 0.000000 m over all 540 frames of `rpg_locomotion`.

The one deliberate trade recorded here: the presented pose now lags the
authoritative one by up to a tick, because it interpolates rather than
extrapolates. Body and camera pay it together, which is the point; simulation,
collision and contact are untouched.

## Gate 2: extract and repair the commander motor

Create a focused `CommanderMotor` (name may vary) that is the only normal writer
of commander translation. `CommanderControlController` should orchestrate input
and action requests, not solve geometry.

### 2.1 Motor contract

- [x] Define a motor input: desired planar direction/speed, facing mode, root
      motion request, dodge request, vertical state, and fixed `dt`.
      `CommanderMotorRequest` carries from/to, the named displacement source, an
      airborne flag and `dt`. Facing is not in it: the body yaw contract lives in
      the controller because it depends on combat state, and putting it in the
      motor would have made the motor a combat consumer.
- [x] Define a motor output: authoritative pose, actual velocity, grounded state,
      collision contacts, accepted root motion, and explicit correction/teleport
      flags. `CommanderMotorResult` returns position, velocity, moved, blocked,
      slid and the source. Contact ids and normals are still owed -- the
      underlying query is a walkability point test, which has no contact to
      report.
- [x] Route walk, run, backpedal, strafe, dodge, strike root motion, ability root
      motion, body separation, and landing through the motor. All seven writers
      go through `CommanderMotor::advance()` or `::teleport()`, and
      `OnlyTheMotorTranslatesTheCommander` fails if the controller ever assigns
      `transform->position.x/z` again.
- [x] Normalize diagonal input before speed selection. Already true, and now
      measured: `DiagonalTravelMatchesStraightTravel` holds settled diagonal
      speed within 1% of straight-ahead.
- [x] Use acceleration/deceleration profiles with deterministic time constants;
      preserve responsive first motion while eliminating instant full-speed and
      instant direction reversal. Already true: the 12/16 s^-1 accel/decel pair
      uses the `1 - exp(-rate * dt)` form, which is rate-invariant.
      `TheSamePathIsWalkedAtEverySimulationRate` measures it at 30/60/120 Hz
      and `SettledSpeedMatchesTheConfiguredSpeed` pins the settled speed to 2%.
- [ ] Free mode: rotate the body toward desired movement, independently of camera
      yaw. Locked mode: face the locked target and use radial/tangential movement.
- [x] Add turn-in-place thresholds so looking around while idle does not spin the
      body every tick. The body used to be assigned `m_view_yaw` unconditionally,
      so it followed the camera one to one: a 108 degree look swept the body 108
      degrees on the spot. An idle body now holds its facing until the view is
      50 degrees away, then follows at 260 deg/s; travel turns at 900 deg/s,
      which stays under the 70 degree pelvis-snap ceiling the Arena watches. An
      attack, guard, dodge, jump, bow draw or lock-on still snaps the body to the
      view, so a swing lands where the player is looking --
      `AnAttackFacesTheViewImmediately` pins that.

### 2.2 Person-scale collision and grounding

- [x] Replace destination point/grid rejection with a swept circle/capsule against
      exact person-scale collision geometry. The nav grid may be a broad phase or
      out-of-bounds test; it is not the final body collider. Done for structures,
      which is where both red scenarios lived: `BuildingCollisionRegistry` now
      carries a second, person-scale extent per building type taken from the
      drawn mesh, and every person-scale query -- the commander's body
      collision, the camera's obstruction ray, bow aim and melee line of sight --
      uses it. The navigation footprint and its grid padding are untouched, so
      RTS formations keep their spacing. The swept capsule itself is still owed;
      what is there is an exact rectangle test at the destination.
- [ ] Give collision queries stable surface ids, normals, vertical bounds, and
      material/type. Include registered buildings, authored obstacles, gates,
      trees/boulders/ore that block a body, terrain edges, and map bounds.
- [ ] Perform conservative advancement/substeps for large `dt`, dodge, and root
      motion so no displacement tunnels through thin geometry. **Tried to
      reproduce and could not.** `NoDisplacementTunnelsThroughAThinWall` and
      `ADodgeDoesNotTunnelThroughAThinWall` drive a walk and a dodge into a
      one-cell wall on 250 ms frames -- four times the worst hitch the Arena can
      author -- and nothing crosses. The reason is arithmetic: a 1 m nav cell
      plus a 0.34 m body radius against a 0.94 m walk step, and a dodge whose
      `roll_dt` is clamped to the remaining roll timer. This stays open rather
      than done, because the tests only prove today's numbers; leave them in
      place and re-check if body radius, cell size or dodge speed move.
- [ ] Project remaining velocity onto the contact plane for continuous wall slide;
      solve corners iteratively with a fixed deterministic contact order. Partly
      there and worth knowing why it has not bitten: every collision surface in
      the game today is axis aligned -- nav cells, and building bodies stored as
      world-axis rectangles even when the building is rotated -- so
      `resolve_ground_step`'s axis-aligned slide is exact for all of them.
      `AWallDoesNotResetTheWholeMotor` measures a real slide along one.
      A contact-plane projection is what a non-axis-aligned collider would need,
      and there is not one yet.
- [ ] Use a small skin width and bounded depenetration. Never zero all velocity
      because one axis is blocked.
- [ ] Add stable ground sampling, slope limit, ground snap, crest handling, and a
      step-up/step-down contract. Vertical camera motion must come from this
      grounded pose.
- [ ] Replace all-soldier summed body push with a bounded deterministic separation
      solve. Friendly crowds should yield/fade where appropriate rather than
      repeatedly shove the player and camera.
- [ ] Disable the current synthetic jump until a real vertical motor state passes
      launch/apex/landing, wall, ledge, and no-snap-back tests.

### 2.3 Motor scenarios and numeric gates

Add these deterministic Arena scenarios at 30, 60, and 120 simulation sampling
where supported; compare authoritative results, not wall-clock timing:

- [ ] `rpg_motor_start_stop`: walk/run starts, stops, and rapid reversals.
- [ ] `rpg_motor_diagonal`: all eight input directions; diagonal speed error <= 1%.
- [ ] `rpg_motor_figure_eight`: continuous camera-relative direction changes.
- [x] Repair `rpg_close_quarters`: reach the authored 1.90 m envelope with zero
      penetration and no visual-state oscillation. Green. The cause was not the
      motor: `home` registers a 4.3 x 4.4 m navigation footprint for a building
      drawn 2.36 x 2.42 m, so the commander was stopped 2.54 m from a facade he
      was asked to reach at 1.90 m. The 1.90 m envelope was unreachable, not
      missed. `pose_oscillation` went with it -- the visual state was flipping
      because the commander was walking into an invisible wall.
- [x] Repair `rpg_obstacle_slide`: maintain contact clearance while travelling at
      least the existing 1.50 m along the facade; no zero-speed pulses longer
      than one simulation tick while valid tangential input is held. Green from
      the same fix. Worth noting for whoever picks up the swept-capsule work:
      `motor.slid` was zero in both scenarios and stayed zero, because neither
      was ever a slide failure -- the commander was walking head-on into a
      collider that was not where it appeared to be.
- [ ] `rpg_motor_corner`: enter/leave convex and concave corners without sticking,
      tunneling, alternating normals, or correction oscillation.
- [ ] `rpg_motor_slope_and_crest`: uphill/downhill/sidehill/crest traversal with no
      airborne false positives and no vertical step above 0.04 m per 60 Hz tick
      except an authored step/jump.
- [ ] `rpg_motor_crowd`: pass friendly bodies and withstand enemy pressure without
      a correction above 0.08 m in one tick.
- [ ] Final position after the same 10 s motor script differs by <= 0.03 m across
      supported frame-rate schedules.
- [ ] Actual settled speed differs from configured speed by <= 2%; stopping
      overshoot stays within the authored stop-distance budget.

**Gate 2 passes when:** all displacement sources use the motor, the current
clearance/slide failures are green, fixed inputs produce frame-rate-invariant
paths, and collision traces contain no penetration or unexplained correction.

**Status: all four clauses hold; the section is not finished.**

- All seven writers of commander translation go through `CommanderMotor`, and
  `OnlyTheMotorTranslatesTheCommander` fails if the controller assigns
  `transform->position.x/z` again.
- Both clearance/slide scenarios pass their Gate 2 expectations.
  `rpg_close_quarters` is pinned red again against **gate 4**: fixing the
  clearance let the commander reach a wall for the first time, and the camera
  eye turns out to end up 0.339 m inside the house at the 180 degree yaw snap.
  Its Gate 2 clause is fixed and stays fixed.
- Frame-rate-invariant paths: `TheSamePathIsWalkedAtEverySimulationRate` at
  30/60/120 Hz, within 0.03 m.
- No penetration or unexplained correction: `CommanderMotorCorrectionWithin`
  (0.08 m) rides all twelve scenarios and is green in every one.

What is still owed here, and should not be mistaken for done: ground sampling,
slope limit and the step-up/step-down contract; the synthetic jump; contact ids
and normals; and the six `rpg_motor_*` scenarios. The three tunnelling and
slide items above are annotated with what was measured rather than ticked.

## Gate 3: locomotion quality and pose agreement

- [x] Drive gait blend, phase rate, and footstep timing from the motor's **actual
      presented velocity**, not requested input or a fallback unit speed. The
      commander now publishes `fpv_motion_requested` from the motor's own state
      -- requested speed or a smoothed speed still above the idle floor -- so a
      coasting body keeps its gait instead of flipping to Idle the moment the
      achieved velocity chatters against an obstacle.
- [x] Maintain phase continuity through idle/walk/run/backpedal/strafe transitions.
      The gait was being advanced **twice per frame** -- a second prepare pass
      re-ran the fade with an uninitialised `previous`, wiping the first pass's
      result. A locomotion sample whose `sample_time` has not advanced no longer
      writes persistent state, so preparing a frame twice is a no-op. Presence
      now fades 0.8703 -> 0.7575 -> 0.6592 instead of collapsing to zero, and
      the 0.23 m foot snap at a stop is gone.
- [ ] Add directional locomotion for lock-on strafing/backpedal; do not play a
      forward gait while translating sideways or backward.
- [ ] Add turn-in-place and moving-turn pose contracts for the free/locked facing
      rules from Gate 2.
- [x] Ensure collision-rejected movement decelerates the gait instead of flipping
      walk/idle every tick. `rpg_close_quarters` was flipping visual state five
      times in one second while coasting into a wall: each frame the step was
      alternately blocked and not, so the reported velocity chattered between
      zero and non-zero. It reports one transition on entry and one on exit now,
      and the scenario is green.
- [x] Feed accepted combat root motion to animation and motor from one authored
      curve; the clip root and world root may not independently lunge. They were
      doing exactly that: the motor lunged the commander while the renderer
      applied its own `resolve_combat_root_motion` world offset -- melee lunge
      and hit-reaction stumble -- on top, leaving the drawn body up to 0.2144 m
      from the point the camera framed. For a simulation-owned body the renderer
      keeps the pose half (lean, pitch, squash) and drops the translation.
      `CommanderPresentedPoseAgrees` holds the two to 0.01 m.
- [ ] Repair `rpg_locomotion` and lower the flat-ground planted-foot slide ceiling
      to <= 0.025 m per presented frame after transition settling.
      `rpg_locomotion` itself is green; `rpg_motor_start_stop` is the remaining
      red and it is one measured fact away from a cause. See
      docs/RPG_PLAYABILITY.md: the gait's persistent state arrives at the solver
      holding the **default** value, not a stale one, on the frame after a
      Walk/Idle transition -- so a fresh component is reaching it. Six code
      hypotheses have been tried and reverted; the ceiling should not be lowered
      until that is understood.
- [x] During steady held input, allow at most one locomotion-state transition on
      entry and one on exit; never five changes in one second. Pinned by
      `pose_oscillation` on every `rpg_mode` scenario, green in all of them.
- [ ] Add rendered tests for start/stop, 180-degree reversal, eight-way lock-on,
      wall contact, slope, dodge exit, attack exit, and low-stamina run exit.
- [x] Compare visual root to shared presented pose every frame; unexplained
      position error <= 0.02 m and yaw error <= 0.5 degrees.
      `CommanderPresentedPoseAgrees` enforces both on all fifteen scenarios, at
      0.01 m -- twice as tight as asked -- and 0.5 degrees. The yaw half caught
      an ordering bug in Gate 1.2's own code the moment it was switched on: the
      presentation sample was published before the tick wrote the body yaw, so
      the presented pose reported the previous tick's facing. Under a
      max-rate turn that is exactly 900 deg/s over 60 Hz -- the reported error
      was 15.000 degrees in four scenarios, to three decimal places.

**Gate 3 passes when:** the commander never skates, shuffles in the wrong
direction, pops between idle/walk, or disagrees with the motor in any mandatory
movement scenario.

## Gate 4: rebuild the camera on collision-safe fundamentals

Start from a deliberately plain chase camera. Temporarily disable bob, breathing,
lean, threat bias, focus nudge, impact dolly/FOV, run FOV boost, dodge FOV, and
dodge tilt behind individual switches. Re-enable none of them in this gate.

### 4.1 Plain chase behavior

- [ ] Use one default exploration framing and one lock-on framing. Keep FOV fixed
      during base validation; aiming may use one separately tested FOV.
- [ ] Apply raw look rotation in the current presented frame without spring lag.
      Smooth translation/boom length separately with critically damped behavior.
- [ ] Build the desired camera from the shared presented commander pose, not the
      raw simulation transform.
- [ ] Clamp or subdivide presentation `dt` after a hitch; reset springs on mode
      entry, teleport, load, death, and viewport/aspect rebuild.
- [ ] In free mode, no threat or soft target may rotate the view. In lock mode,
      track the locked target with explicit manual-override and target-switch
      rules; never modify the player's stored free-look yaw to achieve body facing.

### 4.2 Camera collision volume

- [ ] Add a camera-scene query using a configurable sphere/near-plane proxy
      (initial tuning value: 0.20 m radius plus 0.02 m skin), not a ray.
- [ ] Sweep from a safe pivot to the desired eye against 3D building volumes,
      authored obstacles, terrain along the full boom, blocking props, gates, and
      map bounds. Ignore the controlled body deliberately and document every
      other filter.
- [ ] Return contact point, normal, id/type, and safe boom fraction. Unit-test
      edge, corner, roof-height, inside-start, multiple-contact, and grazing cases.
- [ ] Retract immediately enough to prevent penetration; extend with a damped
      release plus hysteresis so alternating edge contacts do not pump the boom.
- [ ] Solve a safe pivot if the commander's head/shoulder is itself close to a
      wall. A valid eye with an invalid pivot is still a clipping bug.
- [ ] Derive the final look target/axis **after** collision resolution so an eye
      retraction cannot create uncommanded aim drift. Assert that the crosshair ray
      is the final camera center ray.
- [ ] Prefer per-body fade/dither for friendly dynamic occluders between lens and
      commander. Do not let bodies push the camera, and do not cull an entire
      formation because one member enters the lens gap.

### 4.3 Camera torture scenarios and gates

- [ ] `rpg_camera_open_ground`: idle, walk, run, strafe, start/stop, and rapid
      mouse sweeps with all optional motion disabled.
- [ ] `rpg_camera_wall_orbit`: 360-degree orbit while commander stands and walks
      beside a long wall.
- [ ] `rpg_camera_convex_corner` and `rpg_camera_concave_corner`.
- [ ] `rpg_camera_doorway`: enter, turn inside, back out, and cross the threshold.
- [ ] `rpg_camera_roof_height`: camera passes above a low footprint without an
      infinite-column false hit and below a roof without clipping.
- [ ] `rpg_camera_terrain_crest`: ridge lies between pivot and desired eye.
- [ ] `rpg_camera_props`: trees, boulders, ore, authored obstacle, gate open/closed.
- [ ] `rpg_camera_crowd`: friendly rank and moving individual bodies cross the
      boom; build on `rpg_escort_crowd` and `rpg_pass_ranks`.
- [ ] `rpg_camera_lock_switch`: target enters/leaves LOS, dies, exits range, and is
      cycled while close to geometry.
- [ ] `rpg_camera_hitch`: inject 33/50/100 ms presentation frames after settling.
- [ ] Across every case, camera-volume penetration depth == 0 and clearance stays
      at or above the configured skin.
- [ ] With no look input and no declared lock/framing transition, uncommanded
      yaw/pitch change stays below 0.05 degrees per frame.
- [ ] During steady unobstructed travel, commander screen-anchor jitter residual
      p99 stays below 1.5 px at 1920x1080.
- [ ] When one obstruction remains active, boom length must not reverse direction
      on alternating frames; after clearing it, extension is monotonic within the
      spring tolerance.
- [ ] No camera teleport, NaN, target/eye inversion, inside-body view, or near-plane
      intersection at 30/60/120/144 presentation schedules.

**Gate 4 passes when:** every torture scenario is green with the plain camera,
captured video contains no clipping/popping/jitter, and three human reviewers can
traverse the camera course for ten minutes without reporting camera interference.
This gate must pass before combat feel work begins.

## Gate 5: Souls-like combat mechanics (one vertical slice first)

Scope the first slice to one on-foot sword commander, one basic sword enemy, and
the verbs move, camera, lock, light attack, guard, and dodge. Temporarily disable
special abilities, rally/aura interactions, camera-mode toggle, finishers, and
other weapon families in the slice. Re-enable them only through the extension
matrix at the end of this gate.

### 5.1 Action and input contract

- [ ] One melee press requests exactly one attack. Remove melee auto-repeat while
      held. Holding has no meaning until a heavy/charge contract is authored.
- [ ] Define an input buffer (initial target 0.15 s) with observable accepted,
      buffered, expired, and refused outcomes. Tune from playtests; do not silently
      drop input.
- [ ] Keep one authored action timeline as the authority for startup, active trace,
      recovery, exit-safe, combo buffer, guard cancel, and dodge cancel windows.
- [ ] Make visible pose acknowledgement occur within one render frame plus one
      simulation tick of an accepted input. Authored startup before contact is
      commitment, not response latency.
- [ ] Make each action's steering and rotation allowance explicit. Do not let raw
      camera yaw rotate a committed body arbitrarily through its strike.
- [ ] Route every action's root motion through `CommanderMotor`; reject/slide it at
      walls and bodies without desynchronizing the clip.

### 5.2 Targeting and lock-on

- [ ] Separate soft melee assist from explicit lock. Soft assist chooses a body
      near the camera-facing attack volume with score hysteresis; it does not steer
      the camera.
- [ ] Explicit lock uses stable per-soldier identity, LOS grace, distance grace,
      death handling, screen-space target cycling, and a manual unlock.
- [ ] Keep the locked target on screen through camera framing, not by overwriting
      free-look yaw. Give manual look input a documented override/switch behavior.
- [ ] Add close-range minimum handling so the camera/body do not spin 180 degrees
      when target and commander overlap.
- [ ] Display refusal reason in diagnostics and minimal HUD state; never convert a
      target miss, stamina refusal, recovery state, or obstruction into a silent
      dead click.

### 5.3 Physical contact and damage

- [ ] Use swept weapon geometry between previous/current authored weapon sockets.
      Make the hurt body used for targeting, trace, reaction, and damage the same
      per-soldier target.
- [ ] Enforce one damage application per target per action unless an action is
      explicitly authored as multi-hit.
- [ ] Make traces frame-rate invariant at 30/60/120 fixed schedules and under a
      deliberately inserted long tick.
- [ ] Require clear structure LOS and motor-valid reach throughout contact; a
      lunge cannot damage through a wall or cross an invalid body position.
- [ ] Align contact time, visible blade position, spark/audio event, health delta,
      hit reaction, and hit confirm to the same resolved event.

### 5.4 Guard, dodge, stamina, and fairness

- [ ] Author guard raise, active guard arc, perfect window, guard release, guard
      break, and rearm on the combat timeline. Publish `PerfectGuard` only when a
      contact resolves as perfect.
- [ ] Author dodge startup, invulnerability interval, movement curve, recovery,
      and cancel permissions from one timeline. Publish `DodgeSuccess` only when
      incoming damage is actually rejected by i-frames.
- [ ] Verify hurtbox and i-frame timing against the rendered roll pose, not only a
      hard-coded timer.
- [ ] Make sprint/attack/guard/dodge stamina costs and regeneration coherent;
      every insufficient-stamina input produces immediate readable refusal.
- [ ] Keep the engagement ring's active attacker budget deterministic. Add an
      off-camera fairness rule and prove a crowd cannot chain unavoidable attacks
      or keep the commander staggered indefinitely.
- [ ] Ordinary guardable, unblockable, perfect-guarded, dodged, behind-the-player,
      simultaneous, and projectile contacts each need explicit tests.

### 5.5 Mandatory combat scenarios

- [ ] `rpg_one_press_one_attack`.
- [ ] Replace `rpg_combo_cadence` with timed individual presses testing early
      buffer, valid buffer, late press, queue expiry, and no held auto-repeat.
- [ ] `rpg_attack_whiff_recovery`: misses still commit and recover correctly.
- [ ] `rpg_attack_wall_contact`: root motion and trace stop at a wall.
- [ ] Repair `rpg_strike_lunge` through motor-driven authored root motion.
- [ ] Repair `rpg_defense_contact`: ordinary block publishes one block and loses
      zero protected health; dodge rejects only contacts inside its authored window.
- [ ] Keep `rpg_projectile_block`, but assert exact guard phase and one outcome.
- [ ] `rpg_guard_edges`: attack arrives one tick before, inside, and one tick after
      the perfect window.
- [ ] `rpg_dodge_edges`: same boundary matrix for i-frames at 30/60/120.
- [ ] `rpg_stamina_refusal_and_recovery`.
- [ ] `rpg_lock_occlusion_death_cycle`.
- [ ] `rpg_duel_ten_minutes`: deterministic repeated exchange with no missed input,
      double damage, stuck state, camera fault, or action/pose disagreement.
- [ ] `rpg_skirmish_three_attackers`: attacker budget, off-camera rule, no permanent
      stun, and guaranteed recovery opportunities.
- [ ] Keep `rpg_melee_contact` but add frame-exact contact/pose/damage assertions.
- [ ] Keep `rpg_bow_volley` outside the first slice; bring bow back only after the
      sword matrix passes, then revalidate the final collision-resolved camera ray.

**Gate 5 passes when:** the sword vertical slice is mechanically complete, every
input and contact has one traceable outcome, the defense failure is repaired,
30/60/120 results agree, and blind testers describe losses as their decision or a
readable enemy action rather than camera, targeting, or input failure.

## Gate 6: satisfying feedback, added one layer at a time

This is intentionally after mechanics. Re-enable or tune only one item at a time
and rerun Gates 1-5 after each addition.

- [ ] Hit reaction and enemy recoil first; contact must read with camera motion
      disabled.
- [ ] Weapon/contact audio second; audio event comes from resolved contact.
- [ ] Very short presentation-local hit pause only if blind A/B tests prefer it.
      Remove the current 0.10-0.18 s near-global freeze. Never pause input sampling
      or camera look.
- [ ] Minimal controller vibration/API event if supported, with accessibility off.
- [ ] Add restrained camera impulse only after collision resolution, with a strict
      amplitude/velocity budget and reduced-motion scale. It may never change the
      authoritative crosshair ray during bow aim.
- [ ] Reconsider run FOV, dodge FOV/tilt, bob, breathing, lean, focus nudge, and
      threat bias independently. Default to leaving out any layer testers do not
      reliably miss in an A/B test.
- [ ] Re-enable hit effects/telegraphs only when they communicate an actual combat
      state and do not hide weapon animation. Animation is the primary tell.
- [ ] Add accessibility gates: reduced camera motion, camera impulse off, head bob
      off, FOV control, sensitivity X/Y, invert Y, hold/toggle guard option, and
      remappable bindings.

**Gate 6 passes when:** feedback improves contact recognition in blind tests
without causing a regression in camera clearance, aim truth, frame pacing, input
latency, or reduced-motion behavior.

## Gate 7: performance, stability, and release hardening

### 7.1 Performance gate

- [ ] Name the reference CPU/GPU, resolution, quality preset, OS, and driver used
      for the 60 fps contract.
- [ ] Prewarm shaders/assets before measuring. Report startup compilation hitches
      separately, then fix or precompile them for the shipped path.
- [ ] On reference hardware at 1080p High: CPU and GPU p95 <= 16.67 ms, p99 <=
      20.0 ms, and no post-prewarm frame > 33.3 ms in any mandatory RPG scenario.
- [ ] Simulation tick p95 must leave enough headroom for presentation/rendering;
      record RPG-only costs for motor, targeting, weapon traces, engagement, and
      camera queries.
- [ ] Run the full gate at 30/60/120 presentation caps and VRR/unlocked where
      available. Behavior is identical; only presentation sampling changes.
- [ ] Fail performance reports on missing GPU timing, incomplete trace, watchdog,
      dropped simulation ticks, or an unmarked warmup sample.

### 7.2 Soak and lifecycle matrix

- [ ] Enter/exit RPG mode 100 times: mouse capture, selection, camera bookmark,
      input state, world render mode, fog, and commander flags restore exactly.
- [ ] Ten-minute runs for open traversal, building torture course, dense friendly
      crowd, duel, three-attacker skirmish, and bow aim.
- [ ] Pause/unpause during every action phase; open/close menu while keys/buttons
      are held; alt-tab/focus loss; resize/aspect change; fullscreen toggle.
- [ ] Save/load in idle, locomotion, lock-on, guard, action recovery, and after
      commander death. Unsupported transient states must normalize explicitly.
- [ ] Death, pending removal, victory/defeat, rally placement, and spectator mode
      cannot leave a live camera/input pointer or stuck cursor.
- [ ] Run ASan/UBSan, TSAN where viable, replay determinism, full tests, content
      validation, and packaged renderer self-test.

### 7.3 Human release gate

- [ ] Use at least three testers who did not implement the current work.
- [ ] Each completes: five-minute movement course, five-minute camera torture
      course, three duels, one three-attacker fight, and one bow encounter.
- [ ] Collect defects under fixed labels: input missed/doubled, movement mismatch,
      collision/stuck, camera clip, camera motion/nausea, target wrong/lost, contact
      wrong, defense unfair, unreadable enemy, frame hitch, other.
- [ ] Zero camera clipping, doubled/missed input, unexplained damage, stuck state,
      or >33 ms post-prewarm hitch reports are release blockers.
- [ ] Require three consecutive full playtest sessions with no blocker before RPG
      mode is presented as playable rather than experimental.

**Gate 7 passes when:** automated gates are all green on a clean build, soak and
lifecycle matrices are clean, and the consecutive human-session criterion is met.

## Feature restoration matrix (only after Gate 7)

Bring features back as vertical slices; each inherits every prior camera,
movement, contact, frame-rate, performance, and lifecycle gate.

- [ ] Sword heavy/charged attack and finisher.
- [ ] Spear movement, targeting, trace widths, and full action set.
- [ ] Bow draw/hold/release, camera aim truth, obstruction, projectile contact.
- [ ] Mounted motor, camera, charge, rider/horse presentation agreement.
- [ ] Shield bash.
- [ ] Vanguard rush through motor/root-motion collision (remove instant transform
      displacement and instant damage behavior).
- [ ] Second Wind.
- [ ] Aura and rally interactions.
- [ ] Close/wide camera options as tuned presets using the same collision solver.
- [ ] Optional camera polish and expanded HUD/effects.

## Definition of playable

RPG mode is playable only when all of the following are true:

- [ ] Gates 0-7 are green on a clean build.
- [ ] All registered mandatory `rpg_*` scenarios complete and pass sequentially.
- [ ] There are zero known P0/P1 input, motor, animation, camera, targeting,
      contact, defense, or frame-pacing bugs.
- [ ] Camera and movement remain correct before, during, and after combat; combat
      code cannot bypass their contracts.
- [ ] Sword duel and three-attacker slice meet the Souls-like input/action/contact
      contract without relying on VFX or camera motion to feel readable.
- [ ] The human release gate passes three consecutive times.
- [ ] Only then may feature/content work resume.
