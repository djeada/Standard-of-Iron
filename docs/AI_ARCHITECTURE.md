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
- run an explicit **posture** (`Field` or `Garrison`) on top of the strategy, so campaign garrisons and skirmish opponents share one brain
- answer nearby enemies with a bounded **local engagement** layer that never mobilises the whole army

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

| File                                      | Responsibility                                           |
| ----------------------------------------- | -------------------------------------------------------- |
| `ai_types.h`                              | Snapshot, context, strategy config, commands             |
| `ai_snapshot_builder.cpp`                 | Reads visible world state into `AISnapshot`              |
| `ai_reasoner.cpp`                         | Updates persistent AI context and state                  |
| `ai_base_manager.cpp`                     | Clusters buildings into bases, assigns base roles        |
| `behaviors/assault_behavior.cpp`          | Drives scripted assault waves, whatever the AI's posture |
| `behaviors/local_engagement_behavior.cpp` | Bounded per-cluster response to enemies that stray close |
| `ai_executor.cpp`                         | Runs behaviors and collects commands                     |
| `ai_worker.cpp`                           | Background worker wrapper                                |
| `ai_command_filter.cpp`                   | Prevents duplicate/spammy commands                       |
| `ai_command_applier.cpp`                  | Applies AI commands back to the game                     |
| `ai_strategy.cpp`                         | Strategy presets, personality shaping, difficulty tuning |
| `ai_utils.h`                              | Assignment cleanup and force-role helper functions       |
| `behaviors/*.cpp`                         | Tactical and macro behavior implementations              |
| `game/systems/ai_system.cpp`              | Owns AI instances and update cadence                     |

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

| Behavior                  | Priority | Concurrent? | Current job                                                               |
| ------------------------- | -------- | ----------- | ------------------------------------------------------------------------- |
| `RetreatBehavior`         | Critical | No          | Pull damaged armies back to safety                                        |
| `DefendBehavior`          | Critical | No          | React to local threats, prefer reserve first                              |
| `AssaultBehavior`         | High     | Yes         | Drive scripted wave units at the enemy whatever the AI's posture is       |
| `ProductionBehavior`      | High     | Yes         | Keep barracks producing from style-aware targets                          |
| `BuilderBehavior`         | High     | Yes         | Build homes, barracks, towers, catapults, and outposts                    |
| `CommanderBehavior`       | High     | Yes         | Move commanders and trigger rally ability                                 |
| `ExpandBehavior`          | High     | No          | Capture neutral barracks or escort the main force to an outpost site      |
| `LocalEngagementBehavior` | High     | Yes         | Answer each nearby enemy cluster with the closest few units, in any state |
| `AttackBehavior`          | Normal   | No          | Main-army pushes, target chasing, blind marches to strategic objectives   |
| `HarassBehavior`          | Low      | Yes         | Raider detachment against isolated or strategic targets                   |
| `GatherBehavior`          | Low      | No          | Assemble the main army around the rally area                              |

Three concurrency rules matter:

1. **Production**, **builder**, and **commander** logic keep running during attacks and defenses.
2. **HarassBehavior**, **LocalEngagementBehavior** and **AssaultBehavior** can run alongside the main strategic behavior; each owns a bounded slice of the army, so none of them can empty the line.
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

The lord is also the single most valuable thing the AI owns: `NationCollapse`
turns his death into the loss of the whole nation, barracks, workers and all.
`CommanderBehavior` therefore treats his station as a safety question rather
than a flavour one.

- **Every station is behind the line.** The doctrines differ in how close behind
  he rides -- three metres for the aggressive tempers, eight for the defensive
  ones -- not in whether he stands in front of his own soldiers.
- He only leaves home ground **at the head of a committed wave that is still at
  full strength**, and only for a doctrine that leads its attacks at all. A
  defensive or economic lord stays at the rally point.
- He needs a **real escort**: with fewer than two soldiers on the field he holds
  station at home rather than following a lone scout across the map.
- He **turns for home** as soon as he drops below sixty percent health.

Without those rules an aggressive doctrine walks its lord after the first pair
of scouts, straight into the enemy town, and loses the match in four minutes to
an opponent that never attacked.

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

### The manpower chain

Recruitment manpower is the one resource a settlement cannot cut out of the
landscape, and every macro target above is ultimately in service of it:

```text
farm ripens (60s) -> worker cuts it for 60 food
                  -> home spends 20 food to raise a family (3 per home, ever)
                  -> family walks to a barracks and grants 18 manpower
                  -> barracks spends 52+ manpower on one soldier
```

