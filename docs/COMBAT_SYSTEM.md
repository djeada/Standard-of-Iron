# Combat System

Combat in Standard of Iron is coordinated by `Game::Systems::CombatSystem`. It owns the shared combat query context and advances normal attacks, combat state, formation contacts, siege behavior, elephant behavior, automatic engagement, target commitment, hit feedback, and threat alerts in a fixed order.

The combat system is not a single damage function. It is a pipeline that decides who may fight, whether two subjects can physically engage, which combat mode applies, when an attack is ready, how damage is modified, what special processor owns the exchange, and what feedback or target state must survive into the next tick.

The orchestration entry point is `game/systems/combat_system.cpp`. Most implementation lives below `game/systems/combat_system/`.

## Combat update order

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

The order is part of the current combat contract.

It means, for example, that normal attack processing runs before siege and elephant specials, automatic target acquisition happens after explicit attacks and specials, and target commitment is refreshed again at the end after the processors have had a chance to change targets.

A change to this sequence can alter gameplay even if no individual processor changes, because later stages observe the state published by earlier ones.

## End-to-end attack flow

A typical explicit attack travels through several layers:

```text
player / AI / replay / mission command
        │
        ▼
command validation
        │
        ├─ subject can attack?
        ├─ target exists/alive?
        ├─ ownership/hostility valid?
        └─ ordered target rule valid?
        ▼
attack target/state assigned
        │
        ▼
CombatSystem tick
        │
        ├─ query context
        ├─ target commitment
        ├─ combat-state transition
        ├─ range / obstruction / cooldown
        ├─ normal or special processor
        ├─ damage pipeline
        └─ feedback / alerts / target refresh
```

The command layer decides whether the order may enter the simulation. The combat layer decides whether and how the exchange can resolve on the current tick.

## Shared combat query context

`CombatQueryContext` is rebuilt once at the start of the combat update.

It carries shared entity/spatial lookup information used by normal attacks, special attacks, target acquisition, and related combat queries. Reusing one context has two purposes:

1. avoid rebuilding equivalent world/spatial views in every processor; and
2. keep target and hostility decisions consistent across processors within one combat update.

Code that already receives `CombatQueryContext` should use it rather than performing a second world-wide combat scan.

This is especially important in massed battles, where repeated “find nearby enemy” work can become much more expensive than the actual per-contact combat calculation.

## Target rules

The common targeting API is defined in `game/systems/combat_system/target_rules.h`.

A target check has the shape:

```cpp
Combat::evaluate_target(
    owners,
    attacker_owner_id,
    target,
    {.intent = ..., .allow_buildings = ...});
```

The result is a `TargetRefusal` value.

| Result         | Meaning                                                      |
| -------------- | ------------------------------------------------------------ |
| `None`         | the target is allowed                                        |
| `NoTarget`     | null, dead, pending removal, or not a combat target          |
| `SelfOrAllied` | target belongs to the attacker or an allied team             |
| `Passive`      | passive wildlife is protected from automatic acquisition     |
| `Structure`    | the target is a building while the query disallows buildings |

`may_attack()` exposes the same rule as a boolean when the caller does not need the refusal reason.

The important design point is that target legality is shared. UI target classification, command validation, AI hostile-contact collection, and combat processors should not each invent their own owner/team/wildlife rule.

## Ordered vs automatic engagement

`EngagementIntent` separates deliberate orders from opportunistic acquisition:

- `Ordered` — a player, AI command, script, or other command producer intentionally selected the target;
- `AutoAcquired` — combat logic selected the target because it was nearby and valid.

Passive wildlife is the clearest behavioral difference. A deliberate ordered attack may target passive wildlife; automatic combat acquisition does not turn nearby passive animals into ordinary hostiles.

This distinction keeps “can the player explicitly attack it?” separate from “should an idle soldier spontaneously attack it?”

## Ownership and hostility

Ownership is not tested with a simple `owner_id != attacker_owner_id` comparison.

