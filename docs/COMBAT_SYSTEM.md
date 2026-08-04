# Combat System

This document describes the RTS combat system in **Standard of Iron**, including:

- the per-frame update pipeline;
- shared target lookup and validation;
- normal, siege, tower, and elephant attacks;
- combat animation and visual feedback;
- the recommended approach for adding new combat behavior.

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

- `units`: alive unit entities that are not pending removal;
- `entities_by_id`: fast lookup of entities by target ID;
- `unit_grid`: spatial lookup for nearby non-building units;
- `nearby_unit_ids`: reusable scratch storage for range queries.

### Why It Matters

Combat processors should receive and reuse this context instead of repeatedly calling:

```cpp
World::get_entities_with<UnitComponent>()
```

Using the shared context:

- keeps target selection consistent across combat behaviors;
- avoids rebuilding the same view of the world in multiple processors;
- reduces unnecessary entity queries during each frame.

## Target Validation

All combat behaviors that select or damage targets should use the shared validation helper:

```cpp
Combat::is_valid_enemy_unit(attacker_unit, target, allow_buildings)
```

This helper rejects targets that are:

- null;
- pending removal;
- dead or missing a `UnitComponent`;
- owned by the attacker;
- owned by an allied player according to `OwnerRegistry`;
- buildings when `allow_buildings == false`.

Avoid implementing direct owner checks in individual combat processors. Team and alliance rules are easy to handle incorrectly when duplicated.

## Normal Attacks

Normal attacks are processed by:

```text
combat_system/attack_processor.cpp
```

The `process_attacks()` processor handles:

- target resolution;
- melee and ranged attack behavior;
- attack cooldowns;
- melee lock behavior;
- tactical damage multipliers;
- ranged arrow visuals;
- projectile-based attacks configured through `SpecialAttackComponent`;
- combat animation triggers.

### Applying Damage

Resolved attacks should apply damage through:

```cpp
Combat::deal_damage(world, target, damage, attacker_id)
```

This is the preferred damage entry point because it centralizes:

- health reduction and death handling;
- retaliation behavior;
- hit feedback;
- blood and fire status side effects;
- combat event publication.

New attack behaviors should avoid modifying health directly unless there is a strong architectural reason to bypass the standard combat flow.

## Siege Weapons and Defense Towers

Siege weapons and defensive-building combat are handled by:

```text
combat_system/siege_special_processor.cpp
```

This processor owns:

- catapult loading, firing, and stone projectile spawning;
- ballista loading, firing, bolt visuals, and delayed hit checks;
- defense tower target selection, arrow volleys, and damage application.

### Siege Loading State

Catapults and ballistas share `CatapultLoadingComponent` for their loading state.

Their state machine is:

```text
Idle -> Loading -> ReadyToFire -> Firing -> Idle
```

If a siege unit begins moving while loading or firing, its loading state is reset. This prevents the unit from firing at a previously locked target after changing position.

### Siege Ammunition

A catapult picks its ammunition when the shot is locked in, and picks it again whenever the order swings the arm onto a different target:

- structures the catapult can damage receive `ProjectileKind::FlamingStone`;
- troops, and anything else, receive `ProjectileKind::Stone`.

The kind is stored on `CatapultLoadingComponent::loaded_projectile_kind` and copied into the projectile at launch, so a shot already in the air keeps the kind it was fired with even if the catapult retargets behind it. Ballistas and defense towers are unaffected.

## Structure Fire

Structures do not catch fire from being damaged. Fire is a separate track, owned by:

```text
combat_system/structure_fire.cpp
```

Incendiary impacts - flaming siege stones and fireballs - add their applied damage to `StructureFireComponent::ignition_progress`. Once the accumulated damage passes a fraction of the structure's maximum health the structure ignites, burns for a fixed duration, and takes light burn damage per tick. Progress that never reaches the threshold decays away, so scattered incendiary chip damage does not eventually set a building alight.

Every other damage path - melee, ballista bolts, ordinary siege stones, script removal - leaves the structure without the component, and therefore without flames: the renderer draws structure fire only from `structure_fire_intensity()`, never from a health ratio. A fire ends when it burns out, when the structure collapses, or when the entity is removed.

### Defense Tower Targeting

Defense towers select the nearest valid enemy within range.

They may attack:

- enemy units;
- enemy defense towers.

They ignore ordinary buildings.

Tower arrow spread is generated deterministically from entity IDs, ensuring that repeated runs produce stable visual results.

## Elephant Combat Behavior

Elephant-specific combat behavior is handled by:

