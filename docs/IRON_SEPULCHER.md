# The Iron Sepulcher

The Iron Sepulcher is the undead faction. It has no economy, no recruitment, and
no player-selectable slot. Its troops reach the battlefield one way only: an
**undead zone** on the map wakes up and raises them.

Everything below is driven by `undead_zones` in a map file and implemented by
`Game::Systems::UndeadAwakeningSystem`.


## Zone schema

```json
"undead_zones": [
  {
    "id": "shrine_sentinels",
    "anchor_type": "magic_shrine",
    "x": 33, "z": 16,
    "radius": 7.0,
    "leash_radius": 12.0,
    "owner_id": 99,
    "team_id": 99,
    "awaken_on": ["unit_enters_radius"],
    "anchor_structure": true,
    "fog_density": 0.28,
    "waves": [
      { "trigger": "initial", "units": { "skeleton_swordsman": 2 } },
      { "trigger": "after_clear", "units": { "skeleton_archer": 3 } }
    ]
  }
]
```

| Field | Meaning |
| --- | --- |
| `awaken_on` | `unit_enters_radius` (default) or `mission_start`. |
| `waves` | Optional. Omit it to get the default garrison (see below). |
| `anchor_structure` | Optional override. Defaults to true for `magic_shrine`, false for every other anchor type. |
| `fog_density` | Optional. `0` disables the zone haze. |

`owner_id` is a real owner: it is registered as an AI owner of nation
`iron_sepulcher`, which resolves to the `sepulcher_defense` AI profile.


## The default garrison

A zone that declares no `waves` raises **2 skeleton swordsmen, 1 skeleton
archer, and 1 grave priest** in a single wave. A map that declares `waves`
overrides that completely — the default is never merged in.

The whole wave rises on one tick. Guardians are placed on a golden-angle
sunflower spiral filling the zone radius, so they appear spread around the
anchor rather than stacked on it, and impassable ground is skipped.


## Shrines are capturable barracks

A `magic_shrine` anchor is not just scenery. When the zone is configured, the
system spawns a real barracks entity at the shrine:

- owned by the zone owner, nation `iron_sepulcher`;
- **no `ProductionComponent`** — it cannot recruit anything, it exists so the
  player can capture or raze it;
- the shrine keeps its existing mesh. The building is drawn by
  `troops/iron_sepulcher/barracks`, which submits no geometry, while the shrine
  itself is still rendered by the terrain scatter pass from its world prop.
  Selection rings and health bars come from the shared entity path.

When that building is destroyed **or** changes owner, the zone's garrison breaks:
every living guardian dies immediately, no further waves spawn, and the zone
reports itself cleared and — for a shrine — purified. That is what routes the
outcome back into the normal victory system: a mission or map using
`clear_undead_zone` or `purify_shrine` wins on it without any special casing.

Ruins and other anchor types stay decorative; set `"anchor_structure": true` to
force a structural anchor, or `false` to suppress one on a shrine.


## Zone haze

Each zone with a positive `fog_density` contributes a light, semi-transparent
`FogZone` sized to the zone radius, rendered CPU-side by `AmbientFogRenderer`.
Fog patches carry a ground height, so whoever owns the terrain (the skirmish
loader, or the Arena) lifts each zone onto the surface before handing it to the
renderer — otherwise the patches sink under raised ground.


## Announcements

The system publishes `Engine::Core::MissionAnnouncementEvent`, which `GameEngine`
forwards to the existing mission-announcement toast. Each zone announces at most
once per event:

- when it wakes;
- when its garrison is put down, either by being fought to the last guardian or
  by losing its shrine.


## Save/load

`serialize_state()` / `restore_state()` carry the awakened flag, the broken-
garrison flag, the anchor entity id, wave progress, and the live spawn ids. A
restored save never re-spawns an active wave and never stamps out a second
shrine.


## Testing

- `tests/systems/undead_awakening_system_test.cpp` — waves, defaults, spread,
  the shrine barracks, garrison break, announcements, save/load.
- `tests/map/iron_sepulcher_skirmish_test.cpp` — end-to-end through the real
  skirmish loader: load the map, wake the zone, win by purifying, capture, raze.
- Arena: eight `sepulcher_*` scenarios, see `tools/arena/README.md`.
