# AI Architecture

This document describes the **current** enemy AI in Standard of Iron: what it already does well, how it is configured, where mission JSON plugs into it, and which expansions still remain before it reaches a fuller professional RTS standard.

## Current state

The AI is no longer a passive "units occasionally wander" layer. It now has a cheap centralized planner that can:

- gather a main army before proactive attacks
- keep a **reserve** near home for local defense
- run a separate **harass detachment**
- defend based on **local threat memory** instead of global omniscience
- push toward hidden **strategic objectives** even when tactical contacts are gone
- build a base using shared macro targets
- plan and escort a first **forward outpost**
- operate **several bases at once** with distinct main / production / defensive / forward roles
- separate **style/personality** from **difficulty/execution tuning**

It is still intentionally lightweight: one AI brain per player, throttled updates, immutable snapshots, small force-role heuristics, and behavior modules instead of expensive per-unit thinking.

## Design goals

The system optimizes for four things:

1. **Cheap execution**: one planner per AI player, not one behavior tree per unit.
2. **Visible activity**: the AI should keep producing, gathering, defending, harassing, attacking, and expanding instead of stalling in idle loops.
3. **Authorable variation**: missions can shape AI with JSON through strategy, personality, and difficulty.
4. **Extensibility**: reserve, harass, and base-role logic are foundations for future siege groups, difficulty ladders, and personality packs.

## High-level update loop

The AI runs in a snapshot -> reason -> execute -> apply pipeline.

```text
world state
   |
   v
AISnapshotBuilder
   |
   v
AISnapshot (immutable, thread-safe)
   |
   v
AIReasoner
  - updates persistent context
  - refreshes force roles
  - advances state machine
   |
   v
AIExecutor
  - runs eligible behaviors by priority
  - emits AICommand list
   |
   v
AICommandFilter / AICommandApplier
   |
   v
game world
```

The expensive part is the thinking, so it is throttled and handed to a worker thread. The snapshot is immutable specifically so AI code can reason off-thread without touching live world state.

## Main files

Most AI code lives in `game/systems/ai_system/`.

| File                             | Responsibility                                           |
| -------------------------------- | -------------------------------------------------------- |
| `ai_types.h`                     | Snapshot, context, strategy config, commands             |
| `ai_snapshot_builder.cpp`        | Reads visible world state into `AISnapshot`              |
| `ai_reasoner.cpp`                | Updates persistent AI context and state                  |
| `ai_base_manager.cpp`            | Clusters buildings into bases, assigns base roles        |
| `behaviors/assault_behavior.cpp` | Drives scripted assault waves, whatever the AI's posture |
| `behaviors/chase_behavior.cpp`   | Sends a small detachment after enemies that stray close  |
| `ai_executor.cpp`                | Runs behaviors and collects commands                     |
| `ai_worker.cpp`                  | Background worker wrapper                                |
| `ai_command_filter.cpp`          | Prevents duplicate/spammy commands                       |
| `ai_command_applier.cpp`         | Applies AI commands back to the game                     |
| `ai_strategy.cpp`                | Strategy presets, personality shaping, difficulty tuning |
| `ai_utils.h`                     | Assignment cleanup and force-role helper functions       |
| `behaviors/*.cpp`                | Tactical and macro behavior implementations              |
| `game/systems/ai_system.cpp`     | Owns AI instances and update cadence                     |

## What the AI knows

The AI uses two data models:

- **`AISnapshot`**: what is true **right now**
- **`AIContext`**: what the AI **remembers**

### Snapshot

`AISnapshot` is intentionally compact:

- `friendly_units`
- `visible_enemies`
- `strategic_objectives`
- `game_time`

The important change is `strategic_objectives`: the AI keeps enemy structures and commanders as long-range objectives even when they are outside tactical vision. That prevents the classic RTS failure mode where the army stops doing anything just because no enemy is currently visible.

### Context

`AIContext` is where most of the AI’s current strength lives. In addition to basic unit counts and state, it tracks:

- a **sticky primary barracks** used as the main base anchor
- rally and base positions
- **local threat memory** (`last_local_threat_time`)
- **reserve unit IDs**
- **harass unit IDs**
- **assembled unit count**
- shared **macro targets**
- outpost planning data:
    - `has_expansion_site`
    - `expansion_site_x/z`
    - `outpost_barracks_count`
    - `outpost_home_count`
    - `expansion_construction_pending`
    - `last_expansion_order_time`
