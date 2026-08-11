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

`is_valid_enemy_of_owner` is the same rule set keyed on an owner id instead of an attacker component, for callers that have a player but no attacking unit — the attack-mode target highlighting uses it.

## Firing distance

`AttackComponent::range` is the outer reach and `AttackComponent::min_range` the inner one. A ranged attacker whose planar distance to the target falls below `min_range` fails `Combat::is_in_range` outright, before any structure, formation or height rule is consulted; melee contact ignores it. No shipped unit sets a minimum today — the field exists so siege weapons can be given a dead zone in data or by an upgrade without the UI and the combat check disagreeing about where it is.

Anything that needs the reach a unit will actually fire with — the range indicator, a UI verdict, a future ability — should go through `Game::Systems::resolve_attack_range`, which applies `hold_mode_range_multiplier` exactly as `apply_hold_mode_bonuses` does inside the attack processor. Reading `attack->range` directly skips the Hold bonus and drifts from combat the moment a stance changes.

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

### Duel footwork

`MovementSystem::move_unit` zeroes velocity and freezes facing for the whole
time a unit is in a melee lock, which is what stops a mass melee sliding around.
For a commander locked one-on-one that read as two statues trading blows on a
metronome, so `apply_duel_footwork` gives that case a slow circling instead.

It applies only when the entity has a `CommanderComponent` **and** its lock
target is locked back onto it, so line infantry and any crowd around a shared
target are untouched. Each duellist turns about its opponent's position rather
than about the midpoint: rotation about the other man preserves the distance
between them exactly, whichever order the two are updated in, so circling can
never walk a fighter out of his own reach. Both derive the same direction from
the pair's ids, so the pair turns rigidly, and the rate is a sine that reverses
every `k_duel_footwork_period_seconds` so a long duel reads as footwork rather
than as a turntable.

The arena trace is how to check it: post-contact `position` spread should be
about a metre in each axis, and the separation between the two should not drift
at all.

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

### A commander cannot be stagger-locked

Because anything heavier than a `LightFlinch` cancels the action, whatever
decides the tier decides whether the player gets to play. `has_punish_opening()`
used to answer yes for anything carrying a `StaggerComponent`, and the punish
reaction applied a `HeavyStagger` — so the first blow that caught a wind-up
staggered the commander, the stagger made him permanently punishable, and every
later blow re-applied and extended it. Surrounded by six swordsmen the player
never completed a swing again. `rpg_melee_contact` had been failing on exactly
that: the commander swung twice in 5.4 s and the enemy formation never lost a
point of health.

Being staggered is now the _result_ of a punish, not a fresh opening: the
`StaggerComponent` branch is gone from `has_punish_opening()`, so a punish is a
wind-up, an authored punish window, or a broken guard. An ordinary blow that
catches a commander mid-wind-up gives him a `LightFlinch` — recoil over the
swing, and the swing survives — and only a guard break still takes the action
away. A commander who is already staggered is not re-staggered at all, which
puts a hard ceiling on how long a crowd can hold him.

`ABeatenCommanderIsNeverStaggerLocked` and `AStaggeredCommanderIsNotAPunishOpening`
pin both halves.

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

The draw is scored as well as animated. `BowDrawTick` reports the three moments
worth hearing — the string starting back, the draw reaching the wall, and the
hold crossing `k_steady_hold_seconds` — as one-frame edges rather than states,
so `combat.bow_draw`, `combat.bow_full_draw` and `combat.bow_strain` fire once
each instead of every frame. The loose picks its cue from the arrow's visual
style, so a commander's shot leaves on `combat.bow_loose_heavy` while a rank of
archers still gets `combat.arrow_launch`. All four are synthesised recipes in
`tools/audio_synth/cues.py`.

### Free Aim

Nothing about the shot consults a locked target. The arrow is aimed at whatever
the crosshair covers, resolved by ray-casting enemy soldier bodies as upright
cylinders, and an arrow that hits nothing still flies its full range and plants
itself in the ground - a miss has to be legible.

Four details keep aiming honest:

- **The camera axis is the truth.** The reticle is drawn at the centre of the
  screen, so the only line that can be aimed with is the one the projection
  puts there: the axis from the camera's eye to the point it is looking at.
  `CommanderControlController` writes that axis into the aim component every
  frame after `update_camera`, and `crosshair_ray` uses it in preference to the
  raw view angles. Yaw and pitch alone would be a different line whenever the
  camera is still settling, is pushed off a wall, or is being pulled toward a
  lock-on target, and every one of those cases used to move the shot away from
  the reticle without telling the player.
- **The shot is resolved along that line, the arrow is drawn from the bow.**
  `commander_aim_ray` slides the sight ray forward to the commander's eye plane
  so bodies beside and behind him are not in front of the crosshair, and
  `resolve_bow_shot` hit-scans along it. The impact point it finds becomes the
  arrow's destination; the arrow itself launches from `bow_muzzle`. What the
  reticle covers is what dies, and the projectile still leaves the weapon.
- **Aiming settles the view.** Drawing blends the camera to a tighter
  over-the-shoulder framing at `k_fov_aim`, stiffens the camera spring, damps
  head bob and strafe lean, drops the lock-on pull on the camera target, and
  scales look sensitivity by the FOV ratio so the same mouse travel means the
  same on-screen travel zoomed or not. The framing is held for the whole bow
  action, recovery included, so the impact is watched from the same shot the
  player aimed with.
- **The aim cone is earned.** `aim_spread_degrees` opens the cone for movement,
  sprinting, a half-drawn string, a long hold, and low stamina, and the reticle
  draws that same cone in pixels — projected through the live vertical FOV, not
  a hard-coded one. The cone is kept current in bow stance even when the string
  is down, so the penalty for moving is visible before the shot rather than
  after it. A planted archer at full draw is nearly a laser; one shooting on the
  run is not.

