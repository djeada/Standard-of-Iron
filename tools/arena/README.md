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

Any scenario can opt into the same per-soldier check with a
`NoRenderVisibilityChurn` expectation, but it reads
`Render::Profiling::CombatAnimationDiagnostics`, so the scenario **must** also
set `collect_animation_diagnostics = true`. Without it no samples are recorded,
the expectation finds nothing, and the scenario reports PASS while proving
nothing. Once it is live, remember that a unit leaving a fixed batch camera is a
legitimate `Frustum` cull: either give the expectation a
`start_seconds`/`end_seconds` window, or widen the scenario's batch camera so it
holds the group. The batch camera is a verification instrument and is
independent of the promo shot cameras, which each carry their own.

## Units may not walk through buildings

`UnitsClearOfBuildings` fails a scenario when a living, non-building unit in the
named group stands with its body circle inside a registered building footprint.
It samples every unit on every rendered frame and reports the first offender
with its position and the time it happened, which is enough to find the building
without re-running.

Standing in a gateway is legal, so the check skips any position that falls
inside a registered `NavigationPassage` -- otherwise every column that marches
through a town gate trips it. This distinction is the whole contract: walking
the road through a wall is correct, walking through the house beside it is not.

The check depends on the building collision registry being populated, which the
arena viewport does when it spawns scenario buildings. It found real overlaps in
the trailer chapters on its first run, so treat a PASS as meaningful only after
you have seen it go red at least once.

## Ambience scenarios

`arena_ambience_scenarios.cpp` holds the long-form ambience lane: scenes staged
to be _watched_, not reviewed. Nothing happens in them on purpose. The first is
`ambience_night_watch` -- three soldiers resting around a camp fire in the rain
with a flock grazing on the slope behind them, built to be captured once and
looped out to an hour or more.

```bash
build/bin/arena_app --batch --scenario ambience_night_watch
build/bin/arena_app --promo-spec tools/arena/promos/night_watch.json \
  --promo-out artifacts/promo
```

Six things about this lane are not obvious and cost a render each to discover.

**The camp fire is the light source, and it is a real one.** `FireCampRenderer`
emits a flickering `Render::LocalLight` per camp, and `character_skinned.frag`
consumes up to eight of them ranked by `intensity * radius^2 / distance^2`. A
figure sitting within a few metres of a camp is genuinely lit by it. What
defeats this is exposure: the `mediterranean_summer` profile already sits near
0.90 exposure and 0.30 ambient at night, so an `exposure_override` above ~1
lifts the whole frame into a green daylit field and the fire collapses into a
flat orange blob. `ambience_night_watch` overrides exposure _down_ to 0.62 and
lets the fire do the work.

**A resource patch's `scale` does nothing to a camp fire on its own.**
`FireCampRenderer` sizes the flame and its light from `WorldProp::radius` and
`WorldProp::intensity`, neither of which `place_scenario_resource_patches` used
to set -- so an authored fire scale was silently dropped and every arena fire
came out at the 3.0 default radius. Patch placement now multiplies both by the
patch scale for `FireCamp`. The flame height saturates at radius 4.5.

**`suppress_terrain_scatter` also deletes every world prop.** The camp fire,
tents, carts, weapon racks, trees and boulders are all submitted through
`submit_scatters`, so the flag that removes the grass tufts removes the camp
with them. Control clutter with `ground_type` and the terrain seed instead --
`forest_mud` grows a pine canopy dense enough to skewer the fire, `soil_rocky`
gives the open, dark ground these scenes want.

**`renderer_override` needs a registered renderer, not just a loadout.** Adding
`troops/roman/camp_rest` to the equipment catalogue is half the job; until the
key is also listed in that nation's `k_swordsman_renderers`, the override does
not resolve and the unit silently falls back to its troop default -- so the men
who were meant to be unarmed came back carrying scutum and sword. Note also that
`merge_json_loadouts` reads `assets/visuals/unit_equipment_loadouts.json`
through `resolve_resource_path`, which finds `build/bin/assets` before the repo
copy; a loadout edit that appears to do nothing is usually that stale copy.

**Only the swordsman path honours an empty weapon slot.** The Roman spearman and
archer renderers attach their spear, shield and bow independently of the
loadout ids, so a resting figure authored on those troop types keeps its weapon.
Seated groups use `Troop::Swordsman` with a weaponless loadout.

