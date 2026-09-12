# Troop movement review — 2026-09-10

Evidence pass for the complaint that large, multi-unit moves through towns and
other tight ground are glitchy: the warning marker pops up, troops do not react
at once, units spin, direction changes look fake, soldiers get left behind, and
the recovery machinery runs all the time. Everything below was measured on
`main` at `dfcbb886` with binaries built from that tree the same evening, in
three ways:

- **The real game**, an all-AI skirmish watched through `--observe` on
  Sunbaked Terraces (four AIs, 11 simulated minutes) with the movement trace on
  (`SOI_MOVEMENT_TRACE_DIR`, stride 6).
- **The Arena**, the existing tight-space scenarios plus three new manoeuvre
  scenarios written for this review (`maneuver_town_relay`,
  `maneuver_obstacle_slalom`, `maneuver_countermarch_streets`), on the RTX at
  30 Hz with `--animation-diagnostics` so every soldier's root is in the trace.
- **The headless gate**, `battlefield_gameplay_verifier --all --seconds 60`
  with the trace on, read back through `movement_trace_report`.

The new scenarios exist because the old ones were too kind: one straight
corridor, four units, one order. A player drives fourteen units of four troop
types through a staggered town and changes their mind every ten seconds. That
is what the manoeuvre scenarios do, and they fail on `main`.

## 1. What was observed

### 1.1 The real game (observed skirmish)

Army units only (swords, spears, archers, knights), 26 725 samples of units
under an active order:

| Measurement                                                       | Value                  |
| ----------------------------------------------------------------- | ---------------------- |
| Samples in `LocallyBlocked` / `Repathing` / `Recovering`          | 33 % of active samples |
| Samples where the player-facing **Blocked** marker would be shown | 3.1 %                  |
| Orders that ended `Unreachable` (abandoned by the ladder)         | 29 of 1 953            |
| Longest single order held without arriving                        | 83 s                   |
| One unit's repath count inside one order                          | 37                     |

Two units account for most of the blocked time, and both show the same
mechanism. Entity 100, a swordsman unit, stands **exactly on its resolved goal**
(`remaining = 0`, no waypoints, no neighbours, no contact, no rejected motor
step) and is declared `LocallyBlocked` for 28 s at a stretch, escalates to
`RelaxFormation`, is abandoned as `Unreachable`, is re-ordered by the AI to the
same spot, and repeats for the rest of the match. A builder (entity 65) does
the same 0.8 m from a goal it can never reach because the requested point is
inside a footprint 6.5 m away. Neither ever moved a body while "blocked"; the
motor was asked for nothing (`des_v = 0`).

Root yaw rate while under orders, by body type: formation blocks stay under
160 °/s; single bodies (civilians, healers, commanders, builder crews) reach
the raw 720 °/s cap; sheep and wolves exceed 200 °/s on 20–31 % of their frames.

### 1.2 Arena, existing tight-space scenarios

Every scenario was run with frame captures and per-soldier diagnostics. Stalls
are the arena's own `movement:` summary (seconds a group made no progress while
holding an order); soldier numbers are computed from `trace.jsonl` over living,
unculled soldiers.

| Scenario                         | Verdict on main             | Longest unit stall                  | Soldier frames > 200 °/s | Soldier pops (> 4 m/s) | Max soldier speed |
| -------------------------------- | --------------------------- | ----------------------------------- | ------------------------ | ---------------------- | ----------------- |
| `stuck_town_streets`             | **FAIL** permanent stall    | 25.3 s (Following), 21.6 s Yielding | 3.3–4.8 %                | 1 293 frames           | 15.0 m/s          |
| `nav_town_crossing`              | stall                       | 18.1 s (Following)                  | 3.3 %                    | yes                    | 15.2 m/s          |
| `stuck_interrupted_march`        | stall                       | 20.1 s (Following)                  | 2.5 %                    | yes                    | 15.2 m/s          |
| `path_building_alley`            | **FAIL** missed destination | 17.4 s (Following)                  | 3.8 %                    | no                     | 4.2 m/s           |
| `army_formation_obstacle_course` | **FAIL** missed destination | 4.9 s (LocallyBlocked), 1 wedged    | 6.1–6.5 %                | yes                    | 15.0 m/s          |
| `army_formation_narrow_gate`     | **FAIL** missed destination | 4.0 s                               | 1.1 %                    | yes                    | 14.5 m/s          |
| `traversal_city_alley`           | pass (movement)             | 3.4 s                               | —                        | —                      | —                 |
| `crossing_formations`            | pass                        | 1.4 s                               | 3.0–7.0 %                | no                     | 4.7 m/s           |
| `stuck_crossing_congestion`      | pass                        | 0.7 s                               | 0.1 %                    | no                     | 5.1 m/s           |

Two things repeat in every row:

- **Long stalls in `Following`.** A group sits still for 17–25 s while its
  order state says it is following its route. The recovery ladder does not
  see it because the ladder's clock only starts once the route follower stops
  publishing progress, and a unit that is being slowed to a crawl by traffic
  ("Yielding") or that is inching forward keeps resetting it.
- **Soldier pops at 14–15 m/s.** In every scenario with a building or a prop
  near the route, soldiers jump 0.45–0.5 m in one 30 Hz frame. 14 m/s is
  `k_ground_clamp_speed` in `unit_traversal_layout_system.cpp`: when a
  formation slot lands inside a footprint (typically because the root turned
  and the rigid slot offsets swept through a wall), the slot's _current_
  position is yanked inward at that speed. `stuck_town_streets` alone has
  1 293 such frames in 80 s. This is the "soldier gets lost / teleports"
  complaint.

### 1.3 Arena, the new manoeuvre scenarios

`maneuver_town_relay` — 14 units, ~150 soldiers, seven orders in 112 s through
a staggered town (three of them reversing a march in progress, one a full
deploy):

- **FAIL**: two swordsman units ended the run 15.9 s and 16.3 s into a stall
  _in `Following`_; the spear group missed its final destination.
- Three of the four spear units never left the west edge on the last order:
  they finished 42 m from their goal after 10–16 s standing still, five
  repaths and three "recoveries". Units left behind is exactly what a player
  sees when they re-order an army and part of it does not come.
- Per soldier: spears 9.3 % of frames turning faster than 200 °/s, swords
  4.9 %, archers 5.8 %; 15–17 m/s position pops in all three infantry groups
  and 12.7 m/s in the knights; 1 356 spear frames of a soldier sliding along
  in an idle pose.
- Per unit: knights' roots turn at up to 189 °/s and every unit reversed its
  heading by more than 120° between two consecutive samples four to seven times
  — the "fake" direction change. A reversal is instantaneous re-aiming of the
  route at the new goal; nothing wheels.
- Visually, the army walked _around_ the town every time it could. A closed
  variant with palisades on both flanks (forcing a route through the streets)
  is in the file and is what the gate should run.

The palisaded variant is reported in §1.6.

### 1.4 Headless verifier

`battlefield_gameplay_verifier --all --seconds 60` passes all eight scenarios
and the movement trace reports 14 findings: ten `AngularSpeedExceeded` /
`AngularAccelerationExceeded` at 960–1 440 °/s and two `HeadingOscillation`.
Half of those angular numbers are an instrumentation bug: the trace manifest
written from the environment hard-codes `fixed_step_seconds = 1/60` while the
verifier steps at 1/30, so every yaw rate in that run is doubled. Corrected,
they are 480–720 °/s, which is still the single-body cap and still twice what
any RTS body should do. The headless scenarios are otherwise clean, which is
the point: the defects live in crowded, built-up ground, and the headless
gate has no scenario there.

