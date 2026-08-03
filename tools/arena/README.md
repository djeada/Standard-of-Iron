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

# Inhabited settlements, camps and the economy around them
build-debug/bin/arena_app --scenario village_harvest_cycle
build-debug/bin/arena_app --scenario colony_founding
build-debug/bin/arena_app --scenario village_raid
build-debug/bin/arena_app --scenario frontier_outpost
build-debug/bin/arena_app --scenario riverside_mill_town
build-debug/bin/arena_app --scenario quarry_camp
build-debug/bin/arena_app --scenario trade_road_convoy
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

## Cinematic promo capture

Arena can record finished video instead of stills. A **promo spec** is a JSON
shot list; each shot names a catalog scenario, the window of scenario time to
record, and an authored camera track. The arena runs each shot as its own
deterministic scenario pass, renders it into an offscreen target at the
requested resolution -- vertical shorts are not limited by the desktop window --
and streams the frames straight into `ffmpeg`.

```bash
build/bin/arena_app \
  --promo-spec tools/arena/promos/last_stand.json \
  --promo-out artifacts/promo

scripts/promo-edit.py \
  --spec tools/arena/promos/last_stand.json \
  --clips artifacts/promo/last_stand
```

The capture writes one `NN_<shot>.mp4` per shot, an `NN_<shot>.png` poster, and
a `shots.json` manifest. `scripts/promo-edit.py` then concatenates, grades,
captions and scores them into one finished short in a single ffmpeg pass.
`ffmpeg` must be on `PATH` for both steps.

Shot fields:

- `scenario`, `seed`, `start`, `duration` select the deterministic window. The
  clock is scenario time, so the same window always records the same action.
- `slow_motion` shrinks the simulation step rather than duplicating frames, so a
  2.5x shot keeps full temporal detail. The clip runs `duration * slow_motion`
  seconds.
- `focus` aims the camera: a fixed `point`, the living centroid of a `group`,
  the midpoint of a `group_pair`, or `all` spawned units. Group focus holds its
  last good position if the tracked group is wiped out mid-shot, and `smoothing`
  is the tracking half-life in seconds.
- `camera` is a keyframe list over shot-local time: `distance`, `pitch`, `yaw`,
  `fov`, `roll` for a dutch tilt, `height` to raise the aim point off the
  ground, and `ease` (`smooth`, `linear`, `in`, `out`).
- `shake` adds deterministic handheld jitter, so re-recording a shot is
  reproducible.

Promo runs render in cinematic mode: no selection rings, no attack/guard/hold
markers, no order markers, no stats or control overlays. The first recorded
frame of every shot logs its resolved focus and the live soldier culling
counters, which is how a mis-aimed camera is told apart from a camera pointing
somewhere the engine culled.

The `promo_*` scenarios in the catalog are staged for this: short frontages that
fill a vertical frame, dramatic authored light, and enough runtime that one
deterministic pass can supply a whole shot list.

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
  build/bin/tools_tests \
  --gtest_filter='ArenaScenario*'
```

These tests validate the catalog schema, named-group orchestration, event
triggers, production command shape, rendered-root acceptance checks, and local
artifact serialization. Actual rendered acceptance is performed by the batch
command above.

## 100 FPS battle performance contracts

Two rendered scenarios enforce a strict p95 CPU frame-work budget below 10 ms
after a two-second warm-up. Every unit is one full-detail rendered combatant; the
scenes force full creature LOD at Ultra quality while exercising
simulation, combat, terrain, effects, and OpenGL playback:

```bash
build/bin/arena_app --batch --scenario performance_20v20 \
  --fps 120 --artifact-dir artifacts/arena-performance
build/bin/arena_app --batch --scenario performance_30v30 \
  --fps 120 --artifact-dir artifacts/arena-performance
