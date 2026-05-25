# Battle Movement and Melee Refactor TODO

This file records the current movement/combat problems, the verified code paths behind them, and the preferred fix direction. The goal is not to patch individual symptoms one at a time. The game needs a coherent short-range movement, formation, and melee-frontage layer that keeps the global pathfinder simple while making dense battles stable and readable.

## Verified Problems

- Large move orders can collapse into one blob.
  - `CommandService::move_group` assigns formation/path offsets, but `MovementSystem::move_unit` only moves each unit toward its current target and does not account for nearby dynamic units.
  - `FormationPlanner` avoids overlap between planned destination slots, but it does not reserve space against existing units or protect spacing while units are en route.
  - The global map/path system should stay terrain/building focused. Units should not become hard blockers in the global nav grid.

- Melee attacks collapse onto target origins.
  - `CommandService::attack_target` and `AttackProcessor::process_attacks` use the target position directly for normal melee targets.
  - This causes many attackers to chase the same point instead of being assigned frontage positions around the enemy.
  - Large targets get partial special handling, but normal unit-vs-unit melee lacks engagement slots.

- Melee lock can create jitter, snaps, and visual clutter.
  - `MovementSystem::move_unit` stops normal movement for `AttackComponent::in_melee_lock`.
  - `AttackProcessor::initiate_melee_combat` and `AttackProcessor::process_melee_lock` then directly adjust transforms to maintain melee distance.
  - Direct combat-side transform correction bypasses normal movement smoothing and nearby-unit avoidance, which is likely responsible for visible snapping and unstable dense fights.

- Enemy response is too individual or too global.
  - `AutoEngagement` selects targets per unit, which explains cases where only the nearest enemy reacts.
  - `AttackBehavior` can issue commands to the whole ready attack force once enemies are nearby or visible.
  - There is no local cohort concept for "this nearby batch should respond together, but not the whole army."

- Units can visually pass through other troops while chasing an attack.
  - Chasing units steer to target positions without dynamic unit avoidance or approach reservations.
  - Existing terrain/building checks prevent invalid map movement but do not prevent transient unit-unit visual intersections.

- Soldiers can switch direction or target while already committed.
  - `AttackProcessor::process_attacks` can overwrite `AttackTargetComponent` when a different best target is found, unless a valid melee lock is already controlling the attack.
  - Combat animation phases exist in `CombatStateProcessor`, but there is no clear target-switch lease/cooldown tied to those phases.

- Soldiers jitter around elephant attacks.
  - Elephant trample and large-target engagement create heavy local pressure around one moving large entity.
  - Foot soldiers can simultaneously chase, lock, face, get damaged, and be distance-corrected without a single movement authority.
  - Elephants need to be represented as large dynamic avoidance obstacles and use stable knockback/panic handling instead of repeated local corrections.

## Target Architecture

Build a short-range movement layer that sits between high-level orders/pathfinding and final transform writes:

1. High-level commands decide intent: move here, attack that, defend this area.
2. Formations and combat frontage assign desired slots or approach lanes.
3. A local dynamic avoidance system resolves nearby unit conflicts.
4. Movement writes final positions once per simulation step.
5. Combat may request facing, slots, knockback, or lock state, but should not directly teleport units during normal melee maintenance.

Global pathfinding remains terrain/building based. Unit-unit interaction should be handled by local spatial queries and soft avoidance, not by adding all troops as hard grid blockers.

## Workstream 1: Local Dynamic Occupancy and Avoidance

Add a `LocalAvoidanceSystem` or equivalent service that runs after movement/combat intents are produced and before transforms are committed.

Required behavior:

- Maintain a transient spatial hash of active unit circles.
- Query only nearby cells so avoidance stays approximately `O(n * k)` instead of all-pairs.
- Apply soft separation and velocity steering for moving units.
- Give higher priority to stationary, melee-locked, braced, or already-attacking units.
- Make chasers and moving formation members yield or slide around higher-priority units.
- Clamp corrections to a small max distance per tick to avoid popping.
- Feed corrected movement through terrain/building validation before committing.
- Keep behavior deterministic by processing stable entity ids and deterministic tie-breaks.

