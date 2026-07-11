# Production RTS Battlefield Architecture

## Purpose

This document replaces the previous RPG-only combat plan. The current target is a
credible early-2000s-scale RTS battlefield: formations move as organized bodies,
soldiers turn and accelerate believably, ranged ranks fire under explicit doctrine,
melee forms a readable contact line, casualties produce reactions and stable reflow,
and gameplay events stay synchronized with animation.

This is an architectural migration, not a request to tune constants or add visual
exceptions. Existing commander combat work remains useful, but it becomes one client
of a shared action and presentation model.

## Product Scale and Design Standard

The goal is a full-scale early-2000s RTS, not a soldier simulation. The game should
create convincing mass behavior, readable battles, responsive orders, and synchronized
combat for hundreds to roughly one thousand visible soldiers on target hardware. It
does not need modern crowd fidelity, per-foot navigation, physically simulated weapon
collisions for every distant unit, or a designer control for every solver coefficient.

Spend complexity where the player can perceive or command it: group movement and
silhouette, readable front lines, responsive orders, visible combat causality, distinct
unit roles, stable frame time, and deterministic outcomes. Approximate below that
level. A distant strike may use an authored contact event rather than a sampled blade
trace; a formation may use one shared corridor rather than one optimal path per soldier.

## Architectural Principles

### Four primary abstractions

The battlefield model should expose four concepts to the rest of the game:

1. `TacticalGroup`: members, formation, anchor, order, doctrine, and cohesion.
2. `SoldierAgent`: group slot, local motion, engagement assignment, health, and action.
3. `Engagement`: a relationship between groups with a contact front and soldier pairs.
4. `Action`: an authored movement/combat/reaction transaction with timed events.

Navigation, slot matching, avoidance, kinematics, hit queries, and animation sampling
are mechanisms behind these concepts. They must not become independent gameplay models
that other systems manipulate directly.

### Declarative policy, encapsulated mechanism

Use declarative definitions for choices a designer should understand and reuse:
formation archetype, movement class, engagement doctrine, fire doctrine, action set,
reaction set, animation graph, and unit archetype composition.

Keep algorithms and numerical solver internals in C++: pathfinding, slot assignment,
avoidance, kinematic integration, front construction, spatial queries, hit detection,
event scheduling, and replay determinism. Data chooses a named policy and a few
meaningful parameters; it must not reproduce an algorithm as a large configuration.

### Prefer capability composition over unit-type branching

Generic runtime systems operate on capabilities and resolved definition IDs such as
`movement_profile_id`, `formation_profile_id`, `action_set_id`, and `doctrine_id`.
Avoid growing `switch (spawn_type)` blocks across movement, combat, and rendering.
Special units receive a narrow capability or action, not a parallel pipeline.

### One owner per transition

- Orders choose intent; they do not write velocity.
- Group planning chooses anchors and slots; it does not move transforms.
- Steering chooses desired motion; kinematics alone commits transforms.
- Actions alone advance attack and reaction timing.
- Hit resolution alone changes health from combat contact.
- Presentation only consumes snapshots.

### Build the smallest complete vertical slice

Prove the design with line infantry moving, turning, meeting another line, fighting,
dying, and filling front vacancies. Generalize only when archers, spears, or cavalry
demonstrate a real variation. Do not implement every policy or transition in advance.

## Player-Observed Failures

1. Troops rotate abruptly or in implausible directions while moving.
2. Locomotion and transitions look clunky and can disagree with displacement.
3. On contact, one ranged soldier fires while others remain grounded or idle.
4. Enemy melee attacks sometimes apply damage without a visible attack animation.
5. The front rank dies while rear ranks remain far away.
6. Both armies can remain visually separated while simulation-level fighting continues.
7. Survivors reform toward the rear or around stale positions instead of maintaining a
   readable front.

These are coupled failures. They cannot be solved independently without creating more
state disagreement.

