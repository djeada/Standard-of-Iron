# Commander RPG feel: implementation and verification plan

Status: implementation in progress; no full milestone below is complete. Updated 2026-11-14.

The goal is a coherent third-person action experience across movement, jumping,
traversal, animation, fighting, receiving damage, camera control, and switching
between RTS and commander control. Dark Souls is the quality reference for
readability, responsiveness, commitment, and consistency. This document does not
claim that we have measured or reproduced its implementation.

The previous camera patch is a candidate improvement, not proof of RPG quality.
Its 260 passing commander-related tests and short gameplay captures do not cover
sustained combat, traversal, extreme camera angles against terrain, or real-time
frame pacing. Revalidate that patch as part of this plan; revise it if necessary.

## Implemented slices

Slice 1 is documented in [docs/RPG_FEEL_PROGRESS.md](docs/RPG_FEEL_PROGRESS.md).
Local before/after evidence lives in `artifacts/rpg-feel/locomotion-01/`.

- [x] Reproduce and fix reversed left/right movement when acquiring lock-on;
      verify all axis combinations at six headings.
- [x] Require full stamina before starting a jump or dodge; test zero, just below,
      exact and above cost, repeated requests, and side effects on refused dodges.
- [x] Publish accepted motor velocity during a blocked dodge rather than requested
      roll speed; verify displacement, movement component and locomotion state agree.
- [x] Add the previously omitted motor test suite to the RPG gate.
- [x] Add a tracked, repeatable movement/combat playback route and a local baseline.
      This is a focused route, not the complete water/bridge/slope test course.

Slice 2 is documented in the same file; evidence lives in `artifacts/rpg-feel/jump-01/`.

- [x] Airborne motion resolves against the same collision as walking. Jumps no
      longer pass through walls or buildings, and the last-safe-position teleport
      on landing is removed. Jumping clears no obstacle; this is a documented decision.
- [x] Air steering commits to takeoff momentum (4 m/s² limit, no airborne braking).
- [x] Add `CommanderJumpLandsClean` with synthetic pass/fail cases and the
      `rpg_jump_into_facade` scenario (3 repeats). It fails on the old motor.

Slice 3 preserves combat resources across control-mode handoffs; implementation
and 270 commander tests pass. Evidence: `artifacts/rpg-feel/mode-resources-01/`.
Rendered before/after playback, 73 Arena checks, 174 combat-mode tests, and all
34 RPG behavioral scenarios pass. Performance budgets still fail; see the evidence
log. This does not close the full lifecycle or performance milestones.

- [x] Preserve spent stamina and regeneration delay on entry, including first entry
      with a smaller pool; verify recovery using the real stamina system.
- [x] Preserve guard-break penalties across exits and assign their countdown to
      `CommanderSystem` in both modes, with controller-side double ticking removed.
- [x] Verify 100 handoffs preserve health, posture, stamina and ability cooldown;
      include the new resource suite in the RPG gate.
- [x] Complete the 17-action real-time mode-switch route; fix missing selection
      telemetry and verify required coverage passes without weakening the check.

Slice 4 applies shared ford rules to direct control. Evidence and limitations:
`artifacts/rpg-feel/wading-01/`, `docs/RPG_FEEL_PROGRESS.md`.

- [x] Reproduce ignored water/chill slowdown; apply shared movement modifiers and
      suppress sprint in a ford, restoring held sprint on dry ground.
- [x] Scale dodge/recovery travel in water without changing action timing/cost;
      remove the roll's unintended sprint-stamina request.
- [x] Verify ford crossing, dry bridge support and blocked deep water at
      30/60/120 simulation Hz. App/terrain tests: 310 passed; shared ford tests: 10 passed.
- [x] Add a focused playable ford/bridge/hill map and repeatable sprint route.
      Full terrain acceptance remains open, including reverse/diagonal bridge entries.