The opening barracks is seeded with the map's `max_population` as its reserve and
that is the only bulk the AI ever gets for free. Three consequences shape the
macro layer:

- **Housing is sized off the army the doctrine means to field**, not the army it
  happens to have (`wave.size + garrison.minimum_units`, plus a couple). Sizing
  it off the current army leaves a town that can only ever replace its losses
  one at a time.
- **Fields are a macro target of their own.** Without them the granary empties
  around the ten-minute mark, no family is ever raised again, and the town stops
  recruiting while its wood and stone piles look perfectly healthy. Fields are
  broken when the store runs low and worked whenever the granary has room, so
  the crop is cut before the cupboard is bare rather than after.
- **A builder past a working minimum is only raised if the town could still pay
  for a soldier afterwards.** A work crew that eats the manpower pool leaves a
  settlement that can build anything and field nothing.

When the barracks cannot afford a single recruit and no home has a family left
to send it, `raise_homes_first` puts housing ahead of everything else --
including the authored town plan, which yields its ordering (never its
placement) until the town can recruit again.

### Placing what it builds

Two rules keep the settlement from stalling on a site it can never use:

- The **layout frame is fixed by the enemy town**, not by whoever is currently
  in sight. Every authored slot is expressed in that frame, so a facing that
  swung whenever a scout wandered past would rotate the whole plan and no slot
  would ever be recognised as already filled.
- A slot counts as taken by **footprint**, not by a blanket radius. The plans
  author wall runs four metres apart and homes five; anything wider than the two
  buildings' own footprints swallows most of the blueprint and the settlement
  stops halfway through it. The fallback ring placement applies the same test,
  so a site that is already occupied is skipped rather than re-ordered every
  cycle forever.

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

The same behaviour set is shaped into two very different opponents by an explicit **posture** carried on `AIStrategyConfig::posture`, independent of the strategy preset:

- `Field` — the AI may initiate attacks, scout, harass, capture neutral barracks and build outposts. Skirmish opponents run `Field`.
- `Garrison` — the AI never initiates attacks or expansion and keeps no harass detachment. It gathers, defends its bases, answers what comes close through local engagement, and lets scripted assault waves do the offence. Missions default to `Garrison`; an `ai_setups` entry can opt out with `"posture": "field"`.

`AIReasoner` enforces the posture at the source: `can_initiate_attack`, `wants_expansion`, harass sizing and outpost planning all read it, and `validate_state` bounces a garrison out of `Attacking`/`Expanding` if a loaded save left it there. Strategy, personality and difficulty keep shaping _how_ the AI does what the posture allows.

### Local engagement: punish what comes close, keep the line

A defensive AI that only ever sits still is a punching bag, and a unit-level auto-engagement radius of one vision range is not enough of an answer. `LocalEngagementBehavior` runs concurrently in every state except `Retreating`, for both postures:

1. Visible enemy units are grouped into clusters (`k_threat_cluster_radius`).
2. For each cluster, the AI's own units within `local_response_radius` of one of its threats are candidates — reserve units included, assault and harass units excluded, and never a unit that a higher-priority behaviour (`Defend`, `Retreat`, `Attack`, `Expand`) has already claimed. Units that are unassigned or only `gathering`/`positioning` are fair game.
3. The closest `max_local_responders` (plus anyone already in melee) form the response. If nobody is engaged yet and `TacticalUtils::assess_engagement` says the odds are bad, the group stays put rather than charging.
4. Responders get a focus-fire attack order with chase enabled and are claimed as `local-engagement`; when their threats move away they are released, and `GatherBehavior` walks them back. Planner moves are issued as `MoveOrderKind::PlannerMove`, which — unlike a `ScriptedMove` — clears the unit's attack target, so a recall actually recalls a chasing unit.

Units already fighting near a cluster (whatever sent them there) count against the cap, so a fight never snowballs one unit at a time into the whole line.

The cap is the point: a scout draws two or three defenders, not the army, and a base under real attack still hands everything to `DefendBehavior`, which claims at `Critical` and cannot be stolen from.

### Campaign: hold the ground

