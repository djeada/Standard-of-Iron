# AI Architecture

Standard of Iron runs one strategic AI instance per computer-controlled owner. The AI is deliberately separated from the live ECS world: the world-owning thread builds an immutable snapshot, worker code reasons from that snapshot, the result is converted into typed AI commands, command conflicts are filtered, and accepted commands enter the same simulation command path used by other command producers.

This architecture keeps AI decision work parallel without turning gameplay components into shared mutable state. It also keeps player, mission, replay, and AI orders under the same validation and execution rules.

The implementation is concentrated under `game/systems/ai_system/`, with strategy configuration in `ai_strategy.cpp`, mission setup in the mission/runtime layer, and commander/town-plan data under `assets/data/ai/`.

## The decision pipeline

One AI decision round follows this shape:

```text
live World
   │
   │ snapshot on world-owning thread
   ▼
AISnapshotBuilder
   │
   ▼
AISnapshot ───────────────┐
   │                      │ immutable observation
   ▼                      │
AIReasoner                │
   │                      │
   ▼                      │
AIExecutor + behaviors    │ worker-side planning
   │                      │
   ▼                      │
AICommand list ◄──────────┘
   │
   ▼
AICommandFilter
   │ arbitration / validation
   ▼
AICommandApplier
   │
   ▼
shared gameplay command/effect path
```

The important boundary is the snapshot. Worker-side AI does not walk the mutable world while simulation is advancing.

## Timing and deterministic application

AI work is asynchronous, but command application is tied to simulation time rather than worker completion time.

A decision job records the simulation update on which its result is due. If the worker is late, the simulation waits for the result instead of applying it on a later tick simply because a thread happened to finish later. This prevents host scheduling from changing when an AI order enters the deterministic command stream.

That rule gives the AI two separate clocks:

- **wall-clock worker time**, which determines how long reasoning physically takes; and
- **simulation update time**, which determines when the result becomes authoritative.

Only the second one affects gameplay.

The startup path uses the same worker system. Mission loading can wait for the initial decision set before hiding the loading overlay; see [MISSION_STARTUP.md](MISSION_STARTUP.md).

## Snapshot: what the AI can observe

`AISnapshot` is the read-only input to one decision round. It contains the match facts required by the strategic and tactical behaviors, including:

- friendly forces and their current state;
- visible or remembered hostile contacts used by the AI;
- strategic objectives and known enemy holdings;
- economy and production information;
- building/base information;
- navigation-related facts required for planning;
- unit readiness and assignment-relevant state; and
- simulation time.

The snapshot is not a second world. It is a purpose-built observation of the authoritative match. If a behavior needs new live information, the correct change is normally to add that information to the snapshot builder rather than let the worker read ECS state directly.

### What an opponent knows

An opponent plans against what it has seen. `visible_enemies` is what is in sight right now; `strategic_objectives` is every hostile building and commander this opponent has _ever_ had something near, remembered at its last-seen position and dropped the moment it stops existing. That memory lives on the AI instance, not in the snapshot, because it has to survive between decision rounds. An enemy base nobody has scouted is not an objective, and an AI will not march on one.

This perception is deliberately **not** the player's fog of war, and the differences are intentional:

|             | AI perception                    | Player fog (`VisibilityService`)      |
| ----------- | -------------------------------- | ------------------------------------- |
| Sources     | the owner's own units            | the owner's and its allies' units     |
| Range       | raw `vision_range`               | floored at the 12m default, then x1.5 |
| Buildings   | 18m floor so a base is not blind | no floor                              |
| Rally flags | not a source                     | a source                              |

The fog is a presentation rule about how much of the map a player is shown; AI perception is a gameplay rule about what an opponent may plan against. Neither may decide what a unit is _allowed_ to attack — that is the engagement rules' job. `tests/systems/ai_scouted_intel_test.cpp` pins the AI side.

## Persistent strategic context

`AIContext` survives across decision rounds and stores the AI's ongoing plan and memory.

Important categories include:

- current strategic state and timers;
- main/base anchors and rally information;
- reserve, garrison, harass, attack-wave, and behavior assignments;
- threat memory;
- clustered bases and their roles;
- expansion/outpost planning state;
- settlement/station information; and
- diagnostics consumed by tests and tracing.

The distinction is simple:

- `AISnapshot` says **what the AI sees now**;
- `AIContext` says **what the AI is in the middle of doing**.

