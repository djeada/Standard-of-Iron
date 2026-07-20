# Battlefield Work: One Existing Problem per Phase

## Purpose

This is a bug-fix roadmap, not an architectural migration plan. Each phase fixes one
player-visible failure or one measured performance cost that exists in the current
game. A phase must be independently testable and shippable.

Do not add a framework because a later feature might need it. Introduce shared state or
an abstraction only after the current phase requires it and the existing code cannot
express the fix cleanly.

## Rules for Every Phase

Before implementation, every phase must have:

1. one reproducible current failure,
2. one focused automated scenario or test,
3. the smallest change that fixes that failure,
4. a measurable exit condition, and
5. explicit non-goals.

A phase is not complete because new types or systems exist. It is complete only when
its reported failure no longer reproduces.

Do not begin a phase whose failure has not been reproduced. Performance work also
requires a profile showing the cost; code that merely looks expensive is not enough.

## What the Current Code Shows

The following constraints were verified against the current implementation:

- `RendererBootstrap::initialize_world_systems()` runs movement before combat.
  Combat chase/stop requests therefore normally affect movement on the following
  world update. This ordering is not by itself a reason to replace the scheduler.
- `FormationPlanner` calculates destinations for the current command and
  `CommandController::confirm_formation_placement()` submits those destinations as
  independent unit moves. There is no persistent rank/file identity after the order.
- `MovementSystem` commits per-unit velocity, position, and facing. Facing follows
  velocity while moving and is clamped independently of how much translation occurs.
- `LocalAvoidanceSystem` builds a spatial grid, examines neighbors, and calculates
  `sep_x`/`sep_z`, but explicitly discards the result. It consumes update time and
  changes no movement.
- RTS melee damage uses `AttackComponent::pending_melee_*`, while presentation also
  derives combat activity from cooldown, target, melee lock, and combat animation
  state. Gameplay and presentation therefore do not share one authoritative event.
- Ordinary ranged attacks call `begin_attack_animation()` and spawn arrow/projectile
  effects in the cooldown attack branch. Projectile release is not gated by an
  authored release event from the attack animation.
- Individual casualties are selected as a numeric sub-soldier slot range in
  `damage_application.cpp`. Those slot indices do not carry front-rank/rear-rank
  meaning, and the command-time formation has no persistent vacancy to refill.
- `EngagementSlotSystem` and `TargetCommitmentSystem` exist and have tests but are not
  registered in the normal runtime. The radial engagement slots are also not consumed
  by movement. Registering either system alone would not fix formation combat.

These observations justify focused fixes below. They do not yet justify a new global
phase scheduler, six definition registries, four battlefield modules, ORCA/RVO,
general doctrines, or a complete replay architecture.

## Phase 1: Movement and Locomotion Disagree

### Existing bug

Soldiers can translate strongly toward a waypoint while their visible facing catches
up. Sharp waypoint changes can therefore look like sideways sliding or abrupt turns,
and locomotion can look clunky relative to actual displacement.

### Reproduction

Create one deterministic movement test with start, straight travel, a sharp waypoint
turn, and stop. Record transform yaw, velocity direction, speed, and selected
locomotion state each update. Add a visual capture only if the numeric test cannot
describe the reported artifact.

### Smallest intended fix

- Reduce translation when velocity direction and facing differ greatly.
- Select locomotion direction/state from actual local velocity, not merely the
  presence of a movement target.
- Add turn-in-place only if existing clips support it and the heading-error fix still
  looks wrong.

Do not introduce a new motion module or immutable snapshot model unless the focused
fix proves impossible without one.

### Exit condition

The reproduction starts, turns, and stops without large sideways translation,
visible yaw snapping, or idle/walk flashing. Existing movement-order tests remain
green.

### Non-goals

Formation cohesion, collision avoidance, cavalry turn radii, root motion, a fixed
simulation scheduler, and new animation schemas.

## Phase 2: Formation Orders Make Soldiers Cross or Swap

### Existing bug

Formation geometry is recalculated for each command, but soldiers have no stable slot
identity across commands. Sorting the current unit list into new geometry can make
soldiers cross paths or unexpectedly exchange left/right and front/rear positions.

### Reproduction

Move the same formation forward, rotate it, and move it again. Assert a deterministic
maximum number of pairwise path crossings and verify that surviving members retain
their relative rank/file order where the destination remains walkable.

### Smallest intended fix

Add only the persistent data needed to retain assignment, for example a formation ID
and stable slot ID. Reuse the current planner and per-unit movement. On a new order,
prefer the previous valid slot; use deterministic nearest or rank-preserving matching
only for unassigned or invalid slots.