```

The scenarios contain exactly 20 vs 20 and 30 vs 30 units (40 and 60 rendered
combatants respectively). `report.json` records frame-time sample count,
p50, p95, maximum, the 9.99 ms budget, FPS derived from p95, peak visible
soldiers, peak submitted draw commands, and main/shadow rigged-instancing
playback counts. A p95 at or above 10 ms, a frame window containing no rendered
soldiers, or a run that never exercises rigged instancing fails the scenario.

## Settlement and economy contracts

Settlement scenes use the production building factory and nation renderers, so
changes to homes, markets, barracks, walls, and towers appear exactly as they do
in-game.

- `roman_marching_camp` is a full castrum on its own street grid: the via
  praetoria runs from the porta praetoria to the principia, the via principalis
  crosses it between the flanking gates, an intervallum lane rings the inside of
  the rampart, barrack blocks fill the northern half and the officers' houses
  and contubernia the southern one. Gates on all four sides, towers on the
  corners, and townspeople working the streets throughout.
- `carthage_trade_town` is an oblong Punic town: a bazaar street lined with
  stalls and carts runs its whole width, courtyard housing crowds the lanes off
  it, and the mercenary quarter holds the eastern end.
- `village_harvest_cycle` is an unwalled hamlet working its land, with
  woodcutters and quarriers on the tree line, the boulder field and the ore seam.
- `colony_founding` puts a colony under construction beside a finished village so
  both states of a settlement can be judged in one view.
- `village_raid` attacks an inhabited village while its people are still out in
  the street and the watch turns out of the barracks.
- `frontier_outpost` is a camp rather than a town: a watchtower over a palisade
  spur, the tent line, the cook fire, the carts and the section that mans it.
- `riverside_mill_town` straddles a river joined by one bridge, with a carrying
  party proving the crossing.
- `quarry_camp` is a pure extraction camp on broken ground.
- `trade_road_convoy` links two allied market towns along one paved road and
  walks a carrying party the full length of it.

A settlement paves its streets one way: mixing road styles inside one settlement
is a contract violation, enforced by `EachSettlementLaysItsStreetsInOneStyle`.

Settlements that spawn `settlement_resident` civilians must also require those
civilians to be _seen_ moving. Residents run the settlement life system's errand
loop -- they walk between the hearth, the settlement's buildings and the props of
daily life, linger there and move on -- and
`InhabitedSettlementsProveTheirDailyLife` fails any inhabited settlement that
does not assert `MovementAnimationObserved` on its residents, so a town that
quietly stops living is a test failure rather than a thing someone has to notice
in a screenshot.

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

## Iron Sepulcher contracts

Seven scenarios cover the undead faction end to end. The four battle scenes use
equivalent-recruitment-value armies so the roster can be balanced against both
playable nations:

- `sepulcher_roster_lineup` shows the whole roster (skeleton swordsman, skeleton
  archer, grave priest) beside a Roman and a Carthaginian line for silhouette,
  scale, and material review.
- `sepulcher_vs_rome_infantry` and `sepulcher_vs_carthage_infantry` are melee
  clashes; both require that no eligible soldier idles once the lines meet.
- `sepulcher_vs_rome_ranged` is a missile exchange with a grave priest casting
  against the Roman screen.
- `sepulcher_vs_carthage_cavalry` charges a standing skeleton block and requires
  visible contact, a charge impact preceding melee lock, deaths, and a launched
  casualty.

The three awakening scenes drive the production `UndeadAwakeningSystem` rather
than authored enemy groups. The scenario declares real `undead_zones`, the arena
configures the system from them, raises the zone haze, and the guardians are
spawned by the system:

- `sepulcher_shrine_awakening` places a cursed shrine on otherwise empty ground.
  Roman swordsmen walk into its radius, the whole garrison rises together spread
  around the shrine, and it fights the intruders. The zone declares no waves, so
  this scene is also the contract for the default garrison.
- `sepulcher_ruins_awakening_waves` sends a Carthaginian column into sepulcher
  ruins, clears the opening wave, and proves the `after_clear` follow-up wave is
  released only once the first is destroyed. Ruins stay decorative anchors.
- `sepulcher_shrine_siege` razes the shrine instead of grinding the garrison
  down. Because a shrine is the sepulcher's barracks, breaking it puts every
  risen guardian down at once and clears the zone.

```bash
build-debug/bin/arena_app --scenario sepulcher_shrine_awakening
build-debug/bin/arena_app --batch --scenario sepulcher_shrine_siege \
  --fps 30 --artifact-dir artifacts/sepulcher