### 1.5 The other two manoeuvre scenarios

`maneuver_obstacle_slalom` — the same army zigzagging between boulder fields,
ruins, pine clusters and farmsteads, seven orders with two reversals:

- **FAIL**: an archer's rendered root jumped **14.02 m in one frame** (the
  render-side snap, 7 m doubled while wheeling); the spear group finished
  10–20 m short of its last destination after 95 s.
- Longest unit stall 11.0 s (`Following`); soldier frames > 200 °/s: 4.2–4.6 %;
  15 m/s pops in every infantry group and 14.8 m/s in the knights.

`maneuver_countermarch_streets` — two six-unit friendly armies through one
street grid from opposite ends, reversed before contact, sent through each
other twice, then crossed at the junction where an enemy block stands:

- **FAIL**: two western swordsman units ended the run 21.3 s into `Repathing`
  and 17.1 s into `Following` without progress; the group missed its
  destination.
- The western army stalled for **34 s**, repathed ten times and abandoned one
  order; two of its units finished at the far south-east edge of the field
  (their goals had been moved there by `assign_local_recovery_move`, which
  substitutes a "recovery cell" up to 64 cells away for the order) — a unit
  wandering off on its own is this mechanism.
- Soldier frames > 200 °/s: **11.8 % and 12.5 %** in the two western groups
  (the ones in head-on traffic); 2 125 frames of spearmen sliding in an idle
  pose; 15–16 m/s pops in all four groups.

### 1.6 The palisaded relay

With palisades on both flanks of the town (the version now in the file), the
same seven orders give: **FAIL** — a swordsman unit 18.0 s into a `Following`
stall at the end, the archers missing their destination, and an archer unit
20.5 s in `Yielding`. Soldier frames > 200 °/s rise to **7.8–8.9 %** for all
three infantry groups, idle-while-translating frames to 1 514 (swords), and the
knights' roots still turn at 189 °/s. The army also found the one open strip
along the inside of the palisade and marched down it as a blob rather than
threading the gaps, which is why the committed layout hugs the palisades to the
outer rows and opens a plaza in the middle.

Head-on friendly traffic is the worst case in the whole review: the avoidance
stage slows both sides to a crawl inside a lane the width of two body cores,
neither side has room to lean in a 5 m street, the `Yielding` budget (8 s)
expires into `LocallyBlocked`, both repath into the same street and the
recovery ladder scatters the goals.

## 2. The bar: what a clean RTS does

Measured against StarCraft II, Company of Heroes and Total War style unit
handling, the contract a player expects is:

1. **Acknowledge instantly.** Within one or two simulation ticks of a click,
   every unit in the selection has a new heading target and its front rank is
   moving. There is no "thinking" pause and nothing visible about routing.
2. **One path per group, one lane per unit.** A boxed selection gets one
   corridor and every unit its own lane in it, in the order the units already
   stand. Units flow through a gap like water: the shape narrows, files fold
   into the gap in their existing order and unfold on the far side, and nobody
   is left behind — a unit that cannot reach the exact point walks to the
   nearest point it can reach and stands down there, silently.
3. **Bodies never overlap and never freeze.** Two friendly units meeting
   head-on slide past each other; one gives way for a bounded time, never both,
   and neither stands still. Enemy bodies cannot be walked through.
4. **Turning is a body, not a value.** A block wheels around a pivot at a
   bounded rate (infantry ~90–150 °/s at the root, faster for a single man,
   slower for horse and elephant); soldiers walk the arc; a reversal is an
   about-face with the rear rank becoming the front, or a wheel — never all
   twelve bodies spinning on the spot at 300 °/s and then sliding into place.
5. **Soldiers are continuous.** A soldier's presented position moves at most a
   little faster than the unit (catch-up ≈ 1.3× unit speed), never pops, never
   plays idle while translating, and never stands inside a wall for a frame.
6. **Recovery is silent and rare.** Repathing is invisible. A player-facing
   "cannot get there" signal appears once, for a truly unreachable order, not
   after 1.2 s of traffic.

## 3. Where each symptom comes from

The stack is `RouteFollowSystem` (desired velocity) → `LocalAvoidanceSystem`
(traffic) → `MovementSystem` (motor, root yaw) → `UnitTraversalLayoutSystem`
(soldier slots, corridor squeeze) → combat presentation → renderer smoothing
(`soldier_turn_smoothing.cpp`). What produces each complaint:

**The warning marker.** `ActivityKind::Blocked` is raised from
`MovementComponent::stuck_timer ≥ 1.2 s`, a mirror of
`progress.no_progress_seconds` written by the route follower. It is a fourth
stall clock next to `progress.state` (0.35 s to `LocallyBlocked`), the recovery
ladder in `track_objective_stall` (2.5 s rungs, 9 s no-closer rungs) and the
presentation stall gate in `world.cpp` (0.4 s). Each decides "stuck" on its own
definition; the marker fires on the one nobody else uses.

**Units that never arrive and loop through recovery.** In
`RouteFollowSystem::follow`, arrival is refused when the _requested_ goal is
further than `arrive_radius + 0.75 m` from where the route ends
(`route_stops_short_of_the_order`). A goal inside a footprint is resolved to
the nearest walkable cell — deliberately — and then arriving at that cell is
treated as being blocked: state `LocallyBlocked`, desired velocity not
published, repath to the same cell, `Recovering`, `Unreachable`; the AI
re-issues the order and it starts again. The same rule turns the last unit of
a formation order into a permanent "blocked" body when its slot was fitted a
metre away from where it can stand. This is a state machine bug, not a
pathfinding one: reaching the nearest reachable point _is_ the terminal outcome.

**Two stall ladders.** `update_progress` runs its own
`LocallyBlocked → Repathing → Recovering` escalation with
`retarget_unit`/`assign_local_recovery_move`, and `track_objective_stall` runs
a second ladder (`Replan → Sidestep → RelaxFormation → Abandoned`) calling the
same two functions on different clocks. They interleave, which is why a stuck
unit shows repath counts of 16–37 inside one order and why stall fixes have
kept regressing.

**No body separation in the shipped game.** `BodyContactSystem` — the only
stage that pulls overlapping unit bodies apart — is composed into
`MovementPipeline`, which is used by `balance_sim` and tests. The game, the
arena and the verifier build their systems through `register_runtime_systems`,
which never adds it. Traffic avoidance only slows and leans bodies of the same
owner within a lane of two 0.5 m cores; a 3 m wide block is a 0.5 m disc to
it. So friendly blocks walk through each other, enemy blocks are not kept apart
except by melee rules, and the gate test that asserts `BodyOverlap == 0` is
vacuous because nothing writes the metric.

**Rotating on the spot / spinning soldiers.** The root heading target for a
formation is the direction of the _desired_ (pre-avoidance) velocity, which is
the direction to an aim point 0.45–2 m ahead along the route
(`heading_reference`). At a route vertex the aim point jumps to the next leg
and the heading target jumps with it; the motor then throttles translation by
heading error (`heading_translation_scale`, 20°→100°), so the block stops and
turns in place, while the body actually moves along the _steered_ velocity —
the root faces where it wanted to go, not where it goes. Soldier facing is a
separate render-side smoothing at 300 °/s × 0.78–1.18 per man with a
0.015–0.15 s per-rank delay, so twelve men spin at up to 350 °/s about their
own axes (5–9 % of frames in the manoeuvre scenario) whenever the root heading
target moves. Single bodies turn at 720 °/s in the simulation and the verifier
records 1 440 °/s because of the manifest bug above.