Do not add a general tactical-group state machine, shared path corridor, doctrine,
split/merge behavior, or Hungarian solver unless the reproduction demonstrates that
the simpler assignment fails.

### Exit condition

Repeated formation orders preserve relative slot order and remain deterministic.
Units are reassigned only when their old slot is invalid or the formation shape
actually changes.

### Non-goals

Casualty reflow, combat fronts, group navigation, cohesion simulation, and avoidance.

## Phase 3: Melee Damage Can Happen Across a Visible Gap

### Existing bug

Combat owns target selection, chase, melee lock, attack cadence, and pending damage.
A logical target or melee lock can remain authoritative even when visible positioning
does not form a credible contact line.

### Reproduction

Run a deterministic line-versus-line scenario and record attacker/target distance and
facing at every damage event. Include target death or movement during the pending
strike window.

### Smallest intended fix

At the moment damage is requested, validate that both entities are alive and that
distance and facing satisfy the current weapon attack. Cancel the pending hit when
those conditions fail. Keep the existing chase and melee-lock flow for this phase.

### Exit condition

No melee damage event occurs outside the defined distance and facing tolerance, and a
target leaving contact before the strike does not receive delayed damage.

### Non-goals

Formation fronts, support ranks, a general engagement module, authored weapon traces,
and removal of all melee-lock code.

## Phase 4: Rear Ranks Bypass the Front Line

### Existing bug

Combat chooses and chases individual enemies. The unused radial engagement slots are
centered on individual targets rather than on opposing lines. Rear soldiers can join
the chase instead of remaining behind the formation front.

### Reproduction

Meet two formations head-on. Track which original rank enters contact and whether a
rear-rank path crosses the friendly front before a vacancy exists.

### Smallest intended fix

Derive a simple contact line from the two formations' current occupied slots. Allocate
a bounded set of front-relative melee positions and permit only the current eligible
front rank to approach them. Keep reserves on their formation slots.

The implementation may add a small formation engagement record if leases must persist
between updates. It should not create a generic engagement doctrine or globally
replace target selection for unrelated unit types.

### Exit condition

Two formations meet front-to-front. Rear ranks do not pass through the first rank or
surround individual targets while the front remains occupied.

### Non-goals

Spear support ranks, cavalry penetration, multiple doctrines, fire control, morale,
and terrain-aware front optimization.

## Phase 5: Casualties Come From or Refill the Wrong Rows

### Existing bug

Sub-soldier casualties are currently chosen by numeric slot index, not by persistent
formation rank. A death does not create a formation vacancy. This can make the wrong
visible row disappear and leaves no authoritative instruction for the next row to
advance, so survivors may retain stale rear destinations or reform backward.

The exact current visual ordering must be captured before changing it: the casualty
loop removes the slot range from `new_survivors` to `prev_survivors`, but slot-number
order alone does not prove whether the renderer treats those as front or rear rows.

### Reproduction

Use a multi-row formation with labelled slot indices. Apply casualties one at a time
and capture which visible soldier disappears. Then kill members of the combat front
and record the position chosen by the replacement from the next rank.

### Smallest intended fix

- Define the intended visible casualty order explicitly and map health loss to that
  order rather than relying on incidental slot numbering.
- Mark a dead front slot vacant.
- Advance the nearest suitable member in the same column from the next rank.
- Keep the established combat front fixed; do not recompute the surviving formation
  around a rearward center.

Reuse the stable slot identity introduced in Phase 2. Add no configurable reflow
policy until a second required behavior exists.

### Exit condition

Casualties disappear from the intended rows in a deterministic order. A front vacancy
is filled forward from reserve, and repeated front losses never move the battle line
backward.

### Non-goals

Morale, delayed designer-configurable reflow, flank shrink thresholds, group routing,
and split/merge behavior.

## Phase 6: Melee Damage and Attack Animation Use Different Clocks

### Existing bug

Normal RTS melee schedules damage through `pending_melee_*`. Rendering separately
infers combat presentation from cooldown, target, melee lock, and
`CombatStateComponent`. A target change, cancellation, or death can therefore leave
gameplay damage without the corresponding visible attack transaction.

### Reproduction

For every melee damage event in a deterministic duel and line battle, record attacker,
target, animation/action identifier, animation phase, and cancellation reason. Cover
target death, target switch, interruption, and leaving range.

### Smallest intended fix