Slice 5 adds a jump/dodge input buffer. Initially written unbuilt; the combined
unit tests now pass. Rendered integration is recorded in `docs/RPG_FEEL_PROGRESS.md`.

- [x] Hold a jump or dodge pressed in the last 0.20 s of a jump or roll recovery
      and resolve it on the first free tick; verify every edge is consumed or refused
      with the two trace tests (included in the combined commander suite).
- [ ] Play repeated buffered jumps/rolls under combat pressure and review timing.

Slice 6 is reset and handoff hygiene; the combined unit tests now pass.

- [x] Adopt body event counters after a reset; clear stale dive state on handoff;
      single handoff-clear helpers; complete controller reset. Commander suites pass;
      full lifecycle/save/replay acceptance remains below.
- [x] Move ability cooldowns, posture and punish-window decay to `CommanderSystem`
      in both modes, as guard-break recovery already is (slice 9). This also closes
      AI commanders' punish windows, which nothing used to run down.

Slice 7 changes the body rules toward the Souls reference. Integration caught
a twisting backward roll; takeoff facing is now retained during the roll. The
free-movement and shaped-speed changes remain. See the integration evidence.

- [x] Free movement faces travel at full speed; strafing only when squared up
      (attack, guard, bow draw, lock). Idle body no longer follows the camera.
- [x] Front-loaded roll with unchanged distance, duration and i-frames; verify
      dry/wet travel across fixed and irregular timesteps. Keep takeoff facing during
      the roll to avoid twisting the rendered somersault.
- [x] Hit stagger blocks movement, guard, roll and jump for its duration
      (`AStaggeredBodyCannotRollOrJumpOutOfTheHit`); stagger length against enemy
      attack cadence still needs play review.
- [x] Strike target assist turns the body (slice 11): a wind-up bend within the
      authored cone and turn limit, applied by `CommanderBodyFacing` only; the lunge
      no longer slides toward targets outside the swing's cone.

Combined validation for slices 4–7: **317 commander/terrain tests, 73 Arena checks,
174 combat-mode tests and all 34 RPG behavioral scenarios pass** after correcting
the twisting backward roll. Final gate reports have no issue codes. Evidence:
`artifacts/rpg-feel/integration-07/`; see `docs/RPG_FEEL_PROGRESS.md` for the interrupted
baseline, capture timeout, exact builds and manual gameplay notes. The permanent
gate now also includes body-facing, input-port, look and shared-traversal tests.

- [x] Repair the capture-enabled Arena watchdog failure reproduced by
      `rpg_defense_contact --capture-interval 0.05`. Captures were taken on a
      wall-clock timer that starved the fixed-step simulation; they now sample the
      scenario clock after a rendered frame, and the watchdog does not count capture
      time. The captured run passes with 65 frames (`artifacts/rpg-feel/capture-13/`).

Slice 8 is animation overlay polish. Built and run 2026-11-14: its faster overlay
timing snapped and spun the hands (53 motion-quality violations, the PR's CI
failure) and was corrected; see the progress log.

- [x] Validate loop-boundary continuity and time-based overlay fades. Loop wrap,
      seek/cull and preview purity kept; weights are rate-limited at 2.1/s again.
- [x] Review faster combat-pose takeover: rejected (one-frame arm snaps). Only
      water/carry overlays leave quickly (0.12 s) when a full-body pose takes over.
- [ ] Play gait-synchronised wading and guard/release in water; check clip changes,
      cull/re-entry and replay seeks. Prior green gates do not cover slice 8.

Slices 9–12 (2026-11-14) are in the progress log: one world-time owner for
combat timers, perfect guard grants a riposte instead of exposing the defender,
strike assist with a single yaw writer, and rally handoffs without a camera cut.

- [x] Slice 9: combat timers ticked once, in either mode; punish windows survive
      handoffs like guard-break recovery.
- [x] Slice 10: perfect guard opens a one-hit riposte on the turned attacker.
- [x] Slice 12: rally placement/confirm/restore keep the camera rig.
- [ ] Play perfect guard → riposte and off-axis swings against a duelist and
      three attackers; check that hit rates still feel fair.