This prevents a decision from being recomputed as if the AI had no memory every time a new snapshot arrives.

## Strategic state machine

The current strategic state set is:

- `Idle`;
- `Gathering`;
- `Attacking`;
- `Defending`;
- `Retreating`; and
- `Expanding`.

State transitions are influenced by the selected strategy, posture, threat state, force readiness, expansion status, and committed-wave state.

These states are broad strategic modes, not exclusive ownership of every unit. A force can be strategically `Attacking` while still retaining a home garrison, a commander escort, or mission assault units under separate behavior ownership.

## Strategy configuration

`AIStrategyFactory` builds an `AIStrategyConfig` from the selected strategy and then applies posture, personality, difficulty, and commander doctrine overrides.

### Supported strategies

`AIStrategyFactory::parse_strategy()` accepts:

- `balanced`;
- `aggressive`;
- `defensive`;
- `expansionist`;
- `economic`;
- `harasser` / `harassment`;
- `rusher` / `rush`; and
- `sepulcher_defense` / `undead_defense`.

Unknown values resolve to `Balanced`.

A strategy config contains macro targets and thresholds such as desired builders, homes, barracks, towers, catapults, attack assembly size, reserve/harass size, scouting distance, local-response limits, expansion distance, and attack thresholds.

The strategy therefore defines the shape of the plan rather than scripting a fixed sequence of orders.

## Posture

Posture controls whether an AI is expected to operate as a local defender or as a field opponent.

`AIStrategyFactory::parse_posture()` recognizes:

- `garrison`, `hold`, `defend` → `Garrison`;
- `field`, `open`, `expand` → `Field`.

### Garrison

A garrison AI keeps ordinary strategic behavior centered on its holdings. It can still:

- defend bases;
- maintain its economy;
- recruit and construct;
- respond to local contacts; and
- command mission-authored assault waves.

What it does not do is treat ordinary strategic attack/expansion as if it were a field opponent.

### Field

A field AI can use the full ordinary attack and expansion behaviors in addition to defence and economy.

Mission setup chooses posture according to authored mission configuration, while skirmish opponents use the field-oriented path.

## Personality

Mission/commander personality inputs expose normalized aggression, defense, and harassment values.

`AIStrategyFactory::apply_personality()` modifies the selected strategy config. Personality therefore biases a known strategy instead of replacing the strategy enum with an opaque collection of one-off values.

This lets two commanders share a strategic family while still differing in reserve size, willingness to harass, or attack pressure.

## Difficulty

`AIStrategyFactory::apply_difficulty()` modifies execution tuning such as decision cadence, production multiplier, and scouting behavior.

Difficulty does not create a different command system. The AI still produces the same command types and uses the same movement, construction, production, combat, and formation rules as any other issuer.

## Behavior composition

The AI is built from behavior modules rather than one monolithic `update()` function.

The active behavior set includes:

- retreat;
- base defence;
- scripted assault waves;
- troop production;
- builder/economy construction;
- commander positioning and abilities;
- expansion;
- local engagement;
- main-force attack;
- harassment; and
- gathering/stationing.

A decision round can therefore produce several kinds of work at once: defend one base, keep builders working, hold a garrison, advance an assault wave, and recruit units for the next strategic cycle.

## Command ownership and arbitration

Multiple behaviors can notice the same unit. The AI resolves this through explicit priority and ownership rather than relying on whichever behavior happens to run last.

`AICommandFilter::arbitrate_move_ownership()` gives movement ownership to the highest-priority claim when competing behaviors try to move one subject in the same decision batch.

The application side also validates subjects before applying commands. A command cannot act on an entity merely because the entity ID existed in an older snapshot. Ownership, lifetime, and current validity are checked again at application time.

This protects against cases such as:

- a unit dying after the snapshot was built;
- an entity slot being recycled;
- ownership changing; or
- a higher-priority behavior already claiming movement.

## Force partitioning

The strategic AI does not treat every combat unit as one interchangeable pool.

### Main force

The main force is the general-purpose pool used for gathering, strategic attacks, committed waves, expansion escorts, and ordinary field movement after units reserved elsewhere are excluded.

### Garrison and reserve

Reserve/garrison assignments keep home defence from disappearing simply because the AI has enough troops to form an offensive force. Strategy and commander doctrine tune the amount held back.

Defence behavior can escalate when an actual base threat appears, allowing strategic assignments to respond to the match instead of remaining static quotas.

### Harass detachment