```text
combat_system/elephant_special_processor.cpp
```

This processor owns:

- low-health panic checks;
- charge state transitions;
- trample damage;
- stomp-impact records used by visual effects.

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

- the shared `CombatQueryContext`;
- the common enemy-validation helpers.

A unit is not considered freely idle when it has:

- suppressing player intent;
- hold or guard constraints;
- an active patrol;
- an active attack target.

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

- attack animation offsets;
- arrow counts;
- arrow spread.

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

- after normal attacks when it applies special damage;
- before normal attacks when it changes attack eligibility.

IV. Use `CombatQueryContext` for entity lookup and range scanning.

V. Use `Combat::is_valid_enemy_unit()` for target validation.

VI. Use `Combat::deal_damage()` for damage application.

VII. Add focused tests under:

```text
tests/systems/
```

Avoid creating a new top-level `System` for combat damage unless the behavior is genuinely outside the combat simulation. Separate damage systems tend to develop inconsistent targeting, validation, and damage rules.

## Commander signature moves

Every commander owns one duel move that no other commander has, declared beside
its aura in `game/units/commander_catalog.cpp`:

| Commander       | Signature          | Form                                         |
| --------------- | ------------------ | -------------------------------------------- |
| Fabius Maximus  | Bracing Thrust     | `RtsCommanderThrust`, long reach, staggers   |
| Scipio          | Consular Riposte   | `RtsCommanderCut`, heaviest single blow      |
| Marcellus       | Point-blank Volley | `RtsCommanderShot`, bow loosed in the clinch |
| Hanno (spear)   | Phalanx Sweep      | `RtsCommanderThrust`, catches four fighters  |
| Hasdrubal (bow) | Hunting Shot       | `RtsCommanderShot`, heaviest arrow           |
| Hannibal        | Encircling Cut     | `RtsCommanderCut`, two fighters, fast cycle  |

The move is claimed in `begin_rts_melee_action` when the commander's signature
cooldown has run out: the action id, the damage multiplier, the extra reach and
the stagger all come from the catalogue, so a duel reads as _that_ commander
fighting rather than two officers trading the same swing. Cooldowns start at half
so a commander does not open a fight with its signature, and the routine swing is
still the common case.

### How a signature reads on screen

Two cues carry the move, and both are presentation only - nothing in them feeds
back into the simulation:

- **The commander's own glow** (`render/entity/commander_aura_renderer.cpp`).
  Nothing shows for most of the cooldown; a glow gathers over the last quarter of
  it, so an enemy gets a moment's warning that the move is coming back, and it
  flares wide while `signature_strike_active` is set. The aura mesh is a standing
  column rather than a flat ring, which is why it is kept off the field except in
  those moments - left on permanently it reads as a haze curtain from a low
  camera.
- **The contact burst** (`CommanderSignaturePresentationComponent`). When the
  signature lands, `record_signature_contact` stores the contact point, the
  direction the blow travelled and one of three forms; the entry ages out in
  `process_signature_presentations`. `combat_dust_renderer` draws a line of cold
  glints down the shaft for `Thrust`, a warm arc plus a kick of dust for `Cut`,
  and a single hard point with a short back-fan for `Shot`.

`metal_spark` takes the spark's _own age_ as its time argument - its life ends at
t = 0.28 in the shader - so bursts must be driven by the entry's age, never by the
rolling animation clock, or they never appear at all.

It also takes an optional `direction`. The spark mesh is a fan of rays pointing
every which way, which on its own reads as a twinkle; given a direction,
`spark_model_matrix` (`render/gl/backend/spark_orientation.h`) lays that fan along
the line the blow travelled and squeezes it across, so the burst streaks. Signature
bursts pass the blow's line, arrow and bolt impacts pass the shaft's flight, and
fireball embers pass their own outward throw. Leave it zero for a spark with
nothing behind it - a squeezed burst also covers less screen, so a directional
spark needs roughly a third more radius to read at the same distance.

The impact debris shader (`u_effect_type == 2`) used to ignore `u_dust_color`
entirely, so every strike threw the same tan grit no matter what hit what. It now
tints the debris by the caller's chroma, which is what makes a grave priest's
`StructureImpactStyle::Magic` hit throw violet shards while a catapult stone stays
tan.

Anything the signature staggers recovers through `process_stagger_recovery` in
the status-effect pass. Before that existed, staggers were only wound down by the
RPG combat tick, which does not run unless a first-person commander is on the
field - anything staggered in a plain RTS battle stayed staggered forever and
never swung again.

