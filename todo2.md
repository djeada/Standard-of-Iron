# RTS locomotion recovery: pathfinding, movement, formation traversal, and render truth

This is the second recovery plan. [`todo.md`](todo.md) remains the plan for RPG
mode; this file owns normal RTS movement for individual troop entities, selected
groups, the soldiers rendered inside each troop entity, and every ground marker
attached to those soldiers.

The current result is not shippable. Arrival alone is not success. A unit that
eventually reaches a target while walking in place, alternating its heading,
snapping its soldiers into a column, intersecting props, or leaving selection
rings behind has failed.

The repair order is mandatory:

```text
truthful traces and rendered reproductions
  -> one movement state model and one coordinate contract
  -> reliable single-unit routing and motor integration
  -> stable avoidance and group corridor coordination
  -> continuous width-aware soldier traversal layouts
  -> locomotion driven by accepted displacement
  -> one final rendered soldier anchor shared by body and markers
  -> permanent regression, soak, determinism, and performance gates
```

Do not add movement polish, new formations, new terrain traversal exceptions, or
new avoidance constants before the relevant earlier gate is green. Disable or
bypass presentation behavior that hides a defect while its owner is being
repaired.

## Product contract

RTS locomotion is a primary game mechanic and must satisfy all of these rules:

- An accepted move order ends in exactly one declared outcome: arrived,
  intentionally queued/yielding, unreachable, cancelled, or superseded. It
  never remains active forever without measurable route progress.
- A unit never plays walk or run indefinitely while gaining no ground.
- A steady route never makes the unit or its soldiers flick left and right.
  Navigation, avoidance, body facing, and animation use compatible direction
  signals with explicit hysteresis.
- Starts, stops, reversals, corners, repaths, and arrival are continuous. No
  system teleports or silently rewinds a root to repair its own bad step.
- The path planner and the continuous motor agree about obstacle coverage,
  clearance, corner rules, terrain portals, and the shape that is moving.
- Open ground is direct and calm. Obstacles create only the deviation needed to
  avoid them. A unit does not hunt between equally valid sides of the same tree.
- Friendly crowds make deterministic progress. Units queue, yield, merge, and
  separate without vibration, overlap, starvation, or permanent deadlock.
- A selected group preserves stable member identity and a readable overall
  shape while there is room. Chokepoints create an orderly traversal and an
  equally orderly reform, not a pile on a shared waypoint.
- The soldiers inside one troop entity keep stable slot identities. A narrow
  area may change the number of files only when geometry requires it; it must
  use the maximum safe width, preserve faction character where possible, and
  transition continuously.
- Single-file is a last physical consequence of a genuinely one-body-wide
  passage, not the default answer for every constrained sample and not a sudden
  re-layout of a 30-soldier unit.
- A soldier mesh, its shadow/contact anchor, selection ring, hover ring,
  picking proxy, and any soldier-level target marker consume the same final
  presented ground anchor for that frame. Ring-to-soldier X/Z error is zero by
  construction, not merely visually small.
- Behavior is deterministic for a fixed command stream and simulation step.
  Increasing render FPS changes smoothness only; it does not change routing,
  arrival, layout choice, or movement outcome.
- The solution remains viable at battle scale. Correctness gates are never
  disabled by LOD, batching, animation throttling, or large-unit-count modes.

## Terms used in this plan

The codebase has two different formation scales, and confusing them will create
another partial fix:

| Term                         | Meaning in this plan                                            | Current examples                                                       |
| ---------------------------- | --------------------------------------------------------------- | ---------------------------------------------------------------------- |
| **Soldier**                  | One rendered person/creature inside a troop entity              | A single `FormationSoldierPresentation` slot                           |
| **Troop entity**             | One selectable simulation entity that may render many soldiers  | One swordsman or spearman unit with `MovementComponent`                |
| **Selected group**           | Several troop entities receiving one player/AI order            | `CommandService::move_units`                                           |
| **Unit layout**              | Soldier positions inside one troop entity                       | `UnitLayoutSystem`, testudo, shield wall, traversal reflow             |
| **Army formation**           | Troop-entity slots inside a selected group                      | `ArmyFormationPlanner`, `ArmyFormationRegistry`                        |
| **Route corridor**           | Stable high-level path plus width/portal metadata               | To replace copying raw leader waypoints as the complete group solution |
| **Motor result**             | Accepted displacement and velocity after steering and collision | Must become the only source for motion presentation                    |
| **Presented soldier anchor** | Final per-frame X/Z root used to submit one soldier             | Must also place that soldier's selection/hover marker                  |

## Static audit of the current working tree (2026-08-23)

This section records code-level findings, not a claim that the current binaries
have reproduced every symptom below. Milestone 0 must create fresh runtime
artifacts from the same build before implementation starts.

### Movement and path ownership conflicts

1. `MovementComponent::vx/vz` currently means too many things. Navigation uses
   it as integrated velocity, `LocalAvoidanceSystem` overwrites it with its
   correction, orientation reads it, motion presentation reads it, and
   animation derives direction from it. Desired, steered, accepted, and
   presented velocity are not separate facts.

2. Runtime order places `LocalAvoidanceSystem` before `MovementSystem` in the
   Movement phase. Avoidance modifies the prior velocity, then movement
   accelerates/damps that value toward the waypoint and performs collision.
   There is no immutable desired-velocity input and no published accepted motor
   result with which to verify the solver.

3. Local avoidance is overlap-reactive rather than predictive. It acts only
   after circles overlap, has no time-to-collision horizon, stable passing side,
   lane ownership, portal queue, or deadlock state, and averages all overlap
   corrections before clamping them. Symmetric crowds can alternate equally
   plausible escape directions.

4. The current same-formation/friendly-formation exclusions can skip avoidance
   across broad sets of friendly formation members. Ignoring friendly collision
   is not coordination; it permits overlap and moves the visual repair burden
   downstream.

