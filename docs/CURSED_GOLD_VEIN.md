# Cursed Gold Vein

The cursed gold vein is a capturable world element built around a deliberate trade-off. Visually, it is a crag of dark rock split by an ore seam, with gold crystals growing from the fracture and a claim flag planted beside it. Mechanically, it rewards ownership with a steady income while damaging the owner's nearby troops on the same cadence.

Neutral, the vein is scenery and an opportunity. Captured, it becomes an economic asset with a real military cost. The player must decide how much of an army they are willing to expose in exchange for reliable gold.

## Authoring a vein

A cursed gold vein is authored as an ordinary world prop:

```json
{
  "type": "cursed_gold_vein",
  "x": 262,
  "z": 512,
  "scale": 1.0,
  "rotation": 1.2
}
```

`x` and `z` use the same grid coordinates as other props. The vein has a solid ground body; see `world_prop_model_half_extents` in `game/map/map_definition.h`. Scatter placement also treats it as a hard obstacle, keeping grass and trees clear of the crag.

The map editor exposes the prop as **Cursed Gold Vein** in the props group, and the Arena prop panel can place it as well.

### Automatic placement on shipped maps

`scripts/place-cursed-gold-veins.py` distributes veins across shipped maps. The tutorial is intentionally excluded because its scripted stages should not acquire an unscripted neutral capture point.

The placement pass is deterministic and idempotent: it removes existing veins before placing the computed set again. Candidate sites favor contested ground that is:

- open and flat;
- clear of water, roads, bridges, and hills;
- away from camps, settlements, spawns, props, and undead zones; and
- roughly comparable in distance from two players' bases.

The quota scales with map size, from one vein on a 48-cell scenario map to five on Zama.

`CursedGoldVeinSystemTest.EveryShippedVeinStandsOnClearGround` runs the engine's own clearance check, `is_undead_shrine_site_clear`, against every shipped site. A manually moved vein that ends up in a river or another invalid location therefore fails the test suite.

## Runtime behavior

`Game::Systems::CursedGoldVeinSystem`, executed during the `Strategy` phase, owns the gameplay behavior.

The implementation follows the magic shrine pattern. The terrain-scatter pass draws the physical prop, while the capturable component is represented by a real `Barracks` entity created on top of the prop at level start.

The anchor begins neutral under `NEUTRAL_OWNER_ID`, has no production line, and receives `Game::Visuals::k_cursed_gold_vein_flag_asset_key`. That renderable causes `render/entity/cursed_gold_vein_flag_renderer.cpp` to draw only the claim flag rather than a nation's barracks model.

Because the anchor is a normal capturable entity, `CaptureSystem` supplies the capture behavior automatically. The flag lowers and changes colour during capture, and a neutral vein does not require troop superiority: one soldier remaining beside it for the capture duration is enough to claim it.

## The gold-and-curse cycle

While a player owns the vein, the system fires once every `k_cursed_gold_vein_tick_seconds`—currently 6 seconds.

Each tick has two effects.

### Income

`k_cursed_gold_vein_gold_per_tick`, currently 25 gold, is added through `PlayerResourceRegistry::add`.

The system deliberately does not call `add_harvested`, so cursed-gold income does not count toward resource-harvest objectives. The flag also shows the floating `+gold` feedback used to communicate the payout.

### Casualties

Every troop belonging to the owner within `k_cursed_gold_vein_curse_radius`, currently 9 metres, takes `k_cursed_gold_vein_curse_damage`, currently 10 damage, through `Combat::deal_damage`.

Health represents manpower, so the curse produces a steady trickle of casualties rather than a separate status effect. Buildings, wildlife, and other players' troops are unaffected.

The query is bounded by the world spatial index. The system searches only the relevant radius and never scans the entire world for victims.

## Ownership changes and destruction

Changing ownership resets the tick clock, preventing a newly captured vein from inheriting a partially elapsed payout interval.

Capture normally gives a barracks a production line. The cursed-gold system removes that line every frame so the anchor can never train units.

If the anchor is razed and reaches zero health, the vein becomes inert for the remainder of the match. Its income stops—the gold is effectively buried again—and its minimap marker changes to the destroyed state.

## Save and load behavior

Persistent state is stored under `cursed_gold_veins` by `SaveLoadCoordinator`. The saved data includes:

- `anchor_entity_id`;
- current owner;
- tick clock; and
- number of ticks paid.

The state is restored after `configure()`, ensuring that loading a save reconnects the existing anchor instead of spawning a second one.

## Presentation

The vein's presentation reinforces the same risk-reward idea as its mechanics.

### Mesh

`render/gl/backend/cursed_gold_vein_mesh.cpp` builds the mesh through the shared `rock_outcrop_mesh` helpers also used by iron ore. `VegetationPipeline::initialize_cursed_gold_vein_pipeline` uploads it.

The rock is constructed from jittered, lofted masses rather than stacked frustums. Gold shards are irregular prisms whose facets run along their full length. Rock geometry stays below `k_cursed_gold_vein_rock_crown` (`0.46`); geometry above that height is crystal.

`prop_model_footprint_test` checks the generated mesh against the declared half-extents, while `rock_prop_mesh_test` prevents the crown constant in the header from drifting away from the value used by the shader.

### Shader

`assets/shaders/cursed_gold_vein_instanced.{vert,frag}` combines dark, rusted rock with an FBM ore seam, height-keyed gold, and metallic specular response. A slow blood-red pulse travels along the seam through `u_magic_strength`.

The visual target is warm but uneasy, deliberately contrasting with the cooler, steadier presentation of the magic shrine.

### Light

`CursedGoldVeinRenderer` emits one flickering warm `LocalLight` for each visible vein. It is lower and dimmer than the shrine's votive light.

### Minimap

The minimap uses landmark kind `gold_vein` with four states:

| State       | Presentation |
| ----------- | ------------ |
| `neutral`   | gold         |
| `owned`     | success      |
| `enemy`     | danger       |
| `destroyed` | disabled     |

## Tuning the risk and reward

All four gameplay values live at the top of `game/systems/cursed_gold_vein_system.h`. Tests assert against those constants rather than duplicating literal values, so balance changes do not require test rewrites.

At the current values, 25 gold every 6 seconds produces 250 gold per minute. A single 100-manpower guard squad left inside the curse radius also dies in roughly a minute.

That tension is intentional. The cursed gold vein should be valuable enough to contest, but expensive enough that holding it thoughtlessly is not free income.
