# Campaign road generator

`generate-map-roads.py` uses authored roads as network intent and reroutes them
into a clean deterministic network. Existing crossings and T-junctions are
preserved as shared anchors so separate roads continue to meet exactly. An
endpoint inside protected terrain is moved to the nearest legal authored hill
approach. Hills, mountains, lakes, and rivers are hard obstacles. Bridges are
the only legal water crossings.
Each bridge is projected to its nearest river centerline and made perpendicular
to that river before routing; the generated road then contains the repaired
bridge's exact endpoints, so its approach is aligned with the bridge deck.
Deck length is derived from the local visible river width plus one tile of bank
bearing; routing clearance is never allowed to turn a bridge into a causeway.
When an authored road clearly crosses a river with no nearby bridge, the tool
creates the missing perpendicular bridge at that intended crossing instead of
routing the road through a distant part of the map. Bridges left disconnected
from the final road network are omitted from written output.

Impossible authored links and links that would require a map-spanning detour
are reported and omitted. If that separates otherwise useful local roads, the
generator adds the shortest legal connector it can find. It never accepts a
connector that crosses protected terrain or unbridged water.
It also completes every dangling terminal and both ends of every retained
bridge. Every written map is normalized to one road style chosen from its
dominant authored style.
Touching a bridge endpoint is not sufficient: validation requires an aligned
road segment to continue away from each bank. The bridge injector distinguishes
crossing a deck from merely starting an approach at its endpoint, preventing
roads from crossing the bridge and doubling back as a disconnected-looking
stub.

The final graph must also touch at least two distinct map sides. If an authored
network is landlocked, the generator adds the shortest legal edge spurs. When
new water divides required road regions, it plans a terrain-safe perpendicular
bridge before routing instead of repeatedly attempting impossible A* paths.

The command is a dry-run unless `--write` is present:

```bash
# Generate and validate one map without changing it
python3 scripts/generate-map-roads.py assets/maps/map_battle_trebia.json

# Rewrite that map's roads and any repaired bridge geometry
python3 scripts/generate-map-roads.py \
  assets/maps/map_battle_trebia.json --write

# Generate all campaign maps
python3 scripts/generate-map-roads.py --campaign --write

# Audit existing authored roads without generating anything
python3 scripts/generate-map-roads.py --campaign --validate-only
```

Generation fails without writing when any emitted road touches protected
terrain, crosses water outside a bridge, approaches a bridge at the wrong
angle, the resulting road graph is disconnected, or fewer than two map sides
are reachable by road. Dangling endpoints, orphaned bridge approaches,
overlong bridge spans, and mixed road styles also fail validation. Use `--clearance` and
`--max-segment-length` to tune map-scale output while preserving those rules.