5. Group movement can calculate one leader corridor and assign that same raw
   corridor to many troop entities. Per-member connectors exist only when
   simple segment checks pass, and there is no lane offset, occupancy schedule,
   or stable queue at a portal. Members can therefore converge on identical
   waypoints and ask local avoidance to invent group coordination after the
   fact.

6. Ordinary group target placement builds a square from selection order. A
   blocked candidate can fall back to the same center target used by other
   members. The terrain-fitted army-formation planner has stronger slot rules,
   but a normal multi-select move does not share that complete placement
   contract.

7. A* uses formation/body clearance primarily as a cost bias while direct
   segment tests use it as a hard swept-radius check. Continuous movement then
   validates the next root point at zero clearance and falls back to separate X
   or Z axis movement. Planning, path shortening, and integration can therefore
   disagree about a route and about the step that is legal on it.

8. When both integrated axes are blocked, movement zeroes velocity. It does not
   publish a blocked contact normal or immediately advance a deterministic
   repath/recovery state. Repeating the same waypoint can reproduce the same
   failed step.

9. The current stuck timer resets after 0.15 m of displacement in any direction,
   not after forward progress along the route. Lateral orbiting or back-and-forth
   movement can keep an order alive. At the timeout it stops the order rather
   than producing a staged repath, portal queue, or explicit unreachable result.

10. Heading for a formation may come from the current target at one distance
    and raw velocity near it. Local avoidance, waypoint advancement, replanning,
    and axis rejection can all change those inputs. There is no stable route
    tangent/accepted-velocity rule with angular hysteresis.

11. Pending path requests carry a navigation revision but the processing path
    does not use that revision to reject or refresh a stale result. Order/goal
    checks prevent some stale work, but the request does not prove that it was
    solved against the topology it will traverse.

### Narrow-passage and soldier-layout conflicts

12. `MovementSystem::update_traversal_presentation` infers available width by
    probing laterally from the current root orientation on the current tick.
    It does not own a stable corridor/portal identity, entry lookahead, exit
    boundary, tail-clear condition, or state hysteresis. A small yaw or sample
    change can toggle the inferred mode.

13. `MotionPresentationComponent::traversal_lateral_scale` is smoothed, but
    `publish_formation_presentation` decides whether to reflow from the
    instantaneous target scale. Entering reflow replaces row/column/local
    positions immediately. Leaving it returns immediately to the normal layout
    while the lateral scale may still be easing.

14. When reflow is active, the normal per-soldier position blend is skipped.
    This is the direct code path for an instant transition into a new set of
    rows/files. The current headless test verifies separation and eventual
    restoration, but not the transition velocity or visual continuity.

15. The current reflow calculates `files_that_fit` from one width sample and
    linearizes live-slot order into that many files. In a narrow sample this
    commonly becomes one file, regardless of head count, authored silhouette,
    upcoming width, or acceptable aspect ratio.

16. Traversal layout is presentation-only while combat geometry retains the
    normal unit layout. That can be a deliberate abstraction only if its rules
    are explicit. At present it means rendered soldier positions, collision
    coverage, target positions, and combat positions can describe different
    bodies during a passage.

### Render, animation, and marker truth conflicts

17. `FormationPresentationComponent` publishes soldier-local target positions.
    The renderer then independently moves soldier roots through
    `SoldierTurnSmoothingState::world_x/world_z`. This makes the render path a
    second positional simulation.

18. Combat root motion and recoil can shift `inst_ctx.model` after the turn
    smoothing anchor has been calculated. `soldier_world_pos` reflects that
    final shift, but the selection-ring path normally reads the earlier turn
    smoothing anchor. The ring can therefore be internally “correct” according
    to its helper test and still not sit under the final submitted soldier.

19. `build_selection_ring_layout` also has a separate trigonometric fallback
    from unit root plus published local slot. A stale/missing render anchor can
    therefore put the ring at a third plausible coordinate instead of making a
    missing final-anchor contract visible.

20. Motion presentation prefers component velocity as its direction even when
    actual displacement was collision-rejected or substantially different. The
    gait can be told to walk toward a direction the motor did not accept.

21. A no-progress unit is eventually classified idle, but the navigation order
    may remain active. This hides the walk-in-place symptom after a grace period
    without resolving the route state that caused it.

22. Render-side soldier relocation may force a walk animation even when the
    troop root is idle. That can be correct during a real layout transition,
    but it needs an explicit transition velocity and completion contract, not a
    renderer-inferred distance to an independently moving slot.

### Existing gates are insufficient

23. Arena `MovementIsContinuous` currently detects a root step that is too
    large. It does not fail zero progress, alternating heading, excessive
    angular acceleration, repeated replan, repeated waypoint regression,
    collision rejection, or walk animation without accepted displacement.

24. `NoRootTeleport` observes rendered soldier roots only under diagnostics and
    checks a maximum step. It does not compare the body root with the selection
    ring or require a monotonic, bounded layout transition.

25. Selection-ring tests pass synthetic turn anchors into the ring helper. No
    current gate records the final body draw anchor and marker draw anchor from
    the same rendered frame and compares them slot by slot.

26. Tight-gap tests explicitly accept a render-only squeeze and validate only
    that bodies remain separated and eventually cross. They do not reject an
    instant normal-to-column transition, one-frame exit, bad 30-soldier aspect
    ratio, or repeated mode toggles.

## Execution rules

- [ ] Keep this file as the live checklist. Move enduring contracts into
      `docs/PATHFINDING_ARCHITECTURE.md`, `docs/FORMATION_ARCHITECTURE.md`,
      `docs/ANIMATION_ARCHITECTURE.md`, and `docs/RENDERING_ARCHITECTURE.md` only
      after the implementation and gates agree.
- [ ] Do not close a task from an isolated helper test when the defect was
      visible only in an integrated/rendered path.
- [ ] Every defect fix must first add a failing trace assertion or rendered
      scenario that reproduces the actual symptom.