Campaign missions run `Garrison`. Mission JSON authors the flavour per `ai_setups` entry (`strategy`, `personality`, `difficulty`); a mission that names no strategy defaults to `Defensive`. With more than one base, `GatherBehavior` gathers each unit at its **nearest** base rally so every garrison holds its own ground instead of collapsing onto the main base. `DefendBehavior` defends whichever base is threatened with a response proportional to the threat (`k_defenders_per_threat` per visible enemy unit, minus defenders already engaged), and it works from the base model alone: `AIBaseManager` anchors the AI on its main building cluster even without a barracks, so an AI with homes but no barracks still gathers at and defends them. Enemy buildings inside the base radius are not threats — only units and defence towers are — so a neighbouring enemy structure does not keep a garrison permanently on alert.

Authored `guard` units are not part of the planner at all, so their bite is unit-level: a guard melee unit auto-engages inside its `guard_radius`, closes on the intruder, and the attack processor leashes it back to the guard position the moment the target leaves that radius.

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

Skirmish AI players are configured `Expansionist` with the `Field` posture at setup time. That preset already carries the highest `expansion_priority`, two outpost barracks and a wide `expansion_site_distance`, so the AI spreads into forward bases and contests neutral and player-held ground instead of turtling.

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

Mission files are the current authoring surface for AI setup. The loader reads `strategy`, `posture`, `personality`, `difficulty`, `team_id`, starting spawns, and optional mission waves from `ai_setups`.

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
    ]
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

- `strategy` is optional; omitted means `defensive` in missions
- `posture` is optional; omitted means `garrison` in missions, and `field` lets a mission AI attack and expand like a skirmish opponent
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
- local engagement responding with the closest few units, respecting higher-priority claims, refusing bad odds and releasing responders when threats leave
- garrison posture never initiating attacks or expansion while the same strategy in field posture does
- per-base gathering under garrison posture
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

## Commander doctrines, town plans, and attack waves

A commander is not only a unit on the field: it decides how the AI that owns it
plays. That mapping is data, in `assets/data/ai/`, so a designer can retune an
opponent without a compiler.

### assets/data/ai/doctrines.json

Keyed by the commander id from `game/units/commander_catalog.cpp`
(`roman_veteran_consul`, `carthage_bow_commander`, ...). Every field is
optional and falls back, in order, to the file's own `defaults` block, then to
the `CommanderDoctrine` compiled into the commander catalog, then to the AI's
built-in strategy tables. A missing or malformed file leaves the game entirely
on those built-ins -- it logs a warning under `soi.ai.doctrine` and carries on.

| Field                                         | Meaning                                                                               |
| --------------------------------------------- | ------------------------------------------------------------------------------------- |
| `strategy`, `posture`                         | Parsed by `AIStrategyFactory`; an unknown name falls back to `balanced` / `field`     |
| `personality`                                 | `aggression`, `defense`, `harassment`, each clamped to 0..1                           |
| `town_plan`                                   | Names a plan in `town_plans.json`; unknown names fall back to the built-in slot table |
| `recruitment.ranged_share`                    | The mix this commander wants, replacing the inferred melee/ranged ratio               |
| `wave.size`                                   | Fighting units, over and above the garrison, needed before a wave commits             |
| `wave.regroup_seconds`                        | Delay after a wave is spent before the next one may form                              |
| `wave.spent_fraction`                         | A wave disbands below this share of the size it started at                            |
| `wave.target_priority`                        | `army`, `barracks`, `economy`, `commander`, `any`; `any` is appended if absent        |
| `garrison.minimum_units`, `garrison.fraction` | What stays home; a garrison is never allowed to swallow the whole army                |

### assets/data/ai/town_plans.json

An authored settlement blueprint rather than an optimiser -- the same idea as a
castle plan. `steps` is an ordered list of `{ building, x, z }`; the builder
walks it and raises the first building whose slot is still empty, so a town that
cannot afford everything still comes out shaped like its blueprint instead of
stalling on step one. Offsets are metres in the settlement's own frame with
**-Z as the front**: the plan is rotated so that side faces the enemy, which is
what keeps a wall line between the homes and the threat rather than behind them.

Buildings must be named as the construction catalog names them (`home`,
`barracks`, `defense_tower`, `wall_segment`, `wall_gate`, `marketplace`,
`catapult`, `ballista`); an unknown name is skipped with a warning.

The file is generated: `scripts/generate-town-plans.py --write` lays out six
blueprints, one per commander, in the way Stronghold gives each lord a castle
of his own, and rasterises their walls onto the 2 m lattice with the gates cut
and the front wall ordered first:

| Plan id               | Commander                           | Form          | Shape                                                                             |
| --------------------- | ----------------------------------- | ------------- | --------------------------------------------------------------------------------- |
| `roman_bulwark`       | roman_legion_organizer (Fabius)     | `closed_fort` | 22x18 outer rectangle round a 12x10 keep, five towers, three gates, two ballistae |
| `roman_assault_camp`  | roman_veteran_consul (Scipio)       | `closed_fort` | bastioned star trace, curtain 15x13, four towers, two gates, two catapults        |
| `roman_vanguard_camp` | roman_field_commander (Marcellus)   | `open_camp`   | an open V of wall toward the enemy, three barracks behind it, no ring             |
| `punic_trade_town`    | carthage_spear_commander (Hanno)    | `ring_town`   | 19x17 oval, six towers on the ring, two gates, a ballista                         |
| `punic_raider_camp`   | carthage_bow_commander (Hasdrubal)  | `open_camp`   | no walls: three barracks and three towers, a camp that fights in the field        |
| `punic_grand_camp`    | carthage_sword_commander (Hannibal) | `closed_fort` | hexagon with a flat side to the enemy, six towers, two gates, barracks outside it |

The **form** is not authored: `TownPlan::form()` derives it from the wall slots
(share of the compass the trace covers, and how much its radius swings), so a
plan cannot claim one shape and build another. There is one plan per
settlement; construction, the army's stations, the diagnostics and the tests all
read the same slots. `TownPlan::front_gate()`, `front_line_z()` and
`muster_offset()` are likewise computed from the slots.

**The silhouette comes first.** A settle raises twenty-odd buildings in half an
hour, a fifth of a hundred-step plan, so the _order_ of the first fifth is what a
player sees. The generator emits each walled plan as: front towers, then the
skeleton of every wall run (every corner and a post every so often round the
trace), then the front gate - and records how many steps that took as
`silhouette_steps`. The infill of the front wall, the rear towers, the rest of
the wall and the homes follow. Twenty posts laid this way already show a dotted
rectangle, a star or a ring; the same twenty laid end to end showed one straight
wall. The builder lets a silhouette step go up as soon as the town can feed
itself (a farm and two homes) and holds the infill behind the four-roof rule;
`AIDoctrineCatalogTest.ASilhouetteAlreadyDrawsTheOutline` pins the silhouette to
at least 70% of the compass for every closed plan, and
`AiTownPlanTest.EveryCommanderRaisesItsOwnTownFromAnEmptyField` checks the
outline a real settle draws, sector for sector, against the fortification it
raised.

`Blueprint.add` nudges a building off the 9 m anchor circle, off other
buildings and off the wall lines (a gate reaches 4.5 m either side), so a plan
never asks for a slot the builder cannot fill. Plans are capped at 128 wall
links, 12 towers, 12 homes and 4 gates, the builder's own ceilings.

How the builder walks a plan (`behaviors/builder_behavior.cpp`), and why:

- **Four roofs before the castle.** `raise_homes_first` outranks the plan until
  four homes stand; before that a walled commander spent its whole opening on
  palisade and never grew the population to man it.
- **Saving for the expensive step.** When the next unbuilt plan step is a gate
  or a tower and cannot be afforded, cheaper wall links behind it in the plan
  are not bought either - otherwise 2-wood links starved the gate forever and
  the ring closed without a way out. Homes and economy keep going.
- **Planned wood.** The wood the remaining wall links will cost (capped at 700)
  is added to the wood stockpile target, and the reasoner asks for one more
  builder per 24 wall steps, so a castle plan changes what the economy gathers,
  not only what it builds.
- **A slot gives up.** Each plan slot is ordered at most four times per
  building count; a slot the world refuses (terrain, a unit standing on it) is
  skipped until something else gets built, instead of the builder cycling on it
  while the rest of the town waits.
- **Slot clearance.** A wall link's slot counts as filled by a wall within
  1.45 m (the lattice pitch is 2 m; 1.0 m let two links stack on one cell) and a
  gate's by anything within its 4.5 m half span.
- The gate exists at all since 2 Sep 2026: `production_system.cpp` spawned the
  `wall_gate` product as a plain wall segment before, so no AI town ever had a
  way out of its own ring.

`tests/headless/ai_town_plan_test.cpp` raises every commander's town from an
empty field and, in `AWalledCommanderClosesItsCircuitGivenTime`, gives Fabius,
Scipio and Hannibal fifty minutes and expects at least 15% of the plan's links,
three towers and a gate. `SOI_TOWN_MAP=1` makes that test print an ASCII map of
what stood; `SOI_BUILD_TRACE=1` traces each build order and its verdict.

### Where the army stands