Strategies that allocate harassment units remove them from the main-force pool and hand them to `HarassBehavior`.

Harass sizing is strategy/personality driven. Defensive pressure can reclaim those units when the home force needs them.

### Scripted assault units

Mission reinforcement waves receive `AssaultWaveComponent` and are owned by `AssaultBehavior`.

These units are intentionally separated from the AI's ordinary strategic posture. A mission can keep the owning AI in a garrison posture while authored reinforcement waves attack on schedule.

Assault-wave units are excluded from ordinary reserve, gathering, expansion, and retreat pools. Their attack path advances toward eligible hostile targets and can breach obstructing barriers when movement toward the objective has stalled.

A wave whose objective is sealed off counts as stalled from its first decision. The snapshot builder flood-fills walkable cells outward from each wave's march target, treating every visible hostile gate as a wall whatever its current open state (the pathfinder's region map connects through gates, because a gate opens for its owner), and gives up as reachable after a few thousand cells. If the fill never reaches the wave (`EntitySnapshot::march_target_reachable`), the objective is inside a closed ring: the wave commits to breaching the barrier on its line at once, instead of walking to the nearest reachable point outside the ring and waiting there for the stall timer.

### Commander

`CommanderBehavior` manages the commander separately from line units. It positions the commander relative to the force, uses commander abilities, and respects doctrine/health/escort constraints.

This prevents the commander from being treated as merely another infantry entity in a bulk movement pool.

The "army" the commander stations on is the muster only (`stands_in_the_muster`): living line units that are not part of a mission assault wave. Waves march on the enemy under their own orders, and counting them once walked the tutorial's Roman commander out of his outpost behind the raid and into the player's camp — his threat step averages its retreat point with the army centre, so an army centre inside the enemy camp drags him there.

## Local engagement vs strategic attack

The AI distinguishes reacting to nearby threats from launching a committed strategic attack.

Local engagement handles contacts that can be answered without turning the entire army into an attack wave. Strategic attack behavior operates on the assembled main force and uses strategic targets, stations, garrison exclusions, and wave commitment state.

This separation matters because a small contact near a base should not automatically dissolve the AI's higher-level plan, while a committed attack should not be rebuilt from scratch every time one frontline unit enters combat.

A unit counts as **engaged** (`EntitySnapshot::engaged`, read through `is_entity_engaged`) when an enemy troop is within `k_engaged_radius` _or_ when it is already fighting troops: it holds a live, non-building attack target or is in a melee lock (`EntitySnapshot::fighting_troops`). Gather, attack-wave and harass selection skip engaged units, so a planner move never pulls an archer off the raider it is shooting from 10 m or a swordsman off the man it is chasing. Every AI `MoveUnits` command is applied as `MoveOrderKind::PlannerMove`, which clears the unit's attack target, so a behaviour that forgot this check would silently end the fight.

## Committed attack waves

`game/systems/ai_system/ai_attack_wave.cpp` manages strategic attack-wave membership.

A committed wave latches its members. Once assembled, those unit IDs remain part of the wave instead of being recalculated every decision round from whichever troops currently look idle.

The flow is broadly:

```text
available strategic force
        │
        ├─ remove garrison/reserve
        ├─ remove separately owned roles
        ▼
select committed-wave members
        │
        ▼
assemble at station/rally
        │
        ├─ readiness threshold
        └─ assembly patience
        ▼
commit wave
        │
        ▼
advance toward strategic objective
```

Targets can come from known strategic objectives as well as currently visible enemies. An army can therefore continue toward a known enemy holding even if the target is not visible at the exact decision moment.

Commander doctrine can tune committed wave size, garrison requirements, regroup timing, spent-wave threshold, and target priority.

### A wave that knows of no enemy goes looking

An opponent marches only on what it has seen, so on a large map a ready wave used to find no target, dissolve, and form again every round: on the historical battle maps played as skirmishes two AI armies stood a field apart for 25 minutes without a blow. Now a wave with no known target commits anyway, with `target_id == 0` and a place instead of a contact. From home the place is the far side of the map (the base mirrored through the centre, kept 15% inside the edges); on arrival, or after 30 s without closing, the next corner round the map from where the wave stands. Anything it sights becomes its target at once. No extra wave state carries the search: `target_id == 0` on a committed wave means "going to a place".