- [ ] Keep simulation decisions deterministic. Sort neighbors, requests,
      members, and tie breaks by stable IDs; never use wall-clock timing or
      unordered iteration as movement policy.
- [ ] Record the seed, command sequence, map/topology revision, fixed step,
      render cap, build type, commit, graphics preset, and unit composition in
      every artifact directory.
- [ ] Run rendered scenarios sequentially. Concurrent Arena processes make
      frame-time and continuity reports untrustworthy.
- [ ] Do not tune around one troop type. Infantry, cavalry, elephants, siege,
      civilians/builders, and mixed-speed selections need declared behavior.
- [ ] No LOD or large-battle mode may change authoritative movement, traversal
      layout state, final selected-soldier anchors, or gate verdicts.
- [ ] A milestone is green only at 30, 60, 120, and 144 Hz presentation over the
      same 60 Hz simulation input, plus unlocked/VRR where the runner supports
      it.
- [ ] Preserve replay/save-load determinism and explicit system phase order as
      the component contracts change.

## Gate metrics and initial thresholds

These are failure thresholds, not tuning targets. If a threshold proves
physically inappropriate, change it with an artifact and rationale before
changing implementation constants.

| Metric                                    | Required result                                                                                                                                |
| ----------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| Accepted open-ground order                | 100% arrive within route-length / minimum-speed budget plus 1.0 s                                                                              |
| Active order with no declared queue/block | No interval over 0.35 s with route progress below 0.03 m                                                                                       |
| Persistent obstruction                    | Repath/queue/unreachable transition begins within 0.50 s; no indefinite active move                                                            |
| Final arrival                             | Velocity settles below 0.03 m/s and locomotion becomes idle within 0.20 s; no restart without a new cause                                      |
| Straight-route heading                    | No alternating sign changes over 3 degrees on consecutive simulation ticks                                                                     |
| Major direction reversal                  | No more than one uncommanded reversal over 15 degrees in any 0.50 s window                                                                     |
| Route progress                            | Remaining route arclength may not regress over 0.50 m for more than 0.50 s unless state is `Yielding` or `Repathing`                           |
| Static collision                          | Zero blocked-cell occupancy and zero positive swept-body penetration after recovery budget                                                     |
| Dynamic body overlap                      | No overlap above 2 cm after the bounded solver settle window; no growing overlap while both units advance                                      |
| Fairness                                  | Every non-cancelled member crosses a portal or receives an explicit unreachable outcome; no starvation in a 90 s crowd run                     |
| Layout mode stability                     | One enter and one exit per physical passage; no toggling inside hysteresis bands                                                               |
| Soldier layout transition                 | No instantaneous X/Z slot jump; anchor speed and acceleration remain below authored transition limits                                          |
| Slot identity                             | No live soldier changes stable slot ID or crosses another soldier solely because files/ranks change                                            |
| Selection/hover ring                      | Same final per-frame X/Z values as the submitted living soldier root; test uses exact shared anchor identity plus <= 1e-4 diagnostic tolerance |
| Animation truth                           | No walk/run state after 0.20 s of accepted speed below 0.03 m/s; no idle state after 0.20 s above 0.20 m/s                                     |
| Render-rate parity                        | Same command outcome, route revision sequence, portal order, and layout mode sequence at every presentation cap                                |
| Determinism                               | Identical movement digest across repeated runs of the same binary and configuration                                                            |

For layout transitions, define per-archetype speed/acceleration limits from the
soldier's locomotion capability. Do not hard-code one global value that makes
infantry pop and cavalry crawl.

## Required trace before repairs

Add one opt-in `MovementTrace` stream. It must be cheap when disabled and must
not use logging text as its data format. JSONL or a compact binary stream with a
JSON manifest is acceptable.

### Per-tick troop record

- [ ] Simulation tick, entity ID, owner, troop type, command sequence, order kind,
      and declared movement outcome/state.
- [ ] Root position/yaw and previous root position/yaw.
- [ ] Requested goal, resolved goal, route ID/revision, topology revision,
      waypoint index/count, current waypoint, lookahead point, route tangent,
      and remaining route arclength.
- [ ] Desired speed/velocity, avoidance correction, steered velocity, collision
      contact/normal, accepted displacement/velocity, and rejected displacement.
- [ ] Distance advanced along the route, lateral route error, no-progress time,
      blocked-step count, repath count/reason, and queue/yield owner.
- [ ] Neighbor candidates in stable ID order, time-to-collision, selected passing
      side, priority, lane/portal reservation, and solver result.
- [ ] Navigation envelope radius/half-width, sampled corridor width, portal ID,
      traversal mode, current/target file count, transition progress, and state
      dwell timers.
- [ ] Motion-presentation state, speed, direction, direction source, gait state,
      and locomotion phase.

### Per-rendered-frame soldier record

- [ ] Presentation frame, source simulation ticks, interpolation alpha, troop ID,
      stable soldier slot, alive/culled/LOD flags, and layout transition state.
- [ ] Previous/current authoritative soldier anchors, interpolated layout anchor,
      turn/facing result, combat root offset, final submitted body root, contact
      shadow root, selection/hover ring root, and picking proxy root.
- [ ] Position error for every consumer relative to the final anchor.
- [ ] Selected animation clip/blend, gait speed/direction, phase, planted-foot
      world positions, and whether layout relocation contributes locomotion.

### Artifact analysis

- [ ] Add an analyzer that reports progress stalls, distance regressions,
      blocked-step streaks, replans, waypoint churn, heading sign flips, angular
      velocity/acceleration, layout toggles, layout aspect ratio, soldier root
      jumps, ring error, gait mismatch, collision penetration, and starvation.
- [ ] Produce a compact summary table and a timeline plot for the worst entity
      and worst soldier. A video is supporting evidence, not the only evidence.
- [ ] Store the first failing tick/frame and a short window before and after it
      so the cause can be inspected without opening a multi-minute trace.
- [ ] Add a movement digest covering command outcome, accepted root samples,
      route revisions, portal order, traversal modes, and stable slot mapping.

