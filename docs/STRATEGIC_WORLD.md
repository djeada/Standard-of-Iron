# Strategic World

The strategic world is the campaign above the battlefield maps: who holds which
region and town, which roads join them, where the armies stand, whether they can
be fed, and how each campaign event changes all of that. It answers gameplay
questions — _who controls Apulia, which depot feeds Hannibal, which route is
threatened, where can a battle be fought_ — and nothing else. It is not a lore
database.

```
world state → strategic situation → encounter → battlefield → result event → world state
```

| Piece                      | Where                                                                                                                                                                                       |
| -------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Model, queries, validation | `game/strategic/` (`soi_strategic`, Qt Core only)                                                                                                                                           |
| Shipped world              | `assets/world/second_punic_war.json`                                                                                                                                                        |
| Editor view                | the map editor's **Campaign** workspace (`Ctrl+1`), or `map_editor --world`                                                                                                                 |
| Command line               | `build/bin/strategic_world`                                                                                                                                                                 |
| Content gate               | `content_validator assets` validates every `assets/world/*.json`                                                                                                                            |
| In game                    | the campaign screen (`ui/qml/MediterraneanMapPanel.qml`) places each mission marker at its site in the world file; the hand-placed positions were 55–120 km off and are now only a fallback |
| Tests                      | `tests/strategic/` (campaign_tests), `tests/tools/map_editor_world_document_test.cpp`                                                                                                       |

`soi_strategic` uses nothing else in `game/` (see `scripts/module_rules.json`),
so the editor, the validator and a future campaign layer can read a world
without a running match.

## The file

One versioned file per world (`schema_version: 1`). It is written in a canonical
layout — `strategic_world format` or saving from the editor rewrites it, and a
test fails if the shipped file drifts from that layout, so diffs stay small.

| Key                      | Authored or dynamic | Contents                                                                                                                                                |
| ------------------------ | ------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `projection`, `geometry` | authored            | lon/lat bounds of the campaign map; `provinces.json` for region shapes                                                                                  |
| `factions`               | authored            | id, colour, capital, allies. `neutral` is implicit                                                                                                      |
| `regions`                | authored            | the campaign map provinces: terrain, importance, declared adjacency, **districts**                                                                      |
| `sites`                  | authored            | settlements, forts, camps, passes, crossings and fields with lon/lat, population, fortification, port class, resources 0–5, battlefield link            |
| `routes`                 | authored            | major road, road, track, mountain pass, river crossing, coastal road, sea lane; throughput 1–5; chokepoint flag; optional waypoints and length override |
| `commanders`             | authored            | world identity of a commander, optionally linked to the battlefield commander catalogue (`catalog_id`)                                                  |
| `initial_state`          | dynamic             | controllers, contested regions, route status, garrisons, armies, commander status, flags                                                                |
| `events`                 | authored            | the chronology: dated, optionally tied to a mission and a site, each a list of effects                                                                  |

Authored data never changes during a campaign. Everything that does —
controllers, army positions and strength, route status, ports, flags — lives in
a `WorldState`, which is the initial state with events applied in order. A
campaign save is a `WorldState` (`format: soi.strategic_state`, `version: 1`)
carrying the world id and a fingerprint of the definition; `load_state` refuses
saves from another world or a newer version. `replay(def, applied_events)`
rebuilds the same state bit for bit, which is what the tests use to prove
determinism.

### Districts

The campaign map has fifteen provinces, but the campaign names finer places:
`alps`, `apulia`, `campania`. Those are **districts** — named lon/lat outlines
inside a region (the outlines come from `tools/campaign_film/regions.json`). A
campaign `world_region_id` may name a region or a district; validation resolves
it and checks the mission's site really lies there.

Note the shipped province shapes put Capua, Neapolis, Beneventum, Luceria and
Arpi inside `roman_core`, so the Campania and Samnium districts belong to
`roman_core` and Apulia to `southern_italy`. `valid_provinces.json` still maps
`campania_campaign` to `southern_italy`; the strategic world follows the map.