These changes contribute to milestones 0, 1, 2, 3, 4, 6 and 8. Full traversal, jump physics,
animation quality, timing-harness work and acceptance playtesting remain open.

## Completion rules

- Every milestone needs a reproducible defect or baseline, a bounded code change,
  automated behavioral checks, rendered evidence, and a real-time play session.
- Checkboxes require evidence paths, the tested revision and dirty diff, hardware,
  settings, and outstanding failures. A passing test count alone is insufficient.
- Distinguish simulation correctness, visual continuity, player feel, and
  performance. A green result on one axis cannot excuse a failure on another.
- Preserve existing gate thresholds unless a documented design change justifies
  changing them. Do not update expected values merely to match an implementation.
- Each fix stays within its owning subsystem. Integrate and play after each slice;
  do not postpone gameplay verification until a large rewrite is finished.
- All numerical budgets below are proposed project acceptance targets, not
  measurements of Dark Souls. Establish baselines first, then document tuning.

## 0. Establish an honest baseline and a playable test course — P0

- [ ] Reproduce and record each reported problem in the actual game: start/stop,
      turn/reverse, strafe, sprint, jump/land, attack/retreat, take a hit, dodge, guard,
      enter water, cross a bridge, climb/descend a hill, and enter/leave commander mode.
- [ ] Create a compact course using production geometry: flat ground, uneven
      terrain, a slope crest, a steep bank, shallow/deep water, bridge approaches and
      deck, stairs, a narrow doorway, wall corners, props, and friendly formations.
      Include a stationary training target, a duelist, three attackers, and an archer.
- [ ] Author a repeatable route through this course and a separate unscripted
      checklist. Preserve the map, fixtures, seeds, starting positions, and equipment.
- [ ] Record baseline video and traces for the same route before further changes.
      Review ordinary-speed motion as well as slow motion; screenshots cannot prove
      temporal quality. Log symptoms with timestamps and suspected owners.
- [ ] Establish a reference rubric: character weight, input response, turning,
      foot contact, jump arc, landing, attack commitment, contact clarity, hit reaction,
      camera stability, and spatial readability. Compare equivalent actions with a
      named Dark Souls reference/version if footage or a playable copy is available;
      record the source and distinguish observed behavior from design choices.
- [ ] Fix the verification harness before using cross-FPS results as evidence:
      `docs/RPG_PLAYABILITY.md` notes that Arena `--fps` changes simulation timestep.
      Add independent simulation and presentation clocks/input schedules so a fixed
      60 Hz simulation can be presented at 30/60/72/100/120/144/165 Hz.
- [ ] Extend existing traces only where needed: input timestamp/sequence, action
      acceptance/refusal, accepted movement, support surface and height, vertical
      velocity, animation sample/phase, contact ID, damage outcome, and frame timings.
      Every new metric needs a passing synthetic case and an intentionally failing one.

Exit: the route is reproducible, every symptom has an owner, and the harness can
distinguish simulation movement, render jitter, camera motion, and frame stalls.

## 1. Define ownership and clean the architecture — P0

Audit existing owners before moving code. Reuse the contracts in
`docs/RPG_PLAYABILITY.md`; do not build a second movement or combat engine.