## Verified Current Architecture

### Runtime order

`app/core/renderer_bootstrap.cpp` currently registers:

`MovementSystem -> LocalAvoidanceSystem -> PatrolSystem -> GuardSystem -> CombatSystem`

Important consequences:

- Movement commits transforms before combat chooses chase/stop/face intents.
- Combat movement requests generally take effect on the next simulation tick.
- `EngagementSlotSystem` is implemented but not registered.
- `TargetCommitmentSystem` is implemented but not registered.
- `MovementIntentComponent` exists but is not the authoritative input to movement.
- There is no explicit formation/cohesion update system in the runtime loop.

### Formation commands are one-shot destinations

`game/systems/formation_planner.h` calculates final per-unit points and facing angles.
`CommandService::issue_ground_move()` writes desired yaw and calls `move_units()`.
After that, units own independent paths and velocities in `MovementComponent`.

There is no persistent formation entity or runtime state containing:

- group identity and membership,
- formation anchor and orientation,
- stable rank/file slot,
- cohesion and straggler state,
- shared corridor/path,
- speed matching,
- casualty reflow policy,
- combat frontage.

The planner also sorts units into destination geometry but does not solve a minimum-cost
assignment from current positions to stable slots. A new order can therefore induce
crossing paths and surprising turns.

### Movement and turning are per soldier

`game/systems/movement_system.cpp` steers each entity directly toward its current
waypoint. Velocity uses a proportional acceleration followed by damping. Facing is
then derived from velocity and clamped to `720 deg/s` for ordinary infantry. There is
no angular velocity, turn radius, turn-in-place state, formation heading, or speed
reduction based on heading error.

This permits soldiers to translate strongly sideways while their model catches up,
and sharp waypoint changes can produce rapid visible snaps. Final formation yaw is
only used after velocity becomes negligible because velocity-facing takes precedence.

### Local avoidance is non-functional

`game/systems/local_avoidance_system.cpp` detects overlaps and computes `sep_x` and
`sep_z`, then discards both values. It only updates diagnostics. It cannot prevent
overlap, crossing, jitter, or congestion.

Even if those values were applied directly, pairwise separation alone would fight
formation slot following. Avoidance needs a constrained steering contract that
preserves group intent and gives engaged units different rules from marching units.

### Combat independently selects individuals

`game/systems/combat_system/attack_processor.cpp` performs target validation, nearest
enemy search, chase destination creation, stopping, facing, mode selection, melee lock,
attack cadence, projectile spawning, and damage scheduling in one large processor.

The principal RTS melee relationship is `AttackComponent::in_melee_lock` plus one
`melee_lock_target_id`. Reciprocal locking is attempted, but battle frontage and rank
support are not represented. Chasing uses spread angles around individual targets,
not opposing formation fronts.

`EngagementSlotSystem` allocates eight radial slots around an individual target only
after melee lock, but it is not running and its anchors are not consumed by movement.
It would not by itself solve line-vs-line combat.

### Damage and presentation use parallel timing models

Normal RTS melee damage is deferred with fields on `AttackComponent`:

- `has_pending_melee_strike`,
- `pending_melee_target_id`,
- `pending_melee_elapsed`,
- `pending_melee_contact_time`.

Animation is inferred separately from attack cooldown, melee lock, attack target,
`CombatStateComponent`, render-time transaction state, and action manifests. This can
produce simulation attacks without a matching visible transaction, especially when
targets change, die, or leave range during the cycle.

Ranged fire similarly spawns arrows when the combat cooldown fires. Formation volley
intent, draw/release events, rank permissions, and animation readiness are not one
authoritative transaction.

### Casualties are scalar health, not formation vacancies

The formation planner is invoked for commands, not continuously for battlefield
casualties. A dead front soldier does not create a first-class vacant rank/file slot.
Rear soldiers have no advance-to-fill policy and the group has no current front edge.
Individual target chasing can pull some soldiers forward while others retain old move
destinations, creating the large visual gap reported in playtesting.