Team/alliance hostility is resolved through `OwnerRegistry` and `Combat::owners_are_hostile()`. Neutral ownership, allied teams, mission ownership, and wildlife state therefore pass through the shared combat rule instead of being inferred from raw IDs.

This rule matters in mission content where several AI owners can belong to one team, and in scenes where neutral entities exist without being automatic combat targets.

## Attack capability

Target validity is only half of an attack command. Subjects must also be able to attack.

Command validation filters entities that do not support attack mode. Healers, builders, structures, or other non-attacking subjects therefore do not silently become ordinary attackers just because they were part of a mixed selection.

The command path can accept the attacking subset while rejecting or ignoring subjects that do not satisfy the attack capability contract according to the command implementation.

## Range model

`AttackComponent::range` is the outer attack range. `AttackComponent::min_range` provides an optional inner dead zone for ranged attacks.

The effective range used by gameplay is not always identical to the raw component value. `Combat::is_in_range()` and `Game::Systems::resolve_attack_range()` apply the relevant combat state and stance rules.

UI and targeting code that needs to explain the range the unit will actually use should query the resolved value instead of duplicating the base component number.

### Melee range

Melee reach also interacts with formation/contact geometry and obstruction. A raw center-to-center distance alone is not sufficient when two large formations are facing each other or a blocking structure lies between them.

"Have I reached melee contact with this target?" has exactly one implementation, `Combat::melee_contact_reached(attacker, target, geometry)` in `combat_utils.cpp`, fed by one `FormationCombat::contact_geometry()` result. It is the elephant penetration distance when an elephant meets a formation, `FormationCombat::contact_is_active()` when either side has formation slots, and `single_combat_strike_distance()` for two single bodies. The attack processor asks it in three places — to confirm an in-range ordered target, to decide whether a chasing unit may stop, and to filter the in-reach auto-acquire pick — and those three used to carry their own copies of the same three-way branch, which could and did drift apart. Do not add a fourth copy; extend the predicate.

### Combat mode

`update_combat_mode()` picks melee or ranged for a unit whose `preferred_mode` is `Auto`. It only needs to look at enemies when the unit can fight both ways: a unit with `can_ranged == false` is melee on every branch of the decision and a unit with `can_melee == false` is ranged on every branch, so both return before the neighbourhood scan. For the hybrid case the scan sorts the neighbours by centre distance and stops as soon as the next candidate's centre distance minus two contact extents cannot beat the closest surface distance found so far; `contact_geometry()` is only resolved for the candidates that survive. Before this, every unit with a target resolved contact geometry against every enemy within reach plus two formation widths on every tick, which on `sim_benchmark --units 1000` was 85% of the whole simulation tick.

### Ranged dead zones

A ranged unit with `min_range` can have a valid hostile target inside its outer range but still be too close to fire through the ordinary ranged path. That inner limit is part of the attack component rather than an ad-hoc UI rule.

## Obstructions between combatants

Melee combat checks whether a blocking building footprint or closed gate separates attacker and target.

`Combat::structure_separates_combatants()` and the related wall/gate helpers prevent melee damage and formation contact from passing through blocking structures.

This physical-separation rule is important for three different systems:

- direct melee damage;
- formation-contact geometry; and
- pursuit/path selection when a deliberate melee target is behind an obstacle.

### Ordered melee bypass

An explicit ordered melee chase can request a bypass/contact destination around a separating structure. The helper finds a candidate walkable contact position near the target; pathfinding remains responsible for proving an actual route to it.

The combat system therefore does not make every visible enemy “melee reachable” merely because a point exists somewhere on the far side of a wall.

### Automatic engagement near walls

Automatic engagement is intentionally more conservative. An idle unit does not turn every hostile visible through or around a fortification into a long pathfinding detour.