| Responsibility   | Boundary to enforce                                                                                            | Existing starting points                                                                                 |
| ---------------- | -------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------- |
| Physical input   | GUI records held state and edges; tick consumes each edge once                                                 | `app/commander/commander_input_port.*`, `commander_look.*`, `CommanderInputSnapshot`                     |
| Action decisions | Simulation owns action phase, stamina, buffers, cancels and refusals                                           | `commander_control_controller.*`, `commander_strike.*`, `commander_defence.*`, combat action definitions |
| Body movement    | One motor accepts displacement from locomotion, jump, dodge, lunge and reactions; shared collision resolves it | `commander_motor.*`, `commander_locomotion.*`, `game/systems/movement/`, shared body contact             |
| Traversal        | One surface query resolves ground/deck/water/wall support and permitted transitions                            | `game/map/terrain_service.*`, `terrain_water_bridges.cpp`, `commander_wall_deck.*`                       |
| Combat truth     | Simulation contact and damage own health changes; one event identifies each outcome                            | `game/systems/rpg_combat_system/`, weapon traces and combat mode routing                                 |
| Animation        | Consumes accepted movement and action phases; never independently translates the authoritative body            | `animation/`, `game/core/world_motion_presentation.*`, creature rendering                                |
| Presentation     | Resolve one body sample per presented frame; renderer and camera consume that sample                           | `commander_presentation.*`, `game/core/component_presentation.h`, `presentation_clock.h`                 |
| Camera           | Owns view/framing/collision state; cannot write movement or combat truth                                       | `commander_camera_rig.*`, `game/systems/camera_obstruction.*`                                            |
| Mode lifecycle   | Transfer control explicitly; maintain entity, health, action and traversal state                               | `commander_mode_coordinator.*`, RTS camera bookmark, combat mode routing                                 |

- [ ] Inventory every writer of transform, yaw, vertical offset, action phase,
      target, stamina and health. Trace execution order and eliminate competing writers.
- [ ] Document precedence for movement requests: death, hit reaction, committed
      action/root motion, dodge, airborne control, and ordinary locomotion. Resolve
      conflicts once instead of adding special-case overrides in multiple stages.
- [ ] Define a traversal result with support identity/type, height/normal, grounded
      state, water depth, and blocked/slide reason; adapt the existing result types.
      Do not infer bridge membership from height alone or from the closest surface.
- [ ] Separate stored player look intent, explicit lock direction, body facing,
      and final camera orientation. The documented lock-on write into free-look yaw
      must be investigated and removed through a narrow migration.
- [ ] Keep tuning values in named, validated configuration with units and owners.
      Share authored timing across simulation and animation; remove duplicated magic
      values, including camera rest pitch, as touched subsystems are cleaned up.
- [ ] Keep session/world dependencies explicit. Add assertions at ownership
      boundaries; avoid introducing global fallbacks, giant controllers, per-frame
      allocations, or generic frameworks that do not serve a current requirement.
- [ ] Prefer tests of observable behavior over source-text matching. Replace
      fragile touched checks with behavioral tests, retaining actual architecture
      invariants where they are useful.

Exit: a short dependency/ownership diagram, documented update order, and tests
show one authoritative writer per state. Refactor incrementally alongside the
following slices, with no unrelated repository-wide rewrite.

## 2. Ground movement and turning — P0

- [ ] Tune acceleration, braking, reversals and sprint transitions together with
      animation. Stops must be responsive without a pose snap; reversing must not
      preserve full velocity while instantly flipping direction.
- [ ] Specify free movement versus locked movement: camera-relative input,
      direction of travel, body facing, strafing/backpedal, turn-in-place, and attack
      facing. Avoid rotating the whole body toward the camera on every idle frame.
- [ ] Normalize diagonal input. Base gait and stride progression on accepted
      movement, so pushing a wall cannot play a full-speed run in place.
- [ ] Resolve wall sliding, head-on props, doorways and friendly traffic without
      alternating corrections or accumulating hidden velocity. Check uphill and
      downhill speed against an explicit surface-speed policy.
- [ ] Validate 100 repeated starts/stops, all eight directions, rapid reversals,
      figure eights, a full camera orbit while moving, and sprint release at a wall.

Verification: extend motor/presentation/shared-traversal tests and reuse
`rpg_motor_start_stop`, `rpg_motor_diagonal`, `rpg_motor_figure_eight`,
`rpg_obstacle_slide`, and the `rpg_friendly_*` scenarios. At fixed simulation rate,
identical tick-indexed input must produce identical simulation outcomes regardless
of render rate. Proposed settled diagonal-speed error: <=1%. Camera/render body
sample disagreement: <=0.01 m. Preserve existing friendly-progress/correction
budgets. Inspect planted feet and body facing in real-time video.

