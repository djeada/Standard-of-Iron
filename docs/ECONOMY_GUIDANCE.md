# Economy Guidance

The economy guidance layer makes the gather-build-recruit loop visible to the player. Resource counters explain what each resource is for, the economy guide shows costs and prerequisites, and skirmish coaching gives the player a short sequence of economic goals.

These surfaces do not own economic rules. Every displayed value comes from the systems that define the simulation: construction data, troop profiles, resource registries, harvest yields, and live builder state. The UI explains those rules without duplicating them.

## Player-facing surfaces

| Surface                    | Where it lives                | Purpose                                             |
| -------------------------- | ----------------------------- | --------------------------------------------------- |
| Resource counters and tips | `ui/qml/HUDTop.qml`           | Explain resources, gathering state, and shortfalls  |
| Economy guide              | `ui/qml/EconomyHelpPanel.qml` | Show build/recruit options, costs, and requirements |
| Skirmish coaching          | `ui/qml/EconomyCoach.qml`     | Guide the opening gather-build-recruit sequence     |

All three read the `game.economy` view model from `app/viewmodels/economy_view_model.h`. Resource names and explanations come from the shared `EconomyGuide` singleton in `ui/qml/EconomyGuide.qml`, keeping terminology consistent across the HUD and help surfaces.

## Resource counters

The resource bar is generated from the engine's resource list rather than a literal QML list. A resource is **relevant** when at least one of these conditions is true:

- the player owns some of it;
- it can be gathered;
- a buildable item spends it;
- a mission objective requires it; or
- it can be traded at a marketplace the player owns.

Specialized modes can therefore omit resources they genuinely do not use without hiding a resource that matters to ordinary play.

### Objective resources remain visible at zero

A mission can require a resource before the player owns any of it. `GameEngine` collects `accumulate_resources` requirements from victory conditions and optional objectives and exposes them as `objective_resources`, ensuring that an objective resource remains visible even while its value is zero.

### Tooltip contract

Every resource tooltip answers five questions in a fixed order:

1. where the resource comes from;
2. what spends it;
3. how it is stored or credited;
4. what builders are doing with it; and
5. whether the player lacks enough for an available action.

Example:

```text
Wood: 120
Send a builder to chop a tree with Collect, or leave Auto Gather running.
Spent on: Home, Barracks, Archer
Hauled to a barracks yard before it is credited. The yard looks full at 640.
Builders gathering it: 2 · 40 being hauled to a barracks
Short 30 for a Barracks.
```

A shortfall also turns the counter amber, so the warning is visible without opening the tooltip.

## Economy guide

The economy guide opens from the ⚒ button beside the resource counters or from **How it works** in the coaching strip.

It presents:

- every relevant resource, its source, and its uses;
- every structure a builder can construct;
- every unit the selected nation can recruit;
- complete resource and reserve costs;
- build or recruit time; and
- the reason an unavailable action cannot be performed.

Availability failures are kept distinct:

- **Needs a builder** / **Needs a barracks** — a prerequisite is missing.
- **Not enough manpower at the barracks** — the recruiting building lacks reserve.
- **Population limit reached** — the army has reached the mission cap.
- **Missing 5 more Stone, 12 more Wood** — the exact resource deficit.

The UI never collapses those conditions into a generic “cannot build” state.

## Skirmish coaching

`EconomyCoach.qml` presents a four-step progression beneath the wave tracker:

> gather → build → recruit → keep an army

Completion comes from world state rather than scripted tutorial flags.

| Step      | Complete when                                                              |
| --------- | -------------------------------------------------------------------------- |
| `gather`  | anything has been credited through `add_harvested`                         |
| `build`   | the player owns more buildings than at start, or one is under construction |
| `recruit` | fielded manpower has grown, or a barracks is producing                     |
| `army`    | fielded manpower has grown by `k_economy_coach_army_population` (150)      |

Experienced players therefore clear the sequence through ordinary play. Dismissing the strip with ✕ stores the preference under `ui/economy_coach`, and the dismissal persists across missions and application restarts.

The coach appears only in skirmish mode. Campaign missions use their own briefing and teaching goals, while spectators have no economy to manage.

## Authoritative data sources

`app/economy/economy_overview.cpp` assembles the economy view from authoritative runtime sources:

