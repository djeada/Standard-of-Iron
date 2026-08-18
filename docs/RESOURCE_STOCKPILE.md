# Resource Stockpiles and Hauling

Gathered goods do not teleport into the treasury. A worker who fells a tree, breaks a
boulder or digs an ore seam shoulders the load, walks it to the stone yard beside a
friendly barracks, and only there does the player's resource counter move.

This document covers the two halves of that: the yard a barracks owns, and the haul
that ends on it.

## The stockpile yard

Every barracks has a yard: a flat rectangle of loose stones laid on the ground beside
the building. It is part of the barracks, not a separate entity — no spawn type, no
collision footprint, no health. It is drawn by the barracks renderer and it disappears
with the building.

Its layout is authored once, in world units measured from the barracks centre in the
building's own unrotated frame, and shared by gameplay and rendering:

| Constant                            | Value       | Meaning                                   |
| ----------------------------------- | ----------- | ----------------------------------------- |
| `k_stockpile_center_x` / `_z`       | 5.20 / 0.0  | Yard centre; `+X` is always the yard side |
| `k_stockpile_half_width` / `_depth` | 1.45 / 2.10 | Half extents of the stone bed             |
| `k_stockpile_drop_x` / `_z`         | 5.95 / 0.0  | Where haulers stand to unload             |
| `k_stockpile_drop_radius`           | 2.20        | Anywhere this close counts as arrived     |
| `k_stockpile_wood_display_cap`      | 640         | Wood that draws the timber stack at full  |
| `k_stockpile_stone_display_cap`     | 480         | Stone that fills the block courses        |
| `k_stockpile_iron_display_cap`      | 480         | Iron that fills the ore bin               |
| `k_stockpile_food_display_cap`      | 400         | Food that stacks the grain sacks full     |

`game/systems/resource_stockpile.h` owns these numbers and the yaw rotation that turns
them into world space, so a rotated barracks keeps its yard on the same side of the
building and the drop-off point follows it. **Change the offsets there and nowhere
else** — the renderer and the delivery system would otherwise disagree about where the
yard is, and haulers would walk to a patch of grass.

The offsets clear the barracks' 4×4 collision footprint and its one-cell padding, so
the drop-off point is always on walkable ground next to the building rather than inside
the blocked ring around it.

### What is drawn

`render/entity/barracks_stockpile.cpp` draws the yard in world units. It rebuilds its
own frame from the entity transform's position and yaw, deliberately dropping the
barracks' non-uniform mesh scale, so the yard's proportions never depend on how a
nation's building model happens to be scaled.

- A sunken gravel bed.
- A ring of flat, loose stones around it, jittered from a deterministic hash so it reads
  as laid rather than as masonry. Two nations get two palettes: Roman limestone,
  Carthaginian sandstone.
- Worn flagstones inside the ring (dropped at distance, with the rest of the detail LOD).
- Three piles on the inner half — stacked round logs, dressed stone blocks, and an ore
  heap with ingots — leaving the outer strip clear as the apron haulers walk onto, and a
  stack of grain sacks on the east side that grows with the food store.

Pile heights come from `StockpileComponent`, a presentation-only component the delivery
system refreshes each tick from the owner's current stores. Piles therefore grow as the
player gathers and shrink as they spend, and they are never written to a save. Roman and
Carthaginian barracks have yards; other nations' barracks work as drop-off points but
draw no yard.

**Each resource has its own display cap**, because a yard holds far more timber than
dressed stone and the three piles would otherwise disagree about what "full" means. The
caps sit well above a typical map's starting stores, so a new game opens with a part-built
yard rather than a full one, and the piles are quantised finely enough — fourteen logs, ten
blocks, twelve ore lumps — that a single delivery moves them. A pile at zero draws nothing
but its empty frame: bare stakes, an empty bin.

### The carried load

`render/entity/carried_load_renderer.cpp` draws what a worker is carrying, so the walk home
is legible without reading the activity badge: a log across the chest for wood, a dressed
block for stone, a basket of ore for iron, a bound sheaf for food. It reads the same `ResourceCarryComponent` the
delivery system does, picks the heaviest resource in the load, and places one load per
living soldier using the unit's shared formation layout, so a whole work gang hauls
together. It is submitted during the scene walk beside the bird flocks — nothing is added
to the creature pipeline, and a load costs a handful of primitives that stop drawing their
detail past 46 units.

## The haul

1. `ProductionSystem` finishes a harvest job. Instead of crediting the owner it calls
   `load_onto_hauler`, which puts the yield in a `ResourceCarryComponent` on the worker.
   Loads accumulate per resource type, so a worker who gathers twice carries both.
2. `ResourceDeliverySystem` finds the nearest live friendly barracks, issues a
   `ScriptedMove` to that yard's drop-off point, and re-issues it (at most every 1.5 s)
   whenever the worker is idle again — after being blocked, or after the player sent it
   somewhere else in between.
3. When the worker is within `k_stockpile_drop_radius` of the drop-off point the whole
   load is credited through `PlayerResourceRegistry::add_harvested`, the yard flashes
   briefly, a drop-off cue plays, and the carry component is removed.

While carrying, the worker reports `ActivityKind::Deliver`, so the same badge the HUD
already uses for civilians tells the player what it is doing.

### Rules that keep the loop from stalling

- **A player order wins.** The delivery system only steers a worker that has no builder
  job and no movement target of its own. Order the worker elsewhere and it goes; the
  load stays on its back and the walk home resumes once it is idle.
- **No depot, no loss.** With no friendly barracks anywhere in the world the load is
  credited where the worker stands. Arena scenarios and tests that have no barracks
  therefore behave exactly as they did before hauling existed.
- **A dead depot is replaced.** If the target barracks is destroyed or captured
  mid-walk, the worker re-targets the next nearest friendly one.
- **Unreachable yards resolve.** If the drop-off point snaps to walkable ground far from
  the yard, the worker is sent to the barracks itself instead; and after
  `k_stockpile_haul_patience_seconds` of trying, reaching the building is enough to
  unload. A load can never be stuck on a worker forever.
- **The load survives a save.** `ResourceCarryComponent` is authoritative serialized
  state — dropping it would either delete the goods or hand them over for free.

### The next load

Unloading is no longer the end of the job. A worker that finished a harvest carries a
**standing gather order**, and once the load is off its back `GatherLoopSystem` sends it
back to the next node of the same kind near the one it last worked. A forest is one
click, not one click per tree. The rules that end a round — and why the faction AI is
deliberately left out of it — are in
[SETTLEMENT_LIFE.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/SETTLEMENT_LIFE.md).
How the haul is explained to the player — counters, tooltips and the first-skirmish
prompts — is in
[ECONOMY_GUIDANCE.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/ECONOMY_GUIDANCE.md).

### What this changes elsewhere

- Mission `accumulate_resources` objectives count a yield at unload, not at harvest, so a
  gather mission needs a reachable friendly barracks. See
  [MISSION_FRAMEWORK.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/MISSION_FRAMEWORK.md).
- The AI is unaffected in principle and slower in practice: its builder behaviour only
  picks workers with no movement target, so a hauling worker is left alone until it has
  unloaded and is genuinely free again.