## 3. Jumping, falling and landing — P0

- [ ] Audit the current timed jump/height-offset and safe-position recovery paths.
      Define takeoff, ascent, apex, descent, landing and falling as explicit states.
      A cosmetic vertical offset must not disagree with collision or damage position.
- [ ] Specify jump eligibility, input buffering, optional coyote time, air steering,
      stamina cost, obstacle clearance, fall damage and landing recovery. Give each
      rule an authored value or an explicit decision that it is unsupported.
- [ ] Resolve ceiling contact, side contact, walking off an edge, landing above or
      below takeoff, and interrupted jumps using the motor and support queries.
      Ordinary landing must not teleport the player to a last-safe position.
- [ ] Align takeoff/airborne/landing poses with actual vertical motion. Choose
      landing strength from impact speed/drop, not from the elapsed jump timer alone.
- [ ] Test standing/running jumps, repeated jump presses, jumping uphill/downhill,
      landing on a bridge, jumping toward water, overhead obstruction, dodge/attack
      during a jump, taking a hit in the air, and switching modes while airborne.

Verification: deterministic tests of trajectory, collision and action permissions;
rendered checks at takeoff/apex/contact; one landing event per landing; no airborne
ground snapping, double jump, repeated landing damage, or lost grounded state.
Add dedicated Arena scenarios rather than relying only on locomotion tests.

## 4. Terrain, water, bridges and stairs — P0

- [ ] Define shallow-water entry, depth-dependent speed, allowed actions, exit,
      and deep-water behavior. If swimming is unsupported, make the boundary explicit
      and consistent; do not let the character walk under water or block on dry ground.
- [ ] Resolve support using the body's previous surface and reachable transitions.
      At identical X/Z, distinguish a bridge deck from the riverbed below it.
- [ ] Test bridge entry/exit in both directions, diagonally and at sprint speed;
      railings, deck edges, arches/underside where accessible, narrow approaches,
      and jumping/attacking/dodging on the deck. No drop to riverbed or snap onto a
      bridge from underneath. Cover damaged/destroyed supports if supported by gameplay.
- [ ] Define walkable slope/step limits and steep-slope behavior. Test hill crests,
      downhill transitions, uneven triangle seams, stairs, ramps and wall walks.
      Character feet, capsule/support, shadow and camera must agree about elevation.
- [ ] Provide proper stair/climb presentation where needed; current documentation
      says stairs/ladders use the ordinary walk cycle. Do not hide missing clips with
      extra camera smoothing.
- [ ] Cross each surface transition in RTS and direct control, including switching
      modes on the boundary. Share passability rules while preserving intended
      differences between formation navigation and person-scale collision.

Verification: reuse `commander_shared_traversal_test`, hill/gate/fortress headless
tests, bridge coverage/crossing tests, and bridge-water render tests. Add a commander
surface-transition matrix with support IDs and heights in the trace. Run each
boundary forward/reverse, standing/moving, grounded/airborne, with enemies present.
No support oscillation, visible hovering, water/deck confusion, or uncontrolled
position recovery. Water effects and footstep sounds must match the actual surface.

## 5. Animation and body presentation — P0

- [ ] Audit idle, starts/stops, walk/run, backpedal, lateral/diagonal movement,
      turn-in-place, takeoff/landing, dodge, guard, attacks, hit reactions and death.
      Catalogue missing clips separately from blending and timing defects.
- [ ] Complete the directional coverage needed by the movement design. The current
      documented four lateral clips are not evidence of a complete eight-way set.
- [ ] Derive locomotion from accepted speed/direction and synchronize stride phase.
      Preserve planted-foot contact on slopes and through blend transitions; measure
      sliding during movement, not only during a stop.
