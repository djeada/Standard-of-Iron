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

# Unit activity icons: every job and every stalled order in one shot
build-debug/bin/arena_app --scenario unit_activity_showcase

# Inhabited settlements, camps and the economy around them
build-debug/bin/arena_app --scenario village_day_life
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

`village_day_life` is the ambience reference scene: a close camera over a hamlet whose
residents work its buildings and props and whose woodcutters and quarriers run their own
round on a standing gather order after one initial job each. It is the scene to record
for ambience footage and the scene to check after touching
`SettlementLifeSystem` or `GatherLoopSystem` — see
[docs/SETTLEMENT_LIFE.md](../../docs/SETTLEMENT_LIFE.md).

## Unit activity markers

The viewport draws the same activity badges the game HUD puts over a unit,
reading them from `Game::Systems::classify_unit_activity` and painting them with
`Ui::IconArt` — one geometry source shared with QML, so an icon reviewed here is
the icon that ships.

`unit_activity_showcase` is the reference scene for that iconography: builders
raise a structure, gathering crews fell timber and work a boulder field and an
ore seam, carriers walk a load to the barracks, a repair crew mends a battered
home, and a second crew is pulled off its job so the interrupted and unavailable
states can be read on the markers.

Overlays are normally dropped from captured frames so artifacts show the render
alone. A scenario whose subject _is_ an overlay opts back in with
`capture_ui_overlays = true`, which is why this one's `frame_*.png` include the
badges.

## Projectile range indicators

```bash
build-debug/bin/arena_app --scenario range_indicator_archers
build-debug/bin/arena_app --scenario range_indicator_siege_minimum
build-debug/bin/arena_app --scenario range_indicator_mage
build-debug/bin/arena_app --scenario range_indicator_elevation
build-debug/bin/arena_app --scenario range_indicator_hold_bonus
build-debug/bin/arena_app --scenario range_indicator_mixed_selection
```

The viewport draws the selected units' projectile range rings from the same
`Game::Systems::collect_attack_range_rings` the game uses, so a ring reviewed
here is the ring that ships. `RangeIndicatorObserved` asserts the radius against
the number the combat system fires with — which is why the archer scenarios
expect 11.4 and not the 7.6 in the troop data: `TroopProfileService` multiplies
bow range by 1.5 on the way to the unit. `RangeIndicatorCountAtMost` guards the
group cap in `range_indicator_mixed_selection`.

`range_indicator_siege_minimum` is the only place a minimum firing distance
exists today; it comes from the scenario group's `attack_min_range_override`,
because no shipped unit carries one yet.

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

**The short never opens on a black frame.** Social platforms take frame zero as
the thumbnail, so a fade-in there costs the video its cover image. The edit
opens on a hard cut by default (`--opening-fade` re-enables one deliberately),
the capture skips any black lead-in frames before it starts recording a shot,
and the edit reads frame zero back out of the finished file and fails the run if
it is black.

**The score goes through the game's audio mastering.** `promo-edit.py` runs the
music track through `build/bin/audio_master_preview`, which links the same
`game/audio/audio_mastering.cpp` the game applies at decode, so the short is
scored with the audio players actually hear rather than the raw generated
master. Build that target first; without it the edit warns and falls back to the
unmastered track. See `docs/AUDIO_MASTERING.md`.

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
- `gameplay_camera` hands the lens back to the scenario instead of driving an
  authored track, which is how an RPG shot is filmed by the commander's own
  chase camera. A gameplay shot needs no `focus` or `camera` block, and any it
  declares is ignored.

- `rpg_hud` paints the commander's bow HUD onto the captured frame: the spread
  reticle projected through the live vertical FOV, the draw ring, the hit
  confirm, HP and stamina, the renock meter, and a takedown counter. It is drawn
  on the CPU over the finished image, so it lands at the recorded resolution
  rather than the window's, and it reads its state from
  `ArenaViewport::rpg_bow_hud_state`.

`tools/arena/promos/bow_commander.json` is a straight gameplay reel: four
gameplay-camera shots with the bow HUD on, cut end to end across one
deterministic run of nine aimed kills.

`promo_commander_duel` stages single combat for filming: both armies drawn up in
line as spectators, a ring of low hills closing the horizon, and Scipio against
Hannibal in the ground between them until Hannibal falls. It is the scene to
record for duel footage and the scene to check after touching commander
signatures or the melee lock. See
[docs/PROMO_CAPTURE.md](../../docs/PROMO_CAPTURE.md) for how its shots are
aimed.

## RPG commander scenarios

The `rpg_*` scenarios run the production `CommanderControlController`, so the
camera in those captures is the game's own chase camera and the commander
answers scenario steps the way he answers a player.

`rpg_bow_volley` is the bow contract: a bow commander against nine charging
swordsmen, one drawn arrow apiece, with `GroupDestroyed` asserted for every one
of them. It uses two steps that matter for any aimed weapon:

- `RpgAim` points the view. Given a `target_group` it becomes a standing order:
  every tick until the next `RpgAim`, the runner asks the host to put the
  crosshair on that group's living centroid. The host solves the angles from the
  live camera position (`aim_rpg_view_at`), not from the shooter's chest,
  because the shot is resolved along the camera axis - aiming from the body
  would put the reticle on the target and the arrow half a metre beside it.
  Explicit `rpg_view_yaw_degrees` and `rpg_view_pitch_degrees` are used when no
  target group is named, and clear the standing order.