## Authored Combat Actions

Every commander swing is an _authored action_: one entry in

```text
game/systems/combat_actions/combat_action_definition.cpp
```

that owns a normalized timeline (`duration_seconds` plus event markers for
wind-up, weapon trace, recovery, and exit). The action is the single source of
truth for that swing. Three consumers read the same timeline, and none of them
may derive a second one:

- **Gameplay** advances the action in `process_authored_combat_action` and
  applies damage inside the weapon-trace window.
- **The presentation phase machine** (`CombatStateComponent`) takes its per-phase
  durations from `authored_phase_duration()`, so `Advance`, `WindUp`, `Strike`,
  `Impact`, `Recover`, and `Reposition` line up with the authored markers
  exactly instead of running on their own clock.
- **The renderer** plays the action's clip at the action's own normalized time.
  `CombatRawInputs::has_authored_action_phase` makes the visual transaction
  machine defer to that timeline; without it the phase would be re-derived from
  the generic attack windows, which never reach the authored contact pose and
  cannot rewind when a player chains one swing into the next.

Retuning a swing therefore means editing its definition and nothing else.

### Player Commitment

Timing constants encode how much control the player has:

- Contact lands roughly a quarter of a second after the click for a light slash,
  and later for the heavier actions, so weight reads as weight rather than lag.
- Once `RecoveryStart` fires, `CombatActionService::request_attack` cancels a
  player-driven commander's current action straight into the next one, so a held
  attack chains as a combo with the correct variant progression.
- A `LightFlinch` never takes a swing away from the player: the blow is shown as
  recoil layered over the swing. Anything heavier cancels the action in the
  simulation, and the renderer follows.

`rpg_combo_cadence` in the arena catalog is the regression contract for all of
this.

## The Commander's Bow

A commander who carries a bow shoots it the way an archer would: the player
draws, aims, and lets go. Everything that makes that true lives beside the
other RPG combat code, in `game/systems/rpg_combat_system/`:

- `rpg_bow_aim` — where the shot is pointed and what it can hit.
- `rpg_bow_draw` — the draw, the hold, and what the string is worth when it is
  released.
- `rpg_bow_shot` — loosing the arrow itself.

`RpgCommanderAimComponent` carries the state all three read: the view angles,
the camera the player is sighting from, the draw stage, and the weapon stance.

### Weapon Stance

Commanders such as Marcellus and Hasdrubal carry a bow and a blade, and the
tactical layer leaves `AttackComponent::preferred_mode` on `Auto` so the RTS can
pick per range. Under direct control that would have meant a bow commander
swinging steel forever, so `CombatActionService::request_attack` asks the aim
component instead. The opening stance is the commander's stronger weapon, and
`commander.toggle_weapon` (`X`) switches it.

### Draw and Hold

Holding the attack button nocks and draws. The bow shot's authored timeline is
stopped a hair before its `ProjectileRelease` marker for as long as the string
is held, which is what makes the commander stand at full draw instead of
snapping off a shot the moment the animation reaches it. The presentation phase
machine is frozen with it - those phases are cut from the same timeline, so
letting them run on would walk the commander back to idle with the string still
back.

Releasing early looses immediately at whatever the draw was worth, floored at
`k_min_shot_power`; the animation covers the remaining distance to the release
marker at a few times speed so a snap shot still reads as a shot. Draw power
scales damage and arrow speed. Holding at full draw past
`k_steady_hold_seconds` widens the aim cone and bleeds the shot's power; past
`k_max_hold_seconds` the arm gives out, the string relaxes, and the player has
to release the button before the commander will nock another arrow.

### Free Aim

Nothing about the shot consults a locked target. The arrow is aimed at whatever
the crosshair covers, resolved by ray-casting enemy soldier bodies as upright
cylinders, and an arrow that hits nothing still flies its full range and plants
itself in the ground - a miss has to be legible.

Two details keep aiming honest:

- **The camera, not the chest.** The chase camera sits behind and to the side of
  the commander, so a shot fired parallel to the camera lands beside what the
  player has the reticle on. The crosshair line is resolved first, from the
  camera, and the arrow is then aimed from the bow at that point.
- **The aim cone is earned.** `aim_spread_degrees` opens the cone for movement,
  sprinting, a half-drawn string, a long hold, and low stamina, and the reticle
  draws that same cone in pixels. A planted archer at full draw is nearly a
  laser; one shooting on the run is not.

`rpg_bow_volley` in the arena catalog is the regression contract: three
chargers, three drawn shots, three bodies.

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