## Target ownership model

The final flow must have one owner at every boundary:

```text
Move order + stable member/slot assignment
                 |
                 v
Route corridor (polyline, width, portals, revision)
                 |
                 v
Route follower -> immutable desired velocity + stable tangent
                 |
                 v
Predictive avoidance / queue policy -> steered velocity
                 |
                 v
Swept motor + static/dynamic collision -> accepted MotorResult
                 |
        +--------+---------------------+
        |                              |
        v                              v
Progress/outcome state       Motion presentation snapshot
                                       |
                                       v
Stable unit-layout transition (previous/current soldier anchors)
                                       |
                                       v
Per-frame interpolation + allowed combat root offset
                                       |
                                       v
Final PresentedSoldierAnchorBuffer
        |              |              |              |
        v              v              v              v
 soldier body     ground shadow   selection ring   picking/target marker
```

The following data must not be recomputed downstream:

- route side and route tangent;
- accepted root displacement;
- traversal mode and stable soldier slot mapping;
- final interpolated soldier X/Z anchor;
- whether movement is locomotion, layout relocation, forced displacement, or
  no movement.

## Milestone 0: reproduce and freeze the baseline

- [ ] Build the current working tree once in a named RelWithDebInfo directory.
      Do not mix artifacts from an older binary with the source audit above.
- [ ] Add temporary diagnostic-only Arena expectations as needed; do not repair
      behavior in this milestone.
- [ ] Reproduce the reported symptoms with selection rings visible and full
      creature detail forced.
- [ ] Capture at least one entity that walks/animates without route progress.
- [ ] Capture at least one entity whose root or soldiers alternate heading on a
      steady command.
- [ ] Capture a 30-soldier troop entering and leaving passages that fit one,
      two, three, and four files.
- [ ] Capture selected soldiers through normal travel, traversal-layout change,
      turn smoothing, combat root motion, and arrival. Record final body/ring
      anchor error.
- [ ] Run the existing pathfinding, tight-gap, local-avoidance, formation,
      motion-presentation, soldier-turn, selection-ring, and relevant Arena
      suites and record which broken rendered cases they currently accept.
- [ ] Save traces and images as the immutable “before” set used by later
      milestone comparisons.

### Mandatory baseline scenes

- [ ] One infantry troop: 5 m, 30 m, diagonal, S-curve, 90-degree corner,
      180-degree reversal, repeated click at the same goal, and click while
      already moving.
- [ ] One cavalry, elephant, siege engine, builder, and 30-soldier infantry troop
      on the same route.
- [ ] One troop around an isolated tree, a boulder, a building corner, two close
      trees, a staggered grove, and a building alley.
- [ ] Selected groups of 2, 10, 30, and 100 troop entities on open ground.
- [ ] The same groups through a gate, bridge, hill entrance, forest road, and
      cluttered settlement.
- [ ] Same-direction merge, perpendicular crossing, opposing streams, overtaking
      a slow unit, stopped friendly wall, mixed body sizes, and moving combat
      traffic.

### Gate 0

The baseline is complete only when every reported symptom has a deterministic
scenario, a machine-readable failure, and a saved visual artifact. “Could not
reproduce” requires the exact build/configuration and three clean attempts; it
does not remove the scenario from the plan.

## Milestone 1: one state model and explicit contracts

### Separate movement facts

- [ ] Replace overloaded velocity semantics with explicit data, whether as one
      component or narrow components:

    - `RouteIntent`: order/goal/route identity and desired progress;
    - `DesiredMotion`: route tangent and desired velocity before avoidance;
    - `SteeringResult`: deterministic avoidance/queue output;
    - `MotorResult`: accepted displacement/velocity, contacts, and blocked state;
    - `MovementProgress`: route arclength, no-progress state, attempts, outcome;
    - `MotionPresentation`: presentation-only state derived from `MotorResult`.

- [ ] Give each field one writer. `LocalAvoidanceSystem` must never overwrite
      the authoritative velocity later used as if it were already integrated.
- [ ] Make schedule dependencies explicit and test them: route following before
      steering, steering before motor, motor before progress/presentation, and
      presentation before render snapshot publication.
- [ ] Add a monotonically increasing order sequence. Async/pending route work
      must match entity, order sequence, goal revision, and topology revision
      before it can publish.
- [ ] Define terminal outcomes and expose them to tests/diagnostics. Clearing
      `has_target` without a reason is not an outcome contract.

### One spatial contract

- [ ] Document world/grid conversion, cell bounds, obstacle rasterization,
      body/envelope radius, arrival tolerance, and inclusive/exclusive contact
      rules in one navigation contract.
- [ ] Make point walkability, swept segment validation, A* neighbor validity,
      string pulling, motor collision, target snapping, and recovery use the
      same passability source and corner convention.
- [ ] Distinguish a troop's normal formation envelope from its permitted transit
      envelope. Path cost may prefer normal-width routes while a valid transit
      envelope still permits a genuine choke.
- [ ] Treat trees, stones, buildings, walls/gates, bridges, hill entrances,
      forests, and dynamic blockers through declared obstacle categories. No
      renderer-visible solid prop may silently be absent from movement coverage.

### Gate 1

Compile-time/system-access tests prove one writer per movement stage. A trace of
one tick can account for every transition from order to accepted displacement,
and no renderer input reads desired velocity as actual motion.

## Milestone 2: reliable single-troop route following and motor

### Route representation and stability

- [ ] Replace “waypoints only” with a route object that contains a stable ID,
      topology revision, polyline segments, cumulative arclength, clearance/width
      metadata, portal spans, and an explicit final arrival region.
- [ ] Project the root onto the current route segment and advance monotonically
      by segment/arclength. Do not use only distance-to-waypoint circles that can
      be re-entered from alternating sides.
- [ ] Use bounded lookahead along route arclength. Blend corner tangents over a
      distance appropriate to turn radius; never flip directly between grid
      staircase legs.
