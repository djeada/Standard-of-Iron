# Combat System

This document describes the RTS combat system in **Standard of Iron**, including:

* the per-frame update pipeline;
* shared target lookup and validation;
* normal, siege, tower, and elephant attacks;
* combat animation and visual feedback;
* the recommended approach for adding new combat behavior.

## Overview

RTS combat is coordinated by `Game::Systems::CombatSystem`, implemented in:

```text
game/systems/combat_system.cpp
```

Combat-related side effects should flow through this system whenever possible. Normal unit attacks, siege and tower attacks, elephant trample damage, target selection, and auto-engagement all rely on the same combat query context and enemy-validation rules.

This shared path helps keep combat behavior consistent and prevents individual processors from implementing conflicting targeting or damage logic.

## Per-Frame Update Pipeline

Each frame, `CombatSystem::update()` runs the following pipeline:

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

The order matters:

1. The shared query context is rebuilt.
2. Temporary hit feedback and combat animation state are updated.
3. Normal attacks are resolved.
4. Special siege and elephant behaviors are processed.
5. Eligible idle units may automatically acquire nearby enemies.

## Shared Combat Query Context

`CombatQueryContext` is defined in:

```text
combat_utils.h
```

It is rebuilt once at the beginning of each combat update and provides shared lookup data for combat processors.

### Contents

The context contains:

* `units`: alive unit entities that are not pending removal;
* `entities_by_id`: fast lookup of entities by target ID;
* `unit_grid`: spatial lookup for nearby non-building units;
* `nearby_unit_ids`: reusable scratch storage for range queries.

### Why It Matters

Combat processors should receive and reuse this context instead of repeatedly calling:

```cpp
World::get_entities_with<UnitComponent>()
```

Using the shared context:

* keeps target selection consistent across combat behaviors;
* avoids rebuilding the same view of the world in multiple processors;
* reduces unnecessary entity queries during each frame.

## Target Validation

All combat behaviors that select or damage targets should use the shared validation helper:

```cpp
Combat::is_valid_enemy_unit(attacker_unit, target, allow_buildings)
```

This helper rejects targets that are:

* null;
* pending removal;
* dead or missing a `UnitComponent`;
* owned by the attacker;
* owned by an allied player according to `OwnerRegistry`;
* buildings when `allow_buildings == false`.

Avoid implementing direct owner checks in individual combat processors. Team and alliance rules are easy to handle incorrectly when duplicated.

## Normal Attacks

Normal attacks are processed by:

```text
combat_system/attack_processor.cpp
```

The `process_attacks()` processor handles:

* target resolution;
* melee and ranged attack behavior;
* attack cooldowns;
* melee lock behavior;
* tactical damage multipliers;
* ranged arrow visuals;
* projectile-based attacks configured through `SpecialAttackComponent`;
* combat animation triggers.

### Applying Damage

Resolved attacks should apply damage through:

```cpp
Combat::deal_damage(world, target, damage, attacker_id)
```

This is the preferred damage entry point because it centralizes:

* health reduction and death handling;
* retaliation behavior;
* hit feedback;
* blood and fire status side effects;
* combat event publication.

New attack behaviors should avoid modifying health directly unless there is a strong architectural reason to bypass the standard combat flow.

## Siege Weapons and Defense Towers

Siege weapons and defensive-building combat are handled by:

```text
combat_system/siege_special_processor.cpp
```

This processor owns:

* catapult loading, firing, and stone projectile spawning;
* ballista loading, firing, bolt visuals, and delayed hit checks;
* defense tower target selection, arrow volleys, and damage application.

### Siege Loading State

Catapults and ballistas share `CatapultLoadingComponent` for their loading state.

Their state machine is:

```text
Idle -> Loading -> ReadyToFire -> Firing -> Idle
```

If a siege unit begins moving while loading or firing, its loading state is reset. This prevents the unit from firing at a previously locked target after changing position.

### Defense Tower Targeting

Defense towers select the nearest valid enemy within range.

They may attack:

* enemy units;
* enemy defense towers.

They ignore ordinary buildings.

Tower arrow spread is generated deterministically from entity IDs, ensuring that repeated runs produce stable visual results.

## Elephant Combat Behavior

Elephant-specific combat behavior is handled by:

```text
combat_system/elephant_special_processor.cpp
```

This processor owns:

* low-health panic checks;
* charge state transitions;
* trample damage;
* stomp-impact records used by visual effects.

### Panic State

Panic state is stored in:

```cpp
ElephantPanicComponent
```

`ElephantComponent` remains focused on combat statistics and charge/trample state.

Panic behavior does not create hidden movement targets. Instead, it influences the elephant's combat decisions directly.

### Trample Damage

Trample damage applies only to valid enemies.

Friendly and allied troops are rejected through:

```cpp
Combat::is_valid_enemy_unit()
```

As a result, even a panicked elephant cannot damage units on its own side.

## Auto-Engagement

`AutoEngagement` runs after explicit attacks and special combat processors.

Its purpose is to allow eligible idle units to acquire nearby enemies without requiring a direct attack command.

Auto-engagement uses:

* the shared `CombatQueryContext`;
* the common enemy-validation helpers.

A unit is not considered freely idle when it has:

* suppressing player intent;
* hold or guard constraints;
* an active patrol;
* an active attack target.

## Combat State and Visual Feedback

Combat animation state is stored in:

```cpp
CombatStateComponent
```

It is advanced by:

```cpp
process_combat_state()
```

Transient hit feedback, such as hit flashes, is handled by:

```cpp
process_hit_feedback()
```

Combat processors may spawn projectile, arrow, or impact visuals. However, final damage resolution should still go through:

```cpp
Combat::deal_damage()
```

when the attack connects.

## Deterministic Visual Variation

Combat visuals may appear random, but their variation should remain deterministic.

The current combat code uses hash-based values derived from entity IDs and target IDs for effects such as:

* attack animation offsets;
* arrow counts;
* arrow spread.

Avoid introducing:

```cpp
std::random_device
```

or global:

```cpp
std::rand()
```

into combat code. Non-deterministic randomness makes combat tests, debugging, and replay behavior harder to reason about.

## Adding New Combat Behavior

Use the following approach when introducing new combat functionality:

I. Add a dedicated component when the behavior requires persistent state.

II. Add a processor under:
  
```text
game/systems/combat_system/
```

III. Call the processor from `CombatSystem::update()`:

* after normal attacks when it applies special damage;
* before normal attacks when it changes attack eligibility.

IV. Use `CombatQueryContext` for entity lookup and range scanning.

V. Use `Combat::is_valid_enemy_unit()` for target validation.

VI. Use `Combat::deal_damage()` for damage application.

VII. Add focused tests under:

```text
tests/systems/
```

Avoid creating a new top-level `System` for combat damage unless the behavior is genuinely outside the combat simulation. Separate damage systems tend to develop inconsistent targeting, validation, and damage rules.

## Known Boundaries

### RPG Commander Combat

The RPG commander combat resolver has its own implementation under:

```text
game/systems/rpg_combat_system/
```

Although it is related to combat, it represents a different abstraction from RTS unit combat and does not share the same processing path.

### Projectile and Arrow Systems

Projectile movement remains the responsibility of:

```text
ProjectileSystem
```

Arrow trail visuals remain the responsibility of:

```text
ArrowSystem
```

The combat system decides when these effects are created. The projectile and arrow systems own their subsequent simulation and rendering-facing data.
