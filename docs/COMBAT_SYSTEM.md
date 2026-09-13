# Combat System

RTS combat is coordinated by `Game::Systems::CombatSystem`. It owns the shared combat query context and runs the normal attack, formation-contact, siege, elephant, auto-engagement, hit-feedback, and threat-alert processors in a fixed order.

The combat code is split under `game/systems/combat_system/`; `game/systems/combat_system.cpp` is the orchestration entry point.

## Update order

`CombatSystem::update(world, delta_time)` currently runs:

```text
rebuild_combat_query_context
TargetCommitment::update
process_hit_feedback
process_combat_state
process_attacks
update_formation_contacts
process_siege_specials
process_elephant_specials
AutoEngagement::process
tick_threat_alerts
TargetCommitment::update(world, 0)
```

That order defines the combat tick contract. Processors that need target lookup reuse the query context built at the start of the update.

## Shared combat query context

`CombatQueryContext` is built once per combat update and carries the shared entity/spatial lookup data used by attack/special/auto-engagement code.

The context avoids independent full-world target scans inside each processor and keeps hostility/target filtering consistent across normal and special attacks.

Combat code that already receives `CombatQueryContext` should use it rather than rebuilding a second combat view from `World`.

## Target rules

`game/systems/combat_system/target_rules.h` defines the common target decision API.

```cpp
Combat::evaluate_target(
    owners,
    attacker_owner_id,
    target,
    {.intent = ..., .allow_buildings = ...});
```

The result is a `TargetRefusal`:

| Result | Meaning |
| --- | --- |
| `None` | target is allowed |
| `NoTarget` | null, dead, pending removal, or not a combat target |
| `SelfOrAllied` | target belongs to the attacker or an allied team |
| `Passive` | passive wildlife is protected from auto-acquisition |
| `Structure` | target is a building but the query disallows buildings |

`may_attack()` exposes the same rule as a boolean for callers that do not need the refusal reason.

### Ordered vs auto-acquired intent

`EngagementIntent` has two values:

- `Ordered` — a player/script deliberately chose a target;
- `AutoAcquired` — combat logic selected a target opportunistically.

Passive wildlife is the main distinction. Ordered targeting may deliberately attack wildlife. Auto-acquired targeting does not turn nearby passive animals into ordinary enemy contacts.

The same intent model is used by command validation, attack-cursor classification, target highlights, right-click attack picking, and AI hostile-contact collection.

### Ownership and hostility

Team/alliance hostility is resolved through `OwnerRegistry` and `Combat::owners_are_hostile()`. Neutral ownership by itself is not equivalent to combat hostility; target intent and wildlife state still participate in the target decision.

Combat processors should not replace this rule with direct owner-ID comparisons.

## Attack capability

Attack commands also validate the attacking subjects. Units that cannot use attack mode are filtered before the order is accepted. Healers, builders, structures, or other non-attacking subjects therefore do not silently pass command validation as ordinary attackers.

## Attack range

`AttackComponent::range` is the outer attack range. `AttackComponent::min_range` is an optional inner dead zone for ranged attacks.

`Combat::is_in_range()` resolves the actual combat reach after the relevant melee/ranged, structure, formation, and stance rules.

UI and targeting code that needs the range a unit will actually use should call `Game::Systems::resolve_attack_range()` rather than reading the base range directly, because hold-mode bonuses can change effective reach.

## Structures between combatants

Melee combat checks whether a blocking building footprint or closed gate separates the attacker and target.

`Combat::structure_separates_combatants()` and the related melee-wall helpers prevent melee damage and formation contact from passing through blocking structures.

An explicit ordered melee chase can request a bypass destination around a separating structure. The bypass helper searches for a walkable contact position near the target and leaves route feasibility to pathfinding.

Ordinary opportunistic auto-engagement does not turn every enemy visible through/around a wall into a long pathing detour. Retaliation/local-threat logic has its own reachability path for responding to an aggressor that can be walked around to.

Ranged attacks use their ranged targeting/line-of-fire rules rather than the melee separation rule.

## Normal attacks

`combat_system/attack_processor.cpp` handles the ordinary attack path, including:

- target resolution;
- cooldowns;
- melee/ranged range checks;
- melee lock state;
- stance/terrain/counter multipliers;
- ranged projectile/visual submission hooks;
- combat animation state; and
- damage application.

