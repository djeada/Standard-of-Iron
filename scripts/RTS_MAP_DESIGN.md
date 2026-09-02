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
- **Towns are not placed on hills unless a map says so.** A hill's flat crown
  is only about a fifth of its authored width, so a town on one shrinks until it
  is a camp. Put the town on the flat and leave the height as the position
  overlooking it - or author `"on_hill": true` where a small hilltop fort is
  the point of the place, and accept the size that comes with it.
- A hill carrying a settlement may be widened to fit its crown, but never past
  2.4x its authored size, never into a river or lake, and never without keeping
  at least two approaches. A hill that doubles in size stops being terrain and
  becomes a wall that severs the road graph. Where a hill's size is load bearing
  for something else - a road threading past it, one wall of a pass - author
  `"grow_hill": false` and the settlement is shrunk to the crown it already has.

### Authored settlements

A settlement marked `"authored": true` is laid out by hand in `structures` and
`generate-map-settlements.py` leaves both it and its buildings alone - it carries
through anything tagged `"authored": true` exactly as it already carries landmark
pieces. The intent entry stays in `settlements` so the map still says what the
place is; only the generation is skipped. Generated structures are tagged with
their `settlement` id and `"authored": true` as well: the tag is what lets a
rerun replace exactly its own output, and the flag keeps them under
`EveryAuthoredSettlementStandsOnGroundItCanHold`.

Author one when the template is the wrong answer, and the clearest case is a
settlement on a hill. A hill's flat crown is an ellipse, and a rectangle inscribed
in an ellipse reaches only 0.707 of its half-extents, so even a 176x152 rise
carries a ring of about 42x34 - a citadel, not the 116x100 town template. The
generator's response to that is to shove the town off the hill; authoring the
settlement instead lets the hill stay the point of the place. `Crossing the
Rhone`'s Roman citadel is the worked example.

The player's camps and a handful of enemy ones are authored by hand. The rest
of the enemy settlements carry a `plan` (below) and are generated: the plan is
the intent, and the generator stamps it, so their layout is regenerable and
checked rather than frozen.

### Plans

`tier` says how much a place is; `plan` says what shape its wall circuit is.
They are separate axes: a marching camp can be a circle and a town a star. The
rectangle is the Roman default and every other plan exists to make an enemy
read as a different kind of enemy at overview distance.

- **rect** - the four-sided ring, as before. The default.
- **stepped** - a rectangle with its corners cut back: the Polybian playing
  card. `chamfer` sets the cut.
- **circle** - an oval ring on the lattice: a native contour fort, a colonia, a
  Numidian kraal (`gate_count: 1`). `towers` spaces towers evenly round it.
- **star** - a bastioned trace: a curtain polygon with a triangular bastion at
  every corner and a tower behind each. `size` is the curtain's half-extent,
  `bastion` how far the points reach, `flank` how wide the shoulders are.
- **twin** - two lobes and a walled neck between them, one circuit. `offset` is
  the vector between lobe centres; the neck is kept clear of buildings so the
  lobes stay connected. The barracks goes in one lobe, the market in the other.
- **terraced** - two concentric rings; the upper ward holds the barracks, its
  gate faces the settlement's `facing`, the lower ward holds the rest.
- **curtain** - not a ring: an open wall along `path`, for a pass plug or a
  siege line. It holds a band of ground `depth` behind it and a camp at the
  settlement's centre. Author its `gates` explicitly, on the roads it crosses.

Every plan is built the same way. The circuit is defined by the ground it
encloses, the wall cells are the lattice cells inside that ground that touch a
cell outside it, and diagonal steps are closed with an L so the wall renderer
sees a wall rather than a dotted line of posts. Gates are cut into the straight
span of wall nearest where a road actually crosses the enclosed ground - for a
curve the crossing of the bounding box can be a wall's length off - and the
usual flood-fill proves the ring is sealed. Buildings keep off the roads that
run through the ring (`ROAD_VERGE`), which the old rectangular layout did not.

A plan on a hill: `on_hill: true` keeps a town on its hill instead of shoving it
off, and `grow_hill: false` keeps the hill the size it was authored. A round
ring fits its hill when its half-extents fit the crown's; a square one only when
its corner does, so a circle on a hill is bigger than a square on the same hill.
Either way a hill's usable crown defaults to about 38% of its authored
half-extent at campaign scale, so a 150x130 hill carries a ring of at most about
50x44 - a hill fort is a small fort, and the hill is the point. A hill authored
with `crown` (a fraction of its half-extent, at most 0.9) keeps that much of
itself flat instead: `crown: 0.6` on a 150x88 hill is a mesa with a 45x26 top and
short steep flanks, `crown: 0.85` a broad table. The loader, the engine's
`hill_crown_profile` and the generator's `hill_crown_extent` all read that one
number, so a ring fitted by the generator stands where the engine's crown is.

A `rect` plan's `size` is its half-extents like every other plan's, and
`citadel: false` drops the town tier's inner ring from a rect town too shallow to
hold one - Trasimene's column town is 92x28 against the foot of a plateau.

### Raising a hill under a settlement