## Effects

| op                  | target         | fields                                                                  |
| ------------------- | -------------- | ----------------------------------------------------------------------- |
| `set_controller`    | site or region | `faction`                                                               |
| `set_contested`     | region         | `enabled` (default true)                                                |
| `set_route_status`  | route          | `status`: open, contested, blocked                                      |
| `set_port`          | port site      | `enabled`                                                               |
| `set_fortification` | site           | `level` 0–3                                                             |
| `set_garrison`      | site           | `infantry`                                                              |
| `move_army`         | army           | `to`                                                                    |
| `set_army_strength` | army           | any of `infantry`, `cavalry`, `elephants`                               |
| `scale_army`        | army           | `factor`                                                                |
| `create_army`       | new army id    | `name`, `faction`, `commander`, `to`, strength                          |
| `disband_army`      | army           | —                                                                       |
| `set_commander`     | commander      | `status` (active, wounded, recalled, captured, dead), `faction`, `army` |
| `set_flag`          | flag key       | `value` (default `"true"`)                                              |

An event is applied at most once; a failing effect is reported with its index
and the rest of the event still applies, so one typo cannot hide the others.

## Movement

March speed per route kind, km/day, and the detour factor applied to the great
circle when a route has no waypoints:

| kind           | km/day | detour |
| -------------- | ------ | ------ |
| major road     | 26     | 1.15   |
| road           | 20     | 1.25   |
| track          | 15     | 1.30   |
| mountain pass  | 12     | 1.35   |
| river crossing | 12     | 1.25   |
| coastal road   | 18     | 1.30   |
| sea lane       | 90     | 1.10   |

Calibrated against the sources: the Island → Alpine Pass → Taurasia crossing
comes out at about 25 days, New Carthage → Taurasia at about 84 days of marching.
A contested route costs 1.5×; a blocked one is impassable.

`TraversalRules` decide what a query may use:

- `faction` + `avoid_hostile` — never _pass through_ a site held by a hostile
  faction (the destination itself is allowed: that is an attack).
- **Embarking** — an army may only sail from a port not held by a hostile
  faction. Landing at a hostile port is an assault and is allowed. This is what
  makes the Alps the only Carthaginian road into Italy in 218: the sea lanes
  start in Roman harbours.
- Sea lanes need both ports open (`set_port`).

Queries (`StrategicGraph`): shortest path, nearest of a set, reachable within N
days, edge-disjoint route count between two sites, connected components,
chokepoint sites (articulation points whose loss strands at least two sites) and
bridge routes.

## Supply

A supply source is a settlement or camp held by the army's faction or an ally,
with food ≥ 2, not besieged. The army's line is the cheapest _safe_ march to the
nearest source:

| line                                               | level     |
| -------------------------------------------------- | --------- |
| at a depot, or ≤ 4 days                            | supplied  |
| ≤ 10 days                                          | stretched |
| no line, region can feed it (a site with food ≥ 3) | foraging  |
| otherwise                                          | cut off   |

The narrowest route on the line carries `throughput × 9000` rations; an army
whose demand (foot + 2 × horse + 12 × elephants) exceeds that drops a level.
`isolated_sites` lists settlements that cannot reach their faction's capital
through friendly or neutral ground; `threatened_routes` lists routes a hostile
army can reach within N days.

## Encounters and battlefield generation

`encounter_at` / `suggest_encounters` turn the situation into a battle: who
attacks, who defends, where. Between two hostile armies the battle is offered at
the first pass or crossing on the attacker's road, not at the defender's walls.
`encounter_context` derives the kind (siege, pass, river crossing, assault on a
fortified camp, ambush, coastal or field battle), the generator preset, the
biome (from terrain, region tags, latitude and season) and the approach roads;
`generation_request` emits a `scripts/mapgen` request (version 1, deterministic
seed). In the editor, **Generate Battlefield** opens a new map with the
Generator tab prefilled from that request.