**Fake direction changes.** A new order re-aims the route immediately; there
is no about-face, no wheel and no pivot at the simulation level. The render
"wheel path" in `soldier_turn_smoothing.cpp` only bends each soldier's
catch-up path when the root is already turning fast, and is disabled in combat
and hold mode.

**The collapse into a corridor looks bad.** Four independent narrowings are
multiplied in `update_slot_states`: the traversal `lateral_scale`, a second
`corridor_half_width / widest` correction, `onto_walkable_ground` bisection of
the _target_, and the same bisection applied to the _current_ position at
14 m/s. Only the first is reported in `TraversalLayoutFacts`, so the trace
under-reports the squeeze, and the last one is the visible pop. Corridor width
is measured three ways (`measure_width`, `center_constrained_waypoints`,
`fit_lane`) with three probe steps and caps.

**Lost soldiers.** A slot that ends inside a footprint after the root turns is
clamped inward at 14 m/s; if the whole block is pinned against cover the
render side can still snap a body 7 m (14 m while wheeling). Combined with the
missing contact pass, a soldier can be drawn well outside the block it belongs
to and then jump back.

### 3.1 Two sources of truth to remove

| Rule                       | Implementations today                                                                                                                                                       |
| -------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Movement pipeline order    | `MovementPipeline` (tests, balance_sim) vs `register_runtime_systems` (game); the latter lacks `BodyContactSystem`                                                          |
| "Is it stuck"              | `no_progress_seconds`, `stall.stalled_seconds`/`no_closer_seconds`, `stuck_timer` (marker), `MotionPresentation::stalled_seconds`, trace thresholds, `AI::is_going_nowhere` |
| Stall recovery             | `update_progress` ladder and `track_objective_stall` ladder, both calling `retarget_unit` / `assign_local_recovery_move`                                                    |
| Yaw integration            | nine `fmod(diff+540,360)-180` clamps with ceilings 720/360/300/260/240/180/120/900 across motor, combat, RPG, builder bypass, renderer                                      |
| Standability radius        | `is_movement_point_allowed` (radius 0), `Walkability::can_stand` (full clearance), `MotorCollision::allowed_here` (either), avoidance probe                                 |
| Corridor width             | `measure_width`, `center_constrained_waypoints::probe_side`, `fit_lane` scale steps                                                                                         |
| Lateral squeeze            | `lateral_scale`, `corridor_half_width/widest`, target bisection, current bisection at 14 m/s                                                                                |
| Soldier separation floor   | `TraversalPolicy` 0.55, `UnitLayoutStyle::min_separation_scale`, `update_slot_states` 2r·1.04, `resolve_group_slots`, `SlotTerrainFitter`, avoidance 0.15, contact radii    |
| Group pace                 | `declared_group_pace` (min speed at order time), `ArmyFormation::cohesion_pace`, `ArmyFormationRuntime::move_speed_multiplier`                                              |
| Arrival radius / "in slot" | route follower radius, cohesion 1.35×spacing, staging 2.5 m, corridor waypoint tolerance, arena defaults                                                                    |
| Slot layout                | `FormationCombat::resolve_layout` (sim) and `resolve_formation_instances` (render, then overwritten)                                                                        |
| Stable slot order          | `rebuild_stable_mapping` (unused) vs `narrow_file_slots_into`                                                                                                               |
| Trace time base            | `configure_from_environment` writes 1/60 whatever the world steps at                                                                                                        |

## 4. Tests first

Headless, deterministic, fast — these are the loop's inner gate:

- `tests/headless/movement_quality_gate_test.cpp`: two new capture scenarios,
  `army_town_relay` (the palisaded staggered town, fourteen mixed units, the
  seven-order script) and `army_countermarch` (two six-unit armies through
  one street grid with an enemy block in the junction), with budgets on
  findings, `ProgressStall`, `SoldierAnchorJump`, `AngularSpeedExceeded`,
  `IndefiniteActiveOrder` and a new `UnitLeftBehind` finding (a unit whose
  final position is further from its last goal than the group tolerance while
  siblings arrived).
- Route follower: arriving at the resolved goal of an unreachable request is
  terminal (`Arrived`, or a single `Unreachable` with no retarget), never
  `LocallyBlocked`; a unit standing on its goal never enters the ladder.
- One stall authority: a test that walks a blocked unit through the ladder and
  asserts each retarget/recovery is issued exactly once per rung, from one
  place, and that `stuck_timer`, the presentation stall and the marker all read
  the same clock.
- Motor: the root heading follows the accepted velocity when steering deviates
  from the desired one; a 180° reversal wheels at the formation turn rate and
  the block's translation ramps rather than stopping; one yaw integrator, with
  the existing per-type ceilings, used by every writer (architecture test:
  grep for `fmod(` … `540` outside it fails).
- Traversal: slot recovery onto ground moves a soldier no faster than the
  relocation limit; the squeeze is reported once and applied once; the
  corridor width is measured by one function shared with the corridor planner.
- Registry: `BodyContactSystem` is present and ordered after the motor in
  `register_runtime_systems` (`movement_stage_ownership_test`).
- Trace: the manifest's `fixed_step_seconds` equals the world's step.

Arena, rendered, the outer gate: the three `maneuver_*` scenarios pass their
expectations (response ≤ 1 s per order, no permanent stall over 8 s, no root
teleport, soldiers on walkable ground, every group within tolerance of its
last destination, frame budget).

## 5. Improvement loop (ordered by player impact)

1. Arrival semantics and one stall ladder (kills the marker spam, the
   `Unreachable` re-order loop and units left behind).
2. Register the contact pass; make avoidance lanes formation-aware (bodies
   stop walking through each other; head-on traffic resolves).
3. One yaw authority; root heading from accepted velocity; a wheel/about-face
   rule for reversals at the simulation level, with the renderer only
   smoothing (kills the spin and the fake turn).
4. One squeeze, one ground recovery at body speed, one corridor measurement
   (kills the pops and the collapsed-column look).
5. Delete the duplicated rules in §3.1 as each authority lands; the
   architecture tests above keep them from coming back.

Then the visual recheck: the three manoeuvre scenarios and the observed
skirmish again, same seeds, same cameras, side by side with the captures from
this pass (kept under `artifacts/movement-review-2026-09-10/`).

## 6. First fix slice (Sep 10–11)

Landed in the tree with tests first; every suite below is green unless noted.

- **Arrival is terminal.** `RouteFollowSystem::follow`: a route that ends short
  of a requested point that is not ground (a click inside a footprint) is
  `Arrived` with `arrived_short`, silently. A requested point that is ground but
  has no route gets one confirming replan and is then given up once
  (`Unreachable`), without twelve seconds on the ladder. Orders whose issuer
  re-targets them (`AttackChase`, `ScriptedMove`, `GuardReturn`,
  `RecoveryMove`, any structure approach) are never ended by the follower
  (`MovementComponent::issuer_retargets`). A unit pressed against standing
  friends within 1.5 m of its goal has arrived.
- **One recovery ladder.** `update_progress` only classifies
  (`Following`/`Turning`/`Yielding`/`LocallyBlocked`); every retarget and
  recovery move is issued by `track_objective_stall` (Replan 1.5 s, Sidestep
  4 s, RelaxFormation 7 s, Abandoned 12 s of standstill; no-closer clock
  9/18/27/36 s). Time queued behind friendly traffic does not count as a stall.
