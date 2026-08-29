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
`barracks`, `defense_tower`, `wall_segment`, `marketplace`, `catapult`); an
unknown name is skipped with a warning.

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