Spawn is never a station. Every combat unit that nothing higher has claimed is
walked into ranks by `GatherBehavior`, which runs concurrently and last, claims
at `Low` (the detachment and reserve at `VeryLow`) and so never outranks Defend,
Attack, Expand or Harass. Its stations all come from the town plan through the
settlement frame (`ai_settlement_frame.h`), written onto `AIContext` by
`apply_settlement_stations` right after the base manager:

| Station        | Who                                           | Where                                                                                                                                           |
| -------------- | --------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| rally / muster | the attack force, in the doctrine's formation | `muster_offset(Inside)` - the clearest ground behind the front gate - or `Outside` before the gate for a `field` posture with aggression >= 0.7 |
| garrison       | `garrison_unit_ids`, `Defensive` intent       | the inside muster whenever the attack force musters outside                                                                                     |
| reserve        | `reserve_unit_ids`                            | the base anchor, within `reserve_hold_radius`                                                                                                   |
| detachment     | idle harass units                             | ten metres to the right of the outside muster                                                                                                   |

Ranks are stable because each station's units are sorted by id before
`plan_ai_formation`, and a soldier already standing on the rally dot still gets
its slot (the old tolerance ring is what heaped recruits on one point). The
muster intent is `select_ai_intent`, so a `shield_wall` doctrine musters in a
shield wall and a `wedge` doctrine in a wedge, not only when it attacks.

`AIContext::station_report` (in `SOI_AI_TRACE` as `stationed/marching/fighting/
at_spawn/adrift`) counts where every soldier stood at the last decision; the
"no fake army" clause of `EveryCommanderRaisesItsOwnTownFromAnEmptyField`
allows at most one soldier by a barracks and wants seven in ten stationed,
marching or fighting after a 26-minute peaceful settle.

#### One owner for the station

There used to be no such thing as "the station". `AIContext::rally_x/rally_z`
was written by the reasoner's anchor stage, then overwritten by
`AIBaseManager::update`, then overwritten again by `apply_settlement_stations`,
and whatever survived that sequence was the answer. There is now one struct,
`AIContext::station`, and one function that writes it:

```cpp
struct AIStation {
  StationSource source;                 // which candidate won
  float x, z, facing_deg;               // the station itself
  bool fits, relocated;                 // what the fit found
  float required_radius;
  float measured_candidate_x, measured_candidate_z;   // the inputs that
  int measured_strength;                              // decision was
  float measured_at;                                  // made from
  bool turn_pending; float turn_pending_deg, turn_pending_since; int turns;
};
```

`resolve_station` in `ai_settlement_frame.cpp` is its only writer. Everything
else _offers a candidate_, and `choose_candidate` picks in a stated order:

| Rank | Candidate                   | Offered by                                       |
| ---- | --------------------------- | ------------------------------------------------ |
| 1    | the settlement muster       | `apply_settlement_stations`, from the town plan  |
| 2    | the main base rally         | `AIBaseManager::update`, per base                |
| 3    | `AIContext::anchor_station` | the anchor stage of `AIReasoner::update_context` |

The precedence is the one the three old writers produced by overwriting each
other; the difference is that it is stated once and the losers no longer write.

`AIBase::rally_x/rally_z` is a genuinely different thing -- a production
building's spawn rally -- and stays owned by `AIBaseManager` alone. It used to
be overwritten with the muster point so recruits would walk there, which made
the main base's rally a second copy of the station with two writers. Now
`ProductionBehavior` resolves it at the point of use: the main base's buildings
rally on `context.station`, every other base on its own rally. Nothing is
copied.

Gathering still plans its own ranks rather than committing a group to
`ArmyFormationRegistry`. Routing it through the existing `DeployFormation`
command was built and measured, and reverted: it regressed
`AiTownPlanTest.AWalledCommanderClosesItsCircuitGivenTime` (fabius fell from
three towers to one) and `AiEstateEconomyTest.ItsBuildersAreNeverLeftWithNothingToDo`,
and damping the re-commit rate did not recover either. Persistent registry-owned
slots across roster changes therefore remain **open work**; what the muster has
today is order stability (`preserve_member_order` over an id-sorted roster and a
locked station facing), not slot ownership.

`ArmyFormationService::facing_from` is likewise the one implementation of
"which way does a group closing on an anchor face"; `auto_facing` (from a world)
and `ai_formation.cpp`'s `facing_towards` (from a snapshot) now differ only in
where they get the centroid. `stands_in_the_muster` and `muster_strength` in
`ai_utils.h` are the one answer to "which soldiers count", used by both the
station fit and the station report.

