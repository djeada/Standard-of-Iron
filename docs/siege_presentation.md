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

Review both nations with real movement and repeated projectile launches:

```bash
build-home/bin/arena_app --scenario siege_roman_showcase
build-home/bin/arena_app --scenario siege_carthage_showcase
```

The `SiegeMotion.*` tests cover travel, parking, pivot turns, scaled wheel
circumference, replay/teleport resets, release timing and recoil settling. The
showcase scenarios also check projectile impact synchronization.
