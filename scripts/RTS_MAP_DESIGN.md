# Campaign RTS terrain contract

Campaign terrain is a tactical graph first and scenery second. The target is
the terrain-led combat of _Praetorians_: formations move through readable
battle basins, crossings and elevated positions change the plan, and towns or
camps are meaningful route anchors.

## Scale and graph

- Standard campaign maps are `650 x 650`; the final mission is `800 x 800`.
- Divide a standard map into three to five battle sectors. A sector should be
  large enough to deploy several formations, then narrow into readable
  connectors at its edges.
- The traversable graph needs at least one complete loop. Important camps,
  towns, and map exits should normally have two approaches. A single mandatory
  choke is reserved for a mission that explicitly asks the player to hold it.
- Dead ends must terminate at an objective, settlement, defended hill entrance,
  reinforcement edge, or optional reward. Decorative dead-end roads are not
  allowed.
- A blocker is useful only when it separates routes or protects an objective.
  Scattered obstacle blobs that do not change path choice are visual noise.

## Roads and settlements

- Provide two edge-to-edge strategic routes on a standard map. They may share
  part of their course, but must create at least one alternate flank.
- Every road endpoint belongs to a map edge, settlement, camp, bridge, hill
  entrance, or exact road junction. Roads never stop in empty ground.
- Main roads are wide enough for formations and readable at overview distance;
  secondary roads may be narrower but must remain continuous.
- One battlefield uses one road surface style. Width can communicate hierarchy;
  mixing unrelated road materials within a map cannot.
- Roads never cross hills, mountains, lakes, or rivers. A river crossing uses a
  bridge whose deck follows the road and is approximately perpendicular to the
  local river tangent. Its deck spans only the local channel width plus a small
  bank bearing; routing clearance never increases visible bridge length.
- Every retained bridge has a road continuing away from both ends into the
  network or to a map edge. A road merely touching a deck endpoint does not
  count as an approach.
- Put towns and camps at junctions or at the end of intentionally defended
  approaches. Economy is part of the route graph, not scenery beside it.

## Settlements

Settlements are authored as intent in a map's `settlements` array and stamped
into `structures` by `scripts/generate-map-settlements.py`. Never hand-edit the
generated structures; edit the intent and regenerate.

Three tiers, chosen by the settlement's role in its mission:

- **town** - outer curtain wall with two opposed gates, an inner citadel around
  the barracks and market whose gate faces a different side, and built-up
  housing quarters cut by streets. The signature settlement of a map.
- **fortified_camp** - one wall ring, corner towers, a market and housing rows.
  The common garrison holding.
- **marching_camp** - a closed rampart with two gates, barracks and a few homes,
  no market. Temporary and forward positions, and the player's start. A camp with
  no wall at all is authored with `"palisade": false`, which is how the offensive
  missions keep the player's start to a bare barracks.

Rules the generator enforces, and the reasons behind them:

- Every settlement of every tier has a barracks. A camp without one is a place
  the player or AI cannot produce from, and it breaks capture objectives.
- The local player owns at most ten homes across a map. Homes feed manpower to
  the nearest barracks through civilians, so a large player-owned quarter turns
  a supply decision into an automatic refill.
- No building stands on unwalkable ground. Hill slopes, mountains, lakes and
  river channels are all blocked, and terrain wins over the street grid: a
  housing block simply stops where a slope starts.
- **Towns are never placed on hills.** A hill's flat crown is only about a fifth
  of its authored width, so a town on one shrinks until it is a camp. Put the
  town on the flat and leave the height as the position overlooking it.
- A hill carrying a settlement may be widened to fit its crown, but never past
  2.4x its authored size, never into a river or lake, and never without keeping
  at least two approaches. A hill that doubles in size stops being terrain and
  becomes a wall that severs the road graph. Where a hill's size is load bearing
  for something else - a road threading past it, one wall of a pass - author
  `"grow_hill": false` and the settlement is shrunk to the crown it already has.

### Walls and gates

- **A wall ring has no holes.** It is laid one cell at a time on the runtime's
  2-unit wall lattice, and the only cells left out are the ones a gate covers and
  the ones no unit can walk anyway: a hill core, a lake, a river channel. Terrain
  may close a side of a ring; a clearance margin may not. A rampart that stops
  four units short of a river bank leaves a gap units walk straight through, and
  that is what a ring is for.
- The generator proves this rather than asserting it: it floods the inside of
  every walled settlement over walkable, unoccupied ground and fails the map if
  the flood reaches open country without passing a gate.
- **Gates go where roads cross the ring.** A gateway off the road makes
  formations walk the length of a curtain wall to get in. Where no road crosses,
  the gate is aimed at the nearest road instead of parked at the midpoint of a
  side, and every ring gets two, so a garrison always has a sortie route and a
  besieger always has a second front.
- A gate opening is exactly the gate's own span. Anything wider leaves walkable
  ground beside it. Gates open for their owner and allies only, so a walled town
  is a siege.

## Landmarks

The ground between settlements needs places worth walking to. A map's
`landmarks` array is authored intent - a handful of lines each - and
`scripts/generate-map-landmarks.py` stamps it into temples, props, guards and
groves. Four kinds:

- **sanctuary** - a temple standing outside any settlement, a statue-lined
  approach, ruins and a fire. Put one on a hill crown or at a wood's edge; it is
  the map's second temple and its most visible detour.
- **shrine** - a wayside altar of statues and ruins at a junction or a crossing.
- **hamlet** - a dead village of abandoned homes, ruins and dead trees, for a
  burnt flank or the ground around a Sepulcher zone.
- **watch** - a picket camped beside a monument: tents, a cart, a rack, a fire.

Rules the tool enforces, and the reasons:

- Nothing lands inside a settlement's ring or in a road. `statue` and
  `abandoned_home` block the cell they stand on, so a monument in the roadway
  makes a formation file around it.
- Guards are authored with the `guard` behaviour. Without it the AI folds them
  into its strategic pool and marches them off, and the landmark they were put
  there to hold is unheld a minute into the mission.
- Two landmarks inside 60 units read as one place, and the tool says so.
- A landmark owned by an AI player is a side objective; one owned by `-1` is
  neutral ground. Keep landmarks off `player_id` 1 on the offensive missions -
  `MissionAssetRulesTest.OffensivePlayerCampsUseAuthoredMinimalStructures` counts
  every structure the local player owns.

Everything the tool writes carries a `landmark` key naming its landmark, across
`structures`, `world_props`, `spawns` and `terrain`; a run replaces its own
previous output and leaves untagged entries alone. So the two generators can run
in either order, and hand-placed scatter survives both.

### Groves

A map's `groves` array places standalone woods, and the engine reads it: every
open cell inside a grove's radius becomes a **forest** cell in the navigation
grid. Forest is passable to commanders, archers, swordsmen, healers, builders,
civilians, the Sepulcher's dead and wildlife. It is closed to spearmen, all
cavalry, catapults, ballistae and elephants - they path around it.

So a wood is a filter, not just a screen. Place one for what it lets through:
the approach to a wall with no gate on it, the flank of a road a column has to
march down, the timber a gather objective is measured against, the den a wolf
pack comes out of. A wood across the only route to a camp makes that camp an
infantry problem.

Two rules keep this from stranding an army:

- **A road driven through a wood stays open to everyone.** Forest is only
  applied to cells that are not on a road, so laying a road across a wood is how
  you leave a lane for the siege train.
- **Trunks, boulders and buildings keep their own cell value.** Forest only
  claims ground that was already walkable, so a wood never turns a blocked cell
  passable.

A wood inside 22 units of a settlement wall grows through the streets and is
rejected. Woods are editable in the map editor under Terrain → Wood.

After moving settlements or resizing hills, re-run the road generator: approach
roads and bridges are routed around terrain and must be repaired.

## Water

- A river is a barrier between sectors. It must connect map edge to map edge,
  map edge to lake, or join another river. A short isolated stripe is invalid.
- A full-map river barrier normally has two crossings: a direct strategic
  crossing and a longer flanking crossing. One crossing is appropriate only
  for an explicit hold-the-bridge mission.
- A river that feeds a lake terminates on the irregular lake boundary. It must
  never continue beneath the lake mesh.
- A tributary terminates at the receiving channel's bank. Shoreline geometry is
  clipped against the union of all channels so no riverbank is rendered inside
  another river or across a tributary mouth.
- Rivers and lakes use one world-space water material, so color and motion are
  continuous at their join.
- A lake must remove a flank, constrain a road, protect an objective, or form a
  sector boundary. A lake in otherwise open ground is decorative and should be
  removed or integrated into the route graph.

## Height and fantasy

- Hills are defended positions with two or three authored entrances. Entrances
  face useful routes; they are not evenly scattered around the ellipse.
- Use mountains only in mountain regions. Elsewhere, shape routes with hills,
  forests, water, towns, and fieldworks.
- Keep the Iron Sepulcher on a side route so engaging it is a tactical choice.
  It may control a flank or shortcut, but should not replace the historical
  battle's main geography.

## Validation gate

Before visual review, both commands must pass:

```bash
python3 scripts/generate-map-water.py --validate-only assets/maps/MAP.json
python3 scripts/generate-map-roads.py --validate-only assets/maps/MAP.json
python3 scripts/generate-map-settlements.py --validate-only assets/maps/MAP.json
python3 scripts/generate-map-landmarks.py assets/maps/MAP.json
```

The mechanical gate requires a connected road graph, at least two map-edge
connections, no road or river overlap with protected terrain, no unbridged
road-water crossing, no dangling road endpoints, one road style, bridge spans
limited to local channel width, no river self-crossing, and river endpoints on
a map edge, receiving riverbank, or exact lake shoreline. The Arena overview is then used to
check the non-mechanical questions: clear sectors, useful route choices,
formation room, objective placement, and visual readability.

## Trasimene reference layout

Lake Trasimene is the closed side of the ambush basin. The Via Flaminia follows
its shore and links the two Roman field camps. Two tributaries run from the far
map edge to the lake, splitting the playable field into three sectors. The Via
and the southern route cross each tributary once, creating four bridge choices.
Four connecting roads and two central cross-links turn those crossings into
loops. The Carthaginian camp sits behind the ambush line; the Sepulcher altar is
on the outer flank rather than in the historical corridor.