- **The marker reads the ladder.** `ActivityKind::Blocked` appears when the
  ladder has reached Sidestep on a live order; `stuck_timer`/`stuck_ref_*` are
  gone from `MovementComponent`.
- **The contact pass ships.** `BodyContactSystem` is registered after the
  motor. Animals are not pushed; idle combat units are nudged (35 % share) by a
  body under way so a column can pass a huddle; on a bridge, hill entrance or
  gate passage bodies never spread sideways, the one behind backs off.
  `BuildingCollisionRegistry::point_in_navigation_passage` is the one passage
  predicate (steering and contact both use it).
- **Blocks wheel and go.** Formation turn rate floor 180 °/s (outer file
  4.5 m/s or 2× pace), translation stays at full pace within 45° of heading
  error and tapers to zero at 150° (single bodies keep 20°/100°); the root
  faces the _steered_ velocity.
- **Soldiers walk out of walls.** Slot recovery onto ground runs at the
  4.5 m/s recovery pace, never 14 m/s; a slot whose recovery point is already
  held by another slot takes the nearest open ground in sight of the root
  instead of stacking on it.
- **Trace time base.** `MovementTrace::set_fixed_step_seconds` is fed by the
  world every tick; the manifest reports the real step.

New tests: `StuckRecoveryTest.{AnObjectiveInsideASealedPenEndsAtTheWallWithoutShuffling,
StandingOnTheResolvedGoalIsArrivedNotBlocked, ANoRouteOrderIsGivenUpAtOnceWithoutTheLadder,
RecoveryIsIssuedByOneLadder}`, `TightGapNavigationTest.{ASoldierSweptIntoCoverWalksOutInsteadOfPopping,
AReversedBlockWheelsFastAndStepsOffAtOnce}`, `UnitActivityTest.ArrivingShortOfAnUnreachablePointIsNotBlocked`,
`MovementStageOwnershipTest.TheRegistryOrdersFollowThenSteerThenMotor` (now requires the contact pass),
`MovementTraceTest.TheManifestTakesTheStepTheWorldActuallyRuns`, and the three `maneuver_*` arena scenarios.

**Not this work:** `WallSiegeTest.FrontRankVisuallyReachesTheFacadeFromWalkableGround`
fails on `origin/main` since #1449 ("settle the melee stance",
`formation_contact_processor.cpp`); bisected by swapping that one file back to
`dfcbb886`, which makes it pass with every change above in place.

## 7. Second fix loop (Sep 11)

The first slice, re-measured on the tightened town relay, removed the soldier
pops (worst soldier speed 16 m/s down to 6.7 m/s) and every unanswered order.
It also introduced two defects, both found in the trace and both fixed:

- **Treading water in wall gaps.** `WallNetworkService` registers every gap in
  a wall run as a navigation passage, so the spaced palisade was a row of
  "gates". The one-lane contact rule pushed the rear body back along its own
  travel; the motor walked it straight back in. Unit roots oscillated
  ±0.05 m every frame with the walk animation on. In a lane, contact now only
  eases the body ahead forward; the body behind is not pushed. (This loop also
  let avoidance stop a body dead behind a touching neighbour it had no room to
  lean round; §13 takes that back.)
- **Heading jitter.** Facing the steered velocity swung the root ±6° per frame
  whenever a lean or a contact nudge touched it, and soldier frames above
  200 °/s rose from 10 % to 15 %. The root faces the route again.

Also in this loop:

- **One yaw integrator.** `Game::Systems::turn_yaw_toward`,
  `signed_yaw_delta` and `yaw_degrees_from_direction` in
  `game/util/planar_math.h` replace eleven private copies across the motor,
  duel footwork, builder bypass, melee lock facing, commander target assist,
  RPG engagement, formation contact soldier yaw, defensive layout, unit layout,
  wildlife bite facing and trace analysis. The builder bypass now honours the
  formation turn rate. `MovementStageOwnershipTest.EverySimulationYawTurnsThroughOneHelper`
  fails on any new copy under `game/`.
- **Gate test.** `GateTraversalTest.GroupMembersIndividuallyUseTheGateCenterline`
  (24 bodies through one gate) went red with the treadmill and is green again.
- **Scenario.** The relay's palisades are continuous and the army spawns in
  columns; the previous layout overlapped groups at spawn and let the army
  leave the town through the gaps.

### Open decision for Adam

A move order given to a unit locked in melee with a living enemy is dropped
without a word (`prepare_move` in `movement_orders.cpp` returns nothing). That
is the one remaining silent refusal on the player's move path. Letting the
order disengage the unit is a combat rule change -- it interacts with the melee
lock, `rear_rank_bypass` and the commander duels -- so it is not changed here.

## 8. Third fix loop (Sep 11)

Measured on the relay after loop 2: soldier spin down to 9 % (swords), 4.7 %
(spears) and 2.7 % (archers) of frames above 200 °/s, worst soldier speed
7.9 m/s. The movement trace still reported 293 `DirectionReversal` and 16
`HeadingOscillation` findings, and the counter-march had one order go
unanswered for over a second. Three causes, all in the trace:

- **The aim point was pinned to the next corner.** Corridor routes through a
  town are staircases of cell corners, a vertex every metre (a 116 m route had
  29 waypoints, another 109). The follower aimed at `min(s + lookahead,
next vertex)`, so approaching each corner the aim sat 0.2 m ahead and a 4 cm
  nudge swung the desired velocity by up to 60°. 652 of 686 velocity reversals
  were in plain `Following` with no contact at all.
  `MovementRoute::steering_aim_s` now moves an aim that is closer than 75 % of
  the look-ahead on down the route, but only as far as a straight line from
  the body is walkable, so it still never cuts a building corner.
- **The root faced the direction to that aim point.** Replaced by a look-ahead
  heading fact (`DesiredMotionFacts::heading_x/z`, the route tangent at the aim
  point) that depends only on progress along the route, plus a 2° deadband for
  formations. New test `BlocksMarchingShoulderToShoulderHoldTheirHeading`.
- **The traversal root hold parked the block.** When the front rank was wider
  than a pinch ahead, the motor zeroed the block's velocity and reported
  `Yielding` for up to 3 s. That is the order that "did not respond within
  1 s". The block now keeps 35 % of its pace while it folds; the narrow-layout
  test asserts the hold still happens and that the block never stands still.
- **Render soldier turn rate** 300 → 200 °/s (mounted stays 150), so a change
  of block heading reads as men turning rather than spinning.

Tried and reverted: removing the instantaneous corridor correction and the
sideways drive in `update_slot_states`. They are not redundant with
`lateral_scale`: the correction squeezes below the files-one-body-apart floor
at once, which the slew-limited scale cannot, and without it a soldier stood
inside the wall of a one-cell gap.

### Open findings

- **Deploy slots on the wrong side of a wall.** A 14-unit `Line` deploy next
  to a palisade placed five sword slots south of the palisade's end
  (`SlotTerrainFitter` searches outward for free ground without asking whether
  it is reachable from the anchor side). The units walk round the wall for
  16 s. The planner should reject slots not connected to the anchor.
- **Blocks detour round a town instead of using an 8 m street.** The clearance
  cost (`k_clearance_avoid_weight` × block clearance per penalised cell) makes
  a 12-man block's route through a street costlier than a long walk round the
  outside. Visible in the counter-march; not changed here.