- [ ] Fix idle/action seams, upper/lower-body layering, weapon grip, foot placement,
      shield placement and interrupted poses. The documented sword arm discontinuity
      must have a reproducible transition test.
- [ ] Apply root displacement exactly once through the motor. Animation may show
      weight and recoil but cannot create a second lunge or move the visible hurt body
      away from simulation contact geometry.
- [ ] Verify every supported commander weapon set and body rig, including near
      camera views, low/high graphics, and repeated render preparation of one sample.

Verification: foot-contact/slip measurements, action phase markers, weapon socket
paths and body-pose overlays. Preserve existing planted-foot budgets; set any new
budget from the baseline with an explicit rationale. Review starts, stops and
action seams frame by frame and at normal speed. No frozen air pose, skating,
weapon teleport, double animation advance, or render-rate-dependent gait.

## 6. Fighting, damage and readable outcomes — P0

- [ ] Specify each attack's startup, active frames, recovery, movement commitment,
      turn allowance, stamina cost, buffer window and allowed cancels in authored data.
- [ ] Test moving into range, attacking, withdrawing, whiffing, being interrupted,
      re-entering combat, and chaining actions. Inputs must be consumed, buffered or
      refused for a recorded reason; never silently dropped or executed twice.
- [ ] Establish a traceable contact-to-damage path: attacker/action/contact ID,
      target body, simulation phase, guard/invulnerability decision, health/stamina
      before/after, reaction event, audio/VFX event. Keep presentation interpolation
      out of authoritative contact and damage decisions.
- [ ] Verify weapon sweeps against moving bodies, walls, height differences and
      multiple targets. Prevent damage through walls, outside active frames, from
      dead attackers, or twice for the same authored contact. Multi-hit attacks need
      explicit contact/reset rules rather than a blanket one-hit assumption.
- [ ] Verify receiving damage during locomotion, jump, dodge, guard, attack and
      recovery. Check block direction, perfect-guard boundaries, stamina exhaustion,
      stagger, invulnerability boundaries, lethal hits, projectiles and ranged aim.
- [ ] Align visible contact, reaction, sound and health feedback. Preserve attack
      commitment where intended, but make a protected/blocked/armored hit legible.
      HUD health must reflect the actual damaged soldier/entity consistently.
- [ ] Validate explicit lock-on acquisition, target cycling, nearby target
      crossings, occlusion, target death and manual override. Prevent target changes
      from redirecting a committed swing or silently rotating free-look intent.
- [ ] Validate group fights: bounded attacker opportunities, readable offscreen
      threats, escape routes and collision. No friendly crowd pinning or unavoidable
      damage solely because several RTS soldiers strike the same body simultaneously.

Verification: extend weapon-trace, defense-window, combat-mode and damage tests;
exercise existing melee/defense/contact, attack-wall, stamina, duel, bow and
`rpg_lock_occlusion_death_cycle` scenarios. Compare expected health/stamina ledgers
with actual events. Test one tick before/on/after each action boundary, 1v1 and 1v3,
multiple weapon sets, and combat on slopes, bridge decks and water boundaries.
Rendered impact feedback target: the first presented frame consuming the event,
without advancing the gameplay hit early to disguise latency.

## 7. Camera as part of the whole body experience — P1, integrated throughout

- [ ] Verify the current orbit patch at the full pitch range with actual terrain
      and obstacles, not only unconstrained math. Looking up near the ground must not
      lose the body, clip the lens, or produce a sudden vertical correction.
- [ ] Tune follow lag, vertical filtering and look response with movement/jump
      timing. Do not add smoothing to conceal simulation or animation jitter.
- [ ] Keep a stable horizon, immediate player look, predictable shoulder behavior,
      and a deliberate collision recovery policy. Test alternating doorposts, concave
      corners, thin props, overhead cover, banks, crowded bridges and tight duels.