When no candidate exists the last station is retained rather than reset to the
world origin.

#### The station frame does not turn under the troops

A muster used to take its facing from the members' own centroid pointed at the
anchor. As the troops close on the anchor that vector shrinks toward zero and
its direction becomes noise, so the whole formation rotated every second, every
soldier was handed a new slot, and the army orbited its own rally point. Two
things fix it, and both are about making the plan a function of the roster
rather than of where the roster happens to be standing:

- `resolve_station` locks `station.facing_deg` from the settlement frame -- the
  same axis-snapped outward direction the town plan is laid out along. It is
  re-locked without ceremony when the station itself moves more than six metres,
  which `resolve_station` knows because it just placed it. Otherwise a different
  desired facing has to persist for eight seconds before it is adopted, and each
  adoption bumps `station.turns`. `GatherBehavior` passes that angle to the
  planner instead of letting it derive one.
- Gather stations plan with `preserve_member_order`, over members already
  sorted by entity id. Without it the planner sorts each line by the members'
  lateral position, so any drift re-dealt the slots. With it the _n_-th
  smallest id always takes the _n_-th slot, so a recruit joining changes the
  row arithmetic and nothing else.

#### Settled troops are not re-ordered

`GatherBehavior` still re-plans every second, but planning is not commanding.
Each claimed soldier is classified against its own slot before anything is
emitted, and `AIContext::gather_report` counts the outcomes
(`stations/members/settled/holding/ordered/unplaceable`):

| State         | Test                                                                                                         | Result          |
| ------------- | ------------------------------------------------------------------------------------------------------------ | --------------- |
| `unplaceable` | the planner could not seat this member on walkable ground                                                    | no order        |
| `holding`     | it already has a movement objective within half a metre of its slot, or a stall detour is still in flight    | no order        |
| `settled`     | it has no objective, is inside one spacing of its slot, and is unambiguously nearer that slot than any other | no order        |
| `ordered`     | anything else                                                                                                | one `MoveUnits` |

The "unambiguously nearer" clause is what keeps a heap of soldiers standing on
the rally dot from each declaring itself settled in a neighbour's rank: a slot
counts as this soldier's only if it is at least sqrt(2) times closer than the
nearest rival slot. The old duplicate-command cooldown in `AICommandFilter` is
not a substitute for this -- it suppresses a repeat of the _same_ order, and
the whole problem was that the order kept changing.

#### One movement owner per unit per batch

`update_stall_recovery` runs before the behaviors and appends its detours to the
same command list, so a nudged unit used to be re-steered by gathering in the
same tick. Every `AICommand` now carries the `BehaviorPriority` of the task that
raised it, and `AICommandFilter::arbitrate_move_ownership` -- which runs first in
`filter` -- gives each unit to the **highest-priority** claim, not merely the
first one in the list. Recovery claims at `High`; a gather station claims at its
own station priority. `GatherBehavior` also skips units with a detour still in
flight, so the arbitration is a backstop rather than the only guard.

#### Blocked slots are reported, not dispatched

`ArmyFormationService::placements_for` replaces `positions_for` and returns a
`SlotStatus` beside every position. An invalid plan no longer scatters the
members over unvalidated ground: the fallback spread is snapped onto walkable
cells and anything that cannot be is returned `Blocked`. Every AI caller --
gather, attack, defend, harass -- drops blocked members from its `MoveUnits`
command through `move_to_slots` instead of ordering a soldier into a wall.

Because a dropped member receives no order at all, `Blocked` has to mean what it
says. `SlotTerrainFitter` therefore ends its six-ring search with one wide
`find_nearest_walkable_grid` pass before giving up, and still claims the cell so
two displaced slots cannot land on top of each other. A soldier is only left
without an order when there is no free walkable cell within twelve grid cells of
its rank.

The invalid-plan fallback goes through the same machinery. It used to snap each
scattered position to its own nearest walkable cell, with no reservation and no
separation, so several blocked slots could resolve onto one point -- the bunching
this work exists to remove, arriving by a different road. `placements_for` now
builds a `scatter_layout` and hands it to the ordinary `place()`, so the fallback
gets the fitter's claim grid for free and `ArmyFormationService::spread` and the
scatter share one implementation of the grid arithmetic.

Both of those read `NavGrid` from the AI worker thread, which
`SlotTerrainFitter` already did before this change: `is_grid_walkable` calls
`update_navigation_grid()`, so the AI worker mutates navigation state off the
simulation thread. That hazard is older than this work and is not fixed here --
`placements_for` merely avoids widening it, using grid queries rather than
`snap_to_walkable_ground`, whose terrain-height lookup would have pulled
`TerrainService` onto the worker as well.

