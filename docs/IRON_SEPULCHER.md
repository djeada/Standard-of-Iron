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
    "fog_density": 0.16,
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
| `radius`       | Awaken radius and the ring guardians hold their posts on                                   |
| `leash_radius` | How far from the anchor a guardian may fight; defaults to `max(radius, 14)`                |
| `wave_timeout` | Seconds before a `next_wave` wave rises even if the previous wave still stands             |
| `wave_delay`   | Seconds between a wave falling and the next one rising; defaults to 1.5                    |
| `fog_density`  | Optional zone haze; `0` disables it                                                        |
| `clear_reward` | Optional one-time resources granted when the garrison is broken                            |

Wave triggers:

- `initial` (also `awaken`, `mission_start`) — the first wave, raised the moment the zone wakes;
- `after_clear` (also `on_clear`) — rises `wave_delay` seconds (1.5 by default) after every guardian of the previous wave is dead, and never on a timer; and
- `next_wave` (also `after_timeout`, `timed`) — rises when the previous wave dies **or** after `wave_timeout` seconds, whichever comes first. A `wave_timeout` of `0` disables the timer.

The default `wave_timeout` is 45 s, but it only ever applies to `next_wave` waves. An `after_clear` wave with a `wave_timeout` authored on the zone still waits for the kill.

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

## Guardians defend; they never march

The Iron Sepulcher has no army to send anywhere. Every guardian is stationed with a `GuardModeComponent` the moment it rises:

- its **post** is a point on a ring around the anchor at half the zone radius, spread evenly between the living guardians of the zone;
- its **reach** is a circle of `leash_radius` centred on the anchor, not on the post (`GuardModeComponent::has_reach_center`), so every guardian covers the same ground whichever side of the shrine it stands on; and
- guard mode removes the unit from the AI snapshot, so `sepulcher_defense` never orders it to muster, gather, or attack.

Inside the leash the guardians fight normally: they acquire targets on their own, answer threat alerts from their neighbours, and chase. Two reach rules apply, both answered by `Combat::within_guard_reach`:

- a guardian _picks_ a fight only with an enemy inside the leash (`GuardReachRule::Strict`); and
- a guardian _answers_ an attacker — its own attacker, or a neighbour's through a threat alert — out to the leash plus that attacker's weapon range, capped at 12 m (`GuardReachRule::AnswersFire`). An archer that can hit the garrison from where it stands is close enough to be charged; one that cannot is left alone.

The combat system drops any target that leaves the answering reach and walks the guardian back to its post through `Combat::send_guard_home`. The shrine anchor is not a wall: a guardian behind the shrine walks round it to reach a fight on the far side, because `Combat::melee_walled_off_from` only treats a structure as separating two combatants when the walked detour around it is long. Every 0.5 s the awakening system also re-asserts the posts, recalls any guardian whose prey has left the answering reach or who has strayed past `leash_radius` plus that prey's weapon range (clearing its target and melee lock), and turns idle guardians at their posts to face outward.

Before September 2026 guardians on the far side of a stormed shrine stood idle for the whole fight (40 s in `DefenderEngagementTest.GuardiansBehindTheShrineJoinAFightOnItsFarSide`): the anchor's footprint made every enemy "walled off", reach was measured from each post, and a guardian whose walk home ended short kept `returning_to_guard_position` set and refused every fight, retaliation included. The flag now counts only while the guardian is actually moving (`Combat::is_returning_to_guard_post`). The post ring drifts at 4°/s, so an undisturbed garrison slowly walks the ring around its shrine instead of standing frozen on the spawn spiral. All of this is deterministic: posts come from the guardian's index in the zone and a per-zone phase, never from wall-clock randomness.

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

The building renderer `troops/iron_sepulcher/barracks` submits no visible barracks geometry. The shrine itself remains a terrain-scatter prop, so the physical shrine and the capturable entity occupy the same site without drawing two structures. The HUD names the anchor "Sepulcher Shrine" rather than "Barracks".

