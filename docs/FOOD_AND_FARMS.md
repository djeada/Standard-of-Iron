# Food, Farms, and the Settlement Economy

Food connects settlement growth to military production. Farms grow grain, builders harvest grain or slaughter sheep, homes spend food to recruit civilians, and civilians become manpower when they reach a barracks. The result is an economic loop in which food drives population while wood, stone, and iron continue to fund buildings and equipment.

For the mechanics that credit a delivered load, see [RESOURCE_STOCKPILE.md](RESOURCE_STOCKPILE.md). For the UI that explains the resource counters to the player, see [ECONOMY_GUIDANCE.md](ECONOMY_GUIDANCE.md).

## The settlement loop

The complete food-to-army path is:

> builders reap grain or slaughter sheep → **food** → a home recruits a **civilian** → the civilian enters a barracks and adds **manpower** → the barracks recruits **troops**

Wood, stone, and iron pay for infrastructure and military equipment. Food is what turns homes into population. Both sides of the economy are therefore necessary for sustained growth.

## Farms

| Property                  | Value                                   |
| ------------------------- | --------------------------------------- |
| Spawn / building type key | `farm`                                  |
| Cost                      | 40 wood, 10 stone; 8 s of builder work  |
| Footprint                 | 4 × 4                                   |
| Health / vision           | 600 / 10.0                              |
| Growth cycle              | `k_farm_growth_cycle_seconds` = 60 s    |
| Yield per harvest         | `k_harvest_grain_food_reward` = 60 food |
| Nations                   | Roman and Carthaginian variants         |

A farm carries a `FarmComponent` containing `growth` in the range `0..1`, `cycle_seconds`, and `harvests`.

`FarmSystem` advances `growth` every tick for each owned, living farm. Once growth reaches `1`, the field becomes **ripe** and remains ripe until a builder reaps it. Reaping resets growth to zero and the next cycle begins automatically; farms do not require a separate reseeding action.

### Growth stages

`FarmComponent::growth_stage()` divides the continuous growth value into five presentation stages:

| Stage | Growth | Field appearance                                   |
| ----- | ------ | -------------------------------------------------- |
| 0     | 0–25%  | tilled furrows with cut stubble                    |
| 1     | 25–50% | rows of green sprouts                              |
| 2     | 50–75% | knee-high green stalks with leaves                 |
| 3     | 75–99% | tall yellow-green stalks with forming heads        |
| 4     | ripe   | golden wheat with heavy heads; ready for a builder |

The growth stage is included in the render-snapshot signature through `render_entity_signature` in `game/core/world.cpp`. A farm is therefore recopied for rendering only when its visible stage changes. The underlying continuous `growth` value is simulation state and is persisted in saves.

### Nation-specific presentation

Both farm renderers share the crop field through `render/entity/farm_renderer_common.cpp` while dressing the plot differently.

The Roman farm in `render/entity/nations/roman/farm_renderer.cpp` uses a limestone boundary, tiled granary shed, ox cart, haystack, and amphorae. The Carthaginian farm uses a mudbrick boundary, lime-washed flat-roof storehouse with a ladder, a stone threshing floor, a well, and stacked grain sacks. Both variants retain a scarecrow.

Damaged farms keep their crop but tint it with soot. Destroyed farms become scorched fields with collapsed sheds.

Preview any growth state with:

```sh
building_preview --only farm --growth 0.6
```

## Two ways to collect food

Food comes from two builder jobs. Both use the normal harvest-delivery loop: the yield is loaded into the worker's `ResourceCarryComponent`, carried back to a barracks yard, and credited there. The yard visually accumulates grain sacks as stored food increases, and a builder carrying grain shoulders a bound sheaf.

| Job               | Product key       | Target            | Work | Yield |
| ----------------- | ----------------- | ----------------- | ---- | ----- |
| Reap a farm       | `harvest_grain`   | own **ripe** farm | 5 s  | 60    |
| Slaughter a sheep | `slaughter_sheep` | any live sheep    | 4 s  | 35    |

Unlike trees, boulders, and ore seams, farms and sheep are **entities**, not world props. They therefore do not pass through `TerrainService::reserve_world_prop`.