A new hill breaks the roads that ran through the site, and regenerating the
whole road network is not a neutral operation on the campaign maps (roads the
generator cannot route are dropped; a bridge approach it cannot complete aborts
the write). The pipeline is:

1. Author the hill, centred on the settlement (the generator recentres the
   settlement on its hill).
2. `generate-map-settlements.py MAP --write` - fits the ring to the crown and
   writes the hill's entrances.
3. `reroute-map-roads.py MAP --hill X,Z --write` - re-routes only the roads the
   hill broke; endpoints under the hill move to its nearest entrance, roads that
   lay entirely under it are dropped.
4. `generate-map-settlements.py MAP --write` again, so gates follow the roads.
5. `reroute-map-roads.py MAP --to-gates ID --write` bends a road that ends
   inside a ring so it ends at the nearest gate, then run step 4 once more.
6. `fix-map-prop-overlaps.py MAP --max-travel-structure 0 --max-travel-anchor 0`
   nudges the old dressing out of the new walls; dressing that ended up on the
   hill's slope is deleted, not nudged.

The old hand-laid structures of a settlement being handed to the generator have
to be deleted in the same edit, or the generator's output lands on top of them.

**A hill on a hill.** A `terraced` plan on a hill that has a second, smaller
hill authored at the same centre is a real terrace fort: the lower ward on the
outer crown, the keep on the inner one. Hill heights compose as
`max(base + height)`, so the inner hill's `height` is absolute, not stacked on
the outer crown - Zama's north camp is a 150x130 hill of height 2.6 with
`crown: 0.85` under a 56x48 hill of height 10.5 with `crown: 0.6`, a six-metre
keep above the terrace. The engine used to carve the inner hill's entrance
ramps down to ground level straight through the outer crown, which left the
lower ward's homes in a trench; since 2 Sep 2026 a ramp never digs below the
ground that stood before its own hill
(`HillCrownGeometryTest.AHillRaisedOnAnotherHillNeverCarvesBelowTheOuterCrown`).
The generator writes the inner hill one entrance on the settlement's `facing`,
the side the keep's gate is cut on, keeps that corridor free of buildings, and
knows a terrace hill's ramp inside the outer ring is not a breach.

Three rules an authored ring has to earn back, because it gives up the
generator's checks:

- **Wall runs sit on the 2-cell lattice and only run along an axis.** A diagonal
  side cannot be walled - it would be laid as a straight run in the wrong place
  and the circuit would not close. An irregular outline is built by stepping the
  corners instead.
- **A road that crosses the circuit gets a gate on the crossing, and no building
  stands in a roadway.** Gates open for their owner and allies only, so a wall
  across a road with no gate on it severs the network; and the road generator
  will not route around a building, so one in the road is one nobody can pass.
- **Nothing an enemy is meant to assault is a sealed ring.** The Rhone start is
  an open screen - three sides, no fourth - because the first Roman wave has to
  be able to reach it, and it cannot walk through a gate it does not own.

Two guards hold the line: `CampaignContentIntegrationTest.EveryAuthoredSettlementStandsOnGroundItCanHold`
fails if an authored building stands on water or a mountain, or slides off a hill
crown onto a slope; `MissionAssetRulesTest.OffensivePlayerCampsUseAuthoredMinimalStructures`
keeps the player's start on the offensive missions to a bare barracks. Those camps
are made of dressing instead - tent rows, cooking fires, the baggage park - which
costs the player no structures and still reads as an army camped for the night.

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
- Wall cells keep `WALL_WATER_CLEARANCE` (1.5 m) off a river or lake, so a
  curtain that crosses a river stops on the bank instead of standing in the
  water; the channel is the wall there. Buildings keep their own, larger,
  terrain clearance.
- A raised `flat` (a plateau with a `height`) is blended in over the outer fifth
  of its ellipse, and nothing is placed on that rim: a barracks straddling a
  three-metre step is on broken ground. A town at the foot of a plateau builds
  below the step.

## Landmarks

The ground between settlements needs places worth walking to. A map's
`landmarks` array is authored intent - a handful of lines each - and
`scripts/generate-map-landmarks.py` stamps it into temples, props and guards.
Four kinds:

- **sanctuary** - a temple standing outside any settlement, a statue-lined
  approach, ruins and a fire. Put one on a hill crown or at a forest's edge; it
  is the map's second temple and its most visible detour.
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

### Forests

A map's `forests` array is the only way to author forest. There is no
`terrain` entry of type `forest` any more; a forest is one object, `id`, `x`,
`z`, `radius`, and the engine reads it twice.

**As movement.** Every open cell inside the radius becomes a **forest** cell in
the navigation grid. Forest is passable to commanders, archers, swordsmen,
healers, builders, civilians, the Sepulcher's dead and wildlife. It is closed to
spearmen, all cavalry, catapults, ballistae and elephants - they path around it.

**As ground.** The map loader raises a Forest terrain feature over the same
circle, which colours the ground as forest and multiplies the pine and olive
scatter, so a forest you can see is exactly a forest cavalry cannot enter.