**Expectations need `collect_animation_diagnostics`,** and the scenario camera
has to frame every group that carries a `GroupIsRendered` expectation. The
trailer-style `definition()` helper turns diagnostics off, and with it off every
`GroupIsRendered` fails with "produced no rendered soldier observations" even
though the promo capture reports the soldiers drawn. A tight review camera fails
the same expectation for a different reason: a background figure outside the
frustum is never observed.

### Resting poses

Sitting is authored, not runtime. `HumanoidAmbientIdle::SitDown` is a 3 s
squat-and-rise gesture, not a pose to hold, so the lane adds three sustained
showcase moves -- `rest_sit` (cross-legged, hands on the knees), `rest_sit_knees`
(knees drawn up, forearms draped over them) and `rest_kneel` (down on one knee,
reaching in to feed the fire) -- in `animation/showcase_pose_manifest.cpp`. Each
one loops: its `t = 0` and `t = 1` keys are identical, and the frames between
carry only breathing. Scenario groups select them through `showcase_routine`
(`"rest_sit:6.0:0.0"`) with `showcase_loop`, the same mechanism the humanoid
showcase uses.

A routine can also open with a one-shot before it settles into its loop.
`showcase_loop_from` is the index the routine returns to when it wraps, so
`{"rest_sit_down:1.6:0.0", "rest_sit:6.0:0.0"}` with `showcase_loop_from = 1`
plays the sit-down once and then breathes forever. This matters because
`apply_showcase_clip` _clears_ `full_body_blend` -- a showcase clip is a hard
switch with no crossfade, so a man who walks up and is then handed a seated pose
snaps into it in one frame. `rest_sit_down` is the authored bridge from the
neutral standing key to the `rest_sit` t=0 key. While `showcase_start_delay` is
still counting down the routine reports itself inactive, which is what lets the
unit walk in on its normal locomotion clips first.

Showcase clips are resolved by index arithmetic --
`humanoid_showcase_clip(move) = k_humanoid_showcase_first_clip + move - 1` --
so a new move must be inserted into the humanoid clip table immediately after
the existing showcase block, which renumbers `unarmed_*`, `testudo_*` and
`carthage_shield_wall_*` after it. Update the constants in
`animation/clip_manifest.h`, the `static_assert`s in `tools/bpat_baker/main.cpp`,
and re-run `make bake-bpat`.

**A prop cannot share a soldier's ground position.** A staged scene wants to sit
a man on a rock; the engine will not allow it. World props repel units at
runtime, so a boulder placed on a soldier's spot moves him a full metre out of
it over the following seconds -- the unit spawns correctly and drifts off
afterwards, which is why the effect is invisible in the spawn transform and
only shows up in the rendered root. Shrinking the prop does not help. Seat a
figure on the ground instead, and use props as furniture beside it. This is what
`ArenaScenarioResourcePatch::exact` is for: it skips the nudge search that
otherwise pushes an authored prop clear of its neighbours, so a staged rock
lands exactly where the composition wants it, and snaps to the same walkable
cell a unit would so authored coordinates agree between the two.

A seated pose has to be authored at the right height, because nothing grounds
it for you. `derive_clip_flags` withholds the ground-contact flag from every
clip whose name starts with `showcase_`, and `resolve_entity_ground_offset` is
0 for the humanoid, so a showcase pose's `root_y` _is_ its pelvis height above
the terrain -- there is no foot snapping to hide an error. Grounding on the
feet would be wrong here anyway: `palette_contact_y` measures the soles, and a
cross-legged figure's soles are tucked halfway up its own lap. Use the engine's
death poses as the reference for what "resting on the ground" costs -- they
settle a body at `root_y` 0.15-0.19 -- and put a seated pelvis just above that.
The first cut of these poses sat at 0.345 and the men floated visibly.

Shape these poses with a Python twin of `resolve_humanoid_showcase_pose` rather
than in the engine: a stick-figure of the same FK, a grid search to land a hand
on a target joint, and a ground-clearance sweep across the cycle costs seconds,
where a build-and-bake round trip costs twenty minutes. Confirm the result on
the baked clip with `humanoid_preview --clip showcase_rest_sit --view side
--report`, which also reports bone stretch.

