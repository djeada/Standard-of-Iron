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

- [ ] Create `docs/RPG_PLAYABILITY.md` from the product contract and gates below;
      keep `todo.md` as the live checklist and link the permanent document here.
- [ ] Add `scripts/run-rpg-gates.sh` (and a `make rpg-gate` wrapper) that builds
      required targets, runs unit/integration tests, then runs every required RPG
      Arena case **sequentially** into a fresh artifact directory.
- [ ] Make the gate script print one compact table: scenario, completed, passed,
      issue codes, p50, p95, max, and artifact path; exit nonzero on any failure.
- [ ] Save the initial sequential reports as the comparison baseline outside
      tracked source artifacts; record the commit and build type in `run_config`.
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

- [ ] Pin the five current failures as expected-red tests on a recovery branch:
      close-quarters clearance, obstacle slide, planted-foot stability, deliberate
      combo timing, and protected defense contact.
- [ ] Add a manifest listing all required RPG scenarios so adding a new `rpg_*`
      case without adding it to the gate fails a test.
- [ ] Separate behavioral pass/fail from hardware performance reporting. CI can
      enforce deterministic behavior; frame budgets run on named reference
      hardware with prewarming.
- [ ] Add a strict `completed == true` assertion so watchdog exits cannot produce
      an empty-issue report that looks like a normal behavioral failure.

### 0.2 Add a presentation trace

Add opt-in trace records (Arena diagnostics/profiling only) with a stable schema:

- [ ] Input: event sequence, press/release edge counters, sampled frame index,
      simulation-consumed sequence, move axes, raw look delta, yaw, and pitch.
- [ ] Motor: authoritative previous/current pose, presented pose, desired and
      actual velocity, grounded state, ground normal, collision normal/id/type,
      sweep fraction, depenetration distance, and displacement source
      (walk/dodge/root-motion/body-push/teleport).
- [ ] Animation: selected gait/action, phase, root offset, each planted foot's
      world position, and transition count.
- [ ] Camera: pivot, unconstrained eye/target, resolved eye/target, boom length,
      collision id/type/normal, clearance, framing state, yaw/pitch velocity,
      and the commander's screen-space anchor.
- [ ] Combat: consumed input sequence, queue result/reason, action id and phase,
      authored window, trace start/end, contact target/slot/time, damage result,
      guard result, i-frame result, stamina/posture delta, and hit reaction.
- [ ] Frame pacing: presented-frame dt, simulation ticks consumed, snapshot age,
      interpolation alpha, CPU frame time, GPU frame time, and shader/asset
      prewarm state.

### 0.3 Add metric expectations, not screenshot-only checks

- [ ] Implement reusable Arena expectations for maximum camera penetration,
      minimum camera clearance, camera boom discontinuity, uncommanded angular
      motion, screen-space anchor jitter, presentation-pose disagreement, motor
      correction, speed discontinuity, contact multiplicity, and input-edge
      consumption.
- [ ] Keep captures for human review, but never accept a camera/movement repair
      because two sparse screenshots look plausible.

**Gate 0 passes when:** the gate command reliably reports the current five
failures, traces contain enough data to identify the responsible stage, repeated
runs produce the same behavioral verdict, and a watchdog/incomplete run is
unambiguously red.

## Gate 1: one authoritative input and presentation path

### 1.1 Input ownership

- [ ] Remove the direct world attack call from
      `CommanderViewModel::primary_action_down()`. The GUI layer only records an
      input edge; simulation is the sole action consumer.
- [ ] Replace one-frame booleans for dodge/jump/attack presses with monotonic edge
      sequence numbers or a bounded event queue so press+release between ticks is
      not lost.
- [ ] Make `CommanderFrameIntent` (or a renamed `CommanderInputSnapshot`) the
      authoritative render-to-simulation packet. Consume each edge exactly once;
      copy held state every tick.
- [ ] Keep raw mouse accumulation on the presentation path so camera response is
      not delayed by the simulation tick. Publish the resulting view angles to
      simulation as intent; never have simulation and presentation both integrate
      the same raw delta.
- [ ] Reset all held inputs and edge queues on focus loss, mouse-capture loss,
      menu open, mode exit, commander death, loading, and window deactivation.
- [ ] Remove the second interactive input implementation in the Arena or route it
      through the same input packet API as the shipped application.

### 1.2 Shared presented commander pose

- [ ] Store previous/current authoritative commander pose plus tick timestamps in
      the render snapshot.
- [ ] Resolve one `CommanderPresentationPose` per rendered frame from those
      samples and the simulation clock. Feed that exact pose to both humanoid
      submission and `CommanderCameraRig`.
