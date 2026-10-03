# Siege engine presentation

Roman and Carthaginian ballistas and catapults use reinforced timber carriages,
open spoked wheels with iron tyres, bronze straps and hubs, and short moving
standards. The presentation scale is 1.35x for ballistas and 1.20x for catapults,
applied after the troop profile's scale; simulation footprints and combat values
are unchanged.

`render/entity/siege_renderer_common.cpp` owns the shared wheel construction,
regalia, carriage motion and travel history. Each registered renderer keeps its
own bounded history by entity id, shared when the renderer function is copied.
Wheel angles integrate signed rendered displacement and turning, using the actual
model scale and wheel radius. Stopped wheels retain their orientation, while
carriage sway eases down. World changes, replay rewinds, long gaps and teleports
reset the history. Previews use temporary state. No simulation components are
written by these animations.

Loading reads `CatapultLoadingComponent`: the winding stroke eases at both ends
and turns the crank and rope drum. Ballista arms flex together with their nocks
and string. Firing releases the mechanism quickly, kicks the carriage backwards,
and lets the catapult arm rebound against its padded stop. Loaded ammunition
vanishes when the simulation enters Firing, when the live projectile is spawned;
the ready pose still displays it. Incendiary stones retain their existing flame
effect.

## Crews

Every catapult carries four builders and every ballista three, drawn as
presentation-only humanoids through the civilian actor path
(`render/entity/siege_crew.{h,cpp}`). They use each nation's builder look --
`register_nation_crew_rig` in each `builder_renderer.cpp` registers the
builder visual spec, idle loadout and builder palette, and
`civilian_render_scale` resolves the builder troop scale for that rig -- so
Roman engines are worked by Roman engineers and Carthaginian by Punic ones.
No simulation state exists for them: the unit is still one man in the game.

The crew has three jobs, chosen from the carriage motion and
`CatapultLoadingComponent`:

| Job  | When                                            | Stations                                                                         | Clip                                                                                                     |
| ---- | ----------------------------------------------- | -------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------- |
| Push | smoothed speed above 0.22 (released below 0.08) | behind the frame and on the rear wheels                                          | `crew_push`: arms braced forward, torso leaning in, walking legs                                         |
| Load | Loading / ReadyToFire / Firing, and 2.5 s after | at the windlass or winch, and one loader beside the throwing arm or bolt channel | `crew_crank` (hands circling a crank) and `crew_heave` (lift from the ground, press up into the machine) |
| Rest | otherwise                                       | spread around the engine                                                         | idle, idle weave, squat                                                                                  |

Stations are written in the engine's local frame: an anchor in carriage units
scaled by the presentation scale, plus a body offset in metres so hands meet
the timber. A crewman walks between stations at 0.95 m/s, turns with an
exponential ease rather than snapping, and fades the outgoing clip over
0.32 s. The push stride is phased from the distance the engine has actually
rolled (`SiegeTravelState::travelled`, 0.60 m per stride), so feet do not
slide and a stopped engine stops its crew mid-step. The crank phase follows
`loading_time`, and the heave follows `loading_progress`, so the loader
presses the stone or bolt in just before the renderer shows it seated.

`crew_push`, `crew_crank` and `crew_heave` are clips 82-84, baked by
`bpat_baker` from `HumanoidConstructionPoseKind::CrewPush/CrewCrank/CrewHeave`
in `animation/attack_pose_manifest.cpp`; the push bakes a real walk (bake
speed 1.1) and layers the braced arms over it. Review them with
`humanoid_preview --clip crew_push --view side --report`; bone stretch stays
at or below the idle clip's.

Crews are skipped beyond 110 m or below 16 px, and draw at Minimal detail
below 34 px. `SiegeCrewTest.*` covers the jobs, push stations, stride
locking, load phasing and blending.

Review both nations with real movement and repeated projectile launches:

```bash
build-home/bin/arena_app --scenario siege_roman_showcase
build-home/bin/arena_app --scenario siege_carthage_showcase
```

The `SiegeMotion.*` tests cover travel, parking, pivot turns, scaled wheel
circumference, replay/teleport resets, release timing and recoil settling. The
showcase scenarios also check projectile impact synchronization.

## Walls, ladders and siege towers

The palisade's wall walk is a timber balcony on the town face; its measurements
live in `game/core/wall_walk_geometry.h` so troops' feet meet the planks. A
company reaches it by a stair (every fourth straight segment) or by a ladder a
builder raises. A ladder is a wall piece built like a gate: the builder card
places it against the town face of one of your own straight wall segments and
nowhere else (not outside, not on a stretch with a stair or a ladder already).
Climbers go up a ladder hand over hand in the `climb_ladder` clip, whose phase
follows each man's height, two rungs a cycle, so hands and feet stay on the
rungs; they come down facing the rungs and queue a body length apart. After the
leader steps off, the troop stays in the `Leaving` phase until the rest of the
file is down.

A siege tower carries no troops of its own. Sent at an enemy wall (a move next
to it, or an attack order on the wall or gate) it rolls up between two posts,
squares up and lowers its bridge. Infantry escorting it within 14 m is then
called up; any idle infantry that later walks up behind a docked tower climbs it
too, and a docked tower stays a bridge for as long as it stands. Each man walks
round to the back, climbs the inner ladder, crosses the bridge onto the enemy
balcony and from there can go down the enemy's stairs into the town. Wet hides
turn most arrows (ranged damage ×0.3) and rams and towers only ever engage
structures.

Defenders on a wall walk shoot from height (×1.35 for archers and spearmen
instead of a hill's ×1.8) and the stakes cover them from arrows shot up from the
ground (×0.65). A computer-held town posts its idle archers onto the stretch of
balcony nearest an assault, and once there its commander leaves them as the
wall's garrison.