The current job target is stored in `BuilderProductionComponent::structure_task_entity_id`, the same slot used for repair and dismantling. A food target counts as claimed while any builder holds its entity ID together with a food product; `Game::Systems::food_target_claimed` implements that check.

`game/systems/food_targets.{h,cpp}` centralizes food-target behavior. It decides what is harvestable, finds the nearest unclaimed target, and computes the work position: the edge of a farm footprint or a standoff point beside a sheep.

### Issuing food jobs

The **Collect** command accepts ripe farms and sheep under the cursor in addition to ordinary resource nodes. When builders are selected, interaction markers highlight ripe farms as `harvest` targets and sheep as `slaughter` targets. Right-click hints name the action before it is issued.

### Sheep are moving targets

A sheep can move while a builder approaches. The job follows the animal until the builder is within working range. At that point the sheep is held through `WildlifeComponent::held_timer`, keeping it in place during the slaughter action.

When the job completes, the sheep uses the same death sequence as a sheep killed by a wolf. The herd later respawns according to the map's wildlife timer. Mutton is therefore renewable but slow; farms remain the reliable source of food.

## Standing orders and Auto Gather

Reaping or slaughtering creates a **standing round**, just like felling a tree.

A worker assigned a `harvest_grain` round returns to the nearest ripe farm around the round's anchor. Unlike a depleted tree stand, the round is not retired merely because no farm is currently ripe. If a friendly farm remains within reach, the worker waits near it for the next crop.

**Auto Gather** treats ripe farms as normal resource targets. Its priority cycle includes **Food first**, which can also direct the worker toward sheep.

## Making builder work readable

Builders use dedicated work animations rather than swinging a combat animation while performing economic tasks.

The humanoid bake contains five looping construction clips:

- `construct_hammer` — overhead mallet strike with wind-up and bounce;
- `construct_saw`;
- `construct_chisel`;
- `construct_kneel_chisel`; and
- `construct_reap` — a bent-over sickle sweep.

`apply_construction_clip` in `render/creature/pipeline/humanoid_animation_selection.cpp` routes a working humanoid to the clip associated with its `HumanoidConstructionRole`.

The role and visible tool follow the **job** through `Animation::HumanoidWorkJob`, carried by `CreaturePresentationComponent::construction_job` and derived from the builder's product type in `game/core/world.cpp`.

This gives each task a distinct read:

- felling a tree uses the mallet action;
- quarrying stone or ore uses a kneeling chisel;
- reaping uses the sickle; and
- slaughtering uses a kneeling blade action.

Building construction retains the seeded mixture of hammer, saw, and chisel workers that makes a crew look varied. The sickle is excluded from that seed roll through `ArchetypeVariantTable::seed_variant_limit`.

Inspect any work clip with `humanoid_preview`, for example:

```sh
humanoid_preview \
  --bpat build/bin/assets/creatures/humanoid.bpat \
  --clip construct_reap \
  --frames 8 \
  --view side \
  --weapon none \
  --out strip.png
```

## Food is spent on civilians

The civilian is the only recruit that consumes food:

```text
civilian.production.resource_costs.food = 20
```

A home supports three civilians during its lifetime, and each civilian walked into a barracks adds a household of 18 men to its reserve (`k_civilian_delivery_reserve_grant`). One home therefore converts 60 food into 54 men of reserve: three legionaries.

Shipped maps begin with 100–200 food, enough for roughly four to ten civilians depending on the scenario. Beyond that initial reserve, sustained civilian production requires farms or sheep.

The AI does not recruit civilians, so food does not gate AI expansion and the AI does not build farms. It does, however, maintain a **recruit reserve** for wood and iron now that troops consume more of both. `builder_behavior` sends an idle builder to gather whenever wood falls below 80 or iron below 50, rather than waiting until a building itself is blocked by a shortage.

## Economy balance

The following values come from `assets/data/troops/base.json` and `assets/data/construction/catalog.json`, with compiled fallbacks in `game/units/troop_catalog.cpp` and `game/systems/construction_cost_catalog.cpp`. `CompiledDefaultsMatchTheShippedTroopData` verifies that the compiled defaults match the shipped data.