- `RpgAttackHold` holds and releases the attack button, which on a bow is the
  draw and the loose.

Promo runs render in cinematic mode: no selection rings, no attack/guard/hold
markers, no order markers, no stats or control overlays. The first recorded
frame of every shot logs its resolved focus and the live soldier culling
counters, which is how a mis-aimed camera is told apart from a camera pointing
somewhere the engine culled.

The `promo_*` scenarios in the catalog are staged for this: short frontages that
fill a vertical frame, dramatic authored light, and enough runtime that one
deterministic pass can supply a whole shot list.

## Humanoid showcase

`promo_humanoid_showcase` stages the authored humanoid moves for filming rather
than for acceptance. Four lightly armoured, bare-headed performers share one
field: an acrobat looping leap, front flip, side aerial and handstand; a blade
master cutting air; a lancer throwing a spear at a statue; and a fourth actor
walking and running across the frame.

```bash
build/bin/arena_app --scenario promo_humanoid_showcase --seed 44

build/bin/arena_app --promo-spec tools/arena/promos/humanoid_showcase.json \
  --promo-out artifacts/promo
scripts/promo-edit.py --spec tools/arena/promos/humanoid_showcase.json \
  --clips artifacts/promo/humanoid_showcase
```

The scenario is driven by two group fields rather than by scenario steps:
`showcase_routine` is a list of `move[:duration[:hold]]` entries naming moves
from `Animation::HumanoidShowcaseMove`, and `renderer_override` swaps the
per-entity `renderer_id` so the same troop type can appear armed, unarmed or
carrying a spear. `showcase_throw_target` arms the spear release, which spawns a
javelin-styled missile at the clip's authored release marker and swaps the
performer to the unarmed renderer so the spear leaves his hand.

See `docs/PROMO_CAPTURE.md` for why the reel uses `render_scale_override`, seed
44, and shots confined to the acrobat's first routine loop.

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

Twelve scenarios cover the undead faction end to end. The four battle scenes use
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

The seven awakening scenes drive the production `UndeadAwakeningSystem` rather
than authored enemy groups. The scenario declares real `undead_zones`, the arena
configures the system from them, raises the zone haze, and both the guardians and
the zone's magic shrine are spawned by the system:

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
- `sepulcher_zone_shrine_spawn` authors no prop at all: the zone raises its own
  shrine on bare ground, which is the contract for "every zone has a shrine".
- `sepulcher_twin_zone_shrines` puts two zones on one field and requires one
  shrine each - neither zone borrows the other's.
- `sepulcher_shrine_demolition` brings the shrine down mid-scene and requires the
  garrison to crumble with it.
- `sepulcher_shrine_state_reload` round-trips the zone through the save/load path
  (`serialize_state` / `restore_state`) while it is awake, and requires the
  shrine and garrison that already stand to survive the reload unduplicated.
- `sepulcher_fireball_review` is the fireball FX bench: one caster, one target
  that cannot die or close to melee, and nothing else in frame. The cast charge
  in the hand, the ball in flight, its smoke trail and the detonation can all be
  judged without ambient combat dust washing the shot out.

```bash
build-debug/bin/arena_app --scenario sepulcher_shrine_awakening
build-debug/bin/arena_app --batch --scenario sepulcher_shrine_siege \
  --fps 30 --artifact-dir artifacts/sepulcher
```

Five acceptance kinds back these scenes and are reported in `report.json` under
`undead_zones` (spawn totals, peak living guardians, the first spawn time, and
the shrine's fate):

- `UndeadZoneDormantBefore` fails when a zone spawns anything before its declared
  dormancy window, which is what proves the "empty space" opening.
- `UndeadZoneAwakened` fails when a zone never releases the required number of
  guardians.
- `UndeadZoneCleared` fails when guardians never appeared or any remain alive at
  the end of the run.
- `UndeadZoneShrineStands` fails when a zone never raised a shrine, or lost the
  one it raised.
- `UndeadZoneShrineDestroyed` fails when the shrine is still standing at the end
  of the run.

A step may carry a `zone_id` instead of a `group`: `SetHealth` and `ApplyDamage`
then act on that zone's shrine, and `ReloadUndeadZoneState` round-trips the whole
zone state through serialize/restore without touching the rest of the scene.

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

Three longer cross-weapon duels exist for the signature moves, the one duel trick
each commander owns:

- `commander_signature_spear_vs_sword` - Fabius' Bracing Thrust against Hannibal's
  Encircling Cut.
- `commander_signature_sword_vs_bow` - Scipio's Consular Riposte against
  Hasdrubal's Hunting Shot, loosed inside the clinch.
- `commander_signature_bow_vs_spear` - Marcellus' Point-blank Volley against
  Hanno's Phalanx Sweep.

They run for 24 s with both commanders on high health on purpose: a signature is
on a five-to-ten second cooldown, so a short duel that ends in a kill would only
ever show one of them. `report.json`/`trace.jsonl` name the action each frame -
`RtsCommanderThrust`, `RtsCommanderCut` and `RtsCommanderShot` are the signature
forms, distinct from the routine `RtsSwordStrike`/`RtsSpearThrust`/`RtsBowShot`.

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
