# AI Architecture

Standard of Iron runs one strategic AI instance per computer-controlled owner. The AI reads an immutable snapshot of the match, updates persistent strategic context on a worker thread, emits typed AI commands, filters competing commands, and applies the accepted commands back to the simulation.

The implementation lives under `game/systems/ai_system/` and uses the same gameplay command path as other command producers rather than directly mutating combat or movement state.

## Decision pipeline

The AI decision path is:

```text
World
  │
  ▼
AISnapshotBuilder
  │
  ▼
AISnapshot
  │
  ▼
AIReasoner
  │
  ▼
AIExecutor + behavior registry
  │
  ▼
AICommand list
  │
  ▼
AICommandFilter
  │
  ▼
AICommandApplier
  │
  ▼
World command/effect path
```

The world-owning thread builds `AISnapshot`. Worker code reasons from that snapshot rather than reading the live world. Decision jobs carry a simulation update on which their result is due; if computation is late, the simulation waits instead of applying the decision on a different tick.

## Snapshot and persistent context

`AISnapshot` captures the state used by one decision round, including friendly forces, visible contacts, strategic objectives, economy/building information, navigation-related data required by planning, and simulation time.

`AIContext` carries information that persists between decision rounds. It includes, among other state:

- strategic state and timers;
- main/base anchors and rally information;
- reserve, harass, garrison, attack-wave, and behavior assignments;
- threat memory;
- base clusters and base roles;
- expansion/outpost planning state;
- settlement/station information; and
- diagnostics used by tests and tracing.

Snapshot data describes the current observation. Context describes the AI's ongoing plan and memory.

## Strategic states

The strategic state machine uses these states:

- `Idle`;
- `Gathering`;
- `Attacking`;
- `Defending`;
- `Retreating`; and
- `Expanding`.

State transitions are driven by the current strategy configuration, posture, threat state, force readiness, expansion state, and attack-wave state.

## Behaviors

The AI is composed from behavior modules rather than one monolithic update function. The current behavior set includes:

- retreat;
- base defence;
- scripted assault waves;
- troop production;
- builder/economy construction;
- commander positioning/abilities;
- expansion;
- local engagement;
- main-force attack;
- harassment; and
- gathering/stationing.

Behavior priority and unit claiming determine which module owns a unit when several behaviors could issue movement in the same decision batch. `AICommandFilter::arbitrate_move_ownership()` gives movement ownership to the highest-priority claim.

The applier also validates ownership at application time so a stale entity ID, dead unit, recycled ID, or unit owned by another player is not acted on simply because it appeared in an older snapshot.

## Force roles

The strategic AI divides available troops into explicit roles rather than sending every combat unit through one pool.

### Main force

The main force is the pool available for gathering, committed attack waves, expansion escorts, and ordinary field attacks after units reserved for other roles have been excluded.

### Garrison and reserve

Reserve/garrison membership keeps home defence separate from the offensive force. Strategy/doctrine values control how much strength stays behind, and defence behavior can escalate its response when a base is under real threat.

### Harass detachment

Strategies that allocate harassment units keep those IDs out of the main force and use `HarassBehavior` for independent pressure. Harass sizing is strategy/personality driven and is disabled when the AI's defensive state requires the units at home.

### Scripted assault units

Mission reinforcement waves carry `AssaultWaveComponent`. `AssaultBehavior` owns those units independently from the ordinary strategic posture so a mission AI can remain a garrison while its authored reinforcement waves attack.

Assault units are excluded from ordinary reserve, gather, expansion, and retreat pools. Their behavior is to advance on eligible hostile targets and breach obstructing barriers when they have stopped making progress toward the objective.

### Commander

`CommanderBehavior` treats the commander separately from line troops. It positions the commander relative to the force, uses commander abilities, and applies health/escort constraints defined by the current doctrine logic.

## Strategy, posture, personality, and difficulty

`AIStrategyFactory` separates strategic identity from game-mode posture and execution tuning.

### Strategy

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

Each strategy creates an `AIStrategyConfig` containing macro targets and tactical thresholds such as desired builders/homes/barracks/towers/catapults, assembly size, reserve/harass size, scouting distance, local-response limits, expansion distance, and attack thresholds.

### Posture

`AIStrategyFactory::parse_posture()` recognizes:

- `garrison`, `hold`, `defend` → `Garrison`;
- `field`, `open`, `expand` → `Field`.

`Garrison` prevents ordinary proactive attack/expansion behavior while retaining defence, economy, local engagement, and scripted assault waves. `Field` permits the strategic AI to attack and expand normally.

Mission AIs default through the mission setup path to a defensive/garrison style unless their authored configuration says otherwise; skirmish opponents use field behavior.

### Personality

Mission/personality inputs expose normalized aggression, defense, and harassment values. `AIStrategyFactory::apply_personality()` modifies the base strategy config rather than replacing the selected strategy.

### Difficulty

`AIStrategyFactory::apply_difficulty()` changes execution tuning such as decision cadence, production multiplier, and scouting behavior without changing the strategy enum itself.