- [ ] Use interpolation under normal cadence and tightly bounded extrapolation
      only when the next 60 Hz snapshot is late. Never extrapolate combat contact
      or feed a presentation pose back into simulation.
- [ ] Snap/reset interpolation for explicit teleports, spawn, load, mode entry,
      death, and corrections tagged above the teleport threshold.
- [ ] Remove the camera's independent chase of the raw simulation transform once
      the shared presentation pose exists.

### 1.3 Input/presentation tests

- [ ] `OnePhysicalAttackPressProducesOneConsumedAttackEdge`.
- [ ] `PressAndReleaseBetweenSimulationTicksIsConsumedOnce`.
- [ ] `FocusLossClearsEveryHeldCommanderAction`.
- [ ] `MouseLookChangesPresentedCameraWithinOneRenderedFrame` at 30/60/120/144
      presentation Hz over a 60 Hz simulation.
- [ ] `BodyAndCameraUseTheSameInterpolatedAnchor` with a screen-space residual
      below 1.5 px at 1920x1080 during steady straight travel.
- [ ] `PresentationPoseIsFrameRateInvariant`: after the same scripted second,
      30/60/120/144 Hz presented positions agree within 0.03 m and yaw within
      0.25 degrees.

**Gate 1 passes when:** every input edge has exactly one producer and consumer,
camera look responds within one presented frame, movement/action response occurs
within one simulation tick, and the rendered body no longer steps relative to
its camera anchor.

## Gate 2: extract and repair the commander motor

Create a focused `CommanderMotor` (name may vary) that is the only normal writer
of commander translation. `CommanderControlController` should orchestrate input
and action requests, not solve geometry.

### 2.1 Motor contract

- [ ] Define a motor input: desired planar direction/speed, facing mode, root
      motion request, dodge request, vertical state, and fixed `dt`.
- [ ] Define a motor output: authoritative pose, actual velocity, grounded state,
      collision contacts, accepted root motion, and explicit correction/teleport
      flags.
- [ ] Route walk, run, backpedal, strafe, dodge, strike root motion, ability root
      motion, body separation, and landing through the motor.
- [ ] Normalize diagonal input before speed selection.
- [ ] Use acceleration/deceleration profiles with deterministic time constants;
      preserve responsive first motion while eliminating instant full-speed and
      instant direction reversal.
- [ ] Free mode: rotate the body toward desired movement, independently of camera
      yaw. Locked mode: face the locked target and use radial/tangential movement.
- [ ] Add turn-in-place thresholds so looking around while idle does not spin the
      body every tick.

### 2.2 Person-scale collision and grounding

- [ ] Replace destination point/grid rejection with a swept circle/capsule against
      exact person-scale collision geometry. The nav grid may be a broad phase or
      out-of-bounds test; it is not the final body collider.
- [ ] Give collision queries stable surface ids, normals, vertical bounds, and
      material/type. Include registered buildings, authored obstacles, gates,
      trees/boulders/ore that block a body, terrain edges, and map bounds.
- [ ] Perform conservative advancement/substeps for large `dt`, dodge, and root
      motion so no displacement tunnels through thin geometry.
- [ ] Project remaining velocity onto the contact plane for continuous wall slide;
      solve corners iteratively with a fixed deterministic contact order.
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
- [ ] Repair `rpg_close_quarters`: reach the authored 1.90 m envelope with zero
      penetration and no visual-state oscillation.
- [ ] Repair `rpg_obstacle_slide`: maintain contact clearance while travelling at
      least the existing 1.50 m along the facade; no zero-speed pulses longer
      than one simulation tick while valid tangential input is held.
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

## Gate 3: locomotion quality and pose agreement

- [ ] Drive gait blend, phase rate, and footstep timing from the motor's **actual
      presented velocity**, not requested input or a fallback unit speed.
- [ ] Maintain phase continuity through idle/walk/run/backpedal/strafe transitions.
- [ ] Add directional locomotion for lock-on strafing/backpedal; do not play a
      forward gait while translating sideways or backward.
- [ ] Add turn-in-place and moving-turn pose contracts for the free/locked facing
      rules from Gate 2.
- [ ] Ensure collision-rejected movement decelerates the gait instead of flipping
      walk/idle every tick.
- [ ] Feed accepted combat root motion to animation and motor from one authored
      curve; the clip root and world root may not independently lunge.
- [ ] Repair `rpg_locomotion` and lower the flat-ground planted-foot slide ceiling
      to <= 0.025 m per presented frame after transition settling.
- [ ] During steady held input, allow at most one locomotion-state transition on
      entry and one on exit; never five changes in one second.