```

Two acceptance kinds back these scenes and are reported in `report.json` under
`undead_zones` (spawn totals, peak living guardians, and the first spawn time):

- `UndeadZoneDormantBefore` fails when a zone spawns anything before its declared
  dormancy window, which is what proves the "empty space" opening.
- `UndeadZoneAwakened` fails when a zone never releases the required number of
  guardians.
- `UndeadZoneCleared` fails when guardians never appeared or any remain alive at
  the end of the run.

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

## Commander RPG contracts

Four behind-head scenes cover commander (FPV) control:

- `rpg_melee_contact` validates exact in-range soldier highlighting, authored blade
  contact, hit reactions, and visible incoming weapon damage.
  `RpgStrikeAnimationMatched` additionally fails the run when the simulation
  resolves a strike for longer than one frame while the renderer still draws an
  idle or walking commander.
- `rpg_defense_contact` is a frontal sword block followed by a timed dodge; health
  must stay unchanged while the block contact and dodge window remain visible.
- `rpg_projectile_block` requires an authored arrow to arrive at the guard, publish
  a block contact, and leave RPG health unchanged.
- `rpg_escort_crowd` puts the commander inside his own escort with a rank of
  friendly spearmen between him and the lens. It is the contract for chase-camera
  readability: the renderer drops bodies that crowd the gap in front of the lens
  instead of letting them fill the frame or shoving the camera into first person.
  `escort_flank` stands beside the commander and must still render, which is what
  keeps the cull from being over-broad; `escort_rear` stays alive in the trace
  while it is deliberately not drawn.
- `rpg_locomotion` drives the commander's own movement input rather than an order:
  he walks, breaks into a run, backs up and strafes without ever turning. It is
  the contract for locomotion synchronisation — `RpgWalkObserved` and
  `RpgRunObserved` require both simulated gaits to actually occur, and
  `RpgLocomotionAnimationMatched` fails the run when the simulation reports one
  gait while the renderer draws another on the same frame.
- `rpg_close_quarters` walks the commander into a house wall, off it, and back onto
  it from the flank. The RTS navigation grid rounds every structure up to whole
  cells and pads it for formation-sized bodies; a person-scale commander has to
  reach the facade itself. `RpgApproachWithin` fails when he cannot close to the
  declared distance of the structure.

- `rpg_obstacle_slide` walks the commander diagonally into a house facade and
  keeps the input pushed. Direct control has to behave like a body against a
  wall, so `RpgApproachWithin` proves he really is pressed against the facade and
  `RpgTravelObserved` proves he keeps covering ground along it instead of
  stopping dead in front of it.
- `rpg_combo_cadence` holds the attack input against a formation. Every swing has
  to chain out of the previous one inside its cancel window, so
  `RpgSwingCadenceWithin` fails both a swing count that is too low and any gap
  between swings that is too long. `RpgStrikeAnimationMatched` additionally fails
  the run if the renderer ever drops the swing — for a flinch, or for anything
  else — while the simulation is still committed to it.
- `rpg_pass_ranks` walks the commander the length of a friendly line and back
  through it. The chase lens hides bodies standing in the gap between it and the
  commander, and this is the contract that keeps that judgement per body: an
  anchor entering the gap must not take its whole formation with it.
  `RpgFormationSurvivesLensGap` fails a frame that drops more than half of a
  unit's living soldiers to the lens gap, and also fails a unit that vanishes
  from submission entirely for any reason other than the frustum or the fog.
- `rpg_strike_lunge` attacks an enemy standing just outside planted reach with no
  movement input at all. A swing has to carry the body into the target, so
  `RpgTravelObserved` fails a commander who swings from a planted stance and
  `GroupHealthReduced` fails the whiff that follows from it.

These scenes are driven by the `RpgMove` and `RpgAttackHold` scenario commands,
which set the behind-head controller's own movement axes, view yaw, and attack
button instead of issuing RTS orders, so they exercise the same input path a
player uses.

```sh
build/bin/arena_app --batch --scenario rpg_escort_crowd \
  --fps 30 --capture-interval 0.4 --clean-capture --artifact-dir artifacts/rpg