`Combat::melee_walled_off_from(attacker, target, allowed_detour)` is the one answer to "is this enemy behind a wall?". A structure on the straight line only separates two melee combatants when the walked route around it (`Combat::melee_walk_around_length`, a real `Pathfinding::find_path` query) is longer than the straight line by more than the allowed detour: `k_opportunity_walk_around_detour` (8 m) when a unit picks its own fight, `k_answering_walk_around_detour` (16 m) when it answers an attacker or a neighbour's alert. A house, a shrine anchor or a lone tower is walked round; a curtain wall is not. With no pathfinder (bare test worlds) the helper falls back to `melee_bypass_destination`.

### Ranged attacks

Ranged attacks use ranged line/range rules rather than the melee structure-separation rule. A wall that blocks two swordsmen from contacting each other does not automatically imply that a projectile cannot pass over or around it.

## Combat state processing

`process_combat_state()` maintains the state that determines whether units are entering, holding, or leaving a combat interaction.

This state is separate from simply “has an attack target.” A unit can know its target while still moving into range, be locked into melee contact, or be leaving combat after the target disappears.

Keeping combat state explicit allows movement, animation, formation presentation, and targeting to agree on whether a unit is currently engaged.

## Normal attack processor

`combat_system/attack_processor.cpp` owns the ordinary attack path.

Its responsibilities include:

- resolving the current target;
- rejecting targets that became invalid;
- cooldown progression;
- melee/ranged range checks;
- melee lock behavior;
- stance/terrain/counter modifiers;
- projectile/visual submission hooks for ranged combat;
- attack animation state; and
- damage application.

Specialized combat processors run in the same `CombatSystem` update but do not create a second world-level combat authority.

## Damage application

Authoritative health changes go through the combat damage path rather than being inferred from animation or visual contact.

Damage can be modified by several independent systems, including:

- unit/counter relationships;
- high-ground or terrain state;
- defensive unit-layout effects;
- army-formation cohesion state; and
- special attack context.

The exact current counter constants live in `game/systems/combat_system/combat_types.h` and current troop data. [UNIT_BALANCE.md](UNIT_BALANCE.md) documents the active relationships and deterministic balance fixtures.

### Formation and defensive-layout multipliers

Defensive unit layouts and army formations are separate systems, and their combat modifiers compose in the damage path.

For example, the defensive-layout service can apply its context-sensitive protection while `ArmyFormationRuntime::damage_taken_multiplier()` applies the group cohesion modifier. One does not replace the other.

This matches the architecture described in [FORMATION_ARCHITECTURE.md](FORMATION_ARCHITECTURE.md): internal soldier layout and army-scale grouping are independent layers.

## Deterministic melee exchange

RTS melee bodies that participate in the defensive exchange model use `combat_system/melee_exchange.*`.

A swing can resolve as:

| Outcome   | Damage behavior       | Presentation         |
| --------- | --------------------- | -------------------- |
| `Clean`   | clean boosted contact | normal hit/flinch    |
| `Heavy`   | stronger contact      | heavier stagger      |
| `Blocked` | reduced damage        | guard/block response |
| `Evaded`  | no contact damage     | evade/whiff response |

The exchange sequence is deterministic rather than random. Attacker/target identity selects the phase of the exchange sequence, preserving replay stability while still producing varied-looking contact outcomes.

The sequence constants are constructed so the repeated exchange preserves the intended long-run combat rate instead of quietly adding an uncontrolled random DPS bonus or penalty.

Targets that do not participate in this defensive model use the ordinary clean-contact path.

## Melee locks

Melee locks prevent ordinary navigation and combat contact from simultaneously trying to own the same close-quarters motion.

Once units are engaged, the lock expresses that they are fighting rather than still freely pathing through one another.

Single-body one-on-one combat can use duel footwork around the lock. Formation members remain constrained by their formation/contact presentation instead of turning every army engagement into independent per-soldier circling.

## Formation contact

`update_formation_contacts()` publishes contact/front information used by formation combat and presentation.

Formation contact is not just “bounding boxes overlap.” It must respect whether combatants can physically engage. Blocking structures, slot geometry, formation frontage, and the current combat state all influence what the simulation can treat as a valid contact front.

The result helps keep several views of the fight aligned:

- the authoritative combat interaction;
- soldier-level formation presentation;
- hit/weapon origin placement; and
- visual front lines between formations.

See [FORMATION_ARCHITECTURE.md](FORMATION_ARCHITECTURE.md) for soldier anchors, stable slots, traversal layouts, and army-group state.

## Hit feedback

`process_hit_feedback()` advances short-lived combat reaction state.

Damage results can publish:

- reaction type;
- stagger/block/evade presentation;
- knock-step or local reaction data; and
- formation-slot hit presentation.

The reaction layer does not become health authority. A visible stagger can move presentation locally without changing which formation slot the soldier owns or creating a second positional simulation.

## Siege specials

`process_siege_specials()` runs after ordinary attacks and formation contacts.

Siege behavior uses the same ownership/target/damage infrastructure as the rest of combat but adds siege-specific attack behavior for units and structures that require it.

Counter relationships involving siege units are defined in the same combat constants and current troop data used by the balance suite.

## Elephant specials

`process_elephant_specials()` handles elephant-specific contact and special behavior, including the dedicated elephant combat components and trample/contact path.

Elephants still pass through ordinary target ownership/hostility rules. Their special processor changes how an eligible contact resolves; it does not grant permission to attack otherwise invalid subjects.

## Mounted charge interaction

Mounted charge state has its own processor path under combat. Defensive-layout state can block charge initiation where the current defensive service says the unit cannot charge.

This is another example of combat composing subsystem state rather than encoding every stance rule directly inside the attack processor.

## Auto engagement

`AutoEngagement::process()` lets eligible units acquire nearby hostile targets after explicit attack processing and special processors.

It uses `EngagementIntent::AutoAcquired`, which means:

- allied/self targets remain invalid;
- passive wildlife is ignored;
- structure eligibility follows the query context; and
- target commitment can preserve a sensible existing choice.

Auto engagement is therefore an opportunistic targeting layer, not a separate attack implementation.

## Chase policy

Whether a unit walks after its target is decided in one file, `combat_system/target_assignment.{h,cpp}`. Every automatic writer of `AttackTargetComponent` names its reason with a `TargetSource`, and `set_attack_target()` / `assign_attack_target()` fill in `should_chase` and `is_player_command` from `chases_target()`:

| Source        | Writer                                                              | Chases                                                                                     |
| ------------- | ------------------------------------------------------------------- | ------------------------------------------------------------------------------------------ |
| `Opportunity` | `AutoEngagement`                                                    | fighters that close to fire, when not holding                                              |
| `Answering`   | `engage_threat_target` (retaliation, squad alerts, joining an ally) | fighters, including ranged ones, and noncombatants stepping into a brawl, when not holding |
| `InReach`     | the attack processor's in-strike-range pick                         | never                                                                                      |
| `Patrol`      | `PatrolSystem`                                                      | never                                                                                      |
| `MeleeLock`   | `sync_melee_lock_target`                                            | player orders and fighters                                                                 |
| `Commitment`  | `TargetCommitmentSystem` / a blocked lock switch                    | always                                                                                     |

"Holding" is hold mode or a formed defensive unit layout. The three automatic scans (`Opportunity`, `Answering`, `Patrol`) clear `is_player_command`; the others keep it.

The reader is `keeps_pursuing()`, which the attack processor asks whenever a target is out of reach: it drops the target unless `should_chase` is set, a non-pursuing unit is still inside its brawl leash, the unit is free to move, and the target is inside its guard reach. Explicit player and AI attack orders store the order's own chase flag in `CommandService::attack_target`; that module sits below combat and does not decide anything.

### When a chase order is re-issued

