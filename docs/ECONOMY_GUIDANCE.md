# Explaining the Economy Loop

The economy guidance layer makes the simulation's gather-build-recruit loop visible to the player. Resource counters explain what each resource is for, the economy guide shows costs and prerequisites, and first-skirmish prompts answer the immediate question: **what should I do next?**

None of these surfaces owns economic rules. Every number shown to the player is read from the systems that already define it—the construction catalogue, troop profiles, resource registry, and builder state. The guidance is therefore an explanation of the simulation, not a second copy of it.

## The three player-facing surfaces

| Surface                    | Where it lives                | Question it answers                              |
| -------------------------- | ----------------------------- | ------------------------------------------------ |
| Resource counters and tips | `ui/qml/HUDTop.qml`           | What is this resource, and how am I doing?       |
| Economy guide              | `ui/qml/EconomyHelpPanel.qml` | What can I build or recruit, and what does it cost? |
| First-skirmish prompts     | `ui/qml/EconomyCoach.qml`     | What should I do next?                           |

All three read the same `game.economy` view model from `app/viewmodels/economy_view_model.h`. They also share the `EconomyGuide` vocabulary singleton in `ui/qml/EconomyGuide.qml`, so a resource keeps the same name and explanation wherever it appears.

## Resource counters

The top resource bar is built from the engine's resource list rather than from a literal list in QML. This prevents a resource from existing in the simulation while being accidentally omitted from the HUD.

A resource is considered **relevant** when at least one of these conditions is true:

- the player already holds some of it;
- it can be gathered;
- a buildable item spends it;
- a mission objective requires it; or
- it can be traded at a marketplace the player owns.

On ordinary maps, that usually means every resource appears. The relevance rule exists so specialized modes that genuinely do not use a resource do not need to show a permanent zero.

### Objective resources must remain visible at zero

The objective case is especially important. A mission can require the player to accumulate a resource before the player owns any of it. Without the objective rule, the relevant counter could be hidden precisely while the player is trying to satisfy it.

`GameEngine` therefore collects `accumulate_resources` requirements from victory conditions and optional objectives and passes them to the economy layer as `objective_resources`.

### What a tooltip explains

Every resource tooltip answers the same five questions, in order:

1. where the resource comes from;
2. what it is spent on;
3. how it is stored or credited;
4. what builders are currently doing with it; and
5. whether the player is short of it for a current action.

For example:

```text
Wood: 120
Send a builder to chop a tree with Collect, or leave Auto Gather running.
Spent on: Home, Barracks, Archer
Hauled to a barracks yard before it is credited. The yard looks full at 640.
Builders gathering it: 2 · 40 being hauled to a barracks
Short 30 for a Barracks.
```

A counter turns amber when a shortfall exists, allowing the resource bar itself to communicate the warning without requiring the tooltip to be opened.

## The economy guide

The economy guide opens from the ⚒ button beside the resource counters or from **How it works** in the coaching prompts.

It presents:

- every relevant resource, its source, and its uses;
- every structure a builder can construct;
- every unit the player's nation can recruit;
- complete resource and reserve costs;
- build or recruit time; and
- a specific reason when an action is unavailable.

Availability failures are deliberately separated into different categories:

- **Needs a builder** / **Needs a barracks** — a prerequisite is missing.
- **Not enough manpower at the barracks** — the recruiting building lacks sufficient reserve.
- **Population limit reached** — the army has reached the mission cap.
- **Missing 5 more Stone, 12 more Wood** — exact per-resource deficits.

Keeping these reasons distinct is important. “Cannot build” is not useful guidance if the actual problem might be a missing worker, an empty barracks reserve, a map cap, or an economic shortfall.

## First-skirmish coaching

`EconomyCoach.qml` presents a four-step progression beneath the wave tracker:

> gather → build → recruit → keep an army

Each step is inferred from world state rather than advanced by a scripted tutorial flag.

| Step      | Considered complete when                                                |
| --------- | ----------------------------------------------------------------------- |
| `gather`  | anything has been credited through `add_harvested`                      |
| `build`   | the player owns more buildings than at start, or a building is rising   |
| `recruit` | population has grown, or a barracks is producing                        |
| `army`    | population has grown by `k_economy_coach_army_population` (150)         |

Because the checks read the actual game state, an experienced player completes the sequence naturally and is not forced through prompts they do not need.

The strip can be dismissed with its ✕. That preference is stored under `ui/economy_coach`, so dismissal survives both the mission and the process.

Economy coaching is limited to skirmish mode. Campaign missions already provide their own briefing and teaching goals, while spectators have no economy to manage.

## Where the displayed numbers come from

`app/economy/economy_overview.cpp` is the single assembly point for the data shown by the economy UI. It reads from the following authoritative systems:

- `construction_cost_info` and `construction_build_time` for structure costs and builder time, sourced from `assets/data/construction/catalog.json`;
- `TroopProfileService` for nation-specific recruit cost, army-cap weight, and build time;
- `NationRegistry` for the units the selected nation can actually recruit;
- `PlayerResourceRegistry` for current stores and `get_harvested_all` for coaching progress;
- `harvest_yields.h` for one-trip yields from trees, boulders, ore seams, ripe farms, and sheep; and
- the world itself for builders, carried resources, active jobs, and owned structures.

`harvest_yields.h` exists specifically so simulation and explanation use the same constants. Harvest yields once lived as file-local values in `production_system.cpp`, which made them inaccessible to the UI without duplication. The shared header means a rebalance changes the simulation and the guidance together.

## Threading and update frequency

`GameEngine::update` runs on the **render thread**, so it cannot mutate QML-facing state directly.

`sync_economy_state` follows the same model as `sync_attack_targeting`:

1. build a complete economy snapshot on the render thread;
2. compare it with the previous snapshot;
3. enqueue an update only when the snapshot has actually changed; and
4. apply the change to the view model through a queued call.

Refreshes are throttled to four per second. Building the snapshot requires scanning the player's units and jobs, while the HUD gains nothing from repeating the work at frame rate.

## One advertised recruit price

`TroopProductionStats` contains two quantities that serve different purposes and must not be confused.

### `cost`: the barracks reserve price

`cost` is the amount spent from a recruiting building's `manpower_available`. Nation data in `assets/data/nations/*.json` can override it. A Carthaginian archer, for example, may cost 50 reserve even when the base catalogue contains another value.

This is the number the production card must advertise because it is the number the production service actually charges.

### `population_cost()`: the army-cap weight

`population_cost()` is the amount counted against `max_troops_per_player`. Nation files do not override the underlying `population` value, so this quantity remains the base army-cap weight.

It is not a recruit price and must not be used to decide whether a barracks can pay for a unit.

### The UI contract

The app layer exports `production.cost` as the recruit price. `unit_profile.cpp`, `production_readouts.cpp`, and `economy_overview.cpp` all publish the same value.

The old `population_cost` key that QML once used for recruit gating has been removed. When the cap weight is needed, `economy_overview.cpp` exposes it under the explicit name `UnitItem::army_cap_weight`.

The rule is:

> Advertise and gate on the price the recruiting building actually spends. Treat army-cap weight as a separate constraint.

`UnitProfileTest.TheAdvertisedPriceIsWhatProductionCharges` and `EconomyOverviewTest.TheHelpViewQuotesThePriceTheBarracksCharges` enforce that relationship.

## Why the two quantities remain separate

It is tempting to unify recruit price and army-cap weight, but doing so changes game balance rather than merely simplifying code.

AI doctrines and town plans were authored against relatively flat cap weights—such as archer 20, swordsman 15, and catapult 12—while actual recruit prices vary much more widely, for example 50, 95, and 260.

Experiments with nation-priced cap weights destabilized the AI in opposite directions depending on the global multiplier. Larger caps left the AI too economically comfortable to invest in farms; smaller caps prevented it from fielding an army. No single scaling factor satisfied both behaviors in `AiDuelMatchTest`.

Changing this model would therefore require its own balance pass over town-plan priorities, per-map caps, and playtest results. It should not be introduced as a naming cleanup.

## Vocabulary: reserve and manpower

The UI uses two words consistently:

- **Reserve** — the pool stored by a barracks, temple, or home and spent when that building recruits. The production panel, unit card price, and refusal messages use this term.
- **Manpower** — the force currently standing in the field and counted against the map's cap. The top bar and spectator HUD use this term.

The player-facing UI no longer uses “population” for either concept.

`max_population` remains in map JSON for compatibility with existing maps, saves, and the map editor. Runtime code reads it as `ProductionComponent::max_units`, which is the ceiling of a building's reserve rather than the army-wide manpower cap.

## Related documentation

- [RESOURCE_STOCKPILE.md](RESOURCE_STOCKPILE.md) explains carried loads, barracks yards, and why a resource is not credited where it is harvested.
- [SETTLEMENT_LIFE.md](SETTLEMENT_LIFE.md) describes the standing gather order that keeps workers cycling through a resource round.
- [FOOD_AND_FARMS.md](FOOD_AND_FARMS.md) covers farms, food jobs, civilians, and the food-to-manpower loop.
- [UI_DESIGN_SYSTEM.md](UI_DESIGN_SYSTEM.md) documents the UI components used by the three economy surfaces.

The economy guidance works when every message points back to the same rule the simulation uses. That single-source-of-truth approach keeps player education accurate as the economy evolves.
