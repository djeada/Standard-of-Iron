# Arena gameplay scenarios

Arena scenarios use the production `World`, registered runtime systems, unit
factories, command service, humanoid preparation, and OpenGL renderer. The same
scenario catalog is available interactively, from a local batch command, and to
the programmatic test harness.

## Interactive inspection

```bash
build-debug/bin/arena_app --scenario spear_walk_contact

# Settlement and economy inspection
build-debug/bin/arena_app --scenario roman_marching_camp
build-debug/bin/arena_app --scenario carthage_trade_town
build-debug/bin/arena_app --scenario roman_fortification_showcase
build-debug/bin/arena_app --scenario carthage_fortification_showcase
build-debug/bin/arena_app --scenario rival_economies
build-debug/bin/arena_app --scenario water_showcase
```

This loads the named catalog scenario directly and runs it at real wall-clock
speed for visual inspection. Running without `--scenario` still opens the
general Arena UI; use the **Units** panel to choose and load scenarios. The
status bar reports the first automated failure and final PASS/FAIL result while
the viewport remains available for normal RTS camera controls.

## Local batch inspection

Batch mode intentionally opens the real Arena OpenGL window. It requires a
local graphical session and is not installed as a CI test.

```bash
# List the catalog
build/bin/arena_app --list-scenarios

# Run one scenario
build-debug/bin/arena_app \
  --batch \
  --scenario spear_walk_contact \
  --fps 60 \
  --seed 1337 \
  --artifact-dir artifacts/arena

# Run the complete local catalog
build/bin/arena_app --batch --all --artifact-dir artifacts/arena
```

The command exits with status `0` only when every selected scenario passes,
`1` for gameplay/render failures or watchdog timeouts, and `2` for invalid
arguments. Each scenario directory contains:

- `report.json`: machine-readable acceptance results and exact offenders.
- `trace.jsonl`: frame-by-frame entity and rendered-soldier observations.
- `run_config.json`: scenario, seed, fixed FPS, and renderer configuration.
- `final.png`: the final rendered framebuffer for visual inspection.
- `failure_frame.png` or `timeout.png` when applicable. Continuous context is
  retained in `trace.jsonl` without forcing extra OpenGL paints between fixed
  verification frames.

The `--duration` option can shorten a diagnostic run, but the catalog defaults
should be used for acceptance runs because contact and retargeting expectations
need time to complete.

## Temporal render continuity contract

`render_continuity` is the focused regression scenario for intermittent bright
frames and formation-member popping. It keeps the camera fixed, selects Ultra
through the normal graphics-settings path, disables UI overlays and terrain
scatter, and drives infantry, archers, cavalry, healers, missiles, and combat
effects at the same time:

```bash
build-debug/bin/arena_app \
  --batch \
  --scenario render_continuity \
  --fps 60 \
  --seed 1337 \
  --artifact-dir artifacts/render-continuity
```

This contract is Debug-only because per-soldier submission tracing is compiled
only into Debug builds. It reads the actual OpenGL framebuffer on every fixed
simulation frame and fails when a scene-wide luminance jump returns to its
baseline within two frames. It separately tracks each living formation member
and fails on visible-to-culled transitions, missing soldier samples, or an entire
unit disappearing before submission. The report includes the entity, soldier,
and exact frustum, fog, billboard, or temporal cull reason; the batch runner also
captures `failure_frame.png` at the first detected failure. The scenario does not
use Arena's force-full override, so it also proves that Ultra itself disables
formation-count reduction, minimal meshes, billboards, and temporal LOD shedding.

## Campaign terrain review

Arena can render production campaign maps as terrain-only review scenes. This
keeps each map's ground profile, hills, mountains, roads, rivers, lakes, shores,
and bridges while suppressing units, buildings, scatter, weather, boundary fog,
and UI overlays. Add `--map-preview-content` to also render biome scatter,
authored and procedural world props, every authored point building, and wall
networks from the same `structures` collection. Weather, troop spawns, and the
review overlays remain suppressed.

```bash
# Inspect one map interactively
build-debug/bin/arena_app --terrain-map assets/maps/map_crossing_alps.json

# Inspect terrain together with biome, props, and buildings
build-debug/bin/arena_app \
  --terrain-map assets/maps/map_crossing_rhone.json \
  --map-preview-content

# Capture every mission map in campaign order
build-debug/bin/arena_app \
  --batch \
  --campaign-terrain \
  --map-preview-content \
  --artifact-dir artifacts/campaign-terrain-review
```

