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

| Step      | Complete when                                                               |
| --------- | --------------------------------------------------------------------------- |
| `gather`  | anything has been credited through `add_harvested`                          |
| `build`   | the player owns more buildings than at start, or one is under construction |
| `recruit` | fielded manpower has grown, or a barracks is producing                      |
| `army`    | fielded manpower has grown by `k_economy_coach_army_population` (150)       |

Experienced players therefore clear the sequence through ordinary play. Dismissing the strip with ✕ stores the preference under `ui/economy_coach`, and the dismissal persists across missions and application restarts.

The coach appears only in skirmish mode. Campaign missions use their own briefing and teaching goals, while spectators have no economy to manage.

## Authoritative data sources

`app/economy/economy_overview.cpp` assembles the economy view from authoritative runtime sources:

- `construction_cost_info` and `construction_build_time` for structure cost and builder time, sourced from `assets/data/construction/catalog.json`;
- `TroopProfileService` for nation-specific recruit cost, army-cap weight, and build time;
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

## Recruit price and army-cap weight

`TroopProductionStats` exposes two independent quantities.

### `cost`: recruiting-building reserve price

`cost` is spent from a recruiting building's `manpower_available`. Nation data in `assets/data/nations/*.json` can override it.

Production cards, affordability checks, and refusal messages use this value because it is the value `ProductionService` spends.

### `population_cost()`: army-cap weight

`population_cost()` is counted against `max_troops_per_player`. Nation files do not override the base `population` value.

It is not a recruit price and is never used to decide whether a recruiting building can pay for a unit.

### UI contract

`unit_profile.cpp`, `production_readouts.cpp`, and `economy_overview.cpp` expose the recruiting price as `production.cost`.

When the army-cap weight is needed, `economy_overview.cpp` exposes it explicitly as `UnitItem::army_cap_weight`. Recruit gating has no `population_cost` price key.

The contract is:

> Advertise and gate on the value the recruiting building spends. Treat army-cap weight as a separate constraint.

`UnitProfileTest.TheAdvertisedPriceIsWhatProductionCharges` and `EconomyOverviewTest.TheHelpViewQuotesThePriceTheBarracksCharges` enforce that relationship.

## Why recruit price and army-cap weight are separate

The two values serve different balance roles. Army doctrines and town plans use relatively flat cap weights such as archer 20, swordsman 15, and catapult 12, while actual recruiting prices can be much farther apart, such as 50, 95, and 260.

Using recruit price as army-cap weight would couple economic tuning directly to force-cap tuning and would change the assumptions encoded in AI town plans and map caps. The runtime therefore keeps reserve price and cap weight as separate data.

## Vocabulary

The player-facing UI uses two terms:

- **Reserve** — the pool held by a barracks, temple, or home and spent when that building recruits.
- **Manpower** — the force in the field, counted against the map's army cap.

The UI does not use “population” for either concept.

`max_population` remains the map-schema field for compatibility with shipped content and saves. Runtime code reads it as `ProductionComponent::max_units`, the ceiling of a building's reserve rather than the army-wide manpower cap.

## Related systems

- [RESOURCE_STOCKPILE.md](RESOURCE_STOCKPILE.md) covers carried loads, barracks yards, and resource crediting.
- [SETTLEMENT_LIFE.md](SETTLEMENT_LIFE.md) covers standing gather rounds.
- [FOOD_AND_FARMS.md](FOOD_AND_FARMS.md) covers farms, food jobs, civilians, and the food-to-manpower loop.
- [UI_DESIGN_SYSTEM.md](UI_DESIGN_SYSTEM.md) covers the components used by the economy surfaces.