This replaced `AttackBehavior`'s old scouting, which sent the attack force to points 36–60 m from its own base in turn. It never found an enemy that far away, and because it ran on the rounds between the wave's 4 s march orders, it pulled a marching wave back toward home every other round.

### A wave calls up the troops left at home

A committed wave admits reinforcements only from within 30 m of its front, so recruits standing at home cannot keep a spent wave counted as alive. That rule also left an army idle: a wave of four settled into a long siege of towers while twenty soldiers stood at the muster for ten minutes, and no second wave can form while one is committed. After a wave has been out for 60 s (`k_wave_call_up_seconds`) it calls up every committable soldier outside the garrison, wherever it stands and past the usual wave capacity; the recruits march on the wave's target like any member.

### Advancing units belong to the attack

When `AttackBehavior` advances on an enemy it has sighted but that is still out of engagement range, it claims the troops for `attacking` like every other order it gives. It used to move them unclaimed, so `GatherBehavior` still held them for the muster and ordered them back a second later; the two took turns and the army paced the same ground for minutes. The advance is re-ordered only when the target changes or a soldier has stopped short of it, not every round. The same holds at the walls: soldiers already striking the chosen target (`EntitySnapshot::attack_target_id`) are not sent the move-to-ranks and attack orders again, which broke off every blow and left an undefended town standing for minutes.

## Gold veins and the market

Every skirmish map runs out of stone or timber between minutes 10 and 25. What keeps a match going past that is gold: cursed gold veins pay 25 gold every 6 s to whoever holds them, and the marketplace turns gold into whatever the town is short of.

- `AISnapshot::gold_veins` lists every vein anchor with its owner, found by matching barracks-type anchors against the map's `cursed_gold_vein` props. Veins are landmarks every player sees on the minimap, so this is not scouted intel.
- `GoldVeinBehavior` sends two idle soldiers (never the commander, the garrison or wave members) to claim the nearest vein the AI does not hold, neutral or enemy. It releases them the moment the vein is won so the muster pulls them out of the curse, and gives up on a vein it cannot take within 3 minutes for 5 minutes. Expansion ignores vein anchors, so an expansionist AI no longer camps inside the curse.
- When `BuilderBehavior` finds its next building blocked by a resource the map has run out of, it records that building's cost in `AIContext::construction_need`. `EconomyBehavior` buys toward it like the recruiting war chest, and while a building is stalled it sells any stock more than 100 above what it needs, rather than only above 320. A town stripped of stone sells timber for gold and buys stone for its homes.

### Keeping a wave honest

A committed wave records the closest it has come to its target (`best_gap`) and when it last got closer or fought (`progress_at`). A wave that neither gains 4 m on its target nor has a member engaged for 180 s is called off and regroups; before this, nothing read `committed_at`, and a wave that could not reach its target stayed "marching" for a whole match. Reinforcements join a committed wave only from within 30 m of its front, so recruits standing at home no longer keep an exhausted wave from ever counting as spent. When a committed wave sees no enemy, `AttackBehavior` marches it on the wave's own remembered target every 4 s instead of issuing nothing. The `Army` target class excludes non-combatants, so a wave aimed at an army goes for soldiers, not builders and civilians; those remain the `Economy` class. A commander who does not lead from the front anchors on the troops still at home, not on a wave that has marched out, so a garrison commander is not dragged after his own raid.

## Allies

An AI sees what its allies see: allied units are vision sources in its snapshot. Each snapshot also carries two calls from allies:

- `allies_under_attack` — an allied barracks with at least two enemy fighters within 25 m;
- `ally_attacks` — an ally fighting with three or more troops more than 45 m from its own barracks.

`AllyAidBehavior` answers the first: when its own base is quiet, the AI sends half of its spare troops (not in the wave, not in the garrison, not already fighting; at least two) to the nearest threatened allied barracks within 170 m, re-deciding every 3 s. The second is answered by the wave: an AI not yet committed launches early — with half its wave size, at least three — against the enemy nearest the ally's front, so it fights beside the ally instead of mustering alone. Both work for a human ally as well as an AI one.

### Sending and requesting resources

Allies exchange resources through the marketplace panel (`AllyTribute` command). Sending moves up to 500 of a resource at once from the sender's stock. A request is queued on the session's `MarketplaceSystem`; the addressed AI answers it on its next tick (`AISystem::answer_ally_requests`) with `answer_ally_request`:

- It keeps a reserve per resource (gold 150, food 120, wood 150, stone 80, iron 60, raised by up to half for an aggressive commander) and only gives from what is above it.
- Its generosity is `0.45 + 0.35·defence − 0.30·aggression − 0.05·harassment` plus a strategy bias (economic +0.20, defensive +0.10, expansionist +0.05, aggressive −0.10, rusher −0.15, harasser −0.05), clamped to 0.05–0.95. Hanno (economic, defence 0.75) is about twice as open-handed as Marcellus (rusher).
- It offers `surplus × generosity` (a third of that while its own barracks is under attack), rounded down to 5. At least the full request is granted; at least 40% and 10 is a partial grant; less is refused, as "cannot spare" when the surplus is short of the request and "refuses" when it simply will not.

Answers come back as notifications for the player. Only AI allies answer requests.

## Mission reinforcement waves

Mission `ai_setups[].waves` are not the same system as strategic committed waves.

Mission waves are authored reinforcements. When spawned, their units receive the mission assault ownership path and are handled by `AssaultBehavior` independently of the AI's strategic state.

That distinction lets a scenario author say, for example, “this fortress AI remains a garrison, but these three assault columns attack from authored entry points.”

See [MISSION_FRAMEWORK.md](MISSION_FRAMEWORK.md) for wave schema and the built-in archetype catalogue.

## Economy and production

Homes are the only way to refill a barracks reserve (a Home raises three civilians in its lifetime, each worth 18 reserve). Three rules keep that working past the opening:

- a town plan claims its Home slots only while the plan still has more Home steps than Homes standing, so extra Homes beyond the plan are built;
- "raise Homes first" and the builder-versus-soldier check compare the reserve with the cheapest _foot line_ recruit (`is_foot_line_recruit`: not a commander, builder, civilian, horseman, elephant or engine), not with units the doctrine never buys;
- civilians are queued at Homes only when the food to pay for them is in stock, and a starving AI sends every idle builder to ripe fields.

The AI uses the same economy and production systems as the player.

Its macro context tracks needs such as:

- builders;
- homes;
- farms;
- military production;
- defensive structures;
- siege production; and
- resource/manpower support for those goals.

Builder and production behaviors share strategic configuration so settlement growth, workforce allocation, recruitment, and fortification operate as one plan rather than unrelated fixed thresholds.

Food and manpower are part of this loop. Farms and food collection support homes, homes produce civilians, and civilians feed reserve/manpower into production buildings through the same systems documented in [FOOD_AND_FARMS.md](FOOD_AND_FARMS.md).

## Commander doctrines

Commander-specific AI configuration is loaded from `assets/data/ai/doctrines.json`.

A doctrine can tune or override:

- strategy and posture;
- personality;
- recruitment mix;
- town-plan choice;
- committed attack-wave size;
- regroup timing;
- spent-wave threshold;
- target priorities; and
- garrison size/fraction.

Optional fields fall back through the compiled/default strategy path. The doctrine data refines existing runtime concepts rather than creating a separate commander-only AI implementation.

## Town plans

`assets/data/ai/town_plans.json` defines ordered settlement layouts in a local coordinate frame.

A town plan can describe buildings, walls, gates, and defensive structures. Runtime construction rotates the plan toward the selected enemy-facing axis and then applies ordinary construction rules such as affordability, site clearance, queue limits, and strategic priority.

`TownPlan::form()` derives the plan's form from wall geometry rather than trusting a separately authored shape label.

Commander-specific Roman and Carthaginian plans therefore share the same builder behavior while producing different settlement geometry.

## Base model

`AIBaseManager` clusters owned buildings into persistent bases and assigns roles such as:

- `Main`;
- `Forward`;
- `Production`; and
- `Defensive`.

Base identity is matched across updates so a cluster can retain role and threat history while buildings are added or destroyed.

The main base anchors strategic state, but it is not the only base the AI understands. Defence can respond to whichever clustered base is actually threatened. Forward and production bases retain their own useful rally/production context.

## Expansion and outposts

Field AI can plan expansion sites and outposts.

Expansion context records:

- the selected site;
- pending construction;
- failed attempts; and
- sites temporarily abandoned after repeated placement failure.

This makes failed placement part of strategic memory. The AI does not have to retry the same unusable site forever simply because it remains geometrically attractive in the next snapshot.

## Stations and muster positions

Strategic movement uses `AIStation` as the resolved muster/deployment position.

Candidate stations can come from:

- authored settlement/town-plan positions;
- the main-base rally; and
- the reasoner's strategic anchor.