- [ ] Preserve a valid route across small moving-target changes and root motion.
      Repath only for a material goal change, invalid remaining segment,
      topology change, declared deadlock escalation, or failed recovery.
- [ ] Keep a stable side around an obstacle until the chosen branch is cleared.
      Equal-cost alternatives need deterministic tie-breaking and route-retain
      hysteresis.
- [ ] Validate and consume path-request topology/order revisions. Discarded work
      must not stop or reset current movement.

### Swept motor

- [ ] Integrate one accepted planar displacement from steered velocity using a
      swept body/envelope, not endpoint point sampling.
- [ ] Return contact time, contact normal, accepted fraction, penetration, and
      remaining displacement. Slide against the actual contact plane instead of
      trying global X and Z axes independently.
- [ ] Substep only from a declared maximum travel/curvature bound so hitches and
      fast bodies cannot tunnel through thin cells or corners.
- [ ] Make acceleration, deceleration, maximum speed, turn rate, and arrival
      braking explicit per archetype. Heading restrictions may reduce forward
      speed, but must publish `Turning` rather than masquerade as failed travel.
- [ ] Resolve invalid-start recovery as a bounded state with a safe target and
      progress metric. It may authoritatively eject/snap only after a logged
      smooth-recovery budget is exhausted.

### Progress watchdog as a state machine

- [ ] Measure forward route arclength, accepted displacement, distance from the
      current segment, and motor contacts. Do not reset from arbitrary lateral
      displacement.
- [ ] Use explicit escalation:

    1. `Following` — normal progress;
    2. `LocallyBlocked` — hold stable direction/queue briefly;
    3. `Repathing` — one revisioned route request;
    4. `Recovering` — invalid-start or local ejection path;
    5. `Unreachable` — stop, idle, and publish reason.

- [ ] Bound attempts and time in every state. A failed repath cannot reissue the
      same first step forever.
- [ ] A unit may visibly idle while queued, but its order state must say
      `Yielding/Queued`, not “walking with velocity.”

### Gate 2

Every single-troop baseline scene arrives or declares a correct terminal result.
There are zero indefinite active moves, zero blocked-cell penetrations, zero
uncommanded branch flips, and zero locomotion frames unsupported by accepted
root/layout displacement.

## Milestone 3: predictive avoidance and crowd coordination

### Deterministic local solver

- [ ] Build the neighbor set from predicted swept circles/envelopes and a bounded
      time horizon, not only current overlaps.
- [ ] Feed it immutable desired velocity and return a separate steered velocity.
      The motor remains the authority that accepts or rejects displacement.
- [ ] Sort constraints by stable entity ID and explicit priority. Specify the
      exact tie break for symmetric encounters.
- [ ] Add stable left/right passing preference with hysteresis. Keep the chosen
      side until separation and route projection prove the encounter is clear.
- [ ] Preserve a minimum forward-progress objective when a collision-free
      velocity exists. Pure lateral oscillation is not a valid solution.
- [ ] Define interactions for same selected group, same army formation, allied
      unrelated traffic, stopped units, enemies outside melee lock, siege,
      elephants, and builders. Do not implement coordination by broadly ignoring
      friendly bodies.
- [ ] Add deterministic bounded overlap correction for bad initial conditions.
      It must be separated from ordinary steering and may not teleport roots.

### Portal queues and lanes

- [ ] Annotate gates, bridges, hill entrances, alleys, and inferred narrow
      corridor spans in the route.
- [ ] Reserve an entry lane/order before bodies overlap at the mouth. Use stable
      arrival order plus priority; record every grant, yield, and release.
- [ ] Keep enough following distance for body/envelope size and braking. A queue
      must stop before the choke, not oscillate inside its walls.
- [ ] Support same-direction batching and deterministic counterflow phases where
      two streams cannot pass. Prevent permanent starvation with a bounded
      fairness rule.
- [ ] Release a reservation only after the moving envelope and the tail of the
      soldier traversal layout clear the portal exit.

### Gate 3

All crowd scenes complete with zero starvation, unresolved penetration, or
alternating avoidance side. Repeated runs have the same portal order and movement
digest. Open-ground groups do not wobble because crowd logic is armed nearby.

## Milestone 4: selected-group and army-formation movement

### Stable member assignment

- [ ] Canonicalize selection members by stable entity identity, while preserving
      existing army-formation slot identity where it exists.
- [ ] Generate destination slots through one terrain fitter with uniqueness,
      reachability, body separation, and explicit `Valid/Adjusted/Blocked`
      results. Never send several members to one fallback center.
- [ ] Minimize member crossing when assigning current members to destination
      slots. Preserve the mapping across repaths and small destination edits.
- [ ] Use the same high-level corridor for coherence, but give each member a
      route lane/offset and valid start/exit connector. Copying identical leader
      waypoints is not a complete group route.
- [ ] Derive group facing from the stable corridor tangent/formation intent,
      never from whichever member's avoidance velocity changed most recently.

### Group shape lifecycle

- [ ] Define `Formed`, `Opening`, `Traversing`, `Reforming`, `Disrupted`, and
      `Arrived` from measured member/slot state.
- [ ] Preserve formation on open ground within a bounded slot error. Allow a
      declared controlled break at a portal instead of forcing all members onto
      an impossible wide envelope.
- [ ] Pace faster members relative to route/slot error without making the whole
      group repeatedly start and stop.
- [ ] Reform progressively after the last relevant member clears the portal.
      Destination arrival waits for each valid member slot, not just the group
      centroid.
- [ ] Mixed-speed selections either use a declared cohesion pace or split into
      deterministic subgroups. The rule must be visible in the order trace.

### Gate 4

Groups of 2, 10, 30, and 100 troop entities move directly on open ground, route
around clutter without member crossing/churn, queue through every portal, and
reform with stable identities. Every accepted member arrives; no group passes
because only its centroid or leader succeeded.

## Milestone 5: continuous soldier layout through constrained space

This milestone concerns soldiers inside one troop entity. It must not be hidden
inside the combat contact publisher.