- [ ] Add rendered tests for start/stop, 180-degree reversal, eight-way lock-on,
      wall contact, slope, dodge exit, attack exit, and low-stamina run exit.
- [ ] Compare visual root to shared presented pose every frame; unexplained
      position error <= 0.02 m and yaw error <= 0.5 degrees.

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

---

# Simulation performance: the review, checked against the code and the profiler

The incoming review proposed nine changes to the ECS and simulation, ranked by
expected value, and asked for a headless benchmark "immediately, because without
those numbers 'optimized' is mostly guesswork". That last point is the one that
mattered. The benchmark already existed; running it reordered almost everything
else on the list.

Everything below is measured on `build/bin/sim_benchmark`, two armies deployed as
battle lines, fixed tick, RelWithDebInfo-equivalent Release flags.

---

## What the profiler said

At 5,000 units the per-system table is not close:

```
CombatSystem          1 647 300 us/tick      ← 99.4% of the tick
MovementSystem            9 147 us
LocalAvoidanceSystem      5 323 us
EngagementSlotSystem        801 us
CommanderSystem             448 us
AISystem                    160 us
...everything else together under 1 ms
```

`perf` inside that:

```
62.2%  FormationCombat::resolve_contact_context
 3.7%  FormationCombat::resolve_layout
 2.5%  spawn_typeToTroopType
 2.0%  malloc
 1.9%  UnitLayoutLibrary::find
 1.7%  resolve_definition
```

So the whole list below has to be read against one fact: **the simulation is
formation contact geometry, and everything else is rounding error.** Items 1, 4,
6, 8, 9 and 10 of the review are optimisations of code that together accounts
for under 1% of a tick.

---

## The review, item by item

| #   | Proposal                                                               | Status                                                                                                                                                                                                                                                                                              |
| --- | ---------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1   | Zero-allocation multi-component ECS iteration (`world.each<A,B,C>`)    | **Already built.** `World::view<...>` / `World::each<...>` exist, allocate nothing, pick the smallest dense storage as the source and resolve the rest through sparse `try_get`. `get_entities_with<T>()` no longer exists. `MovementSystem` drives its loop from `world->each<MovementComponent>`. |
| 2   | Remove the global recursive mutex from the tick                        | **Already mitigated.** `World::update()` takes the registry lock once; `ScopedRegistryLock` has a reentrancy fast path, so every nested view or emplace inside the tick is one relaxed atomic load and a compare, not a lock.                                                                       |
| 3   | Shared/group pathfinding (priority 2)                                  | **Not attempted, and not indicated.** `MovementSystem` is 9 ms of a 1 670 ms tick. Worth revisiting only after combat stops dominating.                                                                                                                                                             |
| 4   | Cache navigation state, stop re-querying walkability per unit per tick | **Not attempted, same reason.** The redundant walkability checks are real; they are 0.5% of the tick.                                                                                                                                                                                               |
| 5   | Cache formation geometry instead of recomputing it per tick            | **Already built** for the turn radius — `formation_turn_radius` reads a signature-validated layout cache. The review was right about the _shape_ of the problem and pointed at the wrong function; see below.                                                                                       |
| 6   | Different update frequencies per system                                | **Partly, ad hoc.** `AISystem` and `HomeSystem` carry their own intervals; there is no framework-level cadence on `System`. A shared mechanism is a reasonable future change, but the systems it would slow down cost under 1 ms combined.                                                          |
| 7   | A spatial query layer used everywhere                                  | **Already built.** `Engine::Core::WorldSpatialIndex`, uniform grid, `query_radius` / `for_each_in_radius`. The profiler counts it: 985 spatial queries in the last tick.                                                                                                                            |
| 8   | Animation/render LOD                                                   | Covered separately in `docs/RENDERING_ARCHITECTURE.md`; the previous round made LOD selection screen-size relative. High and Ultra deliberately evaluate no LOD at all.                                                                                                                             |
| 9   | SIMD/SoA, fast trig, micro-optimisation                                | Correctly ranked last, and still last.                                                                                                                                                                                                                                                              |
| —   | "Add a headless performance benchmark immediately"                     | **Already built:** `tools/sim_benchmark`, 1k/5k/10k units, per-system table, per-call-site attribution of materialising scans, peak RSS and a world digest.                                                                                                                                         |

---

## What was actually changed

Two defects in `resolve_contact_context`, both invisible from a file listing and
both obvious in a profile:

**It scanned every slot pair twice.** One nested loop found the nearest pair of
bodies (the surface gap); a second nested loop over exactly the same pairs found
the contact distance along the axis. Now one fused pass answers both.

