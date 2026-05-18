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
- separate **style/personality** from **difficulty/execution tuning**

It is still intentionally lightweight: one AI brain per player, throttled updates, immutable snapshots, small force-role heuristics, and behavior modules instead of expensive per-unit thinking.

## Design goals

The system optimizes for four things:

1. **Cheap execution**: one planner per AI player, not one behavior tree per unit.
2. **Visible activity**: the AI should keep producing, gathering, defending, harassing, attacking, and expanding instead of stalling in idle loops.
3. **Authorable variation**: missions can shape AI with JSON through strategy, personality, and difficulty.
4. **Extensibility**: reserve, harass, and outpost logic are foundations for future siege groups, multi-base roles, difficulty ladders, and personality packs.

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

| File | Responsibility |
| --- | --- |
| `ai_types.h` | Snapshot, context, strategy config, commands |
| `ai_snapshot_builder.cpp` | Reads visible world state into `AISnapshot` |
| `ai_reasoner.cpp` | Updates persistent AI context and state |
| `ai_executor.cpp` | Runs behaviors and collects commands |
| `ai_worker.cpp` | Background worker wrapper |
| `ai_command_filter.cpp` | Prevents duplicate/spammy commands |
| `ai_command_applier.cpp` | Applies AI commands back to the game |
| `ai_strategy.cpp` | Strategy presets, personality shaping, difficulty tuning |
| `ai_utils.h` | Assignment cleanup and force-role helper functions |
| `behaviors/*.cpp` | Tactical and macro behavior implementations |
| `game/systems/ai_system.cpp` | Owns AI instances and update cadence |

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

| Behavior | Priority | Concurrent? | Current job |
| --- | --- | --- | --- |
| `RetreatBehavior` | Critical | No | Pull damaged armies back to safety |
| `DefendBehavior` | Critical | No | React to local threats, prefer reserve first |
| `ProductionBehavior` | High | Yes | Keep barracks producing from style-aware targets |
| `BuilderBehavior` | High | Yes | Build homes, barracks, towers, catapults, and outposts |
| `CommanderBehavior` | High | Yes | Move commanders and trigger rally ability |
| `ExpandBehavior` | High | No | Capture neutral barracks or escort the main force to an outpost site |
| `AttackBehavior` | Normal | No | Main-army pushes, target chasing, blind marches to strategic objectives |
| `HarassBehavior` | Low | Yes | Raider detachment against isolated or strategic targets |
| `GatherBehavior` | Low | No | Assemble the main army around the rally area |

Three concurrency rules matter:

1. **Production**, **builder**, and **commander** logic keep running during attacks and defenses.
2. **HarassBehavior** can run alongside the main strategic behavior.
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

The AI now has a **first outpost planner**.

### What it currently does

- keeps the original main-base barracks sticky instead of letting the anchor drift
- chooses an expansion site from **real enemy strategic objectives**
- ignores neutral barracks when deciding outpost direction
- tracks pending construction at the chosen site
- builds an outpost barracks first, then an outpost home
- sends only the main attack force to escort the outpost

### What it does not do yet

- manage several active bases with distinct production roles
- abandon and retarget failed outposts with richer policies
- shift strategic center-of-gravity from main base to forward base

So this is a real step beyond passive single-base AI, but it is not yet a full multi-base RTS economy.

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
    "harassment": 0.30
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
- outpost planning and duplicate-order suppression

For repo validation, the reliable test binary is:

```bash
./build/bin/standard_of_iron_tests --gtest_color=no --gtest_brief=1
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

1. **True multi-base planning**
   - multiple active bases
   - per-base rally points
   - per-base production roles
   - better outpost abandonment / retarget logic

2. **Richer force planner**
   - siege groups
   - flankers
   - synchronized attack waves
   - regroup / reform logic after failed pushes

3. **Data-driven profiles**
   - move strategy presets out of code into assets/data
   - let designers tune AI personalities without recompiling

4. **Stronger strategic economy awareness**
   - more explicit resource pressure
   - better builder safety / routing
   - broader structure placement logic

5. **Team and campaign coordination**
   - allied AI timing
   - shared fronts
   - mission-aware operational goals

## Recommended next expansion order

If you want the next biggest gains per engineering effort, the recommended order is:

1. **Data-driven AI profiles** so design can iterate quickly
2. **True multi-base roles** built on the current outpost foundation
3. **Richer force planner** for siege / flank / regroup behavior
4. **Coordinated allied AI** for campaign-scale scenarios

That sequence builds on the current architecture instead of fighting it.