### Sound

The arena records silent footage. `scripts/ambience-audio.py` builds the bed
and muxes it on, reading an `ambience` block from the same promo spec the arena
consumed. It exists next to `promo-edit.py` rather than inside it because the
two want opposite things: a promo is _scored_ (one track, laid once, plus timed
one-shots), while an ambience piece is layered and endless -- two or three beds
looping for the whole length, and animal calls scattered thinly. A three-hour
sleep video needs the beds looped to three hours, which `promo-edit.py` has no
way to express.

```bash
scripts/ambience-audio.py --spec tools/arena/promos/night_watch.json \
  --clip artifacts/promo/night_watch/01_the_watch.mp4
```

`night_watch` layers rain over a camp fire with a distant camp bed well
underneath, and drops a distant wolf in every 26-64 s. `storm` and
`camp_fire_night` are **recordings**, cut by `tools/audio_field/sources.py`, not
generated: outdoor nature is the case `tools/audio_synth` explicitly does not
claim to cover, and a synthesised first pass at both bore that out. Every bed is
seam-sealed when built, so `-stream_loop` repeats one without a click.

**Rain has to be shaped out of the hiss band.** Real rain carries most of its
energy in 2-6 kHz -- `Source.shelf_db` exists for exactly this, and
`AmbienceAssetsTest.NoBedIsLouderInTheHarshBandThanInItsBody` enforces it: a
bed's 2-6 kHz band must sit at least 3 dB under its 100-800 Hz body. The
shipped `storm.ogg` measures a 1281 Hz spectral centroid on
`tools/audio_synth/analyse.py`, and at that brightness it perceptually buries
anything darker sharing the mix -- a camp fire under it was inaudible at equal
RMS. `storm` takes the same treatment the other field beds get (highpass
90, lowpass 3600, -7 dB shelf) and lands at 1281 Hz with its harsh band 12 dB
under its body. Judge a bed with `analyse.py` rather than by ear.

Two levelling rules worth keeping. `amix` divides by its input count unless you
pass `normalize=0`, which otherwise drops the rain by several dB the moment a
wolf calls. And a distant call has to be checked against the bed rather than
authored by eye: the first pass sat 15 dB under the mix and produced no
momentary-loudness movement at all -- inaudible, which is not the same as
subtle. At 0.34 it lifts the momentary reading about 1.2 dB, which reads as
something far off. The finished bed measures -16.3 LUFS with an 0.9 LU range.

### Long-form output

Never render the full length. `night_watch.json` is two shots against one
continuous scenario -- `arrival` (13.5 s, from 0.5 s) and `watch` (36 s, from
14.0 s) -- and the eight-minute cut is `arrival` plus `watch` thirteen times,
concatenated with `ffmpeg -f concat -c copy`. Because both shots come out of the
same encoder they concatenate without re-encoding, and because the second starts
at exactly the scenario time the first ends on, the join between them is
continuous rather than a cut. Only the `watch`-to-`watch` seam repeats, and with
a locked-off camera the sole thing that changes across it is the flame shape.

That structure exists for a second reason: **sustained renders have hard-crashed
this machine.** Two crashes on 2026-08-17, one during an 8-minute capture,
neither leaving an MCE, thermal event or driver oops in the log -- one boot did
show an `nv_drm_handle_hotplug_event` workqueue storm doubling 4 -> 131 over
80 s. A concurrent `-O3 -flto` build in another checkout was running at the
time, which is the more likely trigger. Keep a capture to tens of seconds
(49.5 s of footage renders in 31 s here), disable DPMS before a long run
(`xset -display :0 -dpms s off`), and never background it: a killed job
truncates its output to a stub and destroys whatever good master shared that
path.

Audio is built to the finished length regardless, so it never repeats on the
same period as the picture. `--loop-to` extends the picture too, for the cases
where looping a single shot is enough.

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