- the **base model** (see _Multi-base model_ below):
    - `bases` with stable ids, centers, rally points and roles
    - `main_base_id` and `forward_base_id`
    - `forward_plan` (site, failed attempts, abandonment count)
    - `abandoned_expansion_sites`

This is still heuristic AI, not a heavyweight planner, but the persistent context makes it feel much more intentional.

## State machine

The AI operates in these strategic states:

- `Idle`
- `Gathering`
- `Attacking`
- `Defending`
- `Retreating`
- `Expanding`

The transitions are driven by cheap battlefield signals:

- nearby or remembered local threats -> `Defending`
- low health / unfavorable posture -> `Retreating`
- enough committed force -> `Attacking`
- need to capture a neutral barracks or establish an outpost -> `Expanding`
- otherwise regroup / assemble -> `Gathering`

The important modern behavior is that **Defending is no longer sticky forever**. It decays from local threat memory instead of global enemy visibility, so the AI can leave defense mode once the base area has actually calmed down.

## Behaviors and priorities

Behaviors are modular and ordered by priority.

| Behavior             | Priority | Concurrent? | Current job                                                             |
| -------------------- | -------- | ----------- | ----------------------------------------------------------------------- |
| `RetreatBehavior`    | Critical | No          | Pull damaged armies back to safety                                      |
| `DefendBehavior`     | Critical | No          | React to local threats, prefer reserve first                            |
| `AssaultBehavior`    | High     | Yes         | Drive scripted wave units at the enemy whatever the AI's posture is     |
| `ProductionBehavior` | High     | Yes         | Keep barracks producing from style-aware targets                        |
| `BuilderBehavior`    | High     | Yes         | Build homes, barracks, towers, catapults, and outposts                  |
| `CommanderBehavior`  | High     | Yes         | Move commanders and trigger rally ability                               |
| `ExpandBehavior`     | High     | No          | Capture neutral barracks or escort the main force to an outpost site    |
| `AttackBehavior`     | Normal   | No          | Main-army pushes, target chasing, blind marches to strategic objectives |
| `ChaseBehavior`      | Normal   | Yes         | Peel a capped detachment onto enemies that wander near our troops       |
| `HarassBehavior`     | Low      | Yes         | Raider detachment against isolated or strategic targets                 |
| `GatherBehavior`     | Low      | No          | Assemble the main army around the rally area                            |

Three concurrency rules matter:

1. **Production**, **builder**, and **commander** logic keep running during attacks and defenses.
2. **HarassBehavior**, **ChaseBehavior** and **AssaultBehavior** can run alongside the main strategic behavior; each owns a bounded slice of the army, so none of them can empty the line.
3. Exclusive force behaviors still rely on unit claiming so they do not fight each other for the same troops.

## Force organization

This is the current force model.

### Main army

The main army is the attack-capable pool after excluding:

- builders
- reserve units
- harass units

`GatherBehavior` organizes this force near the rally point, and `AttackBehavior` uses it for proactive attacks and objective marches.

### Reserve force

The reserve is a stable home-defense group stored in `reserve_unit_ids`.

Current rules:

- reserve membership is sticky across updates
- reserve size is clamped so the AI does not starve its reactive attack floor
- `DefendBehavior` prefers reserve first
- `GatherBehavior` keeps reserve near base
- `AttackBehavior` never sends reserve units forward

This is the first real "do not commit everything" rule in the AI.

### Harass force

The harass force is a separate detachment stored in `harass_unit_ids`.

Current rules:

- harass size is style-driven
- harass is clamped against reserve and defense needs
- harass units are excluded from main-army readiness
- harass shuts off while retreating or when the home area is threatened

This gives the AI a second offensive layer without making the main planner much heavier.

### Commanders

Commanders are handled separately:

- they are not treated as generic line units
- they reposition behind the army centroid
- they periodically trigger the rally ability

## Macro and building logic

The AI now uses shared macro targets instead of scattered hardcoded thresholds.

`AIStrategyConfig` feeds these targets into context:

- builder count
- home count
- barracks count
- defense tower count
- catapult count
- desired assembly size
- assembly radius
- gather spacing

