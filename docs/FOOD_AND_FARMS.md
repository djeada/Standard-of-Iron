# Food, Farms and the Settlement Economy

Food used to be a counter that nothing produced and nothing consumed. This document
covers the loop that now runs through it — the **farm** that grows grain in cycles, the
two ways a builder brings food home, the civilians that eat it — and the balance table
the whole economy is tuned against. Read it with
[RESOURCE_STOCKPILE.md](RESOURCE_STOCKPILE.md) (how a load gets credited) and
[ECONOMY_GUIDANCE.md](ECONOMY_GUIDANCE.md) (how the counters explain it).

## The loop in one line

> builders reap grain or slaughter sheep → **food** → a home recruits a **civilian** →
> the civilian walks into a barracks and adds **manpower** → the barracks recruits
> **troops**.

Wood, stone and iron pay for buildings and arm the troops; food is what turns homes into
population. Neither side of the economy is optional any more.

## The farm

| Property                  | Value                                   |
| ------------------------- | --------------------------------------- |
| Spawn / building type key | `farm`                                  |
| Cost                      | 40 wood, 10 stone; 8 s of builder work  |
| Footprint                 | 4 × 4                                   |
| Health / vision           | 600 / 10.0                              |
| Growth cycle              | `k_farm_growth_cycle_seconds` = 60 s    |
| Yield per harvest         | `k_harvest_grain_food_reward` = 60 food |
| Nations                   | Roman and Carthaginian variants         |

A farm carries a `FarmComponent` (`growth` in `0..1`, `cycle_seconds`, `harvests`).
`FarmSystem` advances `growth` every tick for every owned, living farm; a farm at
`growth >= 1` is **ripe** and holds there until it is reaped. Reaping resets it to zero
and the next cycle starts on its own — nobody has to re-sow.

`FarmComponent::growth_stage()` quantises growth into five stages, and the renderer keys
one instanced archetype per stage and building state off it:

| Stage | Growth   | Field                                               |
| ----- | -------- | --------------------------------------------------- |
| 0     | 0 – 25%  | tilled furrows with cut stubble                     |
| 1     | 25 – 50% | rows of green sprouts                               |
| 2     | 50 – 75% | knee-high green stalks with leaves                  |
| 3     | 75 – 99% | tall yellow-green stalks, heads forming             |
| 4     | ripe     | golden wheat, heavy drooping heads — send a builder |

The stage is hashed into the render-snapshot signature (`render_entity_signature` in
`game/core/world.cpp`), so a farm is only re-copied for the renderer when its stage
changes; growth itself is not presentation state and is written to saves.

Both nation renderers (`render/entity/nations/{roman,carthage}/farm_renderer.cpp`) share
the field through `render/entity/farm_renderer_common.cpp` and dress it differently: the
Roman plot has a limestone boundary, a tiled granary shed, an ox-cart, a haystack and
amphorae; the Punic plot a mudbrick boundary, a flat-roofed lime-washed storehouse with a
ladder, a stone threshing floor, a well and stacked grain sacks. Both keep a scarecrow.
Damaged farms keep their crop (soot-tinted); a destroyed farm is a scorched field with a
collapsed shed. `building_preview --only farm --growth 0.6` renders any stage offscreen.

## Two ways to bring food home

Both are builder jobs and both end the way every harvest ends: the yield is loaded onto
the worker (`ResourceCarryComponent`) and hauled to a barracks yard, where it is credited.
The yard now stacks grain sacks as the food store grows and a carrying builder shoulders a
bound sheaf.

| Job               | Product key       | Target            | Work | Yield |
| ----------------- | ----------------- | ----------------- | ---- | ----- |
| Reap a farm       | `harvest_grain`   | own **ripe** farm | 5 s  | 60    |
| Slaughter a sheep | `slaughter_sheep` | any live sheep    | 4 s  | 35    |