A chase destination that comes from `chase_destination()` is a point on a ring around the target — at the standoff distance, on the attacker's side, rotated by the attacker's spread angle. Because the ring point is derived from the attacker's own position it rotates as the attacker walks, so comparing "where I planned to go" with "where I would go now" re-issues the order because the attacker moved, which is every few ticks for the whole approach. `should_queue_chase_command()` therefore asks whether the *world* changed: for a ring goal it re-issues only when the target has moved off the ring the planned goal sits on (the planned goal's distance to the target differs from the desired standoff by more than `k_new_command_threshold`), and never because the attacker took a step. Goals that do not follow the attacker — an engagement-slot anchor, a structure approach point, a bypass around a wall — keep the plain point comparison, because for them a changed point *is* a changed world. A unit with no active movement always gets its order. Each re-issue is a full `issue_move`: it bumps the order sequence, resets the stall ladder, and re-resolves the route, so it is not free even when the route turns out to be a straight line.

## Target commitment

Target commitment reduces target churn.

Without commitment, repeated proximity queries can cause units to bounce between equally plausible enemies every update. `TargetCommitment::update()` maintains the current commitment state and is run both before attack processing and again at the end of the combat update.

The final zero-delta update lets the commitment system observe target changes made by the processors in the same combat tick.

## Threat alerts

`tick_threat_alerts()` updates combat-threat notification state after attack and auto-engagement processing.

Every "a unit was hit" path goes through `Combat::answer_attacker()`: the damage path (`AnswerPolicy::KeepCurrentFight`, a unit already fighting another _troop_ keeps that fight) and first melee contact (`AnswerPolicy::TurnOnAttacker`). It retaliates when `may_engage(..., Retaliation)` allows and then raises an `UnderAttack` alert. A unit breaching a wall keeps breaching when archers shoot it over the wall (`MeleeEngagementTest.BesiegerShotOverTheWallKeepsBreachingInsteadOfChasing`).

