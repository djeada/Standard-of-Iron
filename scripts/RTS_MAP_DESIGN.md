# Campaign RTS terrain contract

Campaign terrain is a tactical graph first and scenery second. The target is
the terrain-led combat of *Praetorians*: formations move through readable
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