- `construction_cost_info` and `construction_build_time` for structure cost and builder time, sourced from `assets/data/construction/catalog.json`;
- `TroopProfileService` for nation-specific recruit cost (the squad's men), squad size, and build time;
- `NationRegistry` for the units the selected nation can recruit;
- `PlayerResourceRegistry` for resource stores and `get_harvested_all` for coaching progress;
- `harvest_yields.h` for one-trip yields from trees, boulders, ore seams, ripe farms, and sheep; and
- the world for builders, carried resources, active jobs, and owned structures.

`harvest_yields.h` is shared by the simulation and economy guidance, so both read the same harvest constants.

## Threading and update frequency

`GameEngine::update` runs on the **render thread**, so QML-facing state is updated through snapshots rather than direct mutation.

`sync_economy_state`:

1. builds a complete economy snapshot on the render thread;
2. compares it with the previous snapshot;
3. enqueues an update only when the snapshot differs; and
4. applies the change to the view model through a queued call.

Refreshes are throttled to four per second. Building the snapshot scans player units and jobs, while the HUD does not benefit from repeating that work at frame rate.

## One unit everywhere: men

Every population number the player can read is a count of **men** (individuals). A squad is worth the soldiers it fields, a commander, a healer, a siege crew or a civilian is one man.

### `cost`: what a recruiting building spends

`TroopProductionStats::cost` is spent from a recruiting building's `manpower_available`. The rule for every nation file and for `assets/data/troops/base.json` is:

> `production.cost == formation.individuals_per_unit`

A Roman legionary of eighteen costs eighteen men of reserve; a Carthaginian one of sixteen costs sixteen. `HomeManpowerSystemTest.EveryRecruitIsPricedAndCountedInItsMen` enforces the rule for the catalog and for every nation. The undead keep `cost: 0`: they are never recruited.

`TroopCatalog` compiled defaults (`game/units/troop_catalog.cpp`) mirror `base.json`, and `TroopCatalogLoaderTest` keeps them in step.

### The field: `troop_count_for()`

`TroopCountRegistry::rebuild_from_world` sums `Game::Systems::squad_men()` for every living troop of an owner: the nation profile's `individuals_per_unit` scaled by the squad's current strength (`squad_strength / establishment`). This is the number the top bar, the AI, the economy coach and `ProductionService` all read.

`population_cost()` still exists on `TroopProductionStats` and equals the men of the base squad; the army-cap check in `ProductionService` and `ProductionSystem` uses `profile.individuals_per_unit` directly so a nation variant is charged its own squad size.

### The cap the top bar shows

`App::Core::build_manpower_summary(world, owner, map_cap)` returns:

- `fielded` — men in the field (`troop_count_for`);
- `reserve` — men held by the owner's barracks and temples (`manpower_available`), not Home families;
- `map_cap` — the map's `max_troops_per_player`, in men;
- `cap` — the map's `max_troops_per_player`, or `fielded + reserve` when the map has no cap.

`GameEngine::build_player_state_map` publishes it as `manpower`, `manpower_cap`, `manpower_reserve`, `manpower_map_cap`, `manpower_cap_source` (`"reserve"` or `"map"`) and `manpower_tooltip`. The readout therefore means "men in the field / the most this map ever allows". The denominator is a hard, per-map constant: it never grows as civilians are delivered, so hoarding stops at the same number all match. An earlier version showed `min(map_cap, fielded + reserve)`, which made the denominator climb with every refilled reserve and read as a growing cap. The reserve is still a separate limit on what can be raised right now; it is reported in the tooltip and on each recruit card (`reserve_met`, "Not enough reserve" refusals). Map caps sit at 360–900 for skirmish maps. Historical battles run to about 2,500 only because their starting armies already field that many men, and Aurelia Magna's 8,100 covers its pre-placed citizens. `build_production_help` quotes the same cap.

### Battle report

`GlobalStatsRegistry` counts `troops_recruited`, `enemies_killed` and `losses` in men (`squad_men` of the squad at the moment it spawned or died) and `barracks_owned` includes the barracks a player started with. Every non-neutral enemy owner, including an Iron Sepulcher zone owner, gets a row in the report.

### Refusals

A recruit that the reserve cannot pay for is an `OrderFailure::PopulationCap` refusal (`reserve_short_reason`), reported immediately with the numbers ("Not enough reserve: 7 / 18 men"). `InsufficientResources` is reserved for wood, stone, iron, food and gold.

### AI budget

`AIContext::population_cap` is the map cap in men and `population_used` sums `TroopConfig::get_population_cost` (base squad men). A 250-man cap fields roughly fourteen infantry squads including builders (12 men each); maps that were tuned when a swordsman weighed 8 points field about half the squads they used to. Raise `max_troops_per_player` in the map file if a map needs the old army size.

### Splitting and joining squads

`SquadService` treats a squad as a pool of **men standing** (`squad_survivors`: the nominal
roster read through the health pool, exactly as the renderer counts figures) and its health.
Both orders go through one primitive, so there is no per-case branching to fall out of step:

- `share_health(sizes, health, …)` deals a health pool over squads of given sizes. Every squad
  lands between the least health that still shows all its men and a full pool, and the total is
  the input clamped to the sum of those bands. A split or join therefore never loses a man and
  never heals a squad whole; the only top-up is under one man's worth per squad.
- `pack(men, health, …)` fills whole squads first and puts the remainder in one last squad.

**Split** halves the men standing (needs four). **Join** accepts any selection: squads are grouped
by owner, kind and nation, chained by `k_merge_radius` (a line of squads each within reach of the
next is one group), and each group is packed. Three battered squads become as few as their men fill;
two squads of ten with a twelve-man establishment become twelve and eight. A group whose men would
pack the same way (all full, or a full squad beside a battered one) is left alone. The fullest
squads are the ones kept, the rest are absorbed.

Because a squad's `squad_strength` becomes the men it actually has, joining battered squads frees
the population their dead were still holding. The AI's squad discipline still judges strength by
the roster, not by survivors; it only ever asks for two-squad joins and goes through the same code.

### Calling a gathering crew off

A gather job borrows the construction-site fields of `BuilderProductionComponent` (`has_construction_site`, `in_progress`, the reserved node). Stop, and any player move, therefore end the gather job itself through `OrderService::clear_builder_gather_job` — clearing only the standing gather flags left the claimed node behind, and `ProductionSystem` walked the builder straight back to it and re-armed the order when the harvest finished. A construction job is left untouched by the same call.

Stop is the one order a hauling crew accepts: the carried load is still walked home, but the crew stays idle afterwards. Every other order is still refused until the load is dropped off.

For the same reason the Build cards of a gathering builder stay open (`gathering` in `selected_builder_state()`), and `ProductionManager::collect_available_builders` treats a gatherer as free to be sent to a construction.

### Several crews on one building

Every crew named by one `StartConstruction` order shares one site, keyed by owner, building type,
position and rotation. `ProductionSystem` advances the site once per tick by the hands of the
crews actually working it (each crew's `squad_fraction`), so three full crews build three times as
fast and a late crew joins the progress already made. When the site is done exactly one crew raises
the building, with every crew on the site counted as its own so they cannot block it, and the rest
are released and walk out of the footprint. Before this, each crew ran its own timer and raised or
refunded its own copy of the building. Walls keep their own site entities and are not pooled.

## Vocabulary

The player-facing UI uses two terms:

- **Reserve** — the men a barracks, temple or home holds and spends when it recruits.
- **Manpower** — the men in the field, shown against the men the player can still raise.

The UI does not use “population” for either concept.

`max_population` remains the map-schema field for compatibility with shipped content and saves. Runtime code reads it as `ProductionComponent::max_units`, the ceiling of a building's reserve rather than the army-wide manpower cap.

## Related systems

- [RESOURCE_STOCKPILE.md](RESOURCE_STOCKPILE.md) covers carried loads, barracks yards, and resource crediting.
- [SETTLEMENT_LIFE.md](SETTLEMENT_LIFE.md) covers standing gather rounds.
- [FOOD_AND_FARMS.md](FOOD_AND_FARMS.md) covers farms, food jobs, civilians, and the food-to-manpower loop.
- [UI_DESIGN_SYSTEM.md](UI_DESIGN_SYSTEM.md) covers the components used by the economy surfaces.