## Validation

`validate_world` runs on every edit in the editor, in `content_validator`, and
in `campaign_tests`. Each check has a test that breaks a small world to prove it
fires (`tests/strategic/strategic_validation_test.cpp`).

- **Identity** — missing or duplicate ids across regions, districts, sites,
  routes, armies, commanders and events; lower_snake_case ids; enumeration typos.
- **Factions** — declared allies, capitals that exist and are held at the start,
  `#rrggbb` colours.
- **Places** — region and district exist; site inside the map, inside its
  region's province on the campaign map (ports get a coastal tolerance), inside
  its district outline; values in range; settlements that produce nothing; two
  sites closer than 3 km.
- **Roads** — endpoints exist; no self loops or duplicates; sea lanes join two
  ports; authored length not shorter than the great circle; **overland routes
  stay on land** (sampled every 12 km against the province shapes); overland
  routes only between declared-adjacent regions; declared adjacency is mutual,
  matches the map, and **every pair of bordering regions has a route**.
- **Network completeness** — every site has a route and every site is reachable
  from every other; bridges that strand three or more sites but are not marked
  as chokepoints are reported as notes.
- **Battlefield links** — map files exist, missions exist, the site's map is the
  map its mission loads, generator presets exist, one site per mission.
- **Armies and commanders** — locations, objectives and factions exist; no
  commander leads two armies; no army is led by a dead or captured commander;
  commander catalogue ids exist.
- **Chronology** — events in date order, sites and missions exist, every effect
  applies, the state after every event is itself valid, and **no army moves
  between two sites that no route joins**.
- **Campaign** — every campaign mission has a site, the site lies in the
  mission's `world_region_id`, the player's army can reach it, and the mission
  has a world event so its result changes something.

It also cross-checks the shipped campaign: `valid_provinces.json`'s mission
mapping must agree with the site regions (it had Campania in `southern_italy`;
Capua lies in `roman_core` on the map), and every mission's historical cameo
commanders must exist in the world, fight on the side the mission gives them
and be alive at that event, and the mission's stated year must match its
event. All eleven cameos across the eight campaign missions pass.

Running it on the first draft of the shipped world caught two roads cut straight
across the sea (the Mauretanian track over the Rif, the Syrtic coast across the
Gulf of Gabès), Beneventum lying outside the film's Samnium outline, and
Hannibal sailing home in 203 from a Croton the data still gave to Rome.

## Editor

The map editor is one tool with three workspaces, switched from the bar under
the menu (`Ctrl+1` / `Ctrl+2` / `Ctrl+3`):

- **Campaign** — the strategic world (`tools/map_editor/campaign_workspace.cpp`).
- **Battlefield** — the battle map editor.
- **Mission** — objectives, forces and waves on top of the map.

They share the menus, Save/Undo/Redo (routed to the active workspace), the
status bar and a **command palette** (`Ctrl+K`, or the search box) that jumps
to any site, region, route, army, event, mission or map and runs commands
(validate campaign, complete the road network, check battlefield roads, …).
A breadcrumb shows where you are — _The Barcid Road › Cannae ›
map_battle_cannae.json › Battle of Cannae_ — and each part is a link back; a
battlefield opened from anywhere finds its campaign site by mission or map.
`Alt+←` returns to that site. The badge beside the search box shows the
campaign's validation state.

Campaign workspace:

- **Map** — the campaign base map with relief from the ETOPO heightmap,
  corrected for latitude; labels are placed without overlap and in priority
  order (selection, battlefields, population). Selecting a site highlights its
  roads. Layers: relief, faction control (contested regions hatched), borders,
  districts, roads, sites, ports, labels, armies, campaign movements (dashed
  arrows along the roads an army took in the current event), supply lines,
  resources, chokepoints, objectives, problems, legend.
- **Navigation** — wheel or `+`/`-` to zoom, middle-drag, Space+drag, drag on
  empty ground or the arrow keys to pan, `F` to frame the selection, `Home` to
  fit, `Esc` to cancel or clear.