`BuilderBehavior` then builds toward the largest deficit while preserving important early priorities like homes and the first barracks.

`ProductionBehavior` also reads from the same config, so unit production and structure growth are at least pulling in the same strategic direction.

## Expansion logic

The AI has a **first outpost planner** feeding a **multi-base model**.

### What it currently does

- keeps the original main-base barracks sticky instead of letting the anchor drift
- chooses an expansion site from **real enemy strategic objectives**
- ignores neutral barracks when deciding outpost direction
- tracks pending construction at the chosen site
- builds an outpost barracks first, then an outpost home
- sends only the main attack force to escort the outpost
- retargets the site laterally once a site has been abandoned

## Multi-base model

`AIBaseManager` (`ai_base_manager.cpp`) runs at the end of every context update and turns the flat building list into a set of bases. Everything downstream — production, defence, fortification — reads that model instead of assuming a single base.

### Clustering and identity

Owned buildings are clustered by proximity (`k_base_cluster_radius`). Each cluster is matched against the previous frame's bases within `k_base_identity_radius`, so a base keeps its id, its role and its threat history as buildings are added or lost. Unmatched clusters get a fresh id from `next_base_id`.

### Roles

| Role         | Meaning                                                                             |
| ------------ | ----------------------------------------------------------------------------------- |
| `Main`       | The strategic centre: anchors `base_pos`, `rally` and `primary_barracks`            |
| `Forward`    | The non-main base closest to an enemy objective, past `k_forward_base_min_distance` |
| `Production` | Any other base that owns barracks                                                   |
| `Defensive`  | A base with no production, held for map control                                     |

The main role is sticky. It moves only when the incumbent loses all its barracks, or when a challenger's score (`barracks * 3 + homes + towers`) beats it by `k_migration_score_margin`. That is the strategic-centre migration: a forward base that grows into a real settlement takes over as the centre of gravity, and losing the original main base hands the role to whatever survives rather than stalling.

### Per-base production and rally

`ProductionBehavior` groups barracks by base and orders them threatened-first, then Main, Production, Forward, Defensive. Each base carries its own queue budget (`k_production_queue_per_base`) summed across its barracks, so one saturated base cannot starve another, and production continues from any surviving base when one is destroyed. Each base also owns a rally point derived from its primary barracks; the behaviour emits `SetRallyPoint` whenever a barracks' rally drifts from its base's.

### Defence and reassignment

`DefendBehavior` defends the most threatened base rather than always the main one, and tags every claim with that base's id. When a base disappears, `AIBaseManager` drops every assignment that belonged to it, releasing those defenders for re-claiming the same tick. `BuilderBehavior` builds a defence tower at any non-main base that is under threat and has none.

### Outpost abandonment

`note_expansion_order` marks an attempt in flight with a deadline. While a builder still has the site as its construction target the deadline is pushed out; if the deadline passes with no structure and no pending construction, the attempt counts as failed. After `k_max_outpost_failures` the site is pushed to `abandoned_expansion_sites` and the plan is cleared, so site selection rotates laterally to a fresh site and does not retry the same dead ground for `k_abandoned_site_memory` seconds.

## Posture by game mode

The same behaviour set is shaped into two very different opponents by mode.

### Campaign: hold the ground, punish what comes close

Campaign AI players are defensive. Mission JSON authors the posture per `ai_setups` entry (`strategy`, `personality`, `difficulty`), and a mission that names no strategy now defaults to `Defensive` rather than `Balanced`.

A defensive AI that only ever sits still is a punching bag, so `ChaseBehavior` gives it teeth without giving up the position. When an enemy unit comes within `chase_radius` of any of the AI's own troops, and no base is under attack, it peels off a _detachment_ — `min(max_chase_units, available / 3)` of the closest eligible units — and sends them after that target with chase enabled. The cap is the point: the army never abandons its post to run down a scout. Reserve, harass and assault units are never eligible, and the whole behaviour stands down the moment a base is threatened, handing those units to `DefendBehavior`.

### Assault waves: always offensive