#### A station has to be big enough to stand on

Choosing a point is not choosing an assembly area. `fitted_position`, which
`resolve_station` calls on the winning candidate, asks the formation planner for
the muster's real geometry -- `muster_footprint` builds the layout the doctrine
and intent would actually produce for this roster and returns its frontage and
depth -- widens it by two metres of manoeuvre margin, and samples that
**rectangle, oriented along the station facing**, on the navigation grid at slot
spacing. A station passes when its centre is walkable and the rectangle holds at
least one walkable sample per soldier. There is no percentage threshold: the
test is "is there a place for every man".

When it fails the whole layout is _translated_, not deformed: eight compass
directions at 1.0, 1.8 and 2.6 times the required radius are tried in a fixed
order and the first that fits becomes the station, with `station.relocated` set.

If none of the 24 probes fits, `station.fits` goes false, and that flag is
**actionable, not decorative**: `GatherBehavior` refuses to dispatch any station
anchored on the resolved one, counts it in `gather_report.refused_stations`, and
leaves the troops where they are. An army is not marched into ground that cannot
hold it. `gather_report.unplaceable` separately counts soldiers the planner could
not seat inside a station that otherwise fits.

This is the expensive check in the AI tick, so it is cached against the inputs
it was computed from -- `measured_candidate_x/z`, `measured_strength`,
`measured_navigation_revision` and `measured_at`, all inside the same struct as
the answer. It re-runs when the candidate moves more than 2 m, the muster's
strength changes by more than a quarter, the navigation grid's revision changes
(a building raised or razed), or ten seconds pass. The revision reaches the AI
through `AISnapshot`, so the worker never reads live navigation state to decide
whether its own measurement is stale.

#### One answer to "what is this soldier doing"

`soldier_motion` in `ai_utils.h` is the only place that decides it, and both
consumers switch on the same three-valued answer:

| Motion     | Test                                                            |
| ---------- | --------------------------------------------------------------- |
| `Blocked`  | stood down by recovery, stalled, or its objective was abandoned |
| `UnderWay` | it has a live movement target                                   |
| `Standing` | neither                                                         |

`GatherBehavior` reads it relative to a soldier's **slot** and
`update_station_report` relative to the **station**; the reference point is the
only difference, and it used to be the reason two hand-written copies of the
same three tests had drifted apart.

#### Readiness is a standing, not a radius

`station_standing` layers the station's reference point onto `soldier_motion`,
and is the single predicate behind both the station report and the attack wave:

| Standing    | From `soldier_motion`                      |
| ----------- | ------------------------------------------ |
| `Blocked`   | `Blocked`                                  |
| `Arriving`  | `UnderWay`, and outside the station radius |
| `Reforming` | `UnderWay`, and already inside it          |
| `Ready`     | `Standing`                                 |

`AIContext::StationReport` carries all four as one partition of the soldiers
that are neither marching with a wave nor fighting, beside the older
`stationed/at_spawn/adrift` location counts, and `update_attack_wave` counts `Ready`
soldiers instead of soldiers inside a circle. A body still shuffling into its
rank no longer signs the muster roll. The thirty-second patience is unchanged
but it now records what it did: a wave that leaves with fewer than the required
share sets `departed_under_strength`, so a deliberate degraded-force departure
can be told apart from a prepared one.

#### Recovery detours are checked before they are issued

`ai_stall_recovery.cpp` used to clamp its lateral waypoint to the map bounds and
issue it, which is how a wedged soldier got sent into the next building along.
The waypoint is now resolved against the navigation grid _and against the
pathfinder's regions_: `Pathfinding::can_reach` decides it, so a detour on the
far bank of a river is rejected rather than walked at. A detour with nowhere to
go is not issued at all -- it still counts as an attempt, so three of them stand
the unit down instead of looping.

The detour also has a lifetime now. The record used to be erased on the first
snapshot where the unit no longer looked stalled, which is exactly when it starts
walking the detour, so gathering could immediately steer it back. The record is
kept for `k_stall_detour_lifetime` past the last nudge, and `is_under_recovery`
answers over that window.

#### The applier is the ownership boundary

AI commands are built from a snapshot on a worker thread and applied a tick or
more later, and `apply_move` in the dispatcher filters by nothing at all. The
applier therefore drops any subject the AI does not currently own and alive --
a dead id, a recycled id, another player's unit -- and counts them in
`ApplyReport::stale_subjects`.

