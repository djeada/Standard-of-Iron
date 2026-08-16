# Explaining the Economy Loop

The simulation has always had the whole loop — gather, haul, build, recruit — but a new
player could not see it. The counters showed four of the five resources, nothing said
where wood comes from or what stone is for, and a blocked build only said _not enough
wood_ without saying how much was missing. This document covers the player-facing layer
that explains the loop, and where its numbers come from.

Nothing here changes the simulation. Every figure the guidance shows is read back out of
the systems that already own it: the construction catalogue, the troop profiles, the
resource registry, and the builders themselves.

## The three surfaces

| Surface                    | Where it lives                | What it answers                                |
| -------------------------- | ----------------------------- | ---------------------------------------------- |
| Resource counters and tips | `ui/qml/HUDTop.qml`           | What is this resource, and how am I doing?     |
| The economy guide          | `ui/qml/EconomyHelpPanel.qml` | What can I build or recruit, and at what cost? |
| The first-skirmish prompts | `ui/qml/EconomyCoach.qml`     | What should I do next?                         |

All three read one view model, `game.economy`
(`app/viewmodels/economy_view_model.h`), and share one vocabulary singleton,
`EconomyGuide` (`ui/qml/EconomyGuide.qml`), so a resource is named and described the same
way wherever it appears.

### Resource counters

The counter row is **built from the engine's resource list**, not from a literal list in
QML. That is the whole point of the change: `k_all_resource_types` gained `Food` and the
HUD kept showing four counters, because `HUDTop.qml` had `["gold", "wood", "stone",
"iron"]` written into it. It now renders whatever `game.economy.resources` reports as
relevant, so a resource can never again be tracked by the simulation and missing from the
bar.

A resource is **relevant** when the player holds any of it, it can be gathered, it is
spent on something buildable, **a mission objective asks for it**, or it can be traded at
a marketplace they own. In practice every resource on a normal map is relevant; the flag
exists so a mode that genuinely does not use one does not have to show a permanent zero.

The objective clause is the one that is easy to miss. Food is spent on nothing today, so
a mission whose victory condition is _accumulate 400 food_ would otherwise hide the very
counter the player is playing against while it still reads zero. `GameEngine` collects
the `accumulate_resources` amounts from the mission's victory conditions and optional
objectives and passes them in as `objective_resources`.

Each counter's tooltip answers five questions in a fixed order — source, use, storage,
current gathering state, and deficit:

```
Wood: 120
Send a builder to chop a tree with Collect, or leave Auto Gather running.
Spent on: Home, Barracks, Archer
Hauled to a barracks yard before it is credited. The yard looks full at 640.
Builders gathering it: 2 · 40 being hauled to a barracks
Short 30 for a Barracks.
```

The counter turns amber when there is a shortfall, so the bar itself carries the warning
without being read.

### The economy guide

Opened from the ⚒ button beside the counters, or from **How it works** on the prompts.
It lists every resource with its source and what it is spent on, then every structure a
builder can raise and every unit a barracks can recruit, each with its full cost, its
build time, and — when it is not available — the reason, which is one of:

- **Needs a builder** / **Needs a barracks** — a prerequisite, not a cost.
- **Not enough manpower at the barracks** — population held by that building.
- **Population limit reached** — the mission's cap.
- **Missing 5 more Stone, 12 more Wood** — the exact shortfall, per resource.

Those four reasons are deliberately kept apart. Before this, "cannot build" collapsed a
missing builder, an empty barracks and an empty treasury into the same silence.

### The first-skirmish prompts

A four-step strip — gather → build → recruit → keep an army — anchored under the wave
tracker. Each step is judged from the world, never from a script:

| Step      | Done when                                                               |
| --------- | ----------------------------------------------------------------------- |
| `gather`  | anything has been credited through `add_harvested`                      |
| `build`   | the player owns more buildings than they started with, or one is rising |
| `recruit` | population has grown, or a barracks is producing                        |
| `army`    | population has grown by `k_economy_coach_army_population` (150)         |

Because the test is the world state, a player who already knows the game clears all four
steps by playing normally and never sees a prompt they did not need. The strip is
dismissed with its ✕, which is remembered in `ui/economy_coach` — a dismissal outlives
the mission and the process.

The prompts are shown in skirmish only: campaign missions carry their own briefing and
teaching goal, and a spectated match has no economy to run.

## Where the numbers come from

`app/core/economy_overview.cpp` is the only place that assembles them, and it reads:

- `construction_cost_info` / `construction_build_time` — what a structure costs and how
  long a builder works on it. Data-driven from `assets/data/construction/catalog.json`.
- `TroopProfileService` — per-nation recruit cost, population cost and build time, so the
  guide shows Carthaginian numbers to a Carthaginian player.
- `NationRegistry` — which units the player's nation can actually recruit, so the guide
  never advertises an elephant to Rome.
- `PlayerResourceRegistry` — current stores, and `get_harvested_all` for the gather step.
- `harvest_yields.h` — what one trip to a tree, boulder or ore seam is worth.
- The world itself — builders, what each is doing, what they are carrying, and which
  buildings the player owns.

`harvest_yields.h` exists because those three numbers used to be file-local constants in
`production_system.cpp`, invisible to anything that wanted to explain them. The
simulation and the guidance now read the same header, so a rebalance moves both.

## The threading rule

`GameEngine::update` runs on the **render thread**, so it cannot write QML-facing state
in place. `sync_economy_state` follows the same pattern as `sync_attack_targeting`: it
builds the snapshot on the render thread, compares it against the last one, and pushes
only a real change to the view model with a queued call. It also throttles to four
refreshes a second — the scan walks every unit the player owns, and the HUD has nothing
to gain from doing that at frame rate.

## Related

- [RESOURCE_STOCKPILE.md](RESOURCE_STOCKPILE.md) — the haul the counters are describing:
  yards, carried loads, and why a gathered log is not credited where it is cut.
- [SETTLEMENT_LIFE.md](SETTLEMENT_LIFE.md) — the standing gather order that keeps a
  worker running its own round.
- [UI_DESIGN_SYSTEM.md](UI_DESIGN_SYSTEM.md) — the components the three surfaces are
  built from.
