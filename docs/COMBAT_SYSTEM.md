# Combat System

This document describes the current combat system in Standard of Iron: the
runtime update order, how targets are validated, where damage is applied, and
where special unit behavior plugs in.

## Main entry point

Combat is owned by `Game::Systems::CombatSystem` in
`game/systems/combat_system.cpp`.

Each frame, `CombatSystem::update()` runs this pipeline:

```text
CombatSystem::update
  |
  |-- rebuild_combat_query_context
  |-- process_hit_feedback
  |-- process_combat_state
  |-- process_attacks
  |-- process_siege_specials
  |-- process_elephant_specials
  |-- AutoEngagement::process
```

The important rule is that combat side effects should flow through this system.
Normal unit attacks, siege/tower attacks, elephant trample damage, target
selection, and auto-engagement all share the same query context and enemy
validation rules.

## Query context

`CombatQueryContext` lives in `combat_utils.h`.

It is rebuilt once at the start of the combat update and contains:

- `units`: alive unit entities that are not pending removal
- `entities_by_id`: fast lookup for target ids
- `unit_grid`: spatial lookup for nearby non-building units
- `nearby_unit_ids`: reusable scratch storage for range scans

Most combat processors receive this context instead of calling
`World::get_entities_with<UnitComponent>()` repeatedly. That keeps target
selection consistent and avoids each combat subsystem rebuilding its own view of
the world.

## Target validation

The common target gate is:

```cpp
Combat::is_valid_enemy_unit(attacker_unit, target, allow_buildings)
```

It rejects:

- null targets
- pending-removal entities
- dead or missing `UnitComponent`
- same-owner targets
- allied owners from `OwnerRegistry`
- buildings when `allow_buildings == false`

Use this helper for any combat behavior that chooses or damages a target.
Direct owner checks are easy to get subtly wrong, especially with teams/allies.

## Normal attacks

Normal attacks are processed by `process_attacks()` in
`combat_system/attack_processor.cpp`.

This processor handles:

- attack target resolution
- melee/ranged mode behavior
- attack cooldowns
- melee lock behavior
- tactical damage multipliers
- ranged arrow visuals
- special projectile attacks from `SpecialAttackComponent`
- combat animation starts

Damage is applied through:

```cpp
Combat::deal_damage(world, target, damage, attacker_id)
```

That is the preferred damage entry point. It centralizes death handling,
retaliation behavior, hit feedback, blood/fire status side effects, and event
publication.

## Siege and tower specials

Siege and defensive building combat is handled by
`combat_system/siege_special_processor.cpp`.

This processor owns:

- catapult loading, firing, and stone projectile spawning
- ballista loading, firing, bolt visuals, and delayed hit checks
- defense tower target selection, arrow volleys, and damage

Catapults and ballistas use `CatapultLoadingComponent` as their shared loading
state. The state machine is:

```text
Idle -> Loading -> ReadyToFire -> Firing -> Idle
```

If a siege unit starts moving while loading or firing, the loading state is
reset. This prevents stale locked targets from firing after the weapon has moved.

Defense towers choose the nearest valid enemy in range. They can shoot units and
enemy defense towers, but they ignore regular buildings. Tower arrow spread is
deterministic from entity ids, so repeated runs produce stable visuals.

## Elephant specials

Elephant-specific behavior is handled by
`combat_system/elephant_special_processor.cpp`.

The processor owns:

- low-health panic checks
- charge state transitions
- trample damage
- stomp impact records used by visuals

Elephant panic state is stored separately on `ElephantPanicComponent`.
`ElephantComponent` remains focused on elephant combat stats and charge/trample
state. Panic does not write hidden movement targets; it only affects elephant
combat decisions.

Trample damage only hits valid enemies. Friendly and allied troops are rejected
through `is_valid_enemy_unit()`, so a panicked elephant does not damage its own
side.

## Auto-engagement

`AutoEngagement` runs after explicit attacks and special processors.

Its job is to let eligible idle units acquire nearby enemies without requiring a
direct attack order. It uses the shared `CombatQueryContext` and the common
enemy validation helpers. Units with suppressing player intent, hold/guard
constraints, active patrols, or active attack targets are not treated as freely
idle.

## Combat state and visuals

Combat animation state is stored on `CombatStateComponent` and advanced by
`process_combat_state()`.

Hit flash and related transient feedback is handled by `process_hit_feedback()`.
Projectile and arrow visuals are emitted by combat processors, but final damage
still goes through `deal_damage()` when the attack is resolved.

Random-looking combat visual variation should be deterministic. Current combat
code uses hash-based rolls from entity ids and target ids for attack animation
offsets, arrow counts, and arrow spread. Avoid adding `std::random_device` or
global `std::rand()` in combat code; it makes tests and replays harder to reason
about.

## Adding a new combat behavior

Prefer this path:

1. Add unit-specific state as a component if the behavior has persistent state.
2. Add the processor under `game/systems/combat_system/`.
3. Call it from `CombatSystem::update()` after normal attacks if it applies
   special damage, or before normal attacks if it changes attack eligibility.
4. Use `CombatQueryContext` for entity lookup and scanning.
5. Use `is_valid_enemy_unit()` for target validation.
6. Use `deal_damage()` for damage.
7. Add focused tests under `tests/systems/`.

Avoid adding a new top-level `System` for combat damage unless it is genuinely a
non-combat simulation concern. Separate combat systems tend to drift into their
own targeting and damage rules.

## Known boundaries

The RPG commander combat resolver still has its own path under
`game/systems/rpg_combat_system/`. It is related combat code, but it is a
different abstraction from RTS unit combat.

Projectile movement remains in `ProjectileSystem`, and arrow visual trails
remain in `ArrowSystem`. Combat decides when to spawn those effects; the
projectile systems own their simulation and rendering-facing data.