### The Aimed Arrow

A commander's shot is not one of the arrows in a volley and does not look like
one. `ArrowVisualStyle::Aimed` gives it a heavier, longer shaft, a six-segment
streak, a hot glow and a light that travels with it, and draw power scales its
size, brightness and trail on top of the damage and speed it already scaled.

The impact is built to be read at a glance: a cone of sparks thrown back along
the incoming direction — red off a body, white-hot off ground and stone — dark
blood mist on a body hit, a short flash with its own light, and a hit-confirm
ring on the target. Landing a shot bumps `hit_confirm_sequence` on the shooter's
`RpgCommanderTargetComponent`, which is what kicks the camera; a kill kicks it
harder.

`rpg_bow_volley` in the arena catalog is the regression contract: nine
chargers, nine drawn shots, nine bodies, with `GroupDestroyed` asserted for
every one of them.

## The Engagement Ring

`refresh_commander_engagement` (in `rpg_combat_system/rpg_combat_processor.cpp`)
builds the ring around a first-person commander each RPG tick: every living enemy
inside `ring_radius` gets a slot, and three of them get active roles — one front
attacker plus a left and a right threat. Everyone else is support.

Three rules keep the ring from breaking the picture on screen:

- **Fight context is derived, not guessed.** The refresh classifies the ring
  onto `RpgEngagementComponent::fight_context`: `Duel` when every opponent in
  the ring is a single body, `Skirmish` the moment any opponent is a formation
  unit (`FormationCombat::has_formation_slots`), `None` when the ring is empty.
  Camera, HUD, and behavior variants key off this one value instead of each
  deriving their own answer.

- **Roles are sticky.** An incumbent front attacker or side threat keeps its
  role for as long as it stays inside a slightly widened sector (front ±80°
  against ±65° for a fresh pick; sides 20°–145° against 30°–135°). Re-picking
  from scratch every frame made near-tied candidates swap roles on tiny
  position jitter, which read as enemies teleporting between stances. Because
  the roles persist on the component, `find_primary_target` in the controller
  reads last tick's ring instead of forcing a second full refresh per frame.

- **Formation units are pinned.** The ring never issues movement orders to a
  formation unit and never writes its facing. Dragging the squad anchor around
  the commander made the formation layer re-resolve every soldier's slot around
  a moving, rotating origin — soldiers slid sideways, swapped slots, and popped
  between animation clips. A formation that enters the ring holds where the RTS
  combat systems put it; only its engaged soldiers step out, which is the
  formation presentation layer's job. Single-body enemies still approach and
  orbit, but their facing goes through `desired_yaw` so turning is animated by
  the movement system rather than snapped.

`RpgEngagementSystemTest` pins all three rules.

### The ring runs as a system, not from the app layer

`RpgEngagementSystem` (registered in `runtime_system_registry.cpp`, just before
`CombatSystem`) finds the fpv-controlled commander itself and runs the tick.
It used to be called from `CommanderModeCoordinator::update_commander_control_mode`,
which meant only `GameEngine` ever ran it: the arena drove `CommanderControlController`
directly, so every arena capture and promo ran with an empty engagement component —
no fight context, no camera framing, no threat pips — while the shipped game had them.
Anything that hosts a first-person commander now gets the ring for free, and the
duplicate per-frame refresh the controller used to do is gone with it.

### The unblockable heavy

Every enemy swing used to be answerable the same way: raise the guard. `RtsHeavyOverhead`
is the exception the player has to read. Roughly every fourth swing a swordsman throws at
a first-person commander is this action instead of `RtsSwordStrike`: a slower wind-up
(contact at 0.56 of a 1.55 s action against 0.40 of 1.0 s), half again the damage, and
`DamageProfile::unblockable`.

Unblockable is a property of the damage, not of the action, so it travels the existing
path — `request.damage_profile` into `commander_damage_profile()` into
`CommanderDamageProfile` — without a new parameter anywhere. In
`resolve_commander_guard` an unblockable blow skips the block entirely and spends its
guard pressure on posture, so holding block against one is actively worse than moving;
`resolve_perfect_guard` is skipped for the same reason. The dodge is the answer:
`dodge_invincible` is checked before any of this, so i-frames still work.

The cadence advances on `melee_attack_sequence`, which `begin_rts_melee_action` now
increments. It previously never moved for RTS attackers, so a cadence keyed on it was
constant per attacker — a soldier either threw a heavy on every swing or never threw one
at all. Hashing the attacker id only phases _where_ in the cycle each soldier starts.

The tell is the telegraph ring. An ordinary wind-up warms orange to red as it winds; an
unblockable is red from the first frame, half again as wide, and pulses faster.
`AnUnblockableHeavyGoesStraightThroughARaisedGuard`, `ADodgeStillBeatsAnUnblockableHeavy`
and `AnOrdinaryStrikeIsStillStoppedByTheGuard` pin all three halves of the rule.

### One contact, one spark, on the body

A contact spark is drawn at the contact point, and the contact point is sampled from
the weapon socket. For an overhead swing that socket is the blade tip, two to three
metres in the air, so every landed blow threw a starburst that hung above the
fighters' heads and read as a bug from a low camera. `queue_rpg_contact_presentation`
now treats the contact as a mark on the body it landed on: the point is snapped to
the target when it is implausibly far in plan, and its height is clamped into the
impact band between 0.35 m and 1.55 m above the target's feet.

The strike flash that follows an enemy's wind-up is a single quick ground ring. It
used to be two concentric expanding rings in the same visual language the aim and
lock markers use, which put a second highlight under whatever the player was already
aiming at. Ground rings mean _target state_; impact is carried by the spark.

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