Unlike trees, boulders and ore seams these targets are **entities**, not world props, so
they do not go through `TerrainService::reserve_world_prop`. The job is recorded in
`BuilderProductionComponent::structure_task_entity_id` (the same slot repair and dismantle
use) and a target counts as claimed while any builder holds it there with a food product
(`Game::Systems::food_target_claimed`). `game/systems/food_targets.{h,cpp}` is the one
place that decides what is harvestable, finds the nearest unclaimed target, and computes
the work position (a farm's footprint edge; a standoff beside the sheep).

Ordering it: **Collect** now accepts a ripe farm or a sheep under the cursor as well as a
resource node, and the interaction markers light ripe farms (`harvest`) and sheep
(`slaughter`) whenever builders are selected. Right-click hints name the action.

Sheep move. While a builder is on its way the job follows the animal, and once the builder
is within reach the sheep is **held** (`WildlifeComponent::held_timer`) so it stands
through the work instead of drifting off mid-butchering. When the work completes the
sheep is killed through the same death sequence a wolf's bite would cause and the herd
respawns on the map's wildlife timer, so mutton is renewable but slow — the farm is the
reliable source.

### Standing orders and Auto Gather

Reaping or slaughtering starts a **standing round** exactly as felling a tree does. A
worker with a `harvest_grain` round comes back to the nearest ripe farm near its anchor,
and — unlike an exhausted tree stand — a round with no ripe farm is **not retired** while
a friendly farm still stands within reach: the worker waits by the field for the next
crop. **Auto Gather** treats ripe farms as ordinary nodes, and its priority cycle gained
**Food first**, which also sends the worker after sheep.

## How the work reads

Builders no longer swing a sword animation while they work. The humanoid bake gained five
dedicated looping work clips — `construct_hammer` (an overhead mallet strike with a
wind-up and a bounce), `construct_saw`, `construct_chisel`, `construct_kneel_chisel` and
`construct_reap` (a bent-over sickle sweep) — and
`apply_construction_clip` in `render/creature/pipeline/humanoid_animation_selection.cpp`
routes every constructing soldier to the clip for its `HumanoidConstructionRole`.

The role, and the tool in the builder's hand, now follow the **job**
(`Animation::HumanoidWorkJob`, carried on `CreaturePresentationComponent::construction_job`
and derived from the builder's product type in `game/core/world.cpp`): felling a tree
swings the mallet, quarrying stone or ore kneels with the chisel, reaping a farm sweeps a
sickle (a fifth tool archetype in both nations' variant tables), and butchering kneels
with the blade. Raising a building keeps the seeded mix of hammer, saw and chisel crews
that makes a work party look like a crew, with the sickle held back from the seed roll
(`ArchetypeVariantTable::seed_variant_limit`).

`humanoid_preview --bpat build/bin/assets/creatures/humanoid.bpat --clip construct_reap
--frames 8 --view side --weapon none --out strip.png` reviews any of the five.

## Food is spent on civilians

The one recruit that eats is the civilian: `civilian.production.resource_costs.food = 20`.
A home holds 3 civilians for its lifetime and each carries 50 manpower to a barracks, so a
home is 60 food for 150 manpower. Starting stores (100–200 food on the shipped maps)
cover the first four to ten civilians; after that, farms.

The AI does not recruit civilians, so food does not gate it and it builds no farms. It does, however, keep a **recruit reserve** of wood
and iron now that troops cost more of both: `builder_behavior` sends an idle builder to
harvest whenever wood drops under 80 or iron under 50, not only when a building is short.

## The balance table

Everything below is data (`assets/data/troops/base.json`,
`assets/data/construction/catalog.json`) mirrored by the compiled fallbacks in
`game/units/troop_catalog.cpp` and `game/systems/construction_cost_catalog.cpp`; the
`CompiledDefaultsMatchTheShippedTroopData` test keeps them equal.

**What a builder brings home** (one trip = walk out, work, walk back):

| Source    | Work | Yield          |
| --------- | ---- | -------------- |
| Tree      | 6 s  | 40 wood        |
| Boulder   | 6 s  | 35 stone       |
| Ore seam  | 6 s  | 30 iron        |
| Ripe farm | 5 s  | 60 food / 60 s |
| Sheep     | 4 s  | 35 food        |

**What things cost:**

| Recruit / building | Population | Resources                  | Time  |
| ------------------ | ---------- | -------------------------- | ----- |
| Civilian (home)    | 8 (home)   | 20 food                    | 5 s   |
| Builder            | 60         | 10 wood                    | 6 s   |
| Archer             | 50         | 12 wood                    | 5 s   |
| Spearman           | 75         | 12 wood, 6 iron            | 6 s   |
| Swordsman          | 90         | 12 wood, 10 iron           | 7 s   |
| Healer             | 75         | 8 wood, 4 stone            | 7 s   |
| Horse archer       | 120        | 22 wood, 10 iron           | 9 s   |
| Horse spearman     | 135        | 22 wood, 12 iron           | 9.5 s |
| Horse swordsman    | 145        | 20 wood, 14 iron           | 10 s  |
| Elephant           | 350        | 30 wood, 20 iron           | 20 s  |
| Home               | —          | 50 wood, 15 stone          | 10 s  |
| Farm               | —          | 40 wood, 10 stone          | 8 s   |
| Barracks           | —          | 100 wood, 60 stone         | 10 s  |
| Marketplace        | —          | 60 wood, 40 stone, 50 gold | 10 s  |
| Temple             | —          | 40 wood, 90 stone, 30 gold | 18 s  |
| Defence tower      | —          | 60 wood, 80 stone          | 20 s  |

The shape this is tuned for: a squad costs about a third of a wood trip and a third of an
iron trip, so a player who keeps two builders on the tree line and one on the ore seam
recruits without pause; a home's worth of civilians costs one farm cycle; and a farm pays
for itself (40 wood) with its first harvest turned into a home's population. Nation
variants inherit these resource costs and only tune population and build time.

## Where the farm is wired

- `game/units/spawn_type.h`, `building_type.h` — `Farm` (appended after `Wolf`).
- `game/units/farm.{h,cpp}`, `factory.cpp` — the entity; `FarmComponent` in
  `game/core/component.h`, serialised in `game/save/serialization.cpp`.
- `game/systems/farm_system.{h,cpp}` — growth; registered in `runtime_system_registry`.
- `game/systems/food_targets.{h,cpp}` — target rules shared by the dispatcher, the
  production system, the gather loop, interaction targeting and the app.
- `game/systems/production_system.cpp` — completing `harvest_grain` / `slaughter_sheep`,
  following and holding the sheep.
- `game/command/command_dispatcher.cpp` — `StartHarvest` with an entity target.
- `app/economy/production_manager.cpp`, `harvest_targeting` — Collect on a farm or sheep.
- `ui/qml/ProductionPanel.qml` — the Farm card and the selected-farm crop readout
  (`ProductionViewModel::selected_farm_state`).
- `tools/map_editor`, `tools/arena` (`SetFarmGrowth`, `HarvestResource grain|sheep`),
  `tools/building_preview --growth`.