Each map directory contains `overview.png`, `gameplay.png`, `final.png`, and a
`report.json` inventory of map dimensions and rendered terrain features. The
overview camera fits the entire authored map; the gameplay capture uses that
map's production camera. This path loads maps through `MapLoader` and configures
the same `TerrainService` and renderer passes used by the game.

## Programmatic tests

```bash
QT_QPA_PLATFORM=offscreen \
  build/bin/standard_of_iron_tests \
  --gtest_filter='ArenaScenario*'
```

These tests validate the catalog schema, named-group orchestration, event
triggers, production command shape, rendered-root acceptance checks, and local
artifact serialization. Actual rendered acceptance is performed by the batch
command above.

## Settlement and economy contracts

Settlement scenes use the production building factory and nation renderers, so
changes to homes, markets, barracks, walls, and towers appear exactly as they do
in-game.

- `roman_marching_camp` uses an axial castrum composition: central principia,
  barracks street, ordered housing, perimeter walls, and watchtowers.
- `carthage_trade_town` uses dense Punic courtyard housing around a market,
  with a fortified mercantile quarter and warm brick-and-cedar materials.
- `rival_economies` gives Roman and Carthaginian AI builders equivalent starter
  settlements with finite stockpiles plus authored olive groves, stone, and iron.
  Builders must harvest missing materials and then complete construction. Roman
  AI uses a defensive camp plan; Carthaginian AI uses a compact economic plan.
- `architecture_and_props_showcase` presents all Roman and Carthaginian building
  families side by side with the authored shrine, ruin, cursed-tree, ore, and
  armory props for clean silhouette, material, and readability review.
- `roman_fortification_showcase` isolates disciplined timber runs, reinforced
  corners, six tower sockets, and a defended gate opening around an occupied ward.
- `carthage_fortification_showcase` presents bronze-bound irregular palisades,
  jagged corner and gate towers, plus a layered inner ward for close visual review.

Batch runs produce the normal report, trace, and framebuffer artifacts.
`GroupExists` expectations protect required settlement anchors;
`OwnerHarvestsResource` and `OwnerCompletesConstruction` prove the full economic
loop, while the shared frame-budget contract catches overly expensive detail.

## Water rendering contract

`water_showcase` places a river ribbon and an irregular elliptical lake in the
same camera view. Both use the production water material and visibility path;
their geometry, motion profile, foam coordinates, and shoreline meshes remain
shape-specific. Use its batch `final.png` to compare flow, calm-water motion,
shore contact, and water/terrain overlap after renderer changes.

## Formation melee contract

`spear_walk_contact` is the rendered regression contract for formation melee.
It fails when any of these rules are violated:

- Formation roots lock only after deep formation overlap, not first-rank contact.
- Every living soldier receives an opponent lane and is observed in a fight
  animation during the engagement.
- Per-soldier fight phases must contain visible deterministic staggering.
- A melee lock cannot disappear, change target, navigate, translate, or rotate
  while the locked opponent remains alive.
- Pose oscillation, root teleporting, unexpected fall poses, and frame-budget
  regressions remain failures.

The trace records unit yaw and lock target plus each soldier's declared action,
opponent slot, engagement gap, visual state, and attack phase. This lets CLI and
LLM inspection verify the same presentation contract consumed by the renderer.

## Commander duel matrix

Three bodyguard-free reciprocal fights cover all six commanders:

- `commander_consul_vs_broker`
- `commander_field_vs_cavalry`
- `commander_legion_vs_elephant`

Each scene requires both commanders to render repeated authored attacks, register
physical contact damage, show hit reactions, remain visually stable, and stay
inside the frame budget. Their fixed midpoint camera keeps both silhouettes at
the same depth for direct weapon and motion comparison.

## Pathfinding showcase contracts

- `path_bridge_crossing` uses production river, bank, bridge rendering, and
  navigation. It requires a bridge-deck observation, formation-centroid alignment
  within 0.50 metres of the bridge centerline at midspan, and arrival on the far
  bank.
- `path_uphill_advance` uses a smooth four-metre walkable relief patch. It requires
  visible locomotion, at least 2.5 metres of measured elevation gain, and crown
  arrival.
- `path_wall_detour` keeps a full palisade alive and requires infantry to route
  around its end before reaching the far side.
- `path_wall_breach` destroys a low-health center section, waits on the actual
  group-destroyed trigger, then requires the attackers to traverse the navigable
  opening while both intact flanks survive.

The path scenes suppress incidental scatter and use a fixed feature-centered
camera so the route, obstacle, and formation motion remain readable in batch
captures.