## Commander doctrines and town plans

Commander-specific AI data is loaded from `assets/data/ai/`.

### `doctrines.json`

Commander doctrine data can override strategy/posture and tune:

- personality;
- recruitment mix;
- town-plan choice;
- committed attack-wave size;
- regroup timing;
- spent-wave threshold;
- target priorities; and
- garrison size/fraction.

The catalogue falls back through compiled commander doctrine/default strategy data when an optional data field is absent.

### `town_plans.json`

Town plans define ordered construction slots in a settlement-local frame. The plan is rotated toward the enemy-facing axis and gives builder logic a shared blueprint for buildings, walls, gates, and defensive structures.

The current generated plans include commander-specific Roman and Carthaginian layouts. `TownPlan::form()` derives the plan form from its wall geometry rather than trusting a separately authored label.

Builder logic follows the ordered plan while still enforcing affordability, site clearance, population/economy priorities, and maximum construction limits.

## Settlement economy

The AI uses the same economy systems and production costs as the player.

Macro context tracks desired builders, homes, farms, military production, defences, and siege targets. Builder and production behaviors consume the same strategic configuration so workforce growth, housing/manpower supply, unit recruitment, and fortification are coordinated instead of being independent thresholds.

Food and manpower are part of this loop: farms and food collection support homes, homes produce civilians, and civilians deliver reserve/manpower to production buildings according to the settlement systems documented in [FOOD_AND_FARMS.md](FOOD_AND_FARMS.md).

## Base model and expansion

`AIBaseManager` clusters owned buildings into persistent bases and assigns roles such as:

- `Main`;
- `Forward`;
- `Production`; and
- `Defensive`.

Base IDs are matched across updates so a base can retain role/threat history while buildings are added or lost.

The main base anchors strategic state. Forward/production bases retain their own rally and production behavior. Defence chooses the threatened base rather than assuming every attack targets the main base.

Field AI can plan outposts. Expansion state tracks the selected site, pending construction, failed attempts, and temporarily abandoned sites so repeated placement failure does not force the same site forever.

## Formation and station planning

Gathering and strategic movement use the formation planner to place troops into walkable slots rather than issuing one shared point to every soldier.

`AIStation` is the resolved strategic muster position. Candidate stations come from the authored settlement plan, main-base rally, and reasoner anchor; `resolve_station()` chooses and fits the active station.

Station fitting considers the formation footprint and navigation grid. If the roster cannot be placed on valid ground around the candidate, the resolver probes alternate translated positions. A station that cannot fit is marked unusable and gather behavior does not dispatch the army into it.

Formation placement returns per-slot status. Blocked slots are not emitted as movement commands.

## Committed attack waves

`game/systems/ai_system/ai_attack_wave.cpp` manages strategic attack-wave membership.

A committed wave latches its members instead of rebuilding the attack pool every decision while frontline units enter combat. Garrison members are excluded first. The wave assembles at the station/rally, then commits when its readiness rule or assembly patience condition is satisfied.

Target selection can use known strategic objectives in addition to currently visible enemies, allowing an attack to cross the map toward known enemy holdings without requiring a visible contact at the moment the wave forms.

Doctrine values define committed wave size, garrison requirements, regroup timing, spent-wave threshold, and target priority where those values are provided.

## Mission reinforcement waves

Mission `ai_setups[].waves` are authored separately from the AI's strategic committed waves. They create `AssaultWaveComponent` units at mission-defined entry points and are handled by `AssaultBehavior` regardless of the owning AI's ordinary posture.

See [MISSION_FRAMEWORK.md](MISSION_FRAMEWORK.md) for wave authoring and built-in wave archetypes.

## Threading and deterministic application

AI decision work runs on worker threads, but workers do not own the world.

The main thread builds snapshots. Workers reason from immutable data and return commands. The result carries a due simulation update, and the simulation applies it on that update. The worker pool and startup path also expose readiness/wait diagnostics used by mission-startup gating.

See [MISSION_STARTUP.md](MISSION_STARTUP.md) for initial-decision preparation during loading.

## Validation

AI behavior is covered by simulation and headless tests. Coverage includes areas such as:

- strategy/posture parsing and state transitions;
- command filtering and stale-subject rejection;
- vision/perception and strategic objectives;
- reserve, garrison, and harass role separation;
- local engagement;
- assault-wave behavior;
- base clustering and migration;
- per-base rally/production behavior;
- expansion/outpost abandonment;
- commander doctrine loading;
- town-plan construction;
- attack-wave assembly and commitment; and
- station/formation placement.

The primary simulation test binary is:

```sh
./build/bin/simulation_tests --gtest_color=no --gtest_brief=1
```

Arena also includes AI duel/scenario coverage for complete AI-run settlements and battles.

## Source of truth

The AI capabilities documented here are current runtime paths, not a roadmap. Strategy defaults are in `ai_strategy.cpp`; commander overrides and town plans are in `assets/data/ai/`; behaviors and attack/base/station logic are in `game/systems/ai_system/`; mission-specific setup is defined by the mission schema.
