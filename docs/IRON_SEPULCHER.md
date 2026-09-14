# The Iron Sepulcher

The Iron Sepulcher is Standard of Iron's undead faction. It has no economy, recruitment loop, or player-selectable slot. Undead troops reach the battlefield through **undead zones** authored on the map: dormant sites that awaken, raise a garrison, and turn part of the terrain into a temporary strategic problem.

The system is data-driven through `undead_zones` in map files and implemented by `Game::Systems::UndeadAwakeningSystem`.

## Undead zone schema

A zone can define its trigger, guardians, haze, reward, and decorative anchor:

```json
"undead_zones": [
  {
    "id": "shrine_sentinels",
    "anchor_type": "magic_shrine",
    "x": 33,
    "z": 16,
    "radius": 7.0,
    "leash_radius": 12.0,
    "owner_id": 99,
    "team_id": 99,
    "awaken_on": ["unit_enters_radius"],
    "fog_density": 0.28,
    "clear_reward": { "gold": 150, "stone": 80 },
    "waves": [
      { "trigger": "initial", "units": { "skeleton_swordsman": 2 } },
      { "trigger": "after_clear", "units": { "skeleton_archer": 3 } }
    ]
  }
]
```

| Field          | Meaning                                                                                    |
| -------------- | ------------------------------------------------------------------------------------------ |
| `awaken_on`    | `unit_enters_radius` by default, or `mission_start`                                        |
| `waves`        | Optional wave definition; omitting it uses the default garrison                            |
| `anchor_type`  | Decorative prop around which guardians rise; it does not determine whether a shrine exists |
| `fog_density`  | Optional zone haze; `0` disables it                                                        |
| `clear_reward` | Optional one-time resources granted when the garrison is broken                            |

`owner_id` is a real owner registered as an AI player of nation `iron_sepulcher`. That nation resolves to the `sepulcher_defense` AI profile.

## Clearing a zone is a strategic choice

`clear_reward` is paid once when the garrison breaks, whether the shrine is captured or destroyed.

If the shrine is captured, the reward goes to its new owner. If it is destroyed, the reward goes to the local player. Resources are added as spendable balance rather than through the harvested-resource path, so clearing a barrow can finance the next push without accidentally satisfying an `accumulate_resources` objective.

This is the intended role of an undead zone: it is optional, dangerous ground that can be worth contesting. Every mission in the Second Punic War campaign contains at least one.

## Default and authored garrisons

A zone with no `waves` entry raises the default garrison in one wave:

- 2 skeleton swordsmen;
- 1 skeleton archer; and
- 1 grave priest.

An authored `waves` list replaces the default completely; the two are never merged.

A wave rises in a single simulation tick. Guardians are distributed on a golden-angle sunflower spiral across the zone radius so they emerge around the anchor rather than stacked at one point. Invalid or impassable positions are skipped.

## Every zone receives one shrine

During `configure()`, every undead zone is paired with exactly one magic shrine. The map does not need to author that shrine and cannot opt out of it.

`Game::Map::plan_undead_zone_shrine` in `game/map/undead_shrine_placement.h` chooses the location in three steps:

1. If a magic-shrine prop already exists inside the zone, adopt it. This preserves authored placement and allows a later `configure()`—for example after reload or Arena restart—to find the same shrine instead of creating another.
2. Otherwise, use the zone centre if it is clear of water, bridges, roads, buildings, other props, and shrines already assigned to other zones.
3. If the centre is blocked, search outward in rings for the nearest valid point. A zone centered on ruins can therefore place its shrine beside them rather than inside them.

A newly created shrine is inserted into the terrain service as a real world prop, which makes it part of both rendering and save/load state.

## The shrine is the capturable anchor

At the shrine, the system creates a `Barracks` entity that acts as the gameplay anchor for capture, destruction, health, navigation, and ownership.

The anchor:

- belongs to the zone owner and nation `iron_sepulcher`;
- intentionally has **no `ProductionComponent`**, so it can never recruit; and
- uses the shared building path for collision, navigation, selection, health bars, lighting, and shadows.

The building renderer `troops/iron_sepulcher/barracks` submits no visible barracks geometry. The shrine itself remains a terrain-scatter prop, so the physical shrine and the capturable entity occupy the same site without drawing two structures.

If no valid shrine location can be found—for example, because a zone was authored entirely inside a lake—the system logs the failure, creates no barracks, and exposes the zone through `zones_without_shrine()`.

`content_validator` runs the same placement logic against every shipped map and fails when a zone cannot receive a shrine.

## Breaking the garrison

The garrison breaks when the shrine building is destroyed or changes owner.

At that point:

- every living guardian dies immediately;
- no further waves can spawn;
- the zone reports itself cleared; and
- for shrine-backed objectives, the site also reports itself purified.

This feeds directly into the normal victory system. Objectives such as `clear_undead_zone` and `purify_shrine` need no special-case mission logic beyond the state published by the undead system.

Other `anchor_type` values remain decorative. They influence where guardians appear, but they do not replace the mandatory shrine.

## Zone haze

A zone with positive `fog_density` contributes a light, translucent `FogZone` sized to its radius and rendered on the CPU by `AmbientFogRenderer`.

Fog patches include a ground height. The map owner—such as the skirmish loader or Arena—projects the patch onto the terrain before sending it to the renderer so the haze follows raised ground instead of sinking beneath it.

## Announcements

`UndeadAwakeningSystem` publishes `Engine::Core::MissionAnnouncementEvent`, which `GameEngine` forwards to the existing mission-announcement toast.

Each zone announces each event at most once:

- when the zone awakens; and
- when its garrison is broken, either by killing the guardians or by losing the shrine.

## Save and load

`serialize_state()` and `restore_state()` preserve the runtime state needed to resume an active zone:

- awakened state;
- broken-garrison state;
- anchor entity ID;
- wave progress; and
- live guardian entity IDs.

The shrine barracks itself is an ordinary entity, so its health and owner are serialized with the world.

On load, `configure()` runs again and adopts the shrine already present in the restored terrain state. `restore_state()` then reconnects the zone to its existing barracks and live guardians. A restored save therefore neither duplicates the shrine nor replays an already active wave.

## Test coverage

The system is tested at several levels:

- `tests/systems/undead_awakening_system_test.cpp` covers wave behavior, defaults, spawn spread, shrine anchors, one-shrine-per-zone rules, blocked and impossible placements, garrison break, announcements, and save/load.
- `tests/map/undead_shrine_placement_test.cpp` tests the placement algorithm directly and sweeps all shipped maps to ensure every authored zone can receive a shrine.
- `tests/map/iron_sepulcher_skirmish_test.cpp` exercises the complete skirmish-loader path, including awakening, purification victory, capture, and destruction.
- Arena provides eleven `sepulcher_*` scenarios documented in `tools/arena/README.md`.

The resulting design keeps the Iron Sepulcher deliberately outside the normal economy: undead presence comes from authored places in the world, and defeating that presence is expressed through capture, destruction, and ordinary mission objectives rather than a parallel game mode.
