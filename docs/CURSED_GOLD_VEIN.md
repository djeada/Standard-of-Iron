# Cursed Gold Vein

A capturable world element: a crag of dark rock split by a seam of ore, with gold
crystals growing out of the crack and a claim flag planted beside it. Neutral, it
is scenery. Held, it pays its owner gold on a fixed cadence - and on the same
cadence bleeds every one of the owner's troops standing near it. The gold is real
and the curse is real; whoever holds the vein has to decide how much of an army
they are willing to feed to it.

## Authoring

A vein is an ordinary world prop:

```json
{
    "type": "cursed_gold_vein",
    "x": 262,
    "z": 512,
    "scale": 1.0,
    "rotation": 1.2
}
```

`x`/`z` are grid coordinates like every other prop. The prop is solid (it has a
ground body, see `world_prop_model_half_extents` in `game/map/map_definition.h`) and
scatter treats it as a hard obstacle, so grass and trees keep clear of the crag.

`scripts/place-cursed-gold-veins.py` lays veins across every shipped map (the
tutorial is skipped: its scripted stages should not gain a neutral capture point).
It is deterministic and idempotent - it strips existing veins and re-places them -
and it aims for contested ground: open flat land, clear of water, roads, bridges,
hills, camps, settlements, spawns, props and undead zones, at about the same
distance from two players' bases. Quota scales with map size (1 on a 48-cell
scenario map, 5 on Zama). `CursedGoldVeinSystemTest.EveryShippedVeinStandsOnClearGround`
runs the engine's own clearance check (`is_undead_shrine_site_clear`) over every
shipped site, so a hand-moved vein that lands in a river fails the suite.

The map editor (`Cursed Gold Vein` tool in the props group) and the arena prop
panel both place it.

## Runtime

`Game::Systems::CursedGoldVeinSystem` (`Strategy` phase) owns the behaviour. It
follows the magic shrine's pattern exactly: the prop is drawn by the terrain
scatter pass, and the _capturable_ part is a real `Barracks` entity raised on the
prop at level start. That entity is neutral (`NEUTRAL_OWNER_ID`), has no
production line, and its renderable is stamped with
`Game::Visuals::k_cursed_gold_vein_flag_asset_key` so it draws only the claim flag
(`render/entity/cursed_gold_vein_flag_renderer.cpp`) instead of a nation's
barracks model. Everything about capture comes for free from `CaptureSystem`: the
flag lowers and recolours while a capture is in progress, and a neutral vein needs
no troop advantage - one soldier standing beside it for the capture time takes it.

Each tick (`k_cursed_gold_vein_tick_seconds`, 6 s) while a player holds the vein:

- `k_cursed_gold_vein_gold_per_tick` (25) gold is added to the owner through
  `PlayerResourceRegistry::add` (not `add_harvested`, so it does not count toward
  harvest objectives), with the floating "+gold" feedback on the flag.
- every troop of the owner within `k_cursed_gold_vein_curse_radius` (9 m) takes
  `k_cursed_gold_vein_curse_damage` (10) through `Combat::deal_damage`. Health is
  manpower, so this is a steady trickle of casualties. Buildings, wildlife and
  other players' troops are untouched. The query is radius-bounded through the
  world spatial index; the system never scans the whole world.

Ownership changes reset the tick clock. Capture hands a barracks a production
line; the system strips it every frame, so the vein never trains anything. If the
anchor is razed (health reaches zero) the vein goes inert for the rest of the
match - the gold is buried again - and its minimap marker greys out.

State (`anchor_entity_id`, owner, tick clock, ticks paid) is saved under
`cursed_gold_veins` by `SaveLoadCoordinator` and restored after `configure()`, so a
load does not raise a second anchor.

## Presentation

- Mesh: `render/gl/backend/cursed_gold_vein_parts.h`, built by
  `VegetationPipeline::initialize_cursed_gold_vein_pipeline`. Rock parts stay
  below `y = 0.70`; everything above is crystal. `prop_model_footprint_test`
  keeps the declared half-extents honest against the parts.
- Shader: `assets/shaders/cursed_gold_vein_instanced.{vert,frag}`. Dark rusted
  rock, an fbm ore seam threaded through it, gold keyed on local height with a
  metallic specular, and a slow blood-red pulse crawling along the seam driven by
  `u_magic_strength` - warm and uneasy where the shrine is cool and steady.
- Light: `CursedGoldVeinRenderer` emits one flickering warm `LocalLight` per
  visible vein, lower and dimmer than the shrine's votive light.
- Minimap: landmark kind `gold_vein`, state `neutral` / `owned` / `enemy` /
  `destroyed`, tinted gold, success, danger and disabled respectively.

## Tuning

All four numbers live at the top of `game/systems/cursed_gold_vein_system.h`.
The tests assert against the constants, not literals, so retuning does not
require touching them. A sensible balance envelope: at 25 gold / 6 s a vein is
worth 250 gold a minute, and a single guard squad of 100 manpower parked on it
is dead in a minute - which is the point.
