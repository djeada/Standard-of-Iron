# Combat Slowdown Production TODO

## Scope

Production code improvements only. This TODO has been updated against the
current codebase and avoids listing work that is already implemented.

## Already Implemented

Do not redo these unless a review finds a bug:

- `CombatQueryContext` exists in `game/systems/combat_system/combat_utils.*`.
- `CombatSystem::update` builds the combat query context once per frame.
- `AutoEngagement::process` accepts the query context and uses spatial enemy
  lookup.
- `process_attacks` accepts the query context.
- Fallback attack target search uses `find_nearest_enemy` instead of scanning
  every unit directly.
- `CommandService::move_unit` exists and `move_units` delegates to it for
  non-group moves.
- Combat chase requests are collected through `ChaseMoveIntent` and submitted
  after the attacker loop.
- Basic chase repath gating exists in `should_queue_chase_command`.
- Combat tests already cover nearest enemy selection, auto-engagement building
  skipping, guard radius, duplicate chase request prevention, target movement
  repath, and attacking once in range.
- Render template prewarm already includes hit, melee attack, and ranged attack
  animation keys.

## 1. Tighten Combat Query Context Filtering

Target files:

- `game/systems/combat_system/combat_utils.cpp`
- `game/systems/combat_system/combat_utils.h`

Current gap:

- `build_combat_query_context` still inserts pending-removal units into
  `units`, `entities_by_id`, and the spatial grid. Callers filter them later,
  but the context should not carry dead work into hot combat queries.

Production changes:

- Skip entities with `PendingRemovalComponent` while building
  `CombatQueryContext`.
- Consider also skipping units with `UnitComponent::health <= 0` if no current
  combat stage needs dead units in the per-frame context.
- Keep buildings in the context only if explicit target/chase logic needs them;
  otherwise document why they remain.

Acceptance criteria:

- Query context contains only entities that can still affect combat this frame.
- `find_nearest_enemy` no longer needs to defend against pending-removal units
  from its own spatial query, except as a cheap safety guard.

## 2. Add A Shared Enemy Validity Helper

Target files:

- `game/systems/combat_system/combat_utils.h`
- `game/systems/combat_system/combat_utils.cpp`
- `game/systems/combat_system/attack_processor.cpp`
- `game/systems/combat_system/auto_engagement.cpp`

Current gap:

- Enemy validity checks are still duplicated across `find_nearest_enemy`,
  explicit attack-target validation, and other combat branches.

Production changes:

- Add a helper such as:
  - `is_valid_enemy_unit(attacker_unit, target, bool allow_buildings)`
- Cover:
  - null target
  - pending removal
  - missing `UnitComponent`
  - health <= 0
  - same owner
  - allied owner
  - optional building exclusion
- Use it from:
  - `find_nearest_enemy`
  - explicit target validation in `process_attacks`
  - auto-engagement if any local filter remains there

Acceptance criteria:

- Combat ownership/ally/dead/building filtering has one small shared
  implementation.
- Range checks and movement behavior remain outside the helper.

## 3. Make Melee Lock Initiation Idempotent

Target file:

- `game/systems/combat_system/attack_processor.cpp`

Current gap:

- `initiate_melee_combat` always writes lock fields, faces both units, and may
  adjust positions even when the attacker is already locked to the same target.

Production changes:

- Early-detect the common no-op case:
  - attacker already `in_melee_lock`
  - `melee_lock_target_id` equals the current target
  - target reciprocal lock is already correct, if it has `AttackComponent`
- Keep first-time lock setup unchanged.
- Do not reset attack variant or attack offset when continuing the same lock.
- Keep facing updates only if they are still needed for visible orientation;
  otherwise avoid repeated work once lock state is established.

Acceptance criteria:

- Repeated calls for the same attacker/target pair are cheap.
- Combat animation still starts on first lock.
- Lock cleanup still works when target dies, is removed, or becomes separated.

## 4. Finish Hot-Path Component Lookup Cleanup

Target file:

- `game/systems/combat_system/attack_processor.cpp`

Current gap:

- The attack loop still repeats some `get_component` calls for the same target
  and attacker inside nested branches.

Production changes:

- Cache target components after loading the explicit attack target:
  - `target_unit`
  - `target_transform`
  - target building flag
- Reuse `attacker_unit`, `attacker_transform`, and `attacker_atk` consistently.
- Avoid re-fetching `UnitComponent` for `attacker` near arrow spawn; use the
  already cached `attacker_unit`.
- Keep null checks local and obvious.

Acceptance criteria:

- No nested branch repeatedly fetches the same component from the same entity.
- The function remains readable; do not introduce a large abstraction just to
  remove one lookup.

## 5. Reduce Combat Chase Batch Allocations Further

Target file:

- `game/systems/combat_system/attack_processor.cpp`

Current gap:

- `ChaseMoveIntent` collection is implemented, but final submission still
  allocates two vectors every frame that has chase intents.

Production changes:

- Prefer one of these approaches:
  - Add a `CommandService` overload that accepts a span/vector of small
    `{EntityID, QVector3D}` move intents.
  - Or keep a small reusable local buffer type in attack processing if the
    project already has a frame allocator or scratch container pattern.
- Preserve current non-group movement semantics.
- Do not route combat chase through group movement unless intentionally changing
  formation behavior.

Acceptance criteria:

- Combat chase batching no longer converts intents into two separate temporary
  vectors.
- Single-unit `move_unit` behavior remains the source of truth for each intent.

## 6. Replace Magic Chase Thresholds With Named Constants

Target files:

- `game/systems/combat_system/combat_types.h`
- `game/systems/combat_system/attack_processor.cpp`

Current gap:

- `should_queue_chase_command` uses a raw `1.0F` threshold for recent goal
  movement and reuses `k_engagement_cooldown` as a path request timing gate.

Production changes:

- Add combat-specific constants for:
  - recent chase goal movement threshold
  - chase request cooldown
  - near-range buffer if different from `k_new_command_threshold`
- Use those names in `should_queue_chase_command`.

Acceptance criteria:

- Chase gating constants describe chase behavior directly.
- Auto-engagement cooldown and path/chase cooldown are not coupled by accident.

## 7. Cap Purely Visual Arrow Bursts

Target files:

- `game/systems/combat_system/attack_processor.cpp`
- `game/systems/combat_system/combat_types.h`
- `game/systems/arrow_system.*` only if API support is needed

Current gap:

- `spawn_arrows` scales visual arrow count from troop size and does not appear
  to cap the visual volley size.

Production changes:

- Add a named visual-only max arrows-per-volley constant.
- Clamp `arrow_count` after troop-size calculation.
- Keep gameplay damage independent from the visual arrow count.
- Keep catapult and ballista excluded from regular arrow VFX.

Acceptance criteria:

- Very large troop sizes cannot create unbounded arrow VFX.
- Existing ranged damage calculation is unchanged.

## 8. Stagger AI Snapshot Submission

Target files:

- `game/systems/ai_system.h`
- `game/systems/ai_system.cpp`

Current gap:

- Multiple AI instances can still reach `m_update_interval` and build snapshots
  on the same frame.

Production changes:

- Add a per-AI phase offset or initialize `update_timer` with a staggered value
  when AI players are created.
- Preserve the same average `m_update_interval` per AI.
- Keep AI reasoning in `AIWorker`.
- Keep result application on the main thread.

Acceptance criteria:

- AI players do not synchronize snapshot builds by default.
- Strategy behavior and command filtering remain unchanged.

## 9. Trim AI Snapshot Builder Dead State

Target file:

- `game/systems/ai_system/ai_snapshot_builder.cpp`

Current gap:

- The builder maintains local counters such as `skipped_no_ai`,
  `skipped_no_unit`, `skipped_dead`, and `added`, but they are not used.

Production changes:

- Remove unused counters.
- Keep vector reserves.
- Only remove snapshot fields after confirming no AI behavior reads them.

Acceptance criteria:

- Snapshot builder has no unused local accounting.
- Snapshot contents remain compatible with current AI behaviors.

## 10. Make Hit Pause A Named Tunable

Target files:

- `game/core/component.h`
- optionally a combat constants/data file if the project has one

Current gap:

- `CombatStateComponent::kHitPauseDuration` is local and hard-coded.

Production changes:

- Move hit-pause duration to the same combat constants area as other combat
  timing values, or clearly name it as a combat animation feedback constant.
- Keep hit pause local to `CombatStateComponent`.
- Do not let hit pause affect global simulation time, movement, AI, or render
  delta.

Acceptance criteria:

- Hit pause remains a per-unit combat animation state.
- The duration is easy to find and tune.

## 11. Add Missing Focused Tests

Target files:

- `tests/systems/combat_mode_test.cpp`
- `tests/systems/ai_system_test.cpp` if AI staggering is exposed/testable

Remaining production test coverage:

- `build_combat_query_context` excludes pending-removal units.
- Shared enemy validity helper covers buildings allowed vs. buildings excluded.
- Repeated `initiate_melee_combat` for the same pair does not reset animation
  variant/offset.
- Visual arrow count is clamped while damage remains unchanged.
- AI update staggering initializes different AI instances with different phases,
  if exposed cleanly.

Acceptance criteria:

- Tests cover the new production behavior directly.
- Tests do not depend on debug logs, metrics, or manual timing.

## Preferred Remaining Order

1. Tighten `CombatQueryContext` filtering.
2. Add and adopt the shared enemy validity helper.
3. Make melee lock initiation idempotent.
4. Clean up repeated component lookups in `attack_processor.cpp`.
5. Reduce chase batch vector allocations.
6. Replace chase magic thresholds with named constants.
7. Cap visual arrow bursts.
8. Stagger AI snapshot submission.
9. Remove unused AI snapshot builder counters.
10. Make hit pause a named tunable.
11. Add the missing focused tests.