- [ ] Specify framing for free movement, lock-on, bow aim, jumping, receiving a hit,
      and combat exit. Add hysteresis where engagement/crowd state causes oscillation.
- [ ] Verify the crosshair/aim direction after collision adjustments and shoulder
      changes. Prevent camera collision from moving a shot through cover.
- [ ] Verify reduced-motion and impulse/bob settings, FOV/aspect ratios, window
      resize, cursor capture, focus loss, and mouse look over HUD controls.

Verification: existing camera unit tests plus `rpg_camera_prop_gauntlet`,
`rpg_camera_wall_pocket`, `rpg_camera_hill_bank`, `rpg_close_quarters`, and
`rpg_pass_ranks`. Add an explicit camera hitch scenario; it is not currently listed
in the manifest. Measure resolved eye clearance, sight line, boom reversals,
screen-space body framing and input-to-view response. Collision may retract
immediately for safety; release must be continuous and bounded. Investigate any
uncommanded angular motion above 0.1 degrees/frame in a stationary free-look case.

## 8. Combat entry/exit and RTS/RPG lifecycle — P0

- [ ] Treat entering/leaving combat and entering/leaving commander control as
      separate transitions. Leaving combat must not reset health, stamina or cooldowns.
- [ ] Define mode switching during attack startup/active/recovery, guard, dodge,
      airborne motion, stagger, death, lock-on and bridge/wall traversal. Either finish
      or deliberately cancel each action once; never gain a free attack or immunity.
- [ ] Preserve identity, position/support, health, stamina and authored cooldowns.
      Transfer movement/AI ownership once. RTS orders cannot fight the direct motor;
      returning to RTS must not replay stale held keys or queued direct-control edges.
- [ ] Test receiving and dealing damage immediately before/during/after a switch,
      including a projectile already in flight. Prevent duplicate RPG/RTS damage paths.
- [ ] Restore appropriate camera/UI/cursor state; handle pause, focus loss,
      save/load, mission restart and commander death without stale targets or state.

Verification: expand mode-transition, combat-mode, replay and lifecycle-soak tests.
Run 100 enter/exit cycles under movement and combat, including surface boundaries;
assert event counts, ownership, health/stamina continuity and stable memory.
Replay authoritative outcomes independently of presentation rate.

## 9. Verification matrix and performance closure — P0

| Dimension | Required coverage                                                                                                                                    |
| --------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| Timing    | Fixed 60 Hz simulation; independent 30/60/72/100/120/144/165 Hz presentation; 33/50/100 ms authored hitches; variable real-time pacing               |
| Input     | Tap/hold/release, rapid reversals, simultaneous actions, buffering, focus loss, mouse capture; gamepad only if supported, otherwise mark unsupported |
| Traversal | Flat, uneven, uphill/downhill, crest, steep bank, shallow/deep water, bridge entry/deck/exit, stairs, wall walk, narrow door                         |
| Action    | Idle, all directions, sprint, jump/fall/land, dodge, guard, attack/whiff/hit/recovery, stagger/death                                                 |
| Combat    | No target, 1v1, 1v3, friendly crowd, ranged pressure, target death/occlusion, elevation difference, mode switch mid-contact                          |
| Rendering | 16:9, 21:9, 4:3; low/high quality; windowed/fullscreen; default/reduced motion; declared hardware and driver                                         |

Use representative pairwise combinations for routine runs and explicitly test
high-risk intersections: jump onto a bridge, attack uphill, dodge at water's edge,
take damage while switching modes, and lock-on in a doorway. Do not claim the
entire Cartesian product was tested.

- [ ] Unit tests verify numerical invariants and timing boundaries; integration
      tests verify owner handoffs; Arena verifies complete time series and rendered
      geometry; real-time play verifies control feel and responsiveness.
- [ ] Run three identical repeats of new deterministic scenarios; compare outcomes
      and trace metrics. Register scenarios and expectations in
      `tools/arena/rpg_gate_manifest.json` and its manifest/metrics tests.