### Authoritative traversal-layout state

- [ ] Introduce one `UnitTraversalLayoutState` owner containing route/portal ID,
      normal layout ID, traversal mode, current and target file count, stable
      slot mapping, entry/exit progress, transition curve, and dwell timers.
- [ ] Move traversal-layout policy out of
      `publish_formation_presentation`/combat contact code into a dedicated
      system that runs from route corridor metadata and the accepted troop pose.
- [ ] Measure upcoming usable width along the route tangent over the complete
      unit envelope. Center the sample on the corridor, not transient body yaw.
- [ ] Look ahead far enough to finish narrowing before the head reaches the
      passage. Keep the mode until the last soldier/tail clears an explicit exit
      margin.
- [ ] Add separate enter/exit width thresholds, minimum dwell, and tail-clear
      hysteresis. One physical passage produces at most one enter and one exit.

### Width-aware shape ladder

- [ ] Choose the maximum safe number of files from usable width, body diameter,
      minimum separation, and faction/layout constraints.
- [ ] Use a declared ladder such as `Normal -> Narrow ranks -> Marching order ->
Single file`. Skip only modes that physically cannot fit.
- [ ] Add an aspect-ratio/depth policy for large head counts. If a very narrow
      route would make an unacceptable 30-soldier shape and a reasonable wide
      detour exists, path cost should prefer the detour. If the single-body
      passage is the only route, use an orderly stable column because physics
      requires it, not because every constrained sample maps to one column.
- [ ] Preserve recognizable faction spacing/stagger/arc traits within the safe
      width instead of replacing every doctrine with the same generic grid.
- [ ] Keep casualties in stable slots through transition; never compact/redeal
      all survivors merely because the file count changes.

### Continuous slot mapping

- [ ] Compute a deterministic target slot for every stable soldier ID while
      minimizing crossings and preserving front-to-back order.
- [ ] Interpolate previous to target anchors with a C1-continuous curve and
      per-archetype speed/acceleration caps. Do not skip smoothing during reflow.
- [ ] Drive transition progress by actual allowed relocation, not only elapsed
      time, so a blocked soldier does not visually pass through a prop or another
      soldier to meet a deadline.
- [ ] Enforce minimum soldier separation and obstacle clearance throughout the
      entire interpolation, not only at endpoints.
- [ ] Publish previous/current soldier anchors and actual relocation velocity as
      presentation facts. Renderer code must not independently chase the same
      target in world space.

### Simulation/render/combat policy

- [ ] Decide and document whether traversal anchors participate in combat,
      soldier targeting, and picking. The preferred contract is one simulation
      presentation anchor set for all soldier-level spatial queries; if combat is
      intentionally disabled/deferred inside a portal, make that an explicit
      state rather than using the normal combat layout at different coordinates.
- [ ] Ensure the troop collision/transit envelope matches the active soldier
      layout closely enough that visible soldiers never pass through solid
      trees, stones, or buildings while the root remains legal.
- [ ] Keep defensive layout, combat stance, army formation, and traversal layout
      as independent inputs with an explicit precedence/composition table.

### Gate 5

One-, two-, three-, and four-file passages select the widest safe mode. A
30-soldier troop enters before the passage, changes continuously with stable
slot identities and separation, holds until its tail clears, and reforms once.
There are no instant local-position replacements, layout toggles, obstacle
intersections, or generic-column choices where multiple files fit.

## Milestone 6: locomotion and facing driven by accepted motion

### Troop-root presentation

- [ ] Build motion presentation from `MotorResult.accepted_velocity` and accepted
      displacement. Desired/steered velocity may appear in diagnostics but may
      not assert that the body moved.
- [ ] Select direction from accepted velocity above a threshold, otherwise the
      stable route tangent while intentionally turning, otherwise body forward.
      Add angular hysteresis and rate limits appropriate to the archetype.
- [ ] Distinguish `Idle`, `Turning`, `Walking`, `Running`, `Yielding`,
      `Recovering`, and `ForcedDisplacement` where animation needs a different
      visual response.
- [ ] Arrival braking and blocked/yield states must fade gait once and remain
      stable. Collision rejection cannot alternate walk/idle each tick.
- [ ] Preserve gait phase through start/stop/run blends. Repeated move commands
      that retain the route must not reset locomotion phase.

### Soldier locomotion

- [ ] Compose troop accepted velocity with actual layout relocation velocity for
      each soldier. This single per-soldier ground velocity chooses locomotion
      direction, speed, and gait.
- [ ] Remove render-inferred positional relocation as a hidden source of walk.
      Any soldier walking to a new slot must have a published transition anchor
      and velocity.
- [ ] Advance stride phase from accepted travel distance/cadence so a blocked
      body does not cycle its legs in place. Preserve bounded phase continuity
      through small speed changes and route corners.
- [ ] Use turn-in-place or body-turn shaping when angular motion exists without
      translational motion. Do not fake forward walking to visualize a turn.
- [ ] Test forward, diagonal, lateral avoidance, backtracking recovery, sharp
      route corner, layout relocation, queue stop/start, and reform for infantry,
      cavalry, siege, and elephants.

### Gate 6

No mandatory scene contains walk/run without accepted root or layout travel,
idle while moving, consecutive direction flip-flops, phase resets from retained
orders, or visible foot cycling during a prolonged block. Locomotion verdicts are
identical at every render cap.

## Milestone 7: one final rendered soldier anchor and exact markers

### Presentation interpolation

- [ ] Preserve previous/current simulation root pose and previous/current
      soldier anchors in the detached render snapshot.
- [ ] Resolve interpolation alpha once per presented frame. Root body, soldier
      anchors, shadows, rings, picking proxies, activity markers, and diagnostics
      must consume that same sample.
- [ ] Interpolate yaw through the shortest declared arc and preserve stable slot
      identity. Never interpolate vector indices that were reassigned.
- [ ] Reset interpolation explicitly on spawn, load, ownership transfer, death
      removal, teleport/debug placement, and render-snapshot replacement.