Scripted waves are the campaign's pressure, so they must not inherit the defensive posture of the AI that owns them. Wave units are spawned with `AssaultWaveComponent`; the snapshot marks them `is_assault`, and they are excluded from every ordinary force pool — attack force, gather, reserve, harass, base defence recall, **and retreat**. `AssaultBehavior` owns them instead: it advances them on the nearest visible enemy (falling back to a strategic objective, then to the march target baked into the component) and switches to a direct attack once inside engagement range, regardless of whether the parent AI is Idle, Gathering or Defending. The component is serialized, so a save taken mid-wave restores an assault that is still an assault.

A wave is a one-way trip. `RetreatBehavior` runs at `Critical`, above the assault, so before it skipped assault units a wounded wave unit was pulled back to its owner's home base — which in a campaign mission is usually on the far side of the map. From the player's seat that reads as the wave walking away instead of attacking. Assault units are now exempt from retreat entirely: they press on at any health.

A wave also only hunts things somebody owns. Neutral property — roadside temples, ruins, unclaimed halls — is hostile to every AI and therefore shows up in the snapshot's enemy lists, so an assault used to stop and demolish whatever scenery it happened to march past. `select_assault_target` skips neutral-owned contacts entirely; capturing neutral ground is `ExpandBehavior`'s business and it never gets assault units. Neutral _barriers_ stay eligible as breach targets, because a wall in the corridor is in the way whoever owns it.

#### Breaching versus flanking

`AssaultBehavior` can break fortifications, but only ones that are genuinely in the way. It tracks, per assault unit, the best distance to the current objective and the time that best was last improved. Breaching is unlocked only when a unit has gained no ground for `k_advance_stall_seconds` (8 s); until then the wave keeps advancing and the pathfinder is free to route it around or through an opening. Once stalled, `select_breach_target` picks the barrier lying inside the corridor between the group and the objective (preferring gates) and the wave attacks it, holding a tighter engage radius so it presses the wall rather than milling around.

The stall gate is what keeps the two cases separate:

- **Camp fully enclosed** — no route exists, every unit stalls, the wave breaks the rampart down. This is the behaviour `WaveUnitsAttackTheRampartInTheirWay` pins.
- **Rampart with open ends** (the first campaign mission's shape) — the wave keeps closing on the camp and the wall is simply walked past. Without the gate it would fixate on the wall in front of its spawn, kill it, then start on the neighbouring segment, and never enter the camp at all.

Progress tracking resets whenever the objective moves more than `k_objective_drift`, so chasing a live target does not read as a stall.

### Skirmish: take the map, then come home when it burns

Skirmish AI players are configured `Expansionist` at setup time. That preset already carries the highest `expansion_priority`, two outpost barracks and a wide `expansion_site_distance`, so the AI spreads into forward bases and contests neutral and player-held ground instead of turtling.

The counterweight is `full_recall_on_base_threat`, which only the expansionist preset sets. When any of its bases is attacked — main or outpost, tracked per base by `AIBaseManager` — `DefendBehavior` drops its usual reserve-first shortlist and its defender cap and commits every available unit to the defence. Assault units are the sole exception; they keep attacking.

## Style, personality, and difficulty

The system now deliberately separates **what the AI wants** from **how efficiently it executes**.

### Strategy preset

The strategy preset is the coarse style template:

- `balanced`
- `aggressive`
- `defensive`
- `expansionist`
- `economic`
- `harasser` / `harassment`
- `rusher` / `rush`

These presets set the default shape of the AI:

- how many builders it wants
- how many barracks/towers/catapults it prefers
- how large an army it assembles before attacking
- how much reserve it keeps
- how many harassers it sends
- whether it wants an outpost and how far forward it should be

### Personality inputs

Mission JSON can then nudge a preset using three normalized floats:

- `aggression`
- `defense`
- `harassment`

These values tune things like:

- attack thresholds
- reserve size
- tower count
- harass detachment size
- scouting distance
- outpost ambition

### Difficulty tuning

Difficulty now affects execution efficiency, not strategic identity.

Supported values:

- `easy`
- `hard`
- `very_hard`
- `medium` currently falls back to `normal`
- anything else / omitted -> `normal`

Difficulty currently changes:

- AI update interval
- production speed multiplier
- scouting reach multiplier

That means a defensive AI on `hard` is still defensive; it just reacts and scales more efficiently.

## Mission JSON usage

Mission files are the current authoring surface for AI setup. The loader reads `strategy`, `personality`, `difficulty`, `team_id`, starting spawns, and optional mission waves from `ai_setups`.

### Example: balanced frontline opponent

```json
{
    "id": "roman_legion_alpha",
    "nation": "roman_republic",
    "faction": "roman",
    "color": "red",
    "team_id": 1,
    "difficulty": "hard",
    "strategy": "balanced",
    "personality": {
        "aggression": 0.62,
        "defense": 0.55,
        "harassment": 0.3
    },
    "starting_buildings": [
        {
            "type": "barracks",
            "position": { "x": 132, "z": 84 },
            "max_population": 180
        }
    ],
    "starting_units": [
        {
            "type": "spearman",
            "count": 8,
            "position": { "x": 128, "z": 86 }
        },
        {
            "type": "builder",
            "count": 2,
            "position": { "x": 134, "z": 82 }
        }
    ],
    "commander_troop": "roman_field_commander"
}
```

Resulting feel:

- maintains a moderate reserve
- assembles before larger attacks
- grows the base steadily
- pushes harder than `easy` because the execution cadence is faster

### Example: forward pressure harasser

```json
{
    "id": "numidian_raiders",
    "nation": "carthage",
    "faction": "carthaginian",
    "color": "yellow",
    "difficulty": "medium",
    "strategy": "harasser",
    "personality": {
        "aggression": 0.74,
        "defense": 0.28,
        "harassment": 0.84
    },
    "starting_units": [
        {
            "type": "horse_swordsman",
            "count": 5,
            "position": { "x": 18, "z": 80 }
        },
        {
            "type": "builder",
            "count": 1,
            "position": { "x": 16, "z": 82 }
        }
    ],
    "starting_buildings": [
        {
            "type": "barracks",
            "position": { "x": 14, "z": 80 },
            "max_population": 120
        }
    ]
}
```

Resulting feel:

- smaller main-army thresholds
- real raider detachment
- lower reserve
- more forward scouting and earlier pressure

### Notes for authors

- `strategy` is optional; omitted means `balanced`
- `personality` fields default to `0.5`
- `difficulty` can be omitted; the AI falls back to normal execution tuning
- `team_id` is optional; omitted AIs are auto-assigned separate enemy teams
- `waves` add scripted reinforcements on top of the normal AI economy/behavior layer

## Debugging and validation

The main regression coverage lives in `tests/systems/ai_system_test.cpp`.

Current AI test coverage includes:

- assignment lifecycle cleanup
- command filtering
- vision-filtered perception
- state transitions
- macro targets
- strategic objective marching
- reserve and harass role separation
- assault-wave units staying offensive under a defensive AI
- chase detachment sizing, and standing down when a base is attacked
- expansionist full recall when a base is attacked
- outpost planning and duplicate-order suppression
- base clustering, stable base identity and role assignment
- production and strategic-centre migration after a base is destroyed
- isolated bases keeping their own rally point and threat state
- per-base rally commands and per-base production queue limits
- outpost abandonment after repeated construction failures, and retargeting

For repo validation, the reliable test binary is:

```bash
./build/bin/simulation_tests --gtest_color=no --gtest_brief=1
```

## What is already strong

Relative to the original passive AI, the current system is much better at:

- staying active
- not deadlocking force ownership
- not freezing when enemies leave vision
- keeping a home guard
- creating distinct playstyles cheaply
- laying real foundations for expansion

## Biggest remaining gaps

The AI is improved, but it is not yet "finished RTS AI." The most important remaining gaps are:

1. **Richer force planner**
    - siege groups
    - flankers
    - synchronized attack waves
    - regroup / reform logic after failed pushes

2. **Data-driven profiles**
    - move strategy presets out of code into assets/data
    - let designers tune AI personalities without recompiling

3. **Stronger strategic economy awareness**
    - more explicit resource pressure
    - better builder safety / routing
    - broader structure placement logic

4. **Team and campaign coordination**
    - allied AI timing
    - shared fronts
    - mission-aware operational goals

## Recommended next expansion order

If you want the next biggest gains per engineering effort, the recommended order is:

1. **Data-driven AI profiles** so design can iterate quickly
2. **Richer force planner** for siege / flank / regroup behavior
3. **Coordinated allied AI** for campaign-scale scenarios

That sequence builds on the current architecture instead of fighting it.