`--animation-diagnostics` forces per-soldier animation diagnostics on for a
batch run of a scenario that normally leaves them off for speed; the trace then
carries every soldier's `root_yaw_degrees`, pose and cull reason per frame,
which is how the formation heading hunt was measured (see "The path is pulled
taut" in `docs/PATHFINDING_ARCHITECTURE.md`). Expect a trace of several hundred
MB on the massed scenarios.

`tools/arena/promos/massed_battle.json` films `massed_battle_1000` — the
2,000-soldier full-LOD performance fixture — as a seven-shot reel, its windows
read out of the scenario trace so the cavalry, the line contact and the press
are each cut to the second they happen. See "Filming the battle" in
[docs/MASSED_BATTLE_PERFORMANCE.md](../../docs/MASSED_BATTLE_PERFORMANCE.md).

`promo_commander_duel` stages single combat for filming: both armies drawn up in
line as spectators, a ring of low hills closing the horizon, and Scipio against
Hannibal in the ground between them until Hannibal falls. It is the scene to
record for duel footage and the scene to check after touching commander
signatures or the melee lock. See
[docs/PROMO_CAPTURE.md](../../docs/PROMO_CAPTURE.md) for how its shots are
aimed.

## Trailer chapters

`trailer_*` are the master trailer's chapters. Five of them play on one
authored stage: `dress_valley` in `arena_trailer_scenarios.cpp` lays the same
river valley -- village on the north bank, timber fort on the eastern rise, one
bridge, open ground to the south -- so those cuts read as one place across one
day. The rest are deliberately somewhere else, because a trailer cut entirely on
one stage reads as one stage however well each shot is framed, and the finished
film has to show the game's range of ground as well as its range of action.

Range of ground is authored, not inherited. A chapter names its own
`ground_type` and its own `terrain_seed_override`; `ArenaViewport::load_scenario`
applies both and `reset_arena` restores whatever the session had before, so the
surface under a fixture belongs to the fixture rather than to a global every
scenario shares. Weather is authored the same way through `weather` and
`precipitation`, which is what puts snow on the pass and rain in the wood.

```bash
build/bin/arena_app --scenario trailer_dawn            # valley: economy, flock, wolves
build/bin/arena_app --scenario trailer_muster          # valley: gate, march, bridge, deploy
build/bin/arena_app --scenario trailer_clash           # valley: the pitched battle
build/bin/arena_app --scenario trailer_pov             # valley: the same fight in RPG mode
build/bin/arena_app --scenario trailer_barrow_night    # valley: the undead ambush
build/bin/arena_app --scenario trailer_works           # soil_rocky: five crews raising a settlement
build/bin/arena_app --scenario trailer_sanctuary       # grass_dry: hill temple, the healers' rings
build/bin/arena_app --scenario trailer_gate_march      # grass_dry: every arm through one gate
build/bin/arena_app --scenario trailer_highland        # alpine_mix, snow: the column on the pass
build/bin/arena_app --scenario trailer_forest_ambush   # forest_mud, rain: ambush in the wood
build/bin/arena_app --scenario trailer_bridge_defense  # soil_fertile: the crossing held
build/bin/arena_app --scenario trailer_siege_walls     # grass_dry: siege work against a rampart
build/bin/arena_app --scenario trailer_night_snow      # alpine_mix, snow: the host and the priests
build/bin/arena_app --scenario trailer_last_breath     # alpine_mix, snow: the consul's last fight
build/bin/arena_app --scenario trailer_flame_card      # the act-card stage
```

`trailer_flame_card` is deliberately empty: a shot marked `"flame_card": true`
in the promo spec replaces the frame with `ArenaViewport::render_flame_card`, a
procedural fire wall drawn as one fullscreen triangle, so the scenario exists
only to give the runner a cheap pass to hang the card shots on.

Settlements come in four scales -- camp, hamlet, village, town -- and a walled
one adds a rampart with corner and mid-wall towers, a gate at each end of its
main street, watch fires inside the gates, a statue on the forum, and, with
`acropolis`, a temple raised on its own mound behind the town with statues
flanking the approach. The rampart is derived from how far the plots actually
reach, so it always encloses the settlement with the houses standing _against_
the wall rather than inside it -- which is its own contract
(`NoBuildingIsBuiltIntoAWall`), because the first walled version grew the
neighbouring hamlet's houses straight out of the fort's palisade.

Settlements are **laid out, not hand-placed**. `add_settlement` derives every
building position from the settlement's own street grid, so a house cannot land
on a road, in the river, or on top of its neighbour. That is not a tidiness
concern: the first valley had the bridge approach running straight through a
house, and a column ordered across the river then had no legal ground to walk
on, which on screen looked exactly like troops walking through walls.
`tests/tools/settlement_layout_test.cpp` holds those four geometric contracts
over the whole trailer catalog.

The arena also has to **register spawned buildings with
`BuildingCollisionRegistry`** — it did not before, so no arena scenario has ever
had buildings in its navigation grid.

Two catalog contracts catch a hand-built fort, and both caught this one:

- **A group's `origin` is the row's centre, not its first member.** A run is
  laid out as `origin + spacing * (index - (count - 1) / 2)`, so passing the
  west end of a wall as the origin builds it centred on that point and half of
  it ends up outside the fort. `add_rampart` in the catalog takes `first`/`last`
  and derives the centre; copy that shape rather than inventing another.
- **Wall segments and gates must land on the 2 m navigation lattice.**
  `WallGroupsSitOnTheWallNetworkLattice` fails any member whose world position
  is not an even integer, which means the fort's half-extent, its centre and its
  gate clearance all have to be even, and a wall run either side of a gate has
  to be long enough that its own centre stays on the lattice too.

A settlement that spawns `settlement_resident` civilians must also assert
`MovementAnimationObserved` on one of those groups, or
`InhabitedSettlementsProveTheirDailyLife` fails it: a town that quietly stops
living should be a test failure rather than something someone notices in a
screenshot.

**That assertion needs `collect_animation_diagnostics`.** Every
`*AnimationObserved` expectation is decided from per-soldier visual states that
`CombatAnimationDiagnostics` records, and a scenario that sets
`collect_animation_diagnostics = false` -- which capture scenes routinely do,
for speed -- switches that recorder off. The expectation is then unpassable no
matter what the residents do on screen, and the failure reads as "never produced
a visible movement animation" rather than as a disabled recorder. The trailer's
dawn and muster chapters turn diagnostics back on for exactly this reason; it
costs CPU but changes no pixels, so footage captured either way is identical.

Otherwise these chapters are staged for filming rather than for acceptance, and
their expectations are correspondingly loose -- `GroupExists` and the gate and zone
contracts, not the full visual-stability battery. What they are strict about is
lasting long enough to be filmed: the clash and night chapters set explicit
`health_override` values so the melee is still running when the camera arrives.
See [docs/PROMO_CAPTURE.md](../../docs/PROMO_CAPTURE.md) for the shot list, the
score, and the staging rules these scenes were built from.

## The imperial capital

`promo_imperial_capital` is the arena's city-scale stage: one enormous fortified
capital laid out in rings on a **768 m** map, built to be filmed rather than
tested.

```bash
build/bin/arena_app --scenario promo_imperial_capital
build/bin/arena_app --batch --scenario promo_imperial_capital --duration 20 \
  --clean-capture --artifact-dir artifacts/capital
build/bin/arena_app --promo-spec tools/arena/promos/imperial_capital.json \
  --promo-out artifacts/promo
python3 scripts/promo-edit.py --spec tools/arena/promos/imperial_capital.json \
  --clips artifacts/promo/imperial_capital
```

Four things about this scene cost a render each to discover.

**The arena world was 128 m square and nothing said so.** `k_terrain_width` was a
file-local constant, so a scenario that authored content past ±63.5 m simply put
it off the map: the first cut of this city rendered as a sliver against the
boundary mountains and the rest fell into the void. `ArenaScenarioDefinition`
now carries `terrain_grid_extent`; the viewport applies it, `reset_arena`
restores 128, and `NothingIsAuthoredOutsideTheMap` in
`tests/tools/settlement_layout_test.cpp` fails any reviewed scenario that
authors a group, prop, road, bridge or elevation patch outside its own map.

**Three services follow the grid and two of them were being left behind.**
`VisibilityService` and the camera's map bounds are sized from the terrain, so a
scenario that grew the grid rendered the new ground as unexplored fog and had
its camera yanked back inside the old 128 m box. `regenerate_terrain` now
re-initialises visibility and re-syncs the camera bounds alongside `NavGrid`.

**The camera's far plane is 200 m by default.** Maps author their own
(`camera.far` in the map JSON); arena scenarios had nobody to author one, which
did not matter while the world was 128 m across and clipped the entire city to
sky the moment it was not. `apply_scenario_camera_projection` derives the far
plane from the scenario's own world span.

**A hand-placed city cannot satisfy the layout contracts at this size.** The
first attempt authored insulae as explicit rows and produced fifty overlap and
road-clearance failures. The districts are generated instead: `CityPlanner`
holds every footprint already placed — walls, gates, towers, monuments — and
rejects a plot that hits a road, a river, a lake, the bridge approach or a
neighbour, so `fill_district` can carpet a region with a jittered, rotated plot
grid and let the streets carve themselves out. Author a district as a region
with a character (pitch, rotation, density, accent building), not as
coordinates.

The rings, from the outside in:

- countryside — farmland, orchards, two lakes, a quarry, a ruined old town, and
  suburb districts strung along the highways;
- the great wall — an eight-gated circuit with towers at ~44 m intervals, gate
  towers filling the two cells either side of every gate, and bridgehead towers
  on both crossings;
- the new city — insulae, a circus and a theatre laid out as building rings, the
  docks, and the barracks quarter with its garrison drawn up;
- the old Republican wall — the forum, the basilica and curia, the ancient core;
- the citadel — its own wall on the summit of a 96 m sacred mountain, temples,
  the oracle, the treasury, the magic shrine, and the healers who keep them.

Ten single-soldier patrols walk authored routes through the streets, and the
legion crosses the bridge and marches the sacred way for the whole run.

**Six things the first cut got wrong, all visible only in a render.**

- _A lake takes its surface height from the terrain under its centre._ Put one on
  an elevation patch and `add_lakes` carves the bed at that raised level while the
  ground around it drops away - the lake comes out as a cylinder floating over the
  countryside. Lakes belong on flat ground, clear of every patch.
- _Buildings authored outside `arena_floor_half_extent` stand on procedural
  terrain_, which at this scale is a hillside. Keep every authored thing inside
  the flat floor unless you mean it to be on a slope.
- _`ArenaScenarioResourcePatch` lays its members along one spacing vector_, so a
  patch of 18 pine trees is a straight line of 18 pine trees. `grove()` emits one
  single-count patch per tree at a hashed polar offset with size variation; use it
  for anything natural.
- _A hill built from concentric circular patches is a perfect cone._ The patch
  profile now takes a `plateau` radius (constant height inside it, smoothstep
  outside), and the sacred mountain is one plateau patch plus eight offset lobes -
  irregular silhouette, a genuinely flat 150 m summit for the citadel, and the
  south sector deliberately left lobe-free so the sacred way climbs a natural
  shoulder. Nothing is authored on the slopes.
- _The layout contracts measure axis-aligned footprints_, so a house turned 40
  degrees passes them and still visibly clips its neighbour. `CityPlanner` expands
  each footprint to its rotated AABB before testing.
- _A city with no work in it reads as a model._ The scene runs harvest crews on
  repeating cycles at authored groves, boulder fields and ore seams, carrier
  parties on `DeliverToStructure`, AI builder gangs, repair crews on two
  deliberately damaged buildings, and `SetFarmGrowth` on every farm so the fields
  carry crops.

**The river is drawn much wider than the navigation grid blocked it.** This is the
real reason soldiers waded at the crossing, and it was a game-wide defect, not a
scenario one. `add_river_segments` stamped `TerrainType::River` - the only thing
`is_walkable` rejected - out to `width * 0.5`, while the ribbon in
`render/ground/linear_feature_geometry.cpp` draws the water out to
`width_scale * (1 + width_variation)` **plus** a meander of `0.145 * width`, i.e.
`0.81 * width` against the reserved budget. On the capital's 26 m river the
unwalkable band was +-12.5 m and the drawn water reached +-21 m, leaving an 8.5 m
strip of walkable ground under open water on each bank; a 192x192 probe counted
2,708 such cells. `fit_bridge_span_to_riverbanks` had the same hole - it reserved
only the `k_river_drawn_edge_scale` half and capped the landing at 2.4 m, so the
deck stopped 1.8 m short of the drawn waterline and the last step off the bridge
landed in the river.

The fix puts one number behind all of it. `river_drawn_half_width()` in
`game/map/terrain.h` folds both the edge scale and the meander reach into the
reserved budget the renderer already static-asserts against, and
`river_bank_standing_half_width()` adds `k_water_bank_clearance` (0.6 m, a little
under an infantry radius) so a unit standing at the boundary does not overhang the
water. Two masks carry it: `m_water_blocked` marks that band unwalkable **without**
touching terrain type or the carved bed, so no shipped map's silhouette or ground
material moves; `m_bridge_walkable` marks the deck inset by the same clearance, so
nobody stands on the parapet either. Both are rebuilt in `restore_from_data`, so
saves agree with fresh loads. `tests/map/river_bank_walkability_test.cpp` pins all
three properties.

**A bridge also has to be wider than the column's drawn frontage, not its path.**
Separately from the above, a group of `n` units at `spacing` metres draws soldiers
across the whole frontage, so a 4-unit column at 9 m spacing puts ranks 13 m either
side of the axis and they render outside a 14 m deck. Size the deck against
`count * spacing` plus the per-unit block width, or narrow the column.

**Gate towers have to stand clear of the gate, not in it.** `add_wall_side` used to drop
a flanking tower at `gate +- 2 m`. Once gates went to 1.5x scale the passage became 5.7 m
wide, so those towers stood inside the archway. Gates now claim `+- 4 m` of the wall
lattice so segments clear the bigger model, and run towers keep `k_gate_tower_exclusion`
(12 m) away from every gate.

**A builder only plays the construction animation while something is actually being
built.** `is_constructing` comes from `BuilderComponent::in_progress` or a labouring
`SettlementResidentComponent`, nothing else - an `ai_controlled` builder crew standing on
a plot just mills around. Give each crew a named structure, damage it with `SetHealth`,
and re-issue `RepairStructure` on a cadence shorter than the repair takes; the capital
runs seven sites on a 14 s loop for the whole 190 s.

**Carriers do get a load, but it is a rigid prop.** `DeliverToStructure` leaves a
civilian with cargo, so the `capital_carriers_*` groups do render one. Carrying never
reaches the animation layer though - there is no `is_carrying` input - so the arms play a
plain walk and nothing grips the load. Its offsets are also not in the rig's space: one
body height is about 1.105 load units, so author against a measured close-up rather than
against `bind_socket_transform`.

**The camera moves are the difference between a reel and a turntable.** The first
cut orbited every shot at a constant rate with `"ease": "linear"`, which reads as a
game camera, not a lens. Give each shot **one** dominant axis - a push-in
(distance), a crane (height plus a few degrees of pitch), a lateral track (a small
yaw change at a large radius), or the one big pull-back for the reveal - keep yaw
under about 20 degrees except on that reveal, use three keys so the move
accelerates and settles, and set `"ease": "smooth"` on every key after the first.

**It renders well below realtime and that is deliberate.** Roughly 2,200 building
entities at full creature LOD on a 768 m terrain put frame work well past the
30 ms mark, so the scenario
declares no `FrameBudget` expectation - it is a filming stage, not a performance
fixture. Record it in chunks of two or three shots rather than one long pass;
see the crash note in the ambience section above.

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
  settlements with finite stockpiles plus authored olive trees, stone, and iron.
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

The next five are the pinch points a grid pathfinder is usually caught out by.
Every one of them turns on the same rule: the navigation grid is the only
authority on where a unit may stand, and a cell it calls open is open to every
unit regardless of how large that unit is drawn.

- `path_narrow_gap_column` orders twelve swordsmen through the single opening in
  a palisade. They have to queue and every one of them has to come out the far
  side -- not shoulder each other into the wall, and not stall because the
  doorway is narrower than the formation.
- `path_building_alley` puts a slot between two barracks. A pathfinder that
  reserves room for a unit's radius calls it impassable and walks the long way
  round; this one threads it.
- `path_diagonal_wall_seal` lays a palisade corner to corner across the whole
  field. Every pair of open cells across it meets at a corner with a blocked
  cell on each flank, so the wall holds only for as long as nothing is willing
  to squeeze between two corners. The probers must still be on their own side
  when the scene ends.
- `path_bridge_column` funnels a line wider than the deck onto one narrow
  bridge and requires the whole column across.
- `path_hill_entrance_column` gives a hill one cut ramp and requires measured
  elevation gain and crown arrival, so the column has to find the ramp rather
  than scale the flank.

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