## Desired Battlefield Model

### Declarative definition model

Definitions are immutable during a match and referenced by stable IDs. Prefer the
existing typed manifest style under `animation/` initially; JSON or YAML is a storage
choice, not an architecture requirement.

Keep the catalog deliberately coarse:

- `FormationProfile`: shape family, preferred frontage, spacing class, turn style,
  and reflow policy.
- `MovementProfile`: infantry/cavalry/large-body kinematic class and animation graph.
- `EngagementDoctrine`: active ranks, reach/support, replacement, and disengagement.
- `FireDoctrine`: free fire, ripple volley, synchronized volley, or hold fire.
- `ActionDefinition`: clip, duration, events, constraints, and hit/release contract.
- `UnitArchetype`: profile/action references plus combat statistics and capabilities.

Use named presets plus a small override surface. A unit may select
`infantry_standard` and override speed; it should not configure angular acceleration,
damping, avoidance horizons, blend thresholds, and every arrival coefficient.

Every schema requires a stable ID and version, explicit defaults, startup validation,
cross-reference checks, an immutable resolved representation, save/replay compatibility,
and debug output identifying the selected definition. Do not add inheritance deeper
than one preset plus overrides. Do not add a general scripting language, behavior tree,
or node editor during this migration.

### 1. Fixed simulation phases

Replace implicit registration-order behavior with named phases and a fixed simulation
step (for example 30 Hz):

1. `Orders`: consume player and AI commands.
2. `GroupPlanning`: update formation anchors, paths, facing, doctrine, and desired front.
3. `SlotAssignment`: preserve or reassign stable march/combat slots.
4. `Steering`: combine path, slot, avoidance, and combat approach into desired motion.
5. `Kinematics`: apply acceleration, turn limits, terrain constraints, and transforms.
6. `Engagement`: allocate opposing frontage pairs and reserve ranged lanes.
7. `Actions`: start/advance authored attacks and cross animation events.
8. `HitResolution`: validate contact/projectile events and apply damage/status.
9. `CasualtyAndMorale`: mark deaths, release slots, update group integrity.
10. `PresentationSnapshot`: publish immutable interpolation and animation state.

Rendering must never infer whether gameplay attacked. It consumes the snapshot.

### 2. Persistent tactical groups

Introduce a persistent `TacticalGroup` runtime owned by one battlefield group module.
Do not encode large dynamic group state inside every soldier component.

Minimum group state:

- stable `group_id`, owner, troop role, and ordered member IDs,
- current and desired anchor position,
- current and desired heading,
- formation template, width, depth, spacing, and doctrine,
- shared path/corridor and current path segment,
- stable slot table with rank/file and local offsets,
- current speed cap and cohesion envelope,
- state: `Forming`, `Marching`, `Turning`, `Holding`, `Approaching`, `Engaged`,
  `Disengaging`, `Routing`,
- opposing group IDs and combat-front geometry,
- dirty flags for membership, terrain, command, and casualty changes.

Each soldier receives a small `GroupMemberComponent` containing `group_id`, stable
`slot_id`, role, and temporary detached/engaged state.

Formation commands move the group anchor. Soldiers follow transformed local slots.
This is the central change needed to make an army read as an army.

### 3. Stable slot assignment and casualty reflow

Slots must survive orders and ordinary steering. Reassignment occurs only for explicit
reasons: formation change, member death, blocked slot, split/merge, or excessive
straggling.

Use deterministic stable assignment when a group forms or changes shape. Begin with a
bounded rank-preserving nearest assignment. Introduce Hungarian/min-cost matching only
if replay metrics show crossing that the simpler method cannot solve. The abstraction
is stable assignment; the solver remains replaceable.

Casualty policy:

- A dead member vacates a specific slot immediately.
- Reflow is delayed briefly so the death remains readable.
- The nearest suitable soldier from the next rank advances into that column.
- Reassignment propagates rearward, preserving left/right ordering.
- The formation front anchor does not jump backward because the front rank died.
- Width shrinks from the flanks only when rank population falls below a configured
  threshold.
- Engaged soldiers are pinned to combat slots and do not reflow until their action can
  safely transition.

### 4. Formation-aware navigation and steering

Pathfind the group anchor/corridor, not a separate global path for every soldier.
Individual soldiers receive a local target derived from group anchor, heading, and
slot. Detached units may request local recovery paths.

Create a `SteeringRequest`/`SteeringResult` boundary:

- Inputs: preferred velocity, preferred facing, max speed/acceleration/turn rate,
  slot error, neighbor circles, terrain corridor, movement mode, priority.
- Outputs: constrained velocity and facing intent.

Avoidance must be velocity-based and predictive, with constraints for slot cohesion.
Evaluate a proven ORCA/RVO implementation, but keep it behind the steering boundary so
a simpler deterministic solver can ship if it meets acceptance and performance goals.
Do not expose solver coefficients per unit archetype.

Movement quality rules:

- Slow translation when heading error is large.
- Use explicit turn-in-place below a speed threshold.
- Give cavalry a minimum practical turn radius and anticipation distance.
- Use arrival curves with acceleration/deceleration limits, not frame-dependent
  proportional acceleration plus damping.
- Keep formation anchor speed limited by lagging percentile, not the single slowest
  transient member.
- Permit controlled compression near obstacles, followed by deterministic expansion.

### 5. One authoritative motion state for animation

Publish a simulation `MotionSnapshot` per soldier after kinematics:

- previous/current transform for render interpolation,
- planar velocity and acceleration,
- signed local forward/lateral speed,
- angular velocity and heading error,
- grounded state and slope,
- movement mode (`Idle`, `Start`, `Walk`, `Run`, `Stop`, `TurnLeft`, `TurnRight`,
  `Strafe`, `Knockback`),
- gait phase seed and continuity token.

Animation selection must use actual local motion, not only `has_target`, attack target,
or nominal unit speed. Add transition graphs with authored blend durations and phase
matching for idle/start/loop/stop and walk/run changes. Preserve gait phase across clip
changes where compatible. Root motion is not required for RTS locomotion, but clip
speed must be warped within a bounded range to match displacement and prevent foot
sliding.

Simulation transforms remain authoritative. Rendering interpolates fixed ticks and
does not feed pose state back into movement.

### 6. Formation-vs-formation engagement

Replace nearest-individual melee convergence with a two-level allocator:

1. `GroupEngagementPlanner` pairs opposing groups and computes contact fronts from
   anchors, headings, widths, terrain, and weapon reach.
2. `CombatSlotAllocator` creates facing slot pairs along those fronts and assigns only
   eligible soldiers.

Soldier engagement states:

- `Reserve`: holds formation slot.
- `AdvancingToFront`: moves to an allocated combat slot.
- `Ready`: in range and facing tolerance.
- `Acting`: owns an active combat action.
- `Recovering`: remains committed until safe release.
- `ReplacingCasualty`: fills a released front slot.
- `Disengaging`: returns through a reserved corridor.

Only front-rank soldiers, or ranks permitted by weapon reach, may enter melee. Rear
ranks advance as slots open. This prevents simulation combat across a visible gap.

Engagement validity must require physical conditions: compatible slot pair, weapon
reach, facing tolerance, line of approach, and target alive. A logical target ID alone
is insufficient.

### 7. Shared authored combat actions

Extend the existing `game/systems/combat_actions/` architecture to ordinary RTS
soldiers instead of maintaining cooldown-driven pending strikes.

An action definition owns:

- action and weapon identity,
- full-body/upper-body clip,
- duration and interrupt policy,
- movement/facing constraints,
- events such as `Windup`, `Release`, `TraceStart`, `TraceEnd`, `Impact`, `Recover`,
- hit shape/socket data,
- damage/stagger profile,
- target and formation eligibility,
- follow-up/cooldown policy.

An `ActiveCombatActionComponent` owns runtime action ID, target, elapsed time, event
cursor, hit suppression, and completion/cancellation reason. Gameplay and rendering
read the same action clock.

For melee, apply damage only from an authored contact event/trace while the pair is in
a valid combat slot. For ranged attacks, spawn the projectile only at the authored
release event. Remove `pending_melee_*` after all RTS attackers migrate.

### 8. Ranged formation doctrine

Ranged behavior must be group-authored rather than each archer independently firing
whenever cooldown permits.

Support at least:

- `FreeFire`: individual cadence with deterministic phase staggering.
- `RippleVolley`: rank or file waves.
- `SynchronizedVolley`: group draw, hold, and release window.
- `HoldFire` and `FireAtWill` orders.

The planner determines which ranks have line of fire. Friendly soldiers and terrain
must participate in line-of-fire checks. Rear ranks either use an authored overhead
trajectory or do not fire; they must not visually shoot through the front rank.

Every firing soldier starts a visible bow action. Deterministic small offsets avoid
robotic synchronization while preventing the current one-active/all-idle appearance.

### 9. Death, hit reaction, and action interruption

Damage resolution publishes a `CombatOutcomeEvent`. A dedicated reaction system maps
it to hit reaction, stagger, knockdown, or death action. Death must cancel locomotion,
release engagement and formation occupancy, and start a non-looping death clip in the
same simulation tick.

The presentation snapshot retains the corpse pose until cleanup policy removes it.
Enemy and friendly units use the same path. No renderer-side team-specific inference
is allowed.

## Module Boundaries

Do not create one globally registered system for every responsibility below. Organize
the implementation into four cohesive modules aligned with the primary abstractions:

- `BattlefieldGroupModule`: group orders, formation geometry, slots, navigation, and
  casualty reflow.
- `AgentMotionModule`: preferred motion, avoidance, kinematics, and motion snapshots.
- `EngagementModule`: group fronts, soldier pairing, reserves, and fire control.
- `ActionModule`: combat/reaction actions, events, hit requests, and outcomes.

Each module may contain internal processors, but exposes one narrow input/output
contract. The scheduler coordinates phases instead of allowing processors to mutate
one another's internal state.

| Responsibility | Module owner | Existing code to migrate |
|---|---|---|
| Group membership, anchor, slots, corridor | `BattlefieldGroupModule` | one-shot formation/per-unit paths |
| Preferred motion, avoidance, transforms | `AgentMotionModule` | direct chase/no-op avoidance/movement |
| Opposing fronts, pairs, fire control | `EngagementModule` | nearest targets/melee locks/cooldowns |
| Attack and reaction lifecycle | `ActionModule` | pending strike/render inference |
| Damage | `CombatHitResolver` | direct calls in attack processor |
| Presentation snapshots | motion/action modules | render-time inference |

## Migration Roadmap

### Phase 0: Capture, metrics, and acceptance scenes

- [ ] Add a deterministic headless scenario runner with fixed seed and fixed timestep.
- [ ] Add replayable scenes: 20v20 infantry approach, 20 archers vs infantry, mixed
  formation, casualty reflow, cavalry turn/charge, narrow passage, commander in line.
- [ ] Record per tick: transforms, group/slot IDs, target/action IDs, action events,
  damage, death, and animation state.
- [ ] Add debug overlays for group anchors, formation slots, paths, preferred/actual
  velocity, combat fronts, engagement pairs, weapon range, and line of fire.
- [ ] Add performance counters and a 60-second soak test.
- [ ] Define visual acceptance captures at 30 and 60 render FPS from the same replay.