- **Tools** — Select (`S`: click, drag sites, drag route bends), Add Site (`N`;
  returns to Select after one placement, hold Shift to place several; clicking
  an existing site selects it), Add Route (`R`; click sites in turn to chain a
  road), Bend (`B`; double-click a bend to remove it), Measure March (`M`),
  Complete Network. `Delete` removes the selected site or route; everything is
  undoable.
- **Right-click** anything: open or generate its battlefield, march from here,
  reach in 7 days, supply, connect with a road, who holds it at the start, a
  route's opening status and chokepoint flag, add a site of any kind here.
- **Explorer** — _Outline_: a filterable tree of regions and their sites,
  routes, armies and events that follows the selection; _Layers_; _Analysis_:
  march, alternative routes and reach between any two sites as any faction,
  with or without hostile ground and sea lanes, plus isolation, threats and
  chokepoints.
- **Inspector** — a concise mechanical summary with links (economy, walls,
  roads with march days, armies present, which armies it feeds, the nearest
  hostile army and its march, history, problems), buttons to open or generate
  the battlefield and show supply, and an edit form below. Ownership, route
  status and army positions are edited at the opening of the timeline; later
  states come from events.
- **Timeline** — every event at equal spacing with year ticks (missions in
  red), the event's summary and a table of every change it causes. `[` and `]`
  step, `Ctrl+[` / `Ctrl+]` jump to either end.
- **Problems** — every validation issue with severity filters; click to find
  it on the map, where offending sites are ringed and routes dashed.

Double-clicking a ⚔ site, or _Open Battlefield_, opens its mission (or map) in
the Mission (or Battlefield) workspace. _Generate Battlefield_ opens a new map
in Battlefield with the Generator tab prefilled from the encounter.

Screenshots for review: `map_editor --world --world-at battle_of_cannae
--world-select cannae --world-layers supply --screenshot out.png`.

## Battlefield road networks

The battlefield editor gained the same idea one scale down: **Test ▸ Check Road
Network…** (`tools/map_editor/road_network.cpp`) treats roads, bridges and
fords as one network — T-junctions and crossings join roads, a bridge carries a
road over a river, a bridge or ford that touches no road is ignored — and
reports every separate network and every **near miss**: a road end that stops
within 10 cells of a road in another network. **Snap Near Misses** closes them
in one undoable step by extending the road (its old end stays as a bend, so
any road that met it there still does).

The game draws a road as `start → waypoints → end`. Eight shipped maps (Amber
Delta, Trasimene, Copper Canyons, Forest, Mountain, Pinewater Cut, Rivers,
Spanish Grove) had roads whose `start`/`end` lay up to 9 cells beyond their
first and last waypoints, and the editor used to replace `start`/`end` with
those waypoints on load — so it saw a broken network the game does not have,
and saving such a map in the editor cut those tails off the game's roads. The
loader now keeps an authored endpoint as an extra waypoint, the eight maps list
their endpoints as waypoints (the in-game polylines are unchanged), and a test
holds every shipped road to that form.

With the editor reading roads as the game does, every campaign battlefield is
one network except the Rhône (the far bank is reached by raft) and Ticino
(a village network about 30 cells from the main roads, joined only by a bridge
that reaches no road on its far side). Sallow Ford's two approaches end at the
redoubt on purpose, and Aurelia Magna's street grid ends its streets short of
the walls. None of those is a near miss to snap; a test keeps the connected
maps connected.

## Command line

```
strategic_world validate [--strict]
strategic_world --at battle_of_cannae summary | supply | encounters | threats
strategic_world path new_carthage rome --faction carthage [--safe] [--land]
strategic_world reach capua --days 8 --faction carthage
strategic_world routes rome capua
strategic_world chokepoints
strategic_world timeline
strategic_world --at alps_crossed encounter alpine_pass
strategic_world --at battle_of_zama save-state zama.json
strategic_world format
```