**It returned two `FormationLayout` objects by value** — three vectors each — and
`contact_geometry()`, which is what six of the eight call sites use, threw both
away. That is six vector copies per query, and there are about 179 000 queries
per tick at 2 000 units. `resolve_layout_entry` now hands out a pointer to the
cached layout. `resolve_layout` is unchanged for callers that want a copy.

Measured, `--no-systems`, same machine, interleaved, re-baselined against main
at `0e5e1a48` (the numbers below are the post-merge measurement; the pre-merge
run gave the same result to within a percent):

| Scenario               | Baseline mean |            Now | Baseline p95 |            Now |
| ---------------------- | ------------: | -------------: | -----------: | -------------: |
| 2 000 units, 240 ticks |      290.5 ms |   **240.7 ms** |     335.2 ms |   **276.9 ms** |
| 5 000 units, 60 ticks  |    1 698.3 ms | **1 397.0 ms** |   1 723.4 ms | **1 421.0 ms** |

About 17% off the entire simulation, in the one system that had 99% of it.

---

## Two changes that were tried, measured and reverted

**Memoising the contact geometry per (attacker, target) pair.** The same pair is
queried around 59 times over a run, which reads as a textbook memoisation. The
instrumented hit rate says otherwise: of 10 765 133 lookups, 176 915 hit, 180 954
found nothing, and **10 407 264 found the entry and rejected it as stale**. Each
pair is asked about roughly once per tick, and by the next tick the units have
moved. Removed. The lesson generalises: a high repeat count across a run says
nothing about whether a cache can hit.

**Hashing `health > 0` instead of the exact health into `layout_signature`.** The
layout reads health exactly once, as `health > 0` for a rigid body, and never
reads `max_health`, so hashing the values looked like pure over-invalidation —
every point of damage rebuilding three vectors of slots during a melee. It
measured as noise and it broke ten navigation tests. The layout cache is keyed by
`const Entity*` and validated only by that signature, so two `World` instances in
one process can place an entity at the same address with the same id; the exact
health value was accidentally the thing keeping them apart. Reverted. Narrowing
that signature requires fixing the cache key first.

---

## The measurement trap this work fell into

`sim_benchmark` prints a world digest, and it is tempting to treat it as a
"did my refactor change behaviour" oracle. It is not one. Release builds use
`-ffast-math`, so re-associating or re-inlining a hot float expression moves the
last bit of a distance, and a battle diverges from that within a few hundred
ticks.

- Identical digests across two runs of **one** binary are the determinism
  contract. `scripts/check-replay-determinism.sh` enforces the real one by
  record-and-replay.
- Different digests across two **builds** are evidence of nothing on their own.

The rewrite above was proved equivalent by a compile-gated pass that ran the old
and new loops side by side on live data: `surface_gap` was bit-identical in all
37 154 788 comparisons, and `contact_center_distance` differed by one ULP in
0.42% of them — the signature of fast-math code motion, not of an algorithm
change.

Written up in `docs/ARCHITECTURE.md` under _Measuring the simulation_.

---

## Where the remaining time goes

At 2 000 units the combat system still issues about **179 000 exact formation
contact queries per tick — roughly 90 per unit** — each scanning ~550 slot pairs.
That is the real structural cost, and it is a query-volume problem, not a
per-query one. The next move is a broad phase: reject candidate pairs on bounding
circles before asking for exact slot geometry, so the all-pairs scan only runs
for formations that can actually touch. That is the review's item 7 applied where
it pays, rather than where the grid already exists.

Only after that do items 1, 3, 4 and 6 become measurable.

---

## Unrelated bug found and fixed along the way

The selected-unit HUD flickered between empty and full health/stamina during
fights. The values were never wrong — a headless probe
(`tests/headless/selected_unit_readout_test.cpp`) watches exactly what the HUD
reads, every tick, through a real battle, and never sees a dip that recovers.

`IronProgressBar` had its `Behavior` on the fill's **width**. Width changes both
when the value moves and when the bar is laid out, and a delegate created inside
a zero-width parent gets its first real width as a _change_, which is the case a
`Behavior` animates. Since `grouped_by_type()` returns a new array on every
refresh, and a new array on a `Repeater` model rebuilds every delegate, both bars
were recreated and re-swept continuously while a fight churned the selection.

The bar now eases a `real animatedPosition` bound to `visualPosition` and lets
width follow it: layout is immediate, a genuine value change still eases.
`tst_component_library.qml` pins both halves. Written up in
`docs/UI_DESIGN_SYSTEM.md`.