### Final anchor buffer

- [ ] During creature preparation, create one
      `PresentedSoldierAnchorBuffer` keyed by stable soldier slot. It contains
      the exact final world root after unit interpolation, layout transition,
      facing, permitted combat root motion/recoil, and terrain grounding.
- [ ] Build the soldier model matrix from this anchor or build both from one
      shared function. Never calculate the marker by repeating unit-root plus
      local-offset math.
- [ ] Submit selection and hover rings from the same buffer after preparation.
      Remove the dependence on `SoldierTurnSmoothingState` as a marker position
      proxy.
- [ ] Decide ground-marker behavior for combat lunges/recoil. To satisfy this
      contract, the ring follows the soldier's final ground X/Z while remaining
      terrain-grounded in Y.
- [ ] If a selected living soldier lacks a final anchor, fail diagnostics and
      either submit both body and ring from one explicit fallback anchor or
      submit neither. Never silently put only the ring at a separately calculated
      slot.
- [ ] Keep buffer production available for selected/hovered soldiers at every
      LOD. Representative far-formation thinning must not leave a marker attached
      to an undrawn or different soldier.

### Exact regression tests

- [ ] Extend preparation tests to exercise turn smoothing, traversal transition,
      combat lunge/recoil, terrain grounding, casualties, LOD, and snapshot
      transfer, then compare the model root and buffer anchor.
- [ ] Record actual soldier and ring draw commands in a rendered Arena frame and
      assert the same slot key and bit-identical X/Z source values.
- [ ] Add selected 30-soldier passage captures with rings visible at 30/60/120/144
      Hz and with full, simplified, and minimum supported selected-unit LOD.
- [ ] Test first frame, one-frame snapshot lag, paused frame, resize, load, and
      offscreen/re-entry so stale anchors cannot revive.

### Gate 7

Every living selected soldier body and marker share the same final anchor key and
X/Z values in every rendered frame. There are zero fallbacks to independent
formation math in a normal selected-unit frame and zero visible ring slips in
turning, layout transition, combat root motion, or arrival.

## Milestone 8: permanent scenario and CI coverage

### Headless integration matrix

- [ ] Single troop: every archetype, eight headings, short/long route, start,
      stop, reversal, corner, repeated command, arrival, blocked goal, invalid
      start, dynamic obstruction, and topology change.
- [ ] Static clutter: isolated tree/stone, paired obstacles, dense random field
      with fixed seed, building corner, alley, wall gate, forest edge, road,
      bridge, hill entrance, and shoreline boundary.
- [ ] Crowds: 2/10/30/100 troop entities; merge, cross, counterflow, overtake,
      stationary wall, mixed radii, mixed speed, two allied formations, and
      enemy traffic before melee lock.
- [ ] Internal soldier counts: 1, 2, 6, 14, 30, casualties, defensive layouts,
      one/two/three/four-file portals, and a narrow region that should be
      bypassed by a wider route.
- [ ] Assert outcomes, progress, route stability, collision, fairness, stable
      slot mapping, traversal lifecycle, and deterministic digest—not only final
      position.

### Rendered Arena scenarios

- [ ] `rts_locomotion_open_ground` — straight, diagonal, start/stop, reversal,
      and repeated retained command with selected rings.
- [ ] `rts_locomotion_obstacle_slalom` — trees, stones, and buildings with route,
      root, direction, and gait traces.
- [ ] `rts_locomotion_corner_torture` — tight building corners and alternating
      turns without axis-stick or heading chatter.
- [ ] `rts_group_open_formation` — 30 troop entities preserve stable slots and
      calm headings on free space.
- [ ] `rts_group_forest_road` — group merges onto a road between dense solid
      props, passes, and fans out.
- [ ] `rts_group_counterflow_gate` — deterministic fair portal scheduling.
- [ ] `rts_unit_layout_width_ladder` — one 30-soldier troop traverses authored
      one/two/three/four-file widths with one enter/exit each.
- [ ] `rts_unit_layout_abort_and_resume` — passage order is cancelled/reversed
      during narrowing and reforming without a pop or slot swap.
- [ ] `rts_selection_ring_anchor_lock` — full-frame final body/ring anchor
      equality through movement, turns, traversal reflow, combat recoil, and
      arrival.
- [ ] `rts_crowd_soak` — mixed archetypes and intersecting orders for ten minutes
      with no progress stall, starvation, oscillation, or memory growth.

Every rendered locomotion scenario must enable expectations for:

- order outcome and group arrival;
- no progress stall;
- no blocked-step repetition;
- no route/waypoint churn;
- no heading oscillation;
- bounded angular velocity/acceleration;
- no collision penetration;
- gait matches accepted motion;
- no soldier root teleport;
- traversal mode/file stability;
- stable soldier slot mapping;
- exact body/ring anchor equality;
- frame-time and simulation-time budgets.

### Frame rate, determinism, and lifecycle

- [ ] Replay identical command streams at 30/60/120/144 Hz presentation and
      unlocked, while simulation remains fixed. Compare movement digests.
- [ ] Repeat each headless scenario at least three times in one binary and
      compare digests; then compare debug and release behavioral verdicts.
- [ ] Save/load during route following, portal queue, layout enter/hold/exit,
      crowd yield, arrival braking, and combat-root-offset frames.
- [ ] Pause/unpause and change speed during those same states.
- [ ] Destroy a goal, blocker, queue leader, formation member, and selected troop
      during movement; no stale route/request/anchor may survive its identity.
- [ ] Run ASan/UBSan, TSAN where viable, the full normal suite, content
      validation, replay verification, and packaged renderer self-test before
      closing the recovery.

### Performance gates

- [ ] Profile 100, 500, 2,000, and 4,000 troop entities with representative
      internal soldier counts. Report route requests, neighbor constraints,
      portal queues, motor solves, layout updates, presentation preparation, and
      marker submission separately.