Acceptance criteria:

- A large move order preserves visible spacing instead of merging into one point.
- Two groups moving through each other slide and compress temporarily without teleporting.
- Unit-unit avoidance does not modify the global pathfinding grid.
- Performance remains stable with battle-sized armies.

## Workstream 2: Formation Lanes and Arrival Slots

Extend group movement so spacing is preserved during travel, not only at the final destination.

Required behavior:

- Keep per-unit formation slots attached to the order from issue time through arrival.
- For shared paths, project each slot along the path as a lane offset.
- Compress lanes at chokepoints, bridges, and narrow walkable areas, then decompress after the choke.
- Avoid falling back to many independent direct moves unless the formation has truly dissolved.
- Revalidate assigned slots when terrain blocks a slot, but preserve relative order where possible.

Relevant code:

- `CommandService::move_group`
- `CommandService::process_path_results`
- `FormationPlanner::get_formation_with_facing`
- `FormationPlanner::find_nearest_walkable_non_overlapping`

Acceptance criteria:

- A group ordered across open ground remains a group, not a blob.
- A group crossing a bridge narrows to fit the bridge and expands again after crossing.
- The movement result is stable for both player and AI-issued orders.

## Workstream 3: Melee Frontage and Engagement Slots

Introduce an engagement-slot layer for melee.

Required behavior:

- When melee begins, allocate slots around the target or between two opposing groups.
- Attackers move toward assigned engagement slots, not the target origin.
- Limit the number of attackers per target arc based on unit radius and weapon type.
- Overflow attackers should choose neighboring enemies, reserve second-line slots, or wait nearby.
- Store slot data on the attacker combat state: target id, side/arc, anchor offset, lease expiry, and whether the slot is still valid.
- Recompute slots only when the target dies, moves too far, the unit is displaced, or the engagement topology changes.

Relevant code:

- `CommandService::attack_target`
- `AttackProcessor::process_attacks`
- `AttackProcessor::initiate_melee_combat`
- `AttackProcessor::process_melee_lock`
- `AttackComponent` in `game/core/component.h`

Acceptance criteria:

- Melee lines spread naturally instead of all units stacking on one point.
- Attacking one unit with many soldiers creates a readable ring/frontage, not a pile.
- Group-vs-group melee produces a broad contact area.

## Workstream 4: Single Movement Authority During Combat

Remove normal melee maintenance that directly edits transforms from combat code.

Required behavior:

- Combat systems should request movement intents, engagement offsets, facing, knockback, or lock state.
- Movement/avoidance should be the only normal owner of position integration.
- Direct transform displacement should be reserved for rare explicit events, such as spawn placement or scripted hard relocation.
- Replace melee pull corrections with desired engagement distance handled through movement intent.
- Add per-frame movement diagnostics to catch any unexpected combat-side transform writes.

Acceptance criteria:

- Existing melee lock behavior is preserved visually, but without snaps.
- No unit teleports to maintain melee spacing.
- Facing changes remain smooth and do not fight movement corrections.

## Workstream 5: Target Commitment and Direction Stability

Add a clear target ownership policy so soldiers do not change direction mid-fight unless the current target is no longer valid.

Required behavior:

- Add an attack-intent lease or target-switch cooldown.
- Prevent target switching during wind-up, strike, impact, and early recovery phases.
- Allow switching immediately only when the current target is dead, invalid, unreachable, or outside leash range.
- Make opportunistic target acquisition respect the same commitment rules.
- Centralize target-switch decisions in one combat function to avoid scattered overrides.

Relevant code:

- `AttackProcessor::process_attacks`
- `CombatStateProcessor`
- `AttackTargetComponent`
- `AttackComponent`

Acceptance criteria:

- Units do not spin between nearby enemies while already attacking.
- Direction changes happen at understandable moments, such as after recovery or after target death.
- Target commitment does not cause units to chase invalid enemies forever.

## Workstream 6: Local AI Cohorts

Add local defensive cohorts so nearby enemies react as a batch without waking the entire army.