Generalize the existing authored combat-action/contact-event path only as far as a
normal RTS melee soldier requires. The visible attack event becomes the sole request
for melee damage; cooldown remains eligibility/recovery policy. Remove
`pending_melee_*` only after all normal RTS melee attacks use that event.

### Exit condition

Every normal RTS melee damage event references an active visible attack and its
contact event. Cancelling the action prevents damage. No normal RTS attacker uses
`pending_melee_*`.

### Non-goals

A new action scripting language, a general reaction graph, commander rewrites,
weapon-physics simulation, and data-driven definitions for every unit.

## Phase 7: Ranged Projectiles Are Not Released by the Visible Bow Action

### Existing bug

The cooldown attack branch starts an animation and spawns ordinary arrows in the same
gameplay update. Projectile creation is not owned by an authored release event, so a
projectile can disagree with what the bow animation visibly does.

### Reproduction

Record every ordinary archer projectile with the firing entity, visible action, and
animation phase at spawn. Cover interruptions and several archers firing together.

### Smallest intended fix

Spawn each projectile from that archer's authored release event, reusing the action
event mechanism proven in Phase 6. If perfectly synchronized archers look robotic,
add a small deterministic per-entity cadence offset; do not add group volley policy
for that cosmetic issue.

### Exit condition

Every ordinary archer projectile has a living visible firing soldier at its release
event. Interrupting the bow action before release creates no projectile.

### Non-goals

Synchronized/ripple volley doctrines, group fire-control state, overhead trajectories,
and comprehensive friendly-fire or terrain line-of-fire simulation.

## Phase 8: Local Avoidance Spends CPU but Cannot Affect Movement

### Existing bug

`LocalAvoidanceSystem` performs a spatial-neighbor pass and computes bounded separation
corrections, then discards them with `(void)sep_x` and `(void)sep_z`.

### Reproduction

Profile a crowded movement scenario with the system enabled and disabled. Record
update time, overlap count, arrival rate, and formation-order violations.

### Smallest intended fix

First choose from the evidence:

- If the pass has meaningful cost and no safe correction is ready, disable or remove
  it from normal runtime while retaining a cheap diagnostic test if useful.
- If overlap is a reproduced priority, apply a small deterministic bounded correction
  through the existing movement owner and verify that it does not undo stable slots.

Do not adopt ORCA/RVO unless the simple correction fails the reproduced scenario and
the measured failure warrants the extra cost.

### Exit condition

The normal runtime either spends no time on non-functional avoidance or the enabled
system measurably reduces overlap within an agreed update budget without breaking
formation slot order.

### Non-goals

Predictive crowd simulation, per-archetype solver coefficients, shared navigation
corridors, and obstacle compression/expansion.

## Performance and Build-Time Work

Do not create a phase called "move animation work to build time" until profiling names
the repeated runtime work. The animation directory already contains typed manifests
and resolved policy functions, so the existence of runtime selection code does not by
itself prove waste.

When a profile identifies a real cost, create one new phase for that cost only:

- name the function and measured frame/update cost,
- distinguish immutable asset metadata from state that genuinely changes each frame,
- precompute immutable lookup tables or resolved clip metadata at asset load/build
  time,
- compare behavior and CPU time before and after, and
- avoid a new registry or schema unless the measured fix requires it.

Dynamic values such as action time, actual velocity, current target, interruption, and
selected state must remain runtime work.

## Deferred Until a Current Failure Requires Them

The following are deliberately not roadmap phases yet:

- replacing system registration with a global named-phase scheduler,
- fixed-step conversion of the whole game,
- `TacticalGroup`, `SoldierAgent`, `Engagement`, and `Action` as mandatory global
  abstractions,
- six new definition schemas and registries,
- immutable presentation snapshots for all gameplay state,
- shared formation path corridors and group compression,
- ORCA/RVO integration,
- synchronized and ripple volley doctrines,
- cavalry lanes, penetration, disengagement, and morale,
- save-format migration for systems that do not exist yet,
- 1,000-soldier LOD work without a measured target-hardware bottleneck, and
- a complete replay/capture platform before focused deterministic tests need it.

Any item may return as a narrowly scoped phase when a reproduced current problem makes
it the smallest credible solution.

## Definition of Done for the Roadmap

At the end of every phase:

- its original reproduction is retained as a regression test,
- its exit condition is demonstrated,
- unrelated behavior and unit types still pass their tests,
- unused experimental components or systems are not registered merely because they
  already exist, and
- the next phase is reconsidered against the new code instead of being treated as a
  predetermined architectural commitment.