- [ ] Keep route sharing at the corridor-data level without sharing mutable
      progress/waypoint state between members.
- [ ] Bound neighbor count/time horizon, solver iterations, route requests per
      tick, trace buffer size, and layout updates per tick.
- [ ] Cache static corridor width/portal metadata by topology revision. Never
      cache a dynamic body reservation as static navigation data.
- [ ] Trace-disabled cost must be measured and negligible. Trace-enabled gates
      may be slower but must not change behavioral digests.
- [ ] Re-run `massed_battle_1000`, `massed_battle_2000`, and production-LOD
      battle benchmarks after each ownership or layout change.

### Gate 8

The movement gate runs from one command and exits nonzero on any behavioral,
rendered-anchor, determinism, lifecycle, or declared performance failure. It is
required in CI for movement/path/formation/render changes and in the release
candidate checklist.

## Implementation map

Likely existing files to change when this plan is executed:

| Responsibility                                 | Existing areas                                                                                                   |
| ---------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| Movement state/results and snapshots           | `game/core/component.h`, `game/core/world.{h,cpp}`                                                               |
| Route assignment/following/motor               | `game/systems/movement_system.{h,cpp}`, `game/systems/movement_orders.cpp`                                       |
| Static path/corridor/clearance                 | `game/systems/pathfinding.{h,cpp}`, `game/systems/nav_grid.*`, `game/systems/building_collision_registry.*`      |
| Steering and crowd policy                      | `game/systems/local_avoidance_system.{h,cpp}`                                                                    |
| Runtime order/access declarations              | `game/systems/runtime_system_registry.cpp`, core system schedule tests                                           |
| Normal selected-group targets                  | `game/systems/command_service.{h,cpp}`, `app/orders/movement_utils.cpp`                                          |
| Army-formation movement                        | `game/formation/army_formation_*`, `game/systems/formation_move_dispatch_system.*`                               |
| Soldier layout geometry/state                  | `game/formation/unit_layout.*`, `game/formation/unit_layout_state_system.*`, a dedicated traversal-layout system |
| Current traversal/combat presentation coupling | `game/systems/combat_system/formation_contact_processor.cpp`, `game/systems/formation_combat_geometry.*`         |
| Render formation instances                     | `render/entity/formation_instance_layout.*`                                                                      |
| Final soldier model/root                       | `render/humanoid/runtime/instance_prepare.cpp`, `render/humanoid/runtime/soldier_turn_smoothing.*`               |
| Animation inputs/gait                          | `render/gl/humanoid/animation/animation_inputs.*`, humanoid locomotion runtime/selection files                   |
| Rings and frame submission                     | `render/selection_ring_layout.h`, `render/scene_walk.cpp`, creature preparation result types                     |
| Tests                                          | `tests/systems/`, `tests/headless/`, `tests/formation/`, `tests/render/`, `tests/tools/`                         |
| Rendered scenarios/analyzer                    | `tools/arena/arena_scenario*`, `tools/arena/arena_formation_scenarios.*`, new focused movement scenarios         |

Suggested new narrow types/files are allowed, but do not create a second complete
movement stack. Prefer small types around the one pipeline: route corridor,
desired motion, steering result, motor result, movement progress, traversal
layout state, and final presented soldier anchors.

## Explicitly rejected patch patterns

- [ ] Do not only increase the stuck timeout or displacement epsilon.
- [ ] Do not call “stop and idle after N seconds” a pathfinding repair.
- [ ] Do not lower the animation threshold until walking in place is hidden.
- [ ] Do not add random steering noise to break symmetric crowds.
- [ ] Do not let avoidance and movement alternately overwrite the same velocity.
- [ ] Do not disable all friendly collision or all local avoidance in portals.
- [ ] Do not copy one leader's complete waypoint list to every member and rely on
      separation at the choke.
- [ ] Do not make the navigation grid globally wider/looser to fix one prop or
      body type.
- [ ] Do not slow the existing instant column swap with a generic lerp while file
      choice still toggles every tick.
- [ ] Do not use one-file columns for every narrow sample.
- [ ] Do not calculate selection rings with another copy of soldier-layout math.
- [ ] Do not use a pre-root-motion or prior-frame smoothing position as the final
      marker anchor.
- [ ] Do not accept screenshots without time-series assertions.
- [ ] Do not accept tests that prove arrival while ignoring route time,
      direction churn, gait mismatch, layout continuity, or marker drift.
- [ ] Do not weaken a gate because the current architecture cannot expose the
      necessary fact; expose the fact.

## Definition of done

- [ ] All Milestone 0 reproductions are fixed without disabling their assertions.
- [ ] There are zero known P0/P1 RTS routing, motor, avoidance, group movement,
      traversal layout, locomotion, interpolation, or marker-anchor defects.
- [ ] Every accepted individual and group order ends in a declared bounded
      outcome, with 100% arrival in all reachable mandatory scenarios.
- [ ] Open ground is direct and visually calm for every archetype and group size.
- [ ] Dense static clutter and dynamic crowds produce stable, deterministic,
      collision-safe progress with no starvation or direction chatter.
- [ ] A 30-soldier troop uses width-appropriate layouts, transitions before and
      after a passage without snapping, and never reassigns live soldier identity.
- [ ] Walk/run/turn animation always agrees with accepted root plus layout
      displacement and remains phase-continuous across commands and repaths.
- [ ] Selection and hover rings are exactly attached to the final presented
      soldier anchors in every frame and supported LOD.
- [ ] Results and movement digests match across presentation frame rates,
      repeated runs, replay, and save/load.
- [ ] The complete movement gate, sanitizer passes, full test suite, content
      validation, and declared battle-scale performance budgets are green.
- [ ] Architecture documentation describes the implementation that actually
      passed these gates, and the gate is required for future relevant changes.

This recovery is complete only when moving units are boring to debug: one order,
one stable route/queue decision, one accepted motor result, one continuous
soldier layout, and one final coordinate consumed everywhere it is drawn or
selected.