`resolve_station()` fits the candidate against the formation footprint and navigation grid. If the roster cannot fit around the candidate, the resolver probes translated alternatives. A station that still cannot fit is marked unusable, and gathering behavior does not order the army into an invalid deployment.

## Formation integration

AI strategic movement uses the same army-formation planning infrastructure as player-controlled groups.

The planner returns per-slot placement status. Commands are emitted for placeable slots; blocked slots are not turned into impossible movement orders.

That means AI formation behavior inherits the same terrain fitting, doctrine templates, movement policies, and slot semantics documented in [FORMATION_ARCHITECTURE.md](FORMATION_ARCHITECTURE.md).

## Defence model

Base defence combines strategic assignments with current threat information.

A threat is an enemy that can fight (`is_threatening_contact`): combat troops and defence towers, never workers, civilians, healers, other buildings, wildlife, or a garrison nation that holds its ground. An enemy builder wandering past a forward base used to mark the base under threat and flip the whole AI into Defending (`AiSkirmishOpeningTest.AGarrisonNationCampedNextDoorIsNotAStandingThreat`).

The AI can retain a garrison in advance, but active defence still evaluates where the threat is, which base is affected, which defenders are available, and whether ordinary strategic units need to be pulled into the response.

This is separate from mission assault-wave ownership, so a mission AI can defend its fortification while authored attackers continue their own objective.

## Observability

The AI exposes diagnostics for tests and runtime tracing. Context records strategic state, assignments, wave/base information, and other decision facts. The formation path also has its own debug logging.

For formation decisions, enable:

```sh
QT_LOGGING_RULES="soi.ai.formation.debug=true"
```

The trace reports doctrine, intent, member count, anchor, facing, resulting footprint, and blocked/adjusted placement information.

When debugging AI behavior, it is useful to identify which layer made the decision:

1. snapshot observation;
2. strategy/context state;
3. behavior ownership;
4. command arbitration;
5. command validation/application; or
6. downstream movement/combat execution.

That decomposition avoids treating every visible bad movement as a “reasoner” problem when the failure may actually be station fitting, pathing, or command rejection.

## Testing

AI behavior is exercised through simulation tests and scenario-level coverage.

Current coverage includes:

- strategy and posture parsing;
- strategic state transitions;
- command filtering and stale-subject rejection;
- perception and strategic objectives;
- reserve/garrison/harass role separation;
- local engagement;
- scripted assault-wave behavior;
- base clustering and base identity across updates;
- per-base rally/production behavior;
- expansion and outpost abandonment;
- commander doctrine loading;
- town-plan construction;
- attack-wave assembly and commitment;
- station resolution; and
- formation placement.

The primary simulation suite can be run with:

```sh
./build/bin/simulation_tests --gtest_color=no --gtest_brief=1
```

Arena scenarios add complete settlement and battle coverage where the useful observation is the behavior of the whole AI rather than one isolated service.

## Architectural invariants

The current AI design depends on several invariants:

- workers reason from snapshots, not the mutable world;
- persistent planning memory lives in `AIContext`, not hidden statics;
- worker completion time does not decide application tick;
- behaviors claim roles/subjects explicitly;
- AI orders are validated before they affect the world;
- mission assault waves remain distinct from strategic committed waves;
- AI uses production, construction, formation, movement, and combat systems shared with the rest of the game; and
- authored commander data tunes existing systems instead of bypassing them.

These invariants are more important than any individual strategy number because they determine whether AI behavior remains deterministic, testable, and compatible with the rest of the simulation.

## Source map

| Concern                                 | Source                                            |
| --------------------------------------- | ------------------------------------------------- |
| AI snapshot/context/reasoning           | `game/systems/ai_system/`                         |
| Strategy/posture/personality/difficulty | `game/systems/ai_system/ai_strategy.cpp`          |
| Strategic committed waves               | `game/systems/ai_system/ai_attack_wave.cpp`       |
| Commander doctrines                     | `assets/data/ai/doctrines.json`                   |
| Town plans                              | `assets/data/ai/town_plans.json`                  |
| Mission AI setup/waves                  | `game/map/mission_definition.h`, mission assets   |
| Formation behavior                      | formation planner/runtime under `game/formation/` |
| AI startup readiness                    | mission startup/game engine path                  |

The capabilities in this document describe current runtime paths. They are not a backlog or an estimate of what the AI might support later.