Exit criteria: every reported symptom is reproducible without manual play and can be
measured from logs or captures.

### Phase 1: Definition contracts and vertical slice

- [ ] Add typed schemas and registries for the six coarse definition types.
- [ ] Author only the presets needed for standard sword infantry and line formation.
- [ ] Validate IDs, clips, events, capabilities, and cross-references at startup.
- [ ] Build a 10v10 line-infantry slice through group, motion, engagement, action, hit,
  death, and reflow contracts.
- [ ] Keep existing systems behind a scenario flag until the slice is complete.
- [ ] Reject fields or components serving only one incidental implementation detail.

Exit criteria: the complete scenario uses the new contracts, and adding a second
sword-infantry archetype requires definitions rather than generic runtime branches.

### Phase 2: Explicit simulation pipeline

- [ ] Introduce named world phases and fixed-step accumulation.
- [ ] Move render interpolation to immutable previous/current snapshots.
- [ ] Register all systems through the phase scheduler, with assertions against
  cross-phase writes.
- [ ] Remove behavior that depends accidentally on vector registration order.
- [ ] Add deterministic ordering by entity/group ID for allocation and event handling.

Exit criteria: replays produce identical combat outcomes across render frame rates.

### Phase 3: Persistent groups and stable slots

- [ ] Add `TacticalGroupStore`, `GroupMemberComponent`, group state, and serialization.
- [ ] Convert formation commands into group-anchor orders.
- [ ] Implement deterministic stable slot assignment and slot retention.
- [ ] Add formation turning, speed matching, straggler recovery, split, merge, and
  explicit dissolve behavior.
- [ ] Implement casualty vacancies and delayed front-to-rear reflow.
- [ ] Keep legacy individual move orders for civilians, builders, and detached units.

Exit criteria: formations retain rank/file identity through turns and front-rank
casualties without path crossing or backward reforming.

### Phase 4: Steering and movement quality

- [ ] Replace direct waypoint velocity integration with preferred steering plus bounded
  kinematics.
- [ ] Replace the no-op avoidance system with predictive constrained avoidance.
- [ ] Add heading-aware speed, angular velocity, turn-in-place, arrival, and cavalry
  turn-radius policies.
- [ ] Add group corridor compression/expansion and blocked-member recovery.
- [ ] Delete duplicate direct transform/velocity writes from combat and order systems.

Exit criteria: no visible yaw snaps; no routine overlap; units do not translate
sideways through large heading errors; a 100-unit turn remains coherent.

### Phase 5: Locomotion presentation

- [ ] Publish authoritative motion snapshots after kinematics.
- [ ] Build locomotion transition graphs with start/loop/stop/turn clips.
- [ ] Add phase-preserving walk/run blends and bounded playback-rate matching.
- [ ] Remove `has_target` and nominal-speed animation inference where snapshots exist.
- [ ] Add animation continuity tests for waypoint changes, stopping, turning, and
  action interruption.

Exit criteria: feet and displacement agree within defined tolerance; no idle/walk
flashing; animation remains continuous across fixed simulation ticks.

### Phase 6: Battle fronts and melee allocation

- [ ] Add group-vs-group contact front calculation.
- [ ] Replace unused radial engagement slots with front-relative combat slots.
- [ ] Assign stable attacker/defender pairs with leases and deterministic replacement.
- [ ] Gate melee actions on slot arrival, distance, facing, and weapon reach.
- [ ] Keep reserve ranks in formation and advance them only into vacancies.
- [ ] Remove nearest-target chase from engaged group members.

Exit criteria: damage cannot occur across a visible gap; opposing front ranks meet;
rear ranks replace casualties without both armies reforming backward.

### Phase 7: RTS action unification

- [ ] Generalize existing authored commander actions to normal sword, spear, and bow
  soldiers.