- **Melee lock refuses move orders** (see section 7).

## 9. Fourth loop (Sep 11): refusals and freezes

Found by driving the manoeuvre scenarios with the movement trace and an
env-gated diagnostic (`SOI_DEBUG_IDLE_ORDER=1` prints every unit that holds a
live order with ground to cover and no desired motion).

- **Units frozen with a live order.** `ArmyFormationRuntime::move_speed_multiplier`
  ended in `std::clamp`, which passes NaN straight through. A NaN pace gave a NaN
  look-ahead, `std::max(0, NaN)` returned 0, the aim point sat on the unit's own
  feet, and the desired velocity was zero for 12 s until the ladder abandoned the
  order. Guarded at the multiplier, at `formation_navigation_speed` and inside
  `MovementRoute::steering_aim_s`; test `ANonFiniteLookaheadStillAimsDownTheRoute`.
- **Orders into a building walked round it.** An order on a point that is not
  standable resolved to the nearest standable cell by search order, which could be
  the far face. `resolve_walkable_target_toward` now takes the first standable point
  walking back from the click toward the unit (unless it is much further than the
  nearest). Test `AClickInsideABuildingStopsOnTheSideTheUnitCameFrom`; this also
  restored `CommanderSharedTraversalTest` parity.
- **Unreachable orders wait instead of dying or dithering.** A point that is ground
  but has no route: the unit stands at the end of the best route, keeps the order,
  asks the planner again once a second, and goes on the moment a breach or an opened
  gate makes a route. No ladder, no recovery moves; the Blocked marker shows; the
  order is given up once after 12 s. Tests
  `ANoRouteOrderWaitsWithoutShufflingThenEndsOnce`,
  `AnObjectiveInsideASealedPenWaitsAtTheWallWithoutShuffling`.
- **Queues.** A unit whose steering says it is queued behind traffic no longer
  sidesteps (a shove) or abandons; those rungs become a silent replan and a restart,
  while the relaxed-clearance replan still runs (it is what fits a block through a
  one-cell breach). `BuildingObstructionLifecycleTest` depends on it.
- **Treading water against a standing friend.** Avoidance kept a body pressing at a
  third of its pace into a friend that was not moving; the contact pass pushed it
  back every tick. A body touching a standing body now stops pushing and only leans
  round it (or waits, if there is no room).
- **Sideways sliding restored to the original band.** Loop 1 widened the
  translate-while-turning band to 45°/150° to kill the reversal freeze; that let a
  block strafe through a right-angle turn at 57 % pace. The 20°/100° band is back;
  the 180 °/s wheel floor keeps an about-face under a second.
- **Combat presses are not movement overlaps.** The contact pass records body
  overlap only outside a melee press (both bodies engaged, or opposing bodies with
  either engaged), so the quality gate measures traffic, not the fight.
- **A locked noncombatant fights only its captor.** Faster movement let a raider
  lock a builder in melee, and the lock hands the captor to the builder as its
  target (`sync_melee_lock_target`). Gating that path on combat role was tried and
  reverted: `BareHandedMeleeTest` requires a caught builder to fight back.
  `AutoEngagementResponseTest.ANoncombatantNeverPicksItsOwnFight` now separates the
  two: auto-engagement must record `NoCombatRole` for the builder, and a locked
  builder's only allowed target is its captor.
- `AutoEngagementResponseTest.SwordsmenGoToTheAidOfAnAllyBittenByWolves` samples every
  tick: the escort now answers and kills the wolf inside the second, and a dead
  target is cleared.

## 10. Fifth loop (Sep 11): churn, stale facts and every suite green

Measured on loop 4: the headless verifier passed all eight scenarios but its trace
held 144 `RepathChurn` findings (1 026 repaths, 996 of them issued while the unit
was `Yielding`), the quality gate's bot skirmish reported two idle bodies "inside
one another" for 6-8 s, the breach test and `AiTownPlanTest` were red, and
`WallSiegeTest` had been red on origin/main since #1449.

- **Queues replanned every few seconds.** A unit waiting behind friends who were
  moving climbed the ladder like a stalled one: a replan at 1.5 s, a "sidestep"
  replan at 4 s, a relaxed-clearance replan at 7 s, then a restart. The planner
  does not see bodies, so each replan returned the same route -- or one pointing
  back the way the unit came. Every spontaneous unit-root reversal found in the
  relay and countermarch traces (1 and 4, against 50/31 order-driven ones)
  followed exactly such a `Blocked` replan. Two rules now: a queue that creeps
  (at least 0.1 m in the one-second window) is flowing, not stalled; and the
  ladder does not act on a queued unit until it has stood for
  `k_queue_patience_seconds` (6 s). The clocks keep counting, so a queue that
  never moves still escalates. Test
  `TightGapNavigationTest.AQueueAtAGapWaitsItsTurnInsteadOfReplanning` (twenty
  units through one cell, at most one replan each).
- **Idle bodies reported overlaps they no longer had.** Local avoidance resets
  steering facts only for a body under way, and the contact pass only ever raised
  `body_overlap`. An idle unit brushed once kept that overlap for the rest of the
  game, which is what the quality gate's bot skirmish measured (constant 0.483 m
  and 0.600 m). The contact pass owns the fact now and clears it every tick. Test
  `TightGapNavigationTest.AnIdleBodyDoesNotKeepAnOverlapItNoLongerHas`.
- **A full stop that never ended.** Loop 2 let avoidance bring a body to a
  complete stop when it touched a body ahead with no room to lean (and loop 4
  when it touched a standing one). Nothing ever lifted that stop. Three soldiers
  reaching a fresh breach side by side each waited for the others until the
  ladder gave their orders up (`UnitsRerouteThroughNewlyOpenedBreach`), and AI
  builder crews stood behind crews working on the neighbouring links, so the last
  links of a ring went unbuilt (`AiTownPlanTest`: scipio's ring left open in every
  run). Bisected with switches in one build, two to three runs each: removing the
  stop fixed both but let a body press 0.45 m into another for 0.72 s in the
  quality gate; keeping either half of it kept both failures. The stop now lasts
  only while the ladder has seen the body stand still for at most
  `k_stop_patience_seconds` (1 s); after that it presses on at the floor pace and
  the contact pass slides it past. Breach, quality gate, the gap and recovery
  suites and three walled-town runs all pass with it.