Required behavior:

- Cluster idle/guarding AI units by local radius, guard zone, role, and current tactical order.
- When one unit detects an enemy, activate its local cohort.
- Keep strategic army-level attack behavior separate from local defensive response.
- Let cohort size be capped so response is readable and performant.
- Re-form cohorts periodically or when units die/move far from their cluster.

Relevant code:

- `AutoEngagement`
- `AttackBehavior`
- `ai_tactical`

Acceptance criteria:

- Approaching a small enemy group causes the nearby group to respond together.
- The nearest single unit no longer peels off alone when allies are clearly standing with it.
- A full army does not react unless the tactical AI explicitly commits it.

## Workstream 7: Prevent Chasing Through Troops

Combine local avoidance with combat approach reservations.

Required behavior:

- Chasing units should reserve approach slots before entering melee range.
- Units already in melee or holding position should have stronger avoidance priority.
- Attackers should slide around occupied fronts rather than crossing through friendly or enemy bodies.
- If no clean approach slot exists, attackers wait, flank, or choose a nearby valid target.

Acceptance criteria:

- A unit cannot visually run through a packed friendly or enemy line just because it detected a target behind it.
- Dense battles remain cluttered in a believable way, not visually intersecting.

## Workstream 8: Elephant Interaction Stability

Treat elephants as large dynamic obstacles and combat anchors.

Required behavior:

- Register elephants in local avoidance with a large radius and high priority.
- Use wider engagement rings for attackers around elephants.
- Apply trample knockback/panic as a short-lived movement intent with cooldown per victim.
- Avoid repeated per-frame displacement of the same victim while it is already reacting.
- Keep damage, sound, and hit feedback decoupled from physical displacement so audiovisual feedback does not create movement jitter.

Relevant code:

- `ElephantSpecialProcessor`
- `AttackProcessor`
- `MovementSystem`

Acceptance criteria:

- Foot soldiers near elephants are pushed, routed, or damaged in a stable way.
- Soldiers do not jitter in place under repeated trample/contact updates.
- Elephant movement remains forceful but readable.

## Workstream 9: Performance and Code Quality Rules

Avoid turning the fix into scattered special cases.

Rules:

- Do not add unit occupancy to the global A* grid.
- Do not add one-off "if elephant" or "if melee" position hacks in movement.
- Do not let multiple systems write final positions in the same tick.
- Keep dynamic avoidance data in frame-local snapshots or compact spatial structures.
- Keep combat target ownership in one place.
- Prefer data-oriented loops over per-unit heap allocations.
- Add debug overlays for desired slot, actual position, avoidance vector, lock target, and cohort id.
- Add counters for spatial hash build time, average neighbors checked, slot allocation failures, and target switches.

## Test Plan

Add targeted tests before or alongside the refactor:

- Group movement spacing: large group reaches destination with minimum pair distance above a threshold.
- Formation lane preservation: units crossing open ground preserve relative slot order.
- Bridge/chokepoint compression: group narrows on bridge and expands afterward.
- Melee frontage: many attackers around one target occupy distinct engagement slots.
- Melee no-teleport: combat update cannot move a unit more than allowed movement speed plus permitted knockback.
- Target commitment: unit cannot switch targets during attack wind-up/strike unless current target dies.
- AI cohort response: detecting one player unit activates only the local enemy cohort.
- Elephant stability: repeated trample/contact updates do not produce alternating displacement jitter.
- Performance regression: battle-sized avoidance update stays within the target frame budget.

## Suggested Implementation Order

1. Add diagnostics and tests that reproduce blob movement, melee stacking, target switching, and elephant jitter.
2. Introduce local spatial hash and avoidance behind a feature flag.
3. Route normal movement through local avoidance.
4. Add melee engagement slots and route attack approach through movement intents.
5. Remove direct melee transform corrections from combat maintenance.
6. Add target commitment rules tied to combat phases.
7. Add AI local cohorts for defensive response.
8. Stabilize elephant interaction using large-obstacle avoidance and knockback cooldowns.
9. Remove feature flag once tests and real missions confirm stable behavior.