- [ ] Start one action transaction for every gameplay attack.
- [ ] Route melee trace/contact and projectile release through authored events.
- [ ] Make death/hit reactions interrupt actions through explicit policies.
- [ ] Remove `pending_melee_*` and cooldown-derived render transactions.
- [ ] Retain cooldown as action eligibility/recovery policy, not hit timing.

Exit criteria: every damage/projectile event references a visible action transaction;
enemy and friendly attack/death reactions follow identical code paths.

### Phase 8: Ranged doctrine

- [ ] Add fire-control state to tactical groups.
- [ ] Implement free fire, ripple volley, synchronized volley, and hold fire.
- [ ] Add rank eligibility and terrain/friendly line-of-fire tests.
- [ ] Drive bow draw/release animation from the same volley schedule.
- [ ] Add deterministic cadence variation and target distribution.

Exit criteria: eligible formations produce readable volleys; no invisible shots; idle
ranks have an explicit hold/reload reason; projectiles release from visible bows.

### Phase 9: Combined arms and commander integration

- [ ] Integrate spear reach/support ranks and brace fronts.
- [ ] Integrate cavalry approach lanes, momentum, impact, penetration, and disengagement.
- [ ] Treat commanders as group members when attached and autonomous combatants when
  detached.
- [ ] Reuse authored weapon traces for commander-vs-soldier and commander-vs-commander.
- [ ] Add morale/cohesion hooks only after movement and engagement are stable.

Exit criteria: infantry, ranged, cavalry, and commanders share the same engagement,
action, hit, and reaction contracts without unit-type branches in rendering.

### Phase 10: Removal and production hardening

- [ ] Remove obsolete melee lock, radial slot, render inference, and direct chase paths.
- [ ] Version and migrate save data.
- [ ] Profile 500, 1,000, and target maximum visible soldiers.
- [ ] Add LOD update rates for distant group planning and animation without changing
  deterministic combat outcomes.
- [ ] Run replay, soak, sanitizer, and visual-regression suites in CI.

## Non-Negotiable Acceptance Criteria

- A soldier cannot deal melee damage unless its weapon event and combat slot permit it.
- A ranged projectile cannot exist without a release event from a visible action.
- A death outcome produces a death/reaction transaction for either team that tick.
- Formation members retain stable slots unless a documented reassignment trigger fires.
- Front-rank losses advance reserves forward; they do not move the battle backward.
- No system directly changes movement velocity outside the steering/kinematics phases.
- No renderer decides attacks, deaths, targets, or gameplay timing.
- Simulation results are independent of render FPS.
- All allocations and action schedules are deterministic under a fixed seed.
- A normal unit archetype is assembled from definitions without editing generic
  movement, engagement, action, or rendering code.
- Declarative profiles do not expose avoidance, matching, interpolation, or numerical
  integration internals.
- The core battlefield remains understandable through four primary abstractions.

## Complexity Budget and Rejection Rules

Reject a proposed abstraction, definition field, component, or system unless it:

- represents a player-visible distinction reused by multiple archetypes,
- establishes a necessary ownership boundary,
- enables deterministic testing or replay,
- removes duplicated policy from two or more runtime paths, or
- is required by a measured acceptance failure.

Prefer named presets over independent numbers, one event stream over mirrored booleans,
and derived presentation data over persistent duplicate state. Before exposing a field,
ask whether a designer makes a meaningful choice with it, whether it can be derived,
whether it preserves invariants, and whether two concrete content cases need it. If
not, keep it internal until evidence requires exposure.

## Implementation Sequence Warning

Do not begin by polishing attack clips or changing movement constants. The first
production implementation must establish deterministic captures and the standard
infantry vertical slice, then explicit phases and persistent groups. Animation and
combat synchronization depend on those contracts.

The previous commander action work under `game/systems/combat_actions/` should be
preserved and generalized during Phase 7. It is not the cause of the formation-level
failures, and replacing it would discard the correct direction: authored events shared
by gameplay and presentation.