While any guardian of the zone lives, the anchor's `CaptureComponent::capture_blocked` is set and the anchor is **warded**: `Combat::evaluate_target` refuses it with `TargetRefusal::Warded` for ordered and auto-acquired attacks alike. Player attack clicks on a warded shrine fall through to an attack-move at that ground, so the troops engage the guardians instead of standing in front of an invisible building; AI target lists, auto-acquisition, and held attack targets drop it the same way. Once the last guardian of the last wave falls the ward and the capture lock both lift, and the anchor is an ordinary capturable, destructible barracks again.

If no valid shrine location can be found—for example, because a zone was authored entirely inside a lake—the system logs the failure, creates no barracks, and exposes the zone through `zones_without_shrine()`.

`content_validator` runs the same placement logic against every shipped map and fails when a zone cannot receive a shrine.

## Breaking the garrison

The garrison breaks when the shrine building is destroyed or changes owner.

At that point:

- every living guardian dies immediately;
- no further waves can spawn;
- the zone reports itself cleared;
- the site reports itself purified; and
- `clear_reward` is paid once.

Killing every wave is the other way to clear a zone: `is_zone_cleared` is true once the last wave is dead. That does **not** purify the shrine. Purification needs the anchor itself to fall or change hands, so after the last guardian drops the player still has to take the shrine: any troop standing within the capture radius of the unguarded anchor captures it over the normal capture time, `refresh_anchor_structure` sees the new owner, and `break_garrison` runs with `captured = true` — paying the reward to the captor and purifying the site. The "guardians are put down" announcement tells the player to hold the shrine.

This feeds directly into the normal victory system. Objectives such as `clear_undead_zone` and `purify_shrine` need no special-case mission logic beyond the state published by the undead system.

Other `anchor_type` values remain decorative. They influence where guardians appear, but they do not replace the mandatory shrine.

## Zone haze

A zone with positive `fog_density` contributes a light, translucent `FogZone` sized to its radius and rendered on the CPU by `AmbientFogRenderer`.

Fog patches include a ground height. The map owner—such as the skirmish loader or Arena—projects the patch onto the terrain before sending it to the renderer so the haze follows raised ground instead of sinking beneath it.

## Announcements

`UndeadAwakeningSystem` publishes `Engine::Core::MissionAnnouncementEvent`, which `GameEngine` forwards to the existing mission-announcement toast.

Each zone announces each event at most once:

- when the zone awakens;
- when every later wave rises; and
- when its garrison is broken, either by killing the guardians or by losing the shrine.

`GameEngine` spaces mission announcements 4.5 s apart. The toast host merges any two announcements that land on its channel within its 4 s dwell into one toast with a "×2" suffix and only the newer text, so an unspaced pair (two zones waking together, or a wave rising on top of a commander line) used to lose a message. Queued announcements are shown in order; an identical text already waiting is not queued twice.

When a wave falls and another is still to come, the zone publishes `Engine::Core::UndeadZonePhaseEvent` with phase `Stirring` and the seconds until the next rising. A `wave_delay` of 5 s or more is long enough to be a breather rather than a stutter, so it also gets its own toast ("The ground is moving under the dead. More are coming up."). When the last guardian of the last wave falls, the same event is published with phase `Cleared`, next to the "hold the shrine" toast.

Mission commander lines can answer all three moments: `undead_awakened`, `undead_stirring` and `undead_cleared` triggers take a `zone_id` (or `kind`) that names the zone, so a line can be written for one zone and stay silent for another.

The number of squads in each rising follows the player's difficulty preset; see `docs/DIFFICULTY.md`.

Awakening also publishes `Engine::Core::UndeadZoneAwakenedEvent` with the anchor position. `GameEngine` turns it into a `MinimapAlert::ShrineStirred` ping so a player looking elsewhere gets a spatial cue as well as the toast and the sound.

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