- **The front rank stopped short of the wall it attacked** (red since #1449). Two
  things held it back. #1449 clamped a soldier's step toward the opponent it is
  paired with to `spacing * 0.20` so a line fight does not skate, and a wall
  attacked in melee arrives through that pairing too: every soldier's contact
  offset read exactly that clamp. And the rank that faces a structure was measured
  from the combat layout's anchors, while a traversal layout had moved the soldiers
  a rank behind them, so no soldier counted as facing the wall. The facade rank is
  now measured where the soldiers stand, steps up to the facade (to the
  structure's contact clearance, at most `spacing * 1.6`, as the unpaired
  structure branch already allowed) and strikes it. Line fights keep the short
  pull.
- **An order held for eleven minutes.** In the observed skirmish one mounted unit
  accounted for 98 % of all `LocallyBlocked` samples: it stood on its own cell for
  674 s while every order it was given resolved to that cell. A route of no length
  is invalid, and the follower treated "invalid" as "a new route" on every tick,
  which restarted the wait at the obstruction every tick -- the 12 s give-up never
  fired and the Blocked marker never went away. A wait now restarts only when the
  route revision changes, and it ends with the order: giving the order up, or
  losing it, clears it (it used to survive the give-up in the unit's facts). Test
  `StuckRecoveryTest.AnOrderFromACellWithNoWayOutIsGivenUpNotHeldForever` (it held
  the order with a 0.017 s wait after 20 s before the fix).
- **The relay's palisades were off the wall lattice.** `ArenaScenariosTest.WallGroupsSitOnTheWallNetworkLattice`
  requires every wall piece on the two-metre lattice; the palisades sat at
  z = ±18.5. They are at ±18 now (36 m apart); the relay still passes.
- **A flaky audio test.** `AudioGameplayScenarioTest.ADistantBattleIsCarriedAsOneMassNotAsSilence`
  deals its blows once, and a cue whose only sound is still decoding is dropped,
  not deferred. In the full run the audio loader was still mastering music and
  ambience when the test fired. The test now waits for that sound to be ready.
- **`AiTownPlanTest` depended on machine load.** `AISystem::process_results` waits
  at most 4 ms (`k_default_decision_wait_budget`) for a decision; one that is not
  back from the worker by then is applied on a later update. Over fifty simulated
  minutes that makes a different town on a loaded machine.
  `EveryCommanderRaisesItsOwnTownFromAnEmptyField` passed five isolated reruns but
  failed inside the full `ai_tests` binary on this tree (scipio left soldiers by its
  barracks) and on its parent commit alike (hanno's wall in six runs, hasdrubal's
  soldiers adrift). The test now sets `set_decision_wait_budget` so every decision
  is waited for; the plan is judged, not the machine. (The speed-floor bisect
  above therefore used two to three runs per switch, never one.)

Tried and dropped: letting the one-lane contact rule keep the sideways part of a
push (for side-by-side bodies at a breach). It breaks
`AnArmyClimbsAHillThroughItsEntrance` and `AnArmyCrossesARiverOnTheBridgeDeck`.

### Measured on the final build

- **Every test.** `scripts/run-tests.sh` in the full profile (extended tests, and
  the verifier with two determinism runs per scenario), the QML design-system test
  and the two model harnesses.
- **Arena, against the baseline captures.** `maneuver_town_relay` passes (it had
  three unanswered orders, a unit inside a building and a missed destination);
  `stuck_town_streets` passes (permanent stall before). Worst soldier speed is
  5–9 m/s everywhere (up to 16 m/s and a 420 m/s teleport before); soldier frames
  turning faster than 200 °/s dropped in every scenario (relay 9.1 % → 5.3 %,
  countermarch 7.6 % → 2.1 %). Spontaneous unit-root reversals -- the root turning
  back with no new order in the last 3 s -- are 0–3 per scenario, each after a
  replan; the rest follow orders. Still failing as on the baseline:
  `path_building_alley`, `army_formation_narrow_gate` and
  `army_formation_obstacle_course` miss their destinations, the countermarch's
  west groups miss theirs, and the slalom's archers trip the arena's "went nowhere"
  rule. In the traces those units move at full pace the whole time; the arena
  rule measures progress toward the scenario destination while the unit follows a
  later order or a long detour.
- **Headless verifier trace.** `RepathChurn` 144 → 0. Up: `ObstructionNotEscalated`
  7 → 51 (queued units now wait instead of replanning), `BodyOverlap` 26 → 46 and
  `DirectionReversal` 12 → 27. The quality gate's budgets still hold.
- **Observed skirmish.** Samples with no progress for 1.2 s or more: 11.5 % → 0.2 %
  of active army samples. Most replans in one order: 7 → 2. No order ended
  unreachable. The blocked share was the frozen unit above.

### Open findings

- What enclosed the unit at (2.5, 83.5) on Sunbaked Terraces. Two runs put a unit
  there; its orders now end after the 12 s wait, but it still cannot move.
- The no-closer clock measures straight-line distance to the objective, so a unit
  walking a long detour at full pace climbs to a replan and a sidestep (15 such
  replans in the countermarch, 0–8 in the other scenarios).
- The `BodyOverlap` and `DirectionReversal` increases in the verifier trace are
  the cost of pressing on after a one-second stop; they are within the gate but
  should be driven down.
- The movement trace keeps the last order state of a unit whose movement gate is
  not route following, so a gated unit can read as `LocallyBlocked` in the trace
  without any recovery running.

## 11. Sixth loop (Sep 11): builders that do not set off

Adam started The Timber Levy, selected the builders and sent them somewhere: "they
do not start immediately they need time... they break layout... this should never
happen". Every suite was green. The real mission was driven with an action
fixture (`select_all`, `select_by_type builder`, three `move_to` orders) and the
movement trace, and each order was read for the time until the unit had moved
0.3 m, its root yaw per tick and its order state.

- **A reversal stood the gang still while the block swung round.** Sent the way
  its back was turned, a formation wheeled its rigid block round its centre at
  180 °/s and could not take a step until its heading error fell below 100 °.
  A twelve-man gang stood for 0.85 s, and its outer men were carried round the
  block at 7.2 m/s (walking pace 2.3 m/s) -- that is the "breaks layout". A
  formation now **faces about in place**: past the no-translation error the
  root turns through 180 ° at once and every root-local soldier position (the
  traversal slots, the presentation soldiers, fallen bodies) is negated with it,
  so no soldier moves in the world; the rear rank leads off. The turned frame
  lives in `UnitTraversalLayoutStateComponent::about_faced` and is applied where
  the canonical layout becomes soldier positions: slot targets, the presentation
  fallback, the anchor fallback and the renderer's fallback. Only a half turn is
  used: footprints, clearance and lateral extents all take the layout's x as the
  frontage, and a point reflection keeps them true where a quarter turn would
  not. Tests `TightGapNavigationTest.AGangOrderedBehindItselfFacesAboutAndSetsOffAtOnce`
  (0.85 s and 7.2 m/s before) and `AGangOrderedWellRoundItsShoulderSetsOffAtOnce`
  (125 °: 0.55 s before, threshold first set at 135 °).
- **Arrived and Recovering alternated every frame.** The mission set its gangs
  down on ground `is_movement_point_allowed` rejects. The recovery move walked
  them out, but a formation counts itself arrived within 0.9 m of its target, so
  over the last 0.9 m it arrived, stopped on the rejected ground, and was sent
  again the next tick. A unit no longer arrives where it cannot stand. Test
  `StuckRecoveryTest.AUnitSetDownOnAFootprintStepsOffOnceAndStandsThere` (five
  recovery orders in four seconds before). A start 0.55 m inside a footprint did
  not reproduce it; the start must be further than the arrival radius from open
  ground.
- **The mission put the gangs inside each other.** Starting units were laid on a
  fixed 1.2 m grid whatever their size: the four builder gangs overlapped by
  2.1-3.0 m and stood on unstandable ground, and the swordsmen stood 2.5-3.6 m
  inside the commander. Each starting unit is now set down on the nearest ring
  round its authored point where its footprint stands on open ground clear of
  every unit already on the field. Test
  `MissionStartupTest.StartingUnitsStandApartOnOpenGround`.
- **Every march ran at three quarters of its pace.** The motor pulled the body
  toward the steered velocity and applied drag against that same pull, so a
  body settled at `a / (a + 3)` of what its route asked: 1.52 m/s of 2.1 m/s for
  a builder gang in the trace, 2.37 m/s of 3 m/s in a test, and a correspondingly
  slow set-off. An order the route follower owns (player, formation, planner,
  attack-move) now closes on its velocity with no drag. Removing the drag for
  every body broke `WildlifeFightMotionTest.ASingleWolfClosesOnFleeingSheepAndFinishesTheHunt`
  (three bites for four) and `MeleeExchangeDuelTest.ASwordDuelKeepsItsDamageRateAndShowsEveryReaction`
  (1379 damage against a floor of 1448); an A/B switch in one build showed both
  pass with the old drag and fail without it. Orders whose issuer re-aims them
  every tick -- chases, scripted wildlife walks, guard returns, recovery --
  keep the damped drive they were tuned against. Test
  `CommandServiceTest.AnOrderedUnitMarchesAtItsOwnPace`.
- **The quality gate read an about-face as a body spinning.** The first full
  profile after the about-face failed `MovementQualityGateTest` in three capture
  scenarios (26, 39 and 51 findings against a budget of 20): every about-face
  was a 10 440 °/s body yaw rate and two angular-acceleration findings. The root
  frame flips; no body turns with it. The trace sample now carries
  `about_faced`, and the analysis measures the yaw step beyond a half turn on the
  tick the flag changes, so a real one-tick spin is still reported. Tests
  `MovementAnalysisTest.AnAboutFaceIsNotABodySpinningOnTheSpot` and
  `AHalfTurnInOneTickWithoutAnAboutFaceIsStillASpin`.
- **Two builder crews on one site could never start work.** With the about-face,
  `AiTownPlanTest.AWalledCommanderClosesItsCircuitGivenTime` failed for Scipio
  (one tower, ring open near -3.5,-34.5) in every run, and passed with the
  about-face switched off. The about-face was not wrong: it changed the timeline,
  and in the new one the AI assigned the second tower 30 times instead of 11. Its
  build trace showed crews giving up "0.20 m out" of the tower site, and the
  movement trace showed two crews frozen 0.26 m apart on the site centre for
  thousands of ticks. A site counted as reached only within 15 cm (walls had
  been widened to 1 m in an earlier fix), and the contact pass holds two crews
  further apart than that, so neither ever arrived. Every product now arrives
  within the same metre. Test
  `ProductionSystemTest.TwoCrewsHeldApartOnOneSiteBothStartWork`.
- **A quicker body walked into the back of a friend in a crowd.** Once player
  orders drove at their full pace, `MovementQualityGateTest` still failed in
  archers_vs_infantry (a body 0.91 m inside a friend for 3.5 s) and, under other
  timings, in bot_skirmish (0.66-0.90 m). Logging every contact push for the pair
  showed no rejected pushes and no exhausted budget on the body itself: the body
  behind kept its floor pace (0.35 of its speed) into a friend walking the same
  way, every push it got was against its own travel and was turned sideways by
  `slide_along_travel`, and the friend's own budget was spent on its crowd. A
  larger separation budget moved the failure to the other scenario. Avoidance
  now keeps a body pressed against a friend walking the same way (faster than
  0.2 m/s, within 60 degrees) to that friend's pace; it may still lean past, and
  a friend standing still keeps the wait-then-press rule. A hand-built test of
  two units on open ground did not reproduce it (the quicker one leans past), so
  the capture scenarios of the gate are the regression tests.

### Open findings

- **An order undone the tick after it was given, once.** In the run before these
  fixes, three of the four gangs received the player's second and third orders
  and, one tick later, fresh orders back to the slots of the order before
  (same tick for all three, a new route each, no repath recorded). The command
  queue dispatches each command once, a right click submits one `Move`, and
  army-formation groups are only committed by `DeployFormation`, so the issuer
  was not found by reading. With every order and dispatched command logged
  (`MovementSystem::issue_move*`, `CommandQueue::drain`), three further runs of
  the same fixture on the fixed build showed only the player's orders reaching
  the gangs. The difference between the runs is that the gangs no longer start
  overlapping on unstandable ground, but that is not proven to be the cause.

## 12. Seventh loop (Sep 11): sustained pace, no delays

Adam's bar for this loop: an order is acted on the same tick, a unit reaches
its pace within a few strides, and holds that pace wherever it walks — open
ground, a street, a one-cell gap, a corner, a re-ordered army — with nothing
slowing it that the player can see. `tests/headless/movement_pace_test.cpp`
measures exactly that on the simulation root: set-off tick (first tick at half
pace), full-pace tick (first at 90 %), share of under-way ticks below 90 %, and
the longest run under half pace, excluding the last 2.5 m of approach.

| Scenario (60 Hz ticks)         | before: set-off / full / below 90 % / crawl | after              |
| ------------------------------ | ------------------------------------------- | ------------------ |
| block ordered 90° sideways     | 13 / 22 / 3 % / 12                          | 4 / 7 / 1 % / 3    |
| block reversed mid-march       | 4 / 12 / 6 % / 12                           | 1 / 2 / 0 % / 0    |
| 4-, 3-, 2-cell streets, corner | detoured round the houses                   | 4 / 7 / 1 % / 3    |
| 30 men through a one-cell gap  | (never filed through)                       | 4 / 7 / 1 % / 3    |
| 12-unit army, three orders     | crawls of 50–178 ticks, 23–54 % below pace  | ≤ 40 ticks, ≤ 15 % |

What was throttling the root, and what replaced it:

- **The motor blended velocity exponentially** (`gain = 4·pace·dt`) and every
  route assignment zeroed it, so each re-order or repath restarted the ramp.
  The motor now slews linearly to the steered velocity in 0.12 s
  (`k_motor_ramp_seconds`) and a retarget keeps the velocity it has. Horses
  accelerate at 8 m/s² instead of 5.
- **Translation was scaled by heading error** (full at 20°, nothing at 100°),
  so a block ordered sideways stood and pivoted for 0.2–0.4 s and lost half
  its pace at every route corner. A block now _wheels_: it moves at full pace
  in a direction clamped to within 20° of its facing while the yaw turns
  (`wheel_within_free_band`). Single bodies were never throttled and are not
  touched. A reversal still faces about in place, and now also reverses the
  root velocity so no tick is lost; a 0.5 s cool-down stops a hairpin route
  from flapping the block back and forth.
- **The root heading was the route tangent at the aim point**, which jumps at
  every vertex and flips at a hairpin. It is now the direction to the aim
  point, which moves continuously.
- **Traffic slowed every follower by `blockage²`**, floored at 35 % of pace,
  chained leader-pace × heading-agreement down a column, and stopped a body
  dead for up to a second in any passage. `LocalAvoidanceSystem` is now a
  car-following rule: a body keeps its pace unless the gap to the body ahead
  (measured to that body's disc, not a lane box) is about to close, and then
  matches that body's pace along its own direction, never less. The gap may
  close faster over the time a crossing body needs to leave the lane. Only a
  standing body that does not give way, or a leader that has itself stopped
  making progress, can hold a body up, and then at the follow gap, not before.
- **The traversal layout held the root at 35 % for up to 3 s** in front of a
  pinch while the soldiers folded. The hold is gone. The fold starts early
  enough for the _front man_ of the folded column to be in place by the time
  _he_ reaches the pinch (`fold_lead_metres`), the width probe reaches that
  far, and the corridor bias keeps its longitudinal component so the column
  sorts itself front to back while it closes on the centre line. A soldier the
  root would carry into cover stays where it stood in the world and catches up
  once there is room; the comrades the carry would push into him hold with him.
  Creeping under a quarter step now counts as blocked so the sidestep and the
  squeeze (0.6 × floor, never under 0.25 m) actually fire, and a folded column's
  head never runs past the end of the route.
- **The pathfinder charged a flat 3-cell ring penalty × 6 × clearance** around
  every wall, so a block routed round a town rather than through a street it
  fitted. The penalty is now quadratic in the metres by which the body would
  overlap the nearest obstacle: nothing for a street the body fits, a little
  for a fold, prohibitive for a 6 m body at a 1 m gap.

Found while running every suite on the result:

- **A route that doubles back through the body froze it for 1.5 s.** The lane
  routes an AI wave gets can start with a 0.4 m spur and pass back through
  the spawn point; the lookahead aim then landed on the body itself and the
  desired velocity was zero until the stall ladder replanned. The follower now
  projects the body onto the _later_ pass when a route crosses it twice, and
  walks a degenerate aim forward to the first vertex actually ahead.
- **The new overlap cost sent the Trasimene wave the long way round the lake**
  (603 m -> 1129 m): a 20-plus-man roster's routing clearance is inflated to
  ~6 m, and paying a quadratic overlap on that turned every defile into a
  wall. The clearance used for costing is capped at 3 m
  (`k_max_cost_clearance`); the walkability radius was already capped at 1.5.
  A body narrower than a two-file block cannot fold, so for it the overlap is
  steep (`k_rigid_overlap_cost`) and it steps round a single blocked cell.
- **A standing block chased the aim ray as it was jostled in a press** (±4°
  a tick, the gate's `HeadingOscillation`). Facing the route chord instead
  cost the lateral correction and broke the gate and one-cell-gap tests; a
  block that is not moving simply does not turn for errors inside the free
  band.
- **Two bodies capped to zero inside one another stayed there** for 0.6 s in
  bot_skirmish because their separation budget went to shallower neighbours
  first. `BodyContactSystem` gathers every overlapping pair and resolves the
  deepest first.
- **Builder crews could no longer shoulder in beside a crew already on a
  site**, because the follow gap held them 0.25 m short of any body standing
  its ground; `AiTownPlanTest` raised 40 of 94 wall links. This loop gave
  builders a half-second press and left soldiers queueing behind any friend
  standing its ground -- which is exactly the blocking Adam saw in the game;
  §13 removes the queue for everyone.
- `FactionsAreEvenAtEqualCost` ran four seeds (eight battles); one flipped
  battle moved Rome to 6/8. Over 32 battles Rome wins 56 %; the test now runs
  the fixture's declared eight seeds.

Duplication removed in the same pass: the unused `assign_path_to_movement`,
three copies of the arrival bookkeeping in `RouteFollowSystem` (`settle_arrival`),
two copies of displacement publishing in the motor gates
(`publish_displacement`), the second heading-source branch in
`heading_reference`, and the three entity-object predicates in
`BodyContactSystem` now share `body_stands_its_ground`. The movement systems
read components through the registry (`world.try_get`), which took
`game/systems` from 952 entity-object accesses to 897 and both scan ratchets
under budget.

## 13. Eighth loop (Sep 12): a friend is never a wall

Adam, playing the Sep 12 build: "WHY ARE MY TROOPS BLOCKING OTHER TROOPS
MOVEMENT .... I SAID MILLION TIMES THAT IT SHOULD NEVER EVER HAPPEN". He is
right, and the review's own rule from §3 ("a body is never brought to a stop
by traffic") had been eroded twice: loop 2 let avoidance stop a body touching
a neighbour it could not lean round, and loop 7 made every body queue at the
follow gap behind any friend that was not plain idle -- a friend holding
ground, at work, with a target, or simply one that still held an order.
Anywhere a body cannot lean (a street the block fills, a gate, a bridge, a
hill entrance, a wall gap) that was a full stop.

Five new pace scenarios in `movement_pace_test.cpp` pin the contract: a block
past a friend holding ground in a three-cell street; a block through a
one-cell gap a friend is standing in; a block through a crowd of idle friends;
two blocks meeting head-on in a two-cell street; a four-unit column past a
friend holding ground. On the loop-7 build the friend in the gap held the
block for the entire run (787-tick crawl, then the stall ladder walked it
away) and the head-on pair cost the higher id a 38-tick stop.

`LocalAvoidanceSystem` now:

- **never caps speed for a body that is not under way.** Standing friends get
  the lean, and where there is no room the body walks on; the contact pass
  nudges an idle friend aside (`yields_when_idle`) and lets anything that
  stands its ground be walked through, since in a one-lane passage it pushes
  nobody backwards.
- **only follows a friend walking its way** (`heading_agreement > 0`). An
  oncoming or crossing friend gets the lean and nothing else.
- **floors the follow cap at `k_min_speed_fraction`** unless the leader's
  motor sweep is blocked (`progress.blocked_steps`), i.e. the ground has
  stopped it. A leader that is merely slow, turning or making no progress no
  longer stops the column behind it.
- **breaks a mutual yield by id** for any converging pair, not just head-on.
  Without it the army re-order scenario had pairs converging at 40-50° both
  crawling at the floor for a second (60-91 ticks); with it the longest crawl
  is back under the 48-tick bar.

Two things had to follow, both found by the ai_tests gate and bisected with
env switches in one build:

- **The contact pass no longer pushes a body walking into a standing friend.**
  With the queue gone, every body pressing at a melee line was shoved sideways
  by half a metre a second and its aim ray swung with each shove: 61-153
  `HeadingOscillation` findings in archers_vs_infantry and mixed_formation,
  and a yaw-acceleration spike count over budget in the cavalry charge, all
  of which went to zero with the queue switched back on. Now a standing friend
  can only ease a mover on along the mover's own travel, never back or
  sideways: an idle friend takes the whole separation and steps out of the
  way, a friend standing its ground is walked through by a body moving into
  it, a body with no travel yet is settled out of it as before
  (`LocalAvoidanceTest.ContactCorrectionSettlesWithoutMovingALockedFighter`),
  and a standing _enemy_ keeps the old rule (the mover is stopped, an idle one
  is nudged a third). The same change is what lets `AiTownPlanTest` close
  Scipio's ring again: with the old shares two crews on one wall site shoved
  each other off it.
- **A jostled block holds its yaw inside the free band.** What the contact
  pass does push (two bodies both under way share the overlap) still swung
  the heading a few degrees a tick; the motor now treats a block that was
  pushed on the previous tick like one standing still and does not turn for
  errors inside `full_translation_heading_error_degrees`. For that the
  avoidance pass has to carry the contact pass's three fields (`contact_push`,
  `body_overlap`) through its per-tick reset instead of zeroing them.

Measured on the result: all eight capture scenarios of
`MovementQualityGateTest` at zero findings, the thirteen pace scenarios, the
gate, tight-gap, siege and stuck-recovery suites green, and in the real game
(Hold the Sallow Ford, film mode, swordsmen parked and holding in the cart-gate
lane) the spearman blocks ordered through them set off in 4-7 ticks and
crossed the line without a single tick under half pace.

Gone with it: `blocked_by_standing_body`, the builder press
(`k_press_patience_seconds`), the leader stall clock
(`k_leader_stall_seconds`), and avoidance's reads of hold mode, builder and
order state. The queue-patience clock in the route follower still counts a
`Yielded` steering result, so a body following a slower friend does not climb
the stall ladder.