### Attack waves

`game/systems/ai_system/ai_attack_wave.cpp` forms, holds and retires one
committed attack at a time, from `AIReasoner::update_context` so that a garrison
doctrine still accumulates one.

The membership is latched, and that is the entire point. The force an AI can see
at any instant excludes whichever units are currently in contact, so choosing an
attack force fresh every cycle means that the moment the front rank engages it
stops counting as part of the attack and the rest are re-planned without it --
an army that arrives one soldier at a time and dies that way. A wave decides
once, keeps its members while they fight, drops only the dead, and disbands when
too few are left to be worth the name.

Two rules are load-bearing:

- A wave is made of **soldiers**. `is_combat_role_unit` rules out buildings,
  builders and civilians; healers still follow the line to tend it. Marching a
  civilian off to war strands the eighteen manpower it was carrying to the
  barracks, and a town that does that a few times quietly stops being able to
  recruit at all.
- The **garrison is taken out first**, nearest the base, and never leaves. That
  is how a doctrine splits what it holds from what it sends.
- A wave targets what it **knows**, not only what it can see. `visible_enemies`
  is vision-gated, so on a map where the two towns sit in opposite corners
  nothing is ever in sight at the moment a wave is ready; the selector falls
  back to `strategic_objectives`, which carries every enemy building and
  commander regardless of vision, the way a player knows where the enemy castle
  is. Without that fallback a wave never forms and no AI ever attacks.
- A wave **assembles before it marches.** Once the headcount is met the wave is
  `assembling`: GatherBehavior is already forming those soldiers at the rally,
  and the wave commits when 60% of the required size stands within the assembly
  radius of it, or after thirty seconds regardless - stragglers join on the
  road (`AISystemTest.AttackWaveAssemblesAtTheRallyBeforeItCommits`). There is
  no exact-slot condition to deadlock on.
- A wave **disbands at or below** its spent threshold, rounded up and never
  below two. One survivor still walking into a town is a casualty, not an
  attack.
- An AI with a doctrine **only marches in waves**. `AttackBehavior` has an older
  path that walks whatever is ready toward the nearest strategic objective as
  soon as the state machine says `Attacking`; left enabled alongside the wave
  system it feeds soldiers to the enemy one at a time and the army never
  accumulates to wave size at all. With a doctrine present that path is limited
  to answering enemies already close to the group, and crossing the map is the
  committed wave's job.

Wave sizes are authored against what the economy can actually field. A doctrine
needs `wave.size + garrison.minimum_units` soldiers alive before a single wave
commits, and every one of those is a home that had to be built first -- so a
wave of ten reads as "this commander never attacks", not "this commander
attacks hard."

Without authored data, wave size falls back to the strategy's own
`proactive_attack_size` and the garrison to its `reserve_units`, so an AI with no
doctrine behaves as it always did -- just cohesively.

### Verifying it

`tools/arena` carries `ai_duel_*` scenarios: two AI-run towns in opposite
corners of the map, each with a commander, a barracks, homes, builders and its
own resources, fighting until one is destroyed. The scenario report records per
side what it built, how many units it produced, how many it kept home versus
pushed past the midpoint, how long it spent in an attacking state, and who won.
See `tools/arena/README.md`.

## What is already strong

Relative to the original passive AI, the current system is much better at:

- staying active
- not deadlocking force ownership
- not freezing when enemies leave vision
- keeping a home guard
- answering local threats without mobilising the army
- creating distinct playstyles cheaply
- laying real foundations for expansion

## Biggest remaining gaps

The AI is improved, but it is not yet "finished RTS AI." The most important remaining gaps are:

1. **Richer force planner**
    - siege groups
    - flankers
    - ~~synchronized attack waves~~ -- done, see "Attack waves" above
    - regroup / reform logic after failed pushes (a wave regroups; a failed push
      does not yet change what the next one targets)

2. **Data-driven profiles**
    - ~~move strategy presets out of code into assets/data~~ -- done for
      commander doctrines and town plans, see above
    - the per-strategy tables in `ai_strategy.cpp` are still compiled in

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

1. **Siege** -- a wave that reaches a walled town has no way through it, so a
   defensive doctrine is currently unbeatable by an infantry-only attacker
2. **Wave target selection that learns** -- a push that failed should change what
   the next one goes after
3. **Coordinated allied AI** for campaign-scale scenarios

That sequence builds on the current architecture instead of fighting it.