```

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

## Road and bridge surface contracts

Three scenes exist to review the road network surface itself. They declare authored
`roads` (and, for the bridge scene, a river and a bridge) and drive a short infantry
column through them so the capture also proves the surface stays navigable:

- `road_junction_showcase` puts a crossroads, a T-junction, a Y-branch, a sharp bend,
  and two side turnings only a road-width apart in one frame. Junction geometry is
  built as a single merged surface, so the capture should show no doubled texture, no
  dark seam where roads meet, and no polygon spiking past a corner.
- `road_slope_showcase` runs one road straight up a rise and a second across the fall
  line, crossing on the flank. Use it to check that the surface stays on top of the
  terrain and that the crossing still reads as one continuous junction on a slope.
- `road_bridge_approach` runs a road onto a bridge deck from both banks. The river must
  stay continuous underneath the span, the deck must land on bank at both ends, and the
  approach must rise onto the deck instead of stopping short of it.

```bash
build/bin/arena_app --batch --scenario road_junction_showcase \
  --fps 30 --clean-capture --artifact-dir artifacts/roads
```

## Gate contracts

A gate is a gatehouse three wall cells long: a solid pier on either side that
continues the wall, and a clear opening in the middle wide enough to march a war
elephant through. `GateComponent::k_structure_half_span` and
`k_passage_half_width` are the single source of that geometry -- the collision
footprint, the navigation passage carved through it, the movement blockers and
the rendered leaves all measure themselves against those two numbers, so the
thing units are allowed to walk through and the thing the player sees can never
drift apart. Wall runs therefore stop two cells clear of a gate rather than one.

The leaves only count as passable at the very end of the swing, and the gate
opens faster than it closes and holds after the last body clears, so a column
never has to stop for its own gate and the leaves never shut on someone
mid-crossing.

Five scenes cover the wall gate, each cutting one gate into a palisade running
east-west through the origin:

- `gate_friendly_passage` sends the wall owner's own infantry at it and requires
  the gate to open and the column to arrive on the far side.
- `gate_allied_access` repeats the run with a third player sharing the wall
  owner's team, proving admission follows the diplomacy tables rather than the
  literal owner id.
- `gate_enemy_blocked` walks hostile infantry into a shut gate and requires it to
  stay shut and the raiders to stay on their own side.
- `gate_destroyed_breach` breaks a low-health gate and requires the attackers to
  pour through the opening it leaves, with both flanking runs intact.
- `gate_consecutive_transit` pushes three files through in succession so the gate
  is never observed shutting on a body mid-crossing.

Three acceptance kinds back these scenes:

- `GateOpenedObserved` fails when no gate in the group ever opened far enough to
  walk through.
- `GateRemainedClosed` fails when a gate opened, or when the named group was never
  sampled as a gate at all.
- `GroupHeldOutsideDestination` is the mirror of `GroupReachedDestination`: it
  fails when the group's living centroid ends up within tolerance of a place it
  was supposed to be kept out of.

Scenarios that need two owners on one team declare it with `owner_teams`, which
the Arena applies to the owner registry before spawning.