So a forest is a filter, not just a screen. Place one for what it lets through:
the approach to a wall with no gate on it, the flank of a road a column has to
march down, the timber a gather objective is measured against, the den a wolf
pack comes out of. A forest across the only route to a camp makes that camp an
infantry problem.

Three rules keep this from stranding an army or fighting the terrain:

- **A road driven through a forest stays open to everyone.** Forest is only
  applied to cells that are not on a road, so laying a road across a forest is
  how you leave a lane for the siege train.
- **Trunks, boulders and buildings keep their own cell value.** Forest only
  claims ground that was already walkable, so a forest never turns a blocked
  cell passable.
- **A forest laid over a hill is clipped, not blended.** The terrain feature
  paints only cells that are still flat, so the hill wins and the forest fills
  the ground around it. Overlap one deliberately and you get less forest than
  the radius suggests.

A forest inside 22 units of a settlement wall grows through the streets and is
rejected. Forests are editable in the map editor under Terrain → Forest.

After moving settlements or resizing hills, re-run the road generator: approach
roads and bridges are routed around terrain and must be repaired.

## Map editor

Open and Save As start in the last directory you used, remembered across runs in
`QSettings` under `map_editor/last_dialog_directory`. With nothing remembered
they start at the repository's `assets/maps`, resolved by walking up from the
executable rather than from the working directory: the editor is normally
launched from `build/bin`, which holds a _copy_ of `assets`, and editing that
copy loses the work the next time the build runs.

The editor asks to save on File → New, File → Open and on close, and never at
startup. A panel that writes to the document while it is being built - a spin
box firing `valueChanged` from its own initial value, say - makes the empty
document look modified, and the first thing the user then sees is a save prompt
for a map they have not touched. Refresh handlers block their widgets' signals
for that reason; the startup path also resets the document without prompting, so
a missed blocker is a stale window title rather than a modal on launch.

### Walls and gates in the editor

Walls and gates are one tool group because they are one structure. Both live on
the runtime's two-cell lattice (`WallNetworkService::k_segment_spacing`), so the
editor snaps both to even grid coordinates: a wall drawn a cell off the lattice
and a gate set into it would never line up in game.

Drawing a wall snaps both endpoints and keeps the run axis aligned. Setting a
gate does the rest of the work for you:

- Click on a run and the gate takes that run's axis and line, snaps along it, and
  inherits its owner and nation. Rotation is derived, not typed - 0 spans x, 90
  spans z.
- The run is split around the opening in the same edit, leaving wall faces six
  cells apart, which is the gate's own span. A gate at the end of a run leaves
  one piece; a gate longer than the run leaves none.
- The whole thing is one undo step.
- Click clear of every run and you get a free-standing gate on the lattice,
  with the status bar saying so, for when the wall comes later.

A gate is drawn as its real 6x2 footprint - two piers in the owner's colour with
the opening between them - rather than as another building dot, so a ring reads
at a glance as wall, wall, opening.

Right-click puts the armed tool away and goes back to Select, on the canvas and
in the tool panel both, so you drop a brush the same way you drop anything else.
With nothing armed, right-click is the context menu it always was - that is the
only way to reach Edit JSON, Duplicate and Delete on an element. A half-drawn
wall or road is cancelled first, so the first right-click drops the pending
endpoint and the second puts the tool away.

### Herds in the editor

A sheep pasture or wolf range is a herd, not a landform. It is drawn as a
unit-sized marker like a troop spawn, with the roam radius behind it as a
hairline, and it is picked up by that marker rather than by its whole range -
a range-sized click target swallows everything standing inside it.

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
- A ring river - `{"shape": "ring", "x", "z", "radius", "width"}` with optional
  `radius_z` and `segments` (12..256, default 48) - is a closed channel: a moat,
  an oxbow, the water round the Rhone river town. The loader expands it in
  `game/map/river_geometry.h`, the map editor draws it as a loop and writes it
  back as a ring, and every map script reads it through
  `scripts/map_water_geometry.py`, the one Python mirror of that expansion. A
  ring is exempt from the edge-to-edge rule and is a barrier with no way round,
  so it carries at least two bridges.

## Height and fantasy

- Hills are defended positions with two or three authored entrances. Entrances
  face useful routes; they are not evenly scattered around the ellipse.
- A hill is a wall, not a mound: only cells flood-connected to an entrance ramp
  are walkable, and the rim never is. Two ramps on opposite flanks make a
  crossing - up one side, along the crown, down the other; a pair at the same
  point along the spine makes one gate straight through. A ring with no ramp
  pair is a sealed bowl.
- A hill need not be an ellipse. `shape` takes `corridor`, `arc` (boomerang),
  `elbow`, `ring`, `path` and `mask`; `thickness` sets how wide the ridge is and
  `arc` how far it sweeps. A shaped hill is a ridge along a spine, so its
  concave side is open ground - the pocket of an arc is where a camp goes, and
  the lane past the end of a corridor is where the route goes. The road, water
  and settlement generators read the spine through `scripts/map_hill_shapes.py`
  rather than the bounding ellipse, so authoring a boomerang does not blind them
  to the ground inside it.
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