### What one builder trip returns

One trip means walking to the target, working, and returning with the load.

| Source    | Work | Yield          |
| --------- | ---- | -------------- |
| Tree      | 6 s  | 40 wood        |
| Boulder   | 6 s  | 35 stone       |
| Ore seam  | 6 s  | 30 iron        |
| Ripe farm | 5 s  | 60 food / 60 s |
| Sheep     | 4 s  | 35 food        |

### What units and buildings cost

| Recruit / building | Reserve (men)     | Resources                  | Time  |
| ------------------ | ----------------- | -------------------------- | ----- |
| Civilian (home)    | 1 (home families) | 20 food                    | 5 s   |
| Builder            | 12                | 20 wood                    | 6 s   |
| Archer             | 20 (Rome 24)      | 30 wood, 10 iron           | 5 s   |
| Spearman           | 24 (Rome 26)      | 30 wood, 15 iron           | 6 s   |
| Swordsman          | 15 (Rome 18)      | 10 wood, 30 iron           | 7 s   |
| Healer             | 1                 | 10 wood, 10 stone, 15 food | 7 s   |
| Horse archer       | 10                | 25 wood, 15 iron, 30 food  | 9 s   |
| Horse spearman     | 9                 | 25 wood, 22 iron, 30 food  | 9.5 s |
| Horse swordsman    | 9                 | 15 wood, 30 iron, 30 food  | 10 s  |
| Elephant           | 1                 | 40 wood, 25 iron, 90 food  | 20 s  |
| Catapult           | 1                 | 90 wood, 20 stone, 35 iron | 15 s  |
| Home               | —                 | 50 wood, 15 stone          | 10 s  |
| Farm               | —                 | 40 wood, 10 stone          | 8 s   |
| Barracks           | —                 | 100 wood, 60 stone         | 10 s  |
| Marketplace        | —                 | 60 wood, 40 stone, 50 gold | 10 s  |
| Temple             | —                 | 40 wood, 90 stone, 30 gold | 18 s  |
| Defence tower      | —                 | 60 wood, 80 stone          | 20 s  |

The reserve price of a recruit is the number of men in the squad it fields (`formation.individuals_per_unit`), so a nation whose legionaries march eighteen strong pays eighteen men of reserve for one. Siege engines and commanders are one man each; their cost is in resources and time.

The economy is tuned around a simple rhythm. A squad consumes roughly one third of a wood trip and one third of an iron trip, so two builders on timber and one on ore can sustain troop recruitment. A full home's civilian output costs one farm cycle, and the first harvest effectively replaces the farm's 40-wood construction investment in the broader settlement economy.

Nation variants inherit these resource costs and vary only the squad size (and therefore the men charged) and the production time.

## Implementation map

The farm and food loop is distributed across the following components:

- `game/units/spawn_type.h`, `building_type.h` — `Farm`, appended after `Wolf`;
- `game/units/farm.{h,cpp}`, `factory.cpp` — farm entity creation;
- `FarmComponent` in `game/core/component.h`, serialized by `game/save/serialization.cpp`;
- `game/systems/farm_system.{h,cpp}` — crop growth, registered through `runtime_system_registry`;
- `game/systems/food_targets.{h,cpp}` — shared targeting rules used by dispatch, production, standing gather, interactions, and the application layer;
- `game/systems/production_system.cpp` — completion of `harvest_grain` and `slaughter_sheep`, including sheep following and holding;
- `game/command/command_dispatcher.cpp` — `StartHarvest` with an entity target;
- `app/economy/production_manager.cpp` and `harvest_targeting` — **Collect** on a farm or sheep;
- `ui/qml/ProductionPanel.qml` — the Farm card and selected-farm crop state through `ProductionViewModel::selected_farm_state`; and
- `tools/map_editor`, `tools/arena`, and `tools/building_preview` — authoring and inspection support, including `SetFarmGrowth`, `HarvestResource grain|sheep`, and `--growth`.

The important design boundary is that food is not an isolated counter. It is a visible, harvestable, transportable resource that connects land use, civilian growth, manpower, and troop production into one settlement economy.