- [ ] Profile isolated commander play and a representative busy RTS battle.
      Record CPU/GPU frame times, p50/p95/p99, worst hitch, input age and allocation
      spikes. Target a 60 FPS workload: p95 <=16.7 ms, p99 <=25 ms and no recurring
    > 50 ms stalls after warmup on named reference hardware. Record cold-start stalls
    > separately; software-rendered runs cannot certify hardware performance.
- [ ] Profile surface/collision queries, crowd targeting, animation and rendering
      before optimizing. Bound query work and reuse existing spatial indexes/caches;
      never sacrifice contact correctness to hit a timing budget.
- [ ] Run the complete normal suite and ASan/UBSan at milestone closure as required
      by `docs/RPG_PLAYABILITY.md`. Verify save/load, replay and RTS regressions too.
- [ ] Do at least 20 uninterrupted minutes of real-time play on the course and a
      real campaign battle. Record issues and replay each fixed sequence. Have the
      user review the final feel; automated green results cannot overrule a visibly
      poor or frustrating experience.

## Execution order and reviewable deliveries

1. **Baseline and ownership:** course, defect list, traces, corrected timing harness,
   and ownership diagram. No cosmetic feature work before this evidence exists.
2. **Movement slice:** motor, turns and locomotion animation on flat ground;
   playable start/stop/strafe/reverse route with before/after comparison.
3. **Traversal slice:** jumps, falls, support surfaces, water, bridges and slopes;
   landing/terrain animation and camera support included in the same review.
4. **Combat slice:** first one weapon and one opponent end to end, including
   damage received, misses, guard, dodge, stagger and death; then all supported
   loadouts, multiple opponents and terrain combinations.
5. **Integration slice:** lock-on/camera refinement, combat and control-mode
   transitions, campaign crowds, lifecycle and performance.
6. **Acceptance:** full matrix evidence, real-time play session, user review and
   a list of explicit remaining limitations. Do not label the RPG complete while
   a P0 item is open.

Each delivery should include a small change description, its owning files,
regressions added, exact validation commands, before/after clips, and unresolved
risks. Use discovered complexity to estimate each slice after the baseline;
invented deadlines are not a substitute for an executable scope.

## Commands and evidence

These are existing entry points, not a record of checks performed for this plan:

```sh
cmake --build build --target standard_of_iron app_tests arena_app arena_tests combat_balance_tests -j4
build/bin/app_tests --gtest_filter='*Commander*:*RtsCameraBookmark*'
scripts/run-rpg-gates.sh --skip-build --scenario rpg_locomotion --capture-interval 0.1
scripts/run-rpg-gates.sh --skip-build --scenario rpg_camera_hill_bank --capture-interval 0.1
scripts/run-rpg-gates.sh --skip-build
# Run performance enforcement only on the documented reference hardware:
scripts/run-rpg-gates.sh --skip-build --enforce-performance
```

Arena rendering runs sequentially; do not run another game/renderer concurrently
with performance measurements. Until milestone 0 fixes clock independence,
changing `--fps` is only a timestep stress test, not display-rate verification.
Offline `--film` captures are useful for visual comparison but cannot establish
real-time latency or frame pacing. New scenario names are proposals until registered.

Store evidence under `artifacts/rpg-feel/<revision-or-run-id>/`: baseline/candidate
revision and diff, map/fixture/seed, environment and graphics settings, commands,
test reports, traces, normal-speed clips, frame-time report and playtest notes.
Keep a defect table with symptom, reproduction, owner, failing check, fix, evidence
and status. Mark unavailable verification as **not run**, with its reason.

Final acceptance: complete the course and a busy battle with responsive controls,
continuous grounded/airborne motion, readable and correct damage, coherent
animation, reliable surfaces, stable framing and clean ownership. No unexplained
snaps, silent actions, duplicate damage, bridge/water mistakes or mode-switch exploits.
