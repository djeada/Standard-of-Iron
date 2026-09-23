# Marketplace

Both nations' marketplaces are open courts on a stepped plinth, entered from
the local −X side through a gateway, with a row of three shops along the
back (+X) wall. The plinth stays 2.80 local units wide so the drawn body
matches the collider (`BuildingBodyFootprint` tests).

|       | Rome (`macellum`)                                                                                                  | Carthage (souk)                                                                                                            |
| ----- | ------------------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------- |
| Shops | limestone tabernae under a lean-to tiled roof; amphora racks, a produce counter with a bronze balance, cloth bolts | sandstone shops with a flat roof and parapet, cloth valances over the doorways, spices, Punic amphorae, murex-purple cloth |
| Court | central marble fountain, two awning stalls with produce baskets                                                    | central cistern with a rope-and-bucket frame, two spice stalls, a dye corner with purple vats and a drying line            |
| Gate  | two columns and a lintel with a small aquila, team pennant                                                         | two pylons and a timber beam with a small Tanit sign, team pennants                                                        |

Stall tables stand at waist height for the 0.48-scale townspeople (top at
0.35 local), and shop counters just above it, so the goods sit in plain view
instead of under a slab. Materials are set per part through
`BuildingPartMaterial` (stone, wood, cloth, metal), so wood and cloth get their
own surface detail in `material_detail.glsl` instead of the stone default.

## Moving parts

`submit_market_awnings` draws, every frame, the parts that move:

- **Awnings** are six segments deep and one strip per stripe wide; each
  segment follows a billow plus a travelling ripple whose strength rises and
  falls with a slow gust, and each stripe ends in a flapping valance. Vertical
  awnings (back and front points stacked) are hanging cloths: shop valances
  and the drying cloths.
- **Hanging goods** (garlic, sausages, herbs, spice bags) swing and twist on
  their strings.

Damaged markets lose every third stripe; destroyed ones draw none. Everything
is skipped beyond 95 m. `MarketAwningTest.*` checks the cloth is cloth, moves
and is culled.

## People

Buyers walk in through the gate to a stall or the shop counters and haggle;
stall sellers stand behind their tables under the awnings, a shopkeeper
behind the middle counter, a stroller circles the fountain or cistern, a
porter carries goods to the cloth shop, and one townsperson squats by the
fountain (Rome) or the dye vats (Carthage). Routes keep clear of every table,
post and vat.

`walk_route` (`ambient_people.cpp`) eases each leg's start and stop on a
trapezoid speed profile, phases the walk from the eased distance (so feet stay
planted while slowing), spreads each turn over the first 0.30 m of a leg or
the first 0.55 s of a stop, and fades walk and idle into each other over
0.30 s. Porters walk with the real walk clip and carry their load through the
upper-body `resource_carry` overlay (`CivilianActor::overlay_clip`); playing
the hold clip as a walk slid them along with still legs.

Review with `build/bin/building_preview --only marketplace --states` for the
model and `arena_app --scenario capital_market_crowd` for the people.