Special attacks configured through the relevant attack components are dispatched by the same combat update rather than through a separate world-combat authority.

## Deterministic melee exchange

RTS melee bodies that can defend use the deterministic exchange system in `combat_system/melee_exchange.*`.

A swing can resolve as:

| Outcome | Damage behavior | Presentation |
| --- | --- | --- |
| `Clean` | boosted clean contact | normal hit/flinch |
| `Heavy` | stronger contact | heavier stagger |
| `Blocked` | reduced damage | guard/block response |
| `Evaded` | no contact damage | evade/whiff response |

The exchange sequence is deterministic rather than random. Attacker/target identity selects the phase of the sequence, keeping replay behavior stable.

The melee-exchange constants are constructed so the multi-beat damage/cadence cycle preserves the intended long-run combat rate rather than adding an uncontrolled random DPS modifier.

Targets that do not participate in the defensive exchange model use the ordinary clean-contact path.

## Hit feedback

`process_hit_feedback()` updates short-lived combat reaction state. Damage outcomes can publish reaction kinds, knock-step presentation, block/evade feedback, and formation-slot hit presentation.

Authoritative health/damage is separate from presentation. Formations can show a hit/reaction without moving the authoritative slot assignment for every visual knockback.

## Melee locks and footwork

Melee locks keep engaged units from being simultaneously driven by ordinary movement logic.

Single-body one-on-one combat can use duel footwork around that lock while formation members retain their formation constraints. The current footwork logic changes relative presentation/position within the melee contract without turning formation combat into free-form per-soldier circling.

## Formation contact

`update_formation_contacts()` publishes the combat-front/contact information used by formation combat and presentation.

Formation contact only exists when the combatants can physically engage; blocking structures and other reach constraints therefore affect the contact geometry as well as direct damage.

See [FORMATION_ARCHITECTURE.md](FORMATION_ARCHITECTURE.md) for group/slot ownership and formation movement.

## Siege specials

`process_siege_specials()` handles siege-specific behavior after ordinary attacks/formation contacts and before elephant specials/auto-engagement.

Siege counter multipliers and current unit data are documented from the combat constants and troop assets in [UNIT_BALANCE.md](UNIT_BALANCE.md).

## Elephant specials

`process_elephant_specials()` handles elephant-specific combat behavior, including the special contact/trample path defined by the elephant combat components.

Elephant counter rules use the same damage/target ownership model as other combat rather than bypassing alliance/target validation.

## Auto engagement

`AutoEngagement::process()` lets eligible idle/available combat units acquire nearby hostile targets after explicit attack processing.

Auto engagement uses `EngagementIntent::AutoAcquired`, so passive wildlife remains excluded and the same team/hostility rules used by ordered attacks are reused.

Target commitment state helps prevent constant target churn across repeated scans.

## Threat alerts

`tick_threat_alerts()` updates the combat-threat notification state after attack/auto-engagement processing. Those alerts feed higher-level systems such as local AI response and presentation warnings without becoming a second damage system.

## Terrain and counter multipliers

Terrain/role counters are defined in `game/systems/combat_system/combat_types.h`. Current values include the cavalry/spear, siege/infantry, elephant/archer, high-ground, and hold-mode multipliers.

The combat code is the source of truth for how those multipliers are applied. [UNIT_BALANCE.md](UNIT_BALANCE.md) documents the current relationships and deterministic balance fixtures.

## Command integration

Player, AI, mission, and replay attack commands enter through the typed command pipeline. Command validation uses the shared combat target rules before the combat system executes the resulting attack state.

This keeps replay/AI/player targeting under the same ownership and target-validity semantics.

## Tests

Combat behavior is covered across system, command, movement/formation, balance, and replay tests. Important contracts include:

- target refusal/hostility rules;
- ordered vs auto-acquired wildlife behavior;
- attack-command subject validation;
- range/hold-mode agreement;
- wall/gate melee separation;
- melee exchange determinism;
- siege/elephant special behavior;
- formation contact; and
- auto-engagement/target commitment.

The authoritative processor order is `CombatSystem::update()`. Target semantics are defined by `target_rules.*`, and numerical counter tuning is defined by `combat_types.h` plus troop data.