An `UnderAttack` alert recruits every free ally within `threat_alert_radius()` of the victim, up to `k_max_squad_alert_allies` (12). There is deliberately no per-aggressor quota: with the old limit of three responders per attacker, one raider hitting a group of six left three men standing beside the fight. An `EnemySighted` alert still recruits one ally, and the unit that raised it no longer counts against that one slot (before, the spotter's own target used the slot up, so sighting alerts recruited nobody).

Alerts are pushed once per second from the unit that was hit, so an ally that was busy at that moment would miss the fight. `AutoEngagement` therefore also _pulls_: an idle AI-controlled unit with nothing in its own acquisition range joins the nearest troop fight of an ally within its vision range (`Combat::ally_fight_to_join`, recorded as `EngagementOutcome::AssistedAlly`). The joined target must pass `may_engage(..., SquadAlert)`, so guard reach, walls and hold rules still apply, and only a fight against an armed enemy is joined: an ally battering a building or chasing an unarmed scout fights alone (`MissionWaveAssaultTest.GarrisonAnswersAScoutWithAFewUnitsAndHoldsTheRest`). Player units do not do this; they fight what they see and what hits them.

A ranged unit answering an attack or an alert chases into range (`TargetSource::Answering`, see [Chase policy](#chase-policy)). Opportunistic acquisition still uses weapon range only, so archers do not wander after targets they merely see.

Threat alerts can feed higher-level systems such as AI local response and player presentation, but they are not another source of damage or targeting authority.

Keeping alerts downstream of actual combat processing means they describe combat that the runtime recognized rather than speculative proximity alone.

## Hold and guard interactions

Combat behavior composes with order/stance systems.

Guard mode has one set of helpers in `combat_utils.h`, used by `AutoEngagement`, `may_engage`, the attack processor, `GuardSystem` and the undead leash alike:

- `guard_post_of()` — the post, following a guarded entity when there is one;
- `guard_reach_of()` / `within_guard_reach()` — the reach circle, centred on the post or on `reach_center_*` when set, with the `Strict` and `AnswersFire` rules;
- `is_returning_to_guard_post()` — the return flag, but only while the unit is actually moving, so a walk home that ended short cannot leave a guard refusing every fight; and
- `send_guard_home()` — the single place that issues a `GuardReturn` move.

An automatic `AttackChase` move no longer switches guard mode off; only explicit player, formation, attack-move, scripted and AI planner moves do.

Hold/guard state can change effective attack behavior, movement response, or range without inventing a parallel combat system. For example, effective range exposed to UI uses the same resolved range path that combat uses.

Defensive unit layouts can additionally hold position or alter movement/turn/damage behavior through `DefensiveUnitLayoutService`, as described in the formation documentation.

## Projectile and ranged presentation

Ranged attacks submit projectile/visual state through the attack path while authoritative hit/damage remains simulation-owned.

The visual projectile exists to represent the attack, not to become a second source of truth for whether a target lost health. This separation is important for replay and headless simulation, where combat must remain valid without relying on renderer timing.

## Player feedback and inspection

The UI can show target highlights, attack-state markers, refusal text, damage numbers, and current command target because application/read-model code consumes the same target and combat state produced by the simulation.

Presentation is allowed to aggregate or animate that information, but target legality and health changes remain in simulation.

This avoids cases where an attack cursor says “valid” using one rule while the command path rejects the same target using another.

## Command integration

Player, AI, mission, Arena, and replay attack commands enter through the typed command pipeline.

Command validation uses shared combat target/capability rules before attack state reaches the combat system. Replay playback then reuses the same combat execution path from the recorded command stream.

The result is one set of target semantics for all issuers.

## Balance integration

Combat tuning is checked with production simulation rather than a standalone paper formula.

`balance_sim` loads current troop/combat data and executes deterministic fixtures that encode important matchup relationships. The fixtures cover mirrors and current counter relationships such as spear/cavalry, infantry/siege, archer/elephant, spear/sword, faction line matchups, commander/line interactions, and other maintained cases.

See [UNIT_BALANCE.md](UNIT_BALANCE.md) for the current fixture catalogue and combat constants.

## Common invariants

The current combat architecture depends on these invariants:

- one shared target rule decides ownership/intent legality;
- shared query context is built once per combat update;
- explicit orders and auto-acquisition remain distinct intents;
- melee cannot deal/contact through blocking structures;
- authoritative damage is separate from hit presentation;
- special processors still use the shared ownership/damage model;
- target commitment prevents needless retarget churn;
- combat commands enter through the typed command boundary; and
- deterministic exchange/command timing does not depend on renderer frame timing.

These invariants are more durable than a particular damage constant and are the first things to preserve when changing combat behavior.

## Testing

Combat is covered across simulation, command, formation, balance, replay, and scenario tests.

Important contracts include:

- `TargetRefusal` behavior and hostility rules;
- ordered vs automatic wildlife targeting;
- attack-subject capability validation;
- effective attack range and hold-mode agreement;
- wall/gate melee separation;
- explicit bypass behavior;
- deterministic melee exchange;
- defensive-layout and formation damage composition;
- formation contact;
- siege and elephant special behavior;
- auto engagement;
- target commitment; and
- replay/deterministic combat behavior.

When debugging a combat failure, it is useful to identify the first broken layer:

1. command/subject validation;
2. target legality;
3. path/range/obstruction;
4. combat state/lock;
5. normal vs special processor;
6. damage modifiers/application; or
7. presentation/feedback.

That prevents a visual symptom from being mistaken for an authoritative damage bug.

## Source map

| Concern                      | Source                                              |
| ---------------------------- | --------------------------------------------------- |
| Combat orchestration         | `game/systems/combat_system.cpp`                    |
| Shared target rules          | `game/systems/combat_system/target_rules.*`         |
| Normal attacks               | `game/systems/combat_system/attack_processor.cpp`   |
| Damage application           | `game/systems/combat_system/damage_application.cpp` |
| Melee exchange               | `game/systems/combat_system/melee_exchange.*`       |
| Threat/engagement/commitment | `game/systems/combat_system/`                       |
| Counter constants            | `game/systems/combat_system/combat_types.h`         |
| Troop combat data            | `assets/data/troops/` and nation data               |
| Balance fixtures             | `assets/balance/`, `tools/balance_sim/`             |

The processor order in `CombatSystem::update()` and the shared target rules are the authoritative current contract. Historical bug narratives are not required to explain how the combat system works now.
