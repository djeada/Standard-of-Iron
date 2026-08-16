# How Pathfinding Works

Pathfinding in Standard of Iron is deliberately simple at the core: the game keeps one flat 2D navigation grid, and A* searches that grid. The grid is not a physics simulation, not a unit occupancy map, and not a navmesh. It is a compact answer to one question:

> Is this grid cell free to pass, or is it blocked?

Terrain, buildings, bridges, hills, and resources feed into that answer. Unit radius is not part of path search. Formations, combat locks, and invalid-position recovery sit around the grid without adding extra public navigation concepts. Keeping that separation is what makes the system fast enough to use during normal RTS play without constantly revalidating every unit.

## The Grid

`Game::Systems::Pathfinding` owns `Pathfinding::NavigationGrid`, a flat row-major array of `std::uint8_t` values. Conceptually it is a 2D grid:

```text
grid index = y * width + x

       x=0 x=1 x=2 x=3 x=4
 y=0    0   0   0   0   0
 y=1    0   1   1   1   0
 y=2    0   0   0   1   0
 y=3    0   2   0   0   0
```

The actual memory is one vector:

```text
[0,0,0,0,0, 0,1,1,1,0, 0,0,0,1,0, 0,2,0,0,0]
```

The current values are:

| Value | Name       | Meaning                                                    |
| ----- | ---------- | ---------------------------------------------------------- |
| `0`   | `Walkable` | Free movement cell                                         |
| `1`   | `Blocked`  | Mountain, river, bridge edge, building, or generic blocker |
| `2`   | `Tree`     | Harvestable tree blocker                                   |
| `3`   | `Boulder`  | Harvestable boulder blocker                                |
| `4`   | `IronOre`  | Harvestable iron ore blocker                               |

Only `Walkable` is traversable. Resource cells are intentionally blocked but named, because the collect cursor must know whether the blocked thing is a tree, boulder, or iron ore.

## World Space And Grid Space

Units move in world space with floating-point positions. Pathfinding runs in grid space with integer cells.

```text
world position:  ( 12.4, y, -8.7 )
                         |
                         v
CommandService::world_to_grid()
                         |
                         v
grid cell:       ( 112, 91 )
```

`CommandService` owns this conversion. Gameplay code should not independently round world coordinates and query terrain/pathfinding by hand. Use the shared query functions instead:

| Function                                                | Use                                                          |
| ------------------------------------------------------- | ------------------------------------------------------------ |
| `is_grid_walkable(point)`                               | Test one grid cell against the current navigation grid.      |
| `is_world_position_walkable(position)`                  | Convert world to grid, then test.                            |
| `find_nearest_walkable_grid(origin, max_search_radius)` | Find a nearby valid cell without changing the grid.          |
| `snap_to_walkable_ground(position, max_search_radius)`  | Snap orders, exits, formation slots, and delivery positions. |

This boundary matters. `Pathfinding` owns cell values and A*. `CommandService` owns coordinate conversion and high-level navigation queries. Movement, formations, production, resource gathering, and UI helpers call `CommandService`.

## Building The Grid

Every full rebuild starts with an all-free grid, then layers blockers in a fixed order:

```text
1. Start all cells as Walkable
2. Apply static terrain from TerrainService
3. Apply harvestable resource props
4. Apply completed/loaded building footprints
5. Open doorways: gates, and the gaps a wall network leaves
6. Open map crossings: bridge decks and hill entrances
```

`Pathfinding::update_region` is the only place that composes a cell, and a full
rebuild is just that function over the whole map. The order is the whole ruling:
each layer may overrule the ones above it, and two rules keep a later layer from
quietly deleting an earlier one.

- A **doorway** may reopen the structure it belongs to and no other. A gate's
  opening never punches through the wall segment beside it, however the two
  overlap. The passage carries the entity id of the gate that owns it, and any
  other blocking footprint on the cell wins.
- A **map crossing** outranks the terrain underneath it — that is the point of a
  bridge deck — but not a building standing on it. Reopening that cell would
  route units into something the grid itself calls solid.

A doorway is centred on the gate's own transform, the same position its footprint
and its `GateService` movement blocker are placed from. The wall-network cell a
gate snaps to for its connection mask is up to half the segment pitch away, and a
doorway carved there leaves the real approach blocked while opening ground beside
it.

### One rasterisation rule

Anything that has to agree with the grid about where a rectangle _is_ goes
through `Pathfinding::cells_covering`: building footprints, doorways, map
crossings, and the gate blocker that refuses movement at runtime. It returns the
cells a rectangle covers **with area**, not the cells it merely touches — a
two-by-two footprint centred on a cell centre has its edges exactly on cell
boundaries, and counting those as coverage would make it three cells wide and
swallow the gap its neighbour was leaving open.

A second rasterisation rule is not a rounding detail; it is a hole. Two
descriptions of the same doorway that disagree by half a cell leave a strip that
is walkable on the grid and covered by no blocker, and units file through the
solid part of the wall.

A shut gate widens its blocker to the snapped cell range **along the wall line**
only. Across the wall it stays the structure's own thickness, because a unit
already inside the rectangle is free to move — that is how anyone gets back out
of a doorway — and a rectangle reaching further out would hand that freedom to
whoever is standing in front of it.

Dirty regions are merged before rebuilding. Building candidates come from the
collision registry's spatial buckets rather than a scan of every registered
building, and resource props are indexed by navigation cell. A resource revision
marks only cells whose indexed value changed. A* results are cached by exact
start/end cells and navigation revision; each worker thread owns independent
generation-stamped search buffers while the immutable grid is protected by a
shared read lock.

Group movement computes one corridor for units whose starts and destinations
fall in the same local regions. Individual targets are appended at the corridor
exit, with per-unit A* used only when sharing is unsafe. Expensive fallbacks are
limited per command and placed in a bounded request queue; `MovementSystem`
services a fixed number each simulation tick.

```text
Terrain layer:

  . . . . . . . . .
  . . . M M M . . .
  . . . M M M . . .
  . . . . . . . . .

Building layer:

  . . . . . . . . .
  . B B B . . . . .
  . B B B . . . . .
  . . . . . . . . .

Resource layer:

  . . . . T . . . .
  . . . . . . I . .
  . . S . . . . . .
  . . . . . . . . .

Final navigation grid:

  . . . . T . . . .
  . B B B M M I . .
  . B B B M M M . .
  . . S . . . . . .

Legend:
  . = Walkable
  M = Blocked mountain
  B = Blocked building
  T = Tree
  S = Boulder/stone
  I = Iron ore
```

Units are not written into this grid. A unit standing in a cell does not make that cell blocked for other units. Unit-to-unit spacing is intentionally not a pathfinding concern, because writing every unit into the global grid would make movement unstable and expensive.

The same order is used for regional rebuilds: reset the region to terrain, reapply buildings/resources intersecting the region, then force mandatory traversal cells in that region to `Walkable`.

## Forest cells and passability

`CellValue::Forest` marks the open ground inside an authored forest (`forests` in the map
file). It is the one cell value whose walkability depends on who is asking:

```cpp
enum class Passability : std::uint8_t { Light, Heavy };
```

`Light` treats `Forest` as walkable; `Heavy` does not. Everything else — `Walkable`,
`Blocked`, `Tree`, `Boulder`, `IronOre` — reads the same for both. `find_path`,
`is_walkable`, `is_world_position_walkable`, `is_world_segment_walkable` and
`find_nearest_walkable_point` all take a `Passability`, defaulting to `Light`.

Which units are light is decided by `Game::Units::can_enter_forest`: commanders, archers,
swordsmen, healers, builders, civilians, the Sepulcher's dead and wildlife. Spearmen,
cavalry, catapults, ballistae and elephants are heavy. The answer is stamped onto
`MovementComponent` once at spawn, in `UnitFactoryRegistry::create`, so no call site has to
look up a spawn type to path.

Three details that matter:

- **The path cache is keyed on passability.** Without that, a route computed for a
  swordsman would be served to the elephant behind him.
- **A group move uses the heaviest member's passability.** A shared corridor has to be
  walkable by everyone who will follow it; light units that fall back to individual paths
  still get `Light`.
- **Forest only claims cells that were already `Walkable`,** and roads are excluded, so a
  road driven through a wood stays a lane the siege train can use. `apply_forest_cells`
  runs after the terrain and building passes and before the bridge/hill-entrance pass, so
  a bridge or ramp inside a wood also stays open.

## Enemy Units Are Not Empty Ground

This is the most important separation in the system:

```text
Navigation grid:
  "Can terrain/buildings/resources be traversed?"

Entity world:
  "Which units and buildings exist at this position?"

Visibility grid:
  "Which parts of the map are visible to this player?"
```

An enemy troop is not represented by a navigation `CellValue`. That does not make it the same as nothing. It is an entity with a `TransformComponent`, `UnitComponent`, `owner_id`, health, attack state, render state, and selection/picking identity.

```text
Same map cell, different systems:

  NavigationGrid cell (12, 8):
    Walkable

  World entities near (12, 8):
    Entity #304
      UnitComponent.owner_id = 2
      TransformComponent.position = (12.1, y, 8.2)

  VisibilityService for player 1:
    Visible

Result:
  The cell is traversable terrain, but it is not "empty".
  Rendering, picking, targeting, AI, and combat can see an enemy entity there.
```

The game distinguishes enemy troops from empty ground through the entity layer:

| Question                              | System                                               |
| ------------------------------------- | ---------------------------------------------------- |
| Can I path through this terrain cell? | `Pathfinding::NavigationGrid`                        |
| Is an enemy standing here?            | `World` entity queries and `UnitComponent::owner_id` |
| Is that enemy visible to me?          | `VisibilityService`                                  |
| Did I right-click an enemy?           | `PickingService::pick_unit_first()`                  |
| Should AI react to this enemy?        | `AISnapshotBuilder::visible_enemies`                 |
| Should the renderer draw this enemy?  | `SceneRenderer` + fog visibility                     |

This gives us the missing distinction without polluting pathfinding:

```text
Cell state:

  . = walkable ground
  E = visible enemy entity on walkable ground
  F = friendly entity on walkable ground
  X = blocked navigation cell

  . . . . .
  . F . E .
  . . X . .

Navigation sees:

  . . . . .
  . . . . .
  . . X . .

Entity/visibility systems see:

  friendly at F
  enemy at E, if E is inside current visibility
```

That is intentional. If enemy units were written into the navigation grid, a moving army would constantly rewrite global pathfinding state, every path would invalidate other paths, and large fights would become much more expensive. Dynamic unit avoidance belongs in short-range movement, formations, combat steering, and attack/target selection. Long-range A* should route around terrain and durable blockers only.

If we later need stronger crowd avoidance, add a separate transient occupancy or influence layer. Do not add unit IDs or enemy/friendly values to `NavigationGrid::CellValue`.

## Terrain Rules

Static terrain is converted into cell values by `Pathfinding::terrain_cell_value()`.

Mountains are always blocked:

```text
M M M
M M M   -> all Blocked
M M M
```

Rivers are blocked except for authored bridge traversal cells. Bridge cells and bridge centerline cells are forced back to `Walkable` at the end of grid construction:

```text
River with bridge deck:

  . . R R R . .
  . . e C e . .
  . . e C e . .
  . . R R R . .

Navigation values:

  . . X X X . .
  . . . . . . .
  . . . . . . .
  . . X X X . .

Legend:
  R = river water
  e = visual bridge edge/deck cell
  C = bridge centerline
  X = Blocked
  . = Walkable
```

Pathfinding must never leave an authored bridge crossing non-traversable. Movement still projects bridge waypoints toward `TerrainService::get_bridge_traversal_position()` so units visually enter and exit from the middle instead of drifting into rails.

Bridge decks are fitted to the water they cross at load time by `fit_bridge_span_to_riverbanks`, which both extends a deck that stops short of a bank and trims one that runs far past it. Each end is clamped to `[required_half, required_half + bridge_bank_overhang]`, so a deck always reaches the bank and never overshoots it by more than a short abutment. Two details keep that bounded on awkward maps: the required half-length for an oblique crossing is capped at `k_max_oblique_bridge_span` times the perpendicular half-width, so a bridge authored at a shallow angle to the river does not grow without limit; and when a deck runs alongside a river without crossing its centreline at all, the nearest river within range is used as the reference instead of leaving the deck unfitted. The overhang is derived from the river width rather than the deck width, because `k_min_bridge_width` forces every deck to at least eight units for traversal reasons and that minimum should not dictate how far the structure spills onto land.

Hills are authored as connected plateau and entrance cells:

```text
Hill concept:

  X X X X X X X
  X H H H H H X
  X H P P P H X
  X H P P P H E
  X H P P P H X
  X H H H H H X
  X X X X X X X

Navigation:

  X X X X X X X
  X X X X X X X
  X X . . . X X
  X X . . . . .
  X X . . . X X
  X X X X X X X
  X X X X X X X

Legend:
  P = plateau cell
  E = entrance/ramp through the middle
  H = hill edge/slope
  X = Blocked
  . = Walkable
```

Pathfinding does not invent hill entrances. It consumes the terrain service's walkability mask, then forces authored hill entrance cells back to `Walkable` during grid construction. Plateau cells still come from the terrain mask. That means the terrain builder must guarantee that plateau cells connect through intended entrances and that edge cells remain blocked.

## Dynamic Blockers

Buildings and resources are dynamic, but not per-frame dynamic.

Buildings update the grid when:

1. A map loads or save state restores pre-existing buildings.
2. Construction finishes and the building becomes solid.
3. A building is destroyed or removed.
4. A footprint changes.

The construction preview is not written into the global navigation grid. Once a building becomes solid, `BuildingCollisionRegistry::register_building()` marks the footprint dirty. Destruction calls `unregister_building()` and marks the old footprint dirty so the next grid update rebuilds those cells from terrain plus remaining blockers.

Resources update the grid when:

1. A map loads authored trees, boulders, and iron ore.
2. A builder harvests a resource prop.

Harvesting bumps `TerrainService::world_props_revision()`. If the harvest code marked a local dirty region, `Pathfinding` keeps the update regional. If a revision change appears without a known region, it performs a full rebuild.

```text
Before harvest:

  . . . . .
  . . T . .
  . . . . .

Builder completes harvest:

  TerrainService removes prop
  world_props_revision increments
  Pathfinding marks local region dirty

After regional update:

  . . . . .
  . . . . .
  . . . . .
```

## Movement Command Flow

A normal move order follows this path:

```text
Player/AI order
    |
    v
CommandService
  - convert world target to grid
  - snap target to walkable ground
  - reject movement if unit is in RTS melee lock
  - use direct path for short clear moves
  - run A* synchronously for longer or blocked moves
  - convert grid path to world waypoints
  - pull the waypoint list taut (see below)
    |
    v
MovementSystem
  - follow waypoints
  - stop only for arrival, melee lock, hold mode, direct-control override, or explicit order reset
  - recover immediately if the unit is in an invalid cell
  - recompute the unit's own path if the next integrated step enters a blocked cell
```

The system does not continuously re-check every whole path. That would be expensive and would make large fights unstable. It checks the order up front, then movement checks the current segment and a few recovery cases.

Movement animation is not pathfinding state. `World::finalize_motion_presentation_frame()` treats a unit as moving only when it has active movement state, non-zero movement velocity, actual displacement, chase intent, direct control, or builder bypass. Stale `goal_x`, recent request history, repath cooldown, and unstuck cooldown are not movement intent and must not keep walk animation running after arrival.

## A* Search

A* runs on the navigation grid with eight-directional movement. It uses generation-stamped arrays for closed flags, costs, and parents so each search does not clear large buffers.

```text
S = start
G = goal
X = blocked
* = path

  . . . . . . . .
  . S * * . . . .
  . . X * X X . .
  . . X * * X . .
  . . X X * X . .
  . . . . * * G .
  . . . . . . . .
```

Diagonal movement is allowed only when it does not cut through blocked corners. A* does not expand cells by unit radius. This is deliberate: a single-cell bridge, hill entrance, or tight building gap is passable if the cell itself is walkable. Unit radius is used for final arrival tolerance and visual footprint concerns, not for deciding whether a route exists.

A wall laid on the diagonal, one segment thick, is the shape that tests this: every
pair of open cells across it meets corner to corner with a blocked cell on each
flank, so the wall holds only for as long as nothing is willing to squeeze through
a corner. `path_diagonal_wall_seal` in the arena is that wall.

### What a step costs

```text
straight step        10
diagonal step        14      (a diagonal really is longer)
+1  entering a cell that touches anything blocked
+1  turning instead of carrying straight on
```

Octile costs, and the heuristic is octile distance — the exact cost of the
cheapest unobstructed route, so the search stays admissible. Charging a diagonal
the same as a straight step makes every sideways detour free; the search then
wanders across a plateau of equal-cost routes and returns whichever one the heap
happened to pop. Manhattan distance has the opposite failure: it double-counts
every diagonal, overestimates by up to a factor of two, and quietly turns A* into
a greedy walk.

The two `+1`s are tie-breakers, worth a tenth of a step each:

- **Clearance is priced, never required.** A cell that touches something blocked
  costs a little more, so a route takes the middle of a corridor, a gate, a
  bridge or an alley instead of scraping along its edge. Every cell stays
  passable to every unit — in a one-cell gap both sides are priced the same and
  the gap is still a road.
- **Straightness** breaks the remaining ties, so a staircase between two points
  becomes the straight line a player expects rather than an arbitrary zigzag.

### Is this straight line clear?

`is_world_segment_walkable` answers the question movement asks before taking a
shortcut, and it answers it with the grid: it walks every cell the segment
actually touches and refuses a diagonal that squeezes between two blocked
corners — the same rule the search applies to its own neighbours. Sampling the
line at fixed intervals is a second, weaker rule that steps over the thin corner
of an obstacle and hands out a shortcut through a wall that has no gap in it.

### The path is pulled taut before it is followed

An 8-connected grid path is a staircase: a goal 27 degrees off-axis comes back
as a run of diagonal cells, then a run of straight cells, then diagonals again.
Followed cell by cell, a formation turns to face each leg in turn — the whole
20-horse body swinging 45 degrees one way and back roughly every second, at the
formation turn rate — which on screen reads as troops flicking left and right
rather than routing around anything. `assign_path_to_movement` therefore
string-pulls the waypoint list before the movement system sees it: from the
unit's position it keeps advancing the next waypoint while the straight segment
to it is walkable (`is_world_segment_walkable`, the same corner-safe test above)
and does not cross a bridge or hill-entrance portal, then keeps only the last
reachable one and continues from there. Legs across a portal stay cell by cell,
which is what keeps the bridge-alignment projection working. On
`massed_battle_1000` this took the cavalry columns from 2-8 heading reversals
of more than 15 degrees during the approach down to the two or three legs the
tree scatter actually forces, at no measurable simulation cost.

## Formations And Group Movement

The navigation grid is unit-agnostic. Formations add only initial target offsets:

```text
Grid says:
  "These cells are traversable."

FormationPlanner says:
  "Use this target offset if the slot is walkable; otherwise collapse to the center."

MovementSystem says:
  "Each unit receives and follows its own independent path."
```

Formation planning uses shared walkability queries for every slot. It does not solve packing, overlap, or passage width. Tight spaces intentionally simplify to center movement:

```text
Target center:

      slot slot slot
        \   |   /
         \  |  /
          center
         /  |  \
      slot slot slot

For each slot:
  1. Snap the center to walkable ground.
  2. Use the offset slot if that slot is walkable.
  3. Otherwise use the center target.
```

There is no special group route entity and no shared movement path. A multi-unit move is just a batch of individual unit orders. The only group-level behavior is target assignment at order time.

```text
Normal ground:

  target slots:  a b c
                 d e f

Tight bridge / hill entrance / building gap:

  target slots collapse to center:
                 c c c
                 c c c
```

## Resource Gathering And The Cursor

The resource cursor and builder movement intentionally ask different questions.

The cursor asks:

> Is there a resource cell here?

The builder asks:

> Is there a nearby walkable work cell for my radius?

```text
Cursor hover:

  . . . . .
  . . T . .    cursor may select T
  . . . . .

Builder work search:

  . w w w .
  . w T w .    builder stands on w, never on T
  . w w w .
```

The resource cell remains blocked while the resource exists. If no radius-valid work cell exists near the resource, the preview is invalid and the order is rejected. Invalid-start recovery is the only flow allowed to use a zero-radius escape fallback.

## Invalid Position Recovery

Units should not stand in invalid cells, but it can happen after map edits, save/load changes, construction finishing on top of a unit, terrain changes, or older bugs. Recovery is intentionally narrow.

```text
Normal rule:
  valid -> blocked = stop/repath

Recovery exception:
  invalid -> valid = allow escape
```

Flow:

1. `MovementSystem` checks the unit's current grid cell each frame.
2. If the unit is already invalid, it searches for a nearby walkable recovery cell.
3. If the unit had an active order, recovery splices a path from the safe cell toward the existing goal.
4. If the unit was idle, recovery only moves it to the safe cell.
5. Once the unit reaches valid ground, normal movement rules resume.

Recovery does not use stop reasons, cooldowns, pending path requests, or a separate recovery state. It assigns ordinary movement target/path data on the unit.

## Animated But Not Progressing

The current system is leaner, but a unit can still appear to be "moving forever" if animation state and physical progress disagree. That is possible because these are separate facts:

| Fact                                            | Owner                                |
| ----------------------------------------------- | ------------------------------------ |
| Unit has active movement target/path            | `MovementComponent`                  |
| Unit has non-zero velocity                      | `MovementSystem`                     |
| Unit actually changed world position this frame | `World` motion presentation snapshot |
| Unit is on a walkable cell                      | `CommandService` / `Pathfinding`     |
| Unit is visually playing walk animation         | render motion presentation           |

The dangerous state is:

```text
has_target = true
velocity != 0 or animation sees movement intent
position stays in the same invalid/blocked cell
path is not cleared because arrival never happens
recovery does not produce a different reachable cell
```

This can still happen without old stop reasons or cooldowns:

1. The next integrated movement step is reverted because it enters a blocked cell, but the unit keeps an active target and recomputes the same first blocked step next frame.
2. The unit is already invalid, but the nearest recovery cell resolves to a target that the continuous movement step cannot physically enter from its current world position.
3. The path is grid-valid, but the float-space position, rounding, arrival radius, or blocked-step revert keeps the unit oscillating around a cell boundary.
4. A movement override outside pathfinding, such as melee lock, hold mode, direct commander control, builder bypass, or combat chase, keeps presentation state active while navigation progress is zero.
5. The render/motion presentation layer treats intent or velocity as movement even when actual displacement has been zero for many frames.

Because we control the whole stack, this state should not be allowed to persist forever. The mitigation should be deterministic and centralized, not another scattered cooldown.

Recommended options:

1. **Authoritative Progress Watchdog**

    Track per-unit progress inside `MovementSystem`: current grid cell, distance-to-goal, and actual displacement. If a unit has an active target but makes no meaningful progress for a fixed number of frames, force one deterministic resolution:

    - if current cell is invalid, snap or move to nearest walkable cell
    - if current cell is valid, recompute path once from current cell
    - if the recomputed first step is still impossible, clear movement and mark animation idle

    This is the most direct mitigation for "moving animation but no progress".

2. **Hard Invalid-Cell Ejection**

    If a unit remains in an invalid cell after recovery assignment, do not keep trying ordinary steering forever. Move it smoothly but authoritatively toward the nearest walkable cell center, ignoring unit radius and formation offsets. If it cannot reduce invalid distance after a small frame budget, snap to that cell center.

    This makes invalid placement a temporary visual correction, not a navigation state.

3. **Blocked-Step Retry Budget**

    When movement integration reverts a step because the new grid cell is blocked, count consecutive reverts for that unit and target. After a small limit, invalidate the current path and choose one of:

    - recompute path from current grid
    - advance to a later waypoint if the later segment is walkable
    - collapse to nearest walkable cell and clear movement

    This prevents a unit from requesting or following the same impossible first step forever.

4. **Animation Gated By Displacement**

    Make walk animation require recent actual displacement, not just `has_target` or non-zero desired velocity. A unit may keep an active target, but if it has not moved for several frames it should visually idle or play a blocked/recovering state.

    This does not solve navigation by itself, but it prevents false feedback where the unit looks like it is walking while physically stuck.

5. **Scenario Regression Tests**

    Add frame-budget tests for the exact failure modes:

    - single unit crosses bridge
    - ten selected units cross bridge
    - unit starts inside blocked building footprint and exits
    - group crosses one-cell building gap
    - hill entrance crossing
    - dynamic blocker appears under moving unit
    - blocked first step cannot repeat forever

    Each test should assert either "arrived/reached valid cell within N frames" or "movement cleared and animation idle within N frames". No test should accept an active moving state with zero displacement after the budget.

## Melee Lock

RTS melee lock is a combat rule, not a pathfinding state. A locked unit should not be pulled out of melee by a movement command.

```text
Movement command issued
        |
        v
Is unit in valid RTS melee lock?
        |
     yes| no
        |
        v
 reject movement      continue path planning
```

`CommandService::move_unit()` rejects movement for units participating in melee lock. `MovementSystem` also clears active movement while the lock is valid and keeps combat orientation under combat control. Pathfinding does not break melee lock.

## Dirty Regions

The grid is rebuilt only when inputs change.

```text
Full rebuild:
  map load
  terrain restore
  explicit full navigation invalidation
  unknown world-prop revision change

Regional rebuild:
  building registered/destroyed/moved
  harvested resource with known position
  local footprint change
```

A regional rebuild resets just that rectangle to terrain, then reapplies buildings and resources intersecting the rectangle:

```text
Dirty region:

  . . . . . . .
  . . [-----] .
  . . [-----] .
  . . [-----] .
  . . . . . . .

Only cells inside the brackets are recomputed.
```

That is the main performance contract: changes are sparse, so updates should be sparse.

## File Responsibilities

`game/systems/pathfinding.h`
: Defines `Point`, `DirtyRegion`, `Pathfinding::CellValue`, `Pathfinding::NavigationGrid`, A* search, and dirty-region state.

`game/systems/pathfinding.cpp`
: Builds and updates the navigation grid, applies terrain/building/resource layers, forces mandatory traversal cells walkable, and runs A*.

`game/systems/command_service.cpp`
: Owns the pathfinder instance, converts world/grid coordinates, exposes shared navigation queries, resolves move targets, and issues per-unit movement.

`game/systems/movement_system.cpp`
: Follows waypoints, integrates velocity, suppresses movement during melee lock/hold/direct-control overrides, reverts blocked steps, and assigns immediate local recovery when a unit is in an invalid cell.

`game/core/world.cpp`
: Builds motion presentation snapshots from active movement/combat state. It does not own pathfinding and must not derive walk animation from stale path request bookkeeping or stale goals.

`game/systems/formation_planner.h`
: Computes initial formation target slots using shared walkability. Invalid/tight slots collapse to the resolved center target. It does not write units into the navigation grid and does not create group movement entities.

`game/systems/building_collision_registry.cpp`
: Registers and unregisters solid building footprints and marks affected grid regions dirty.

`app/core/production_manager.cpp`
: Handles construction placement, collect cursor resolution, and harvest work-position selection.

## Maintenance Rules

When adding a blocker type:

1. Add or reuse a `CellValue`.
2. Apply it in the navigation-grid build/update step.
3. Mark the affected region dirty when it appears, disappears, or changes footprint.
4. Add a focused test proving it blocks and clears correctly.

When adding a mandatory traversal feature, such as a bridge crossing or hill entrance:

1. Author the cells in `TerrainService`/height-map data.
2. Force those cells to `Walkable` after terrain, buildings, and resources are applied.
3. Keep visual centering or passage projection outside A* path search.
4. Add tests for full rebuild and regional rebuild behavior.

When changing terrain:

1. Keep mountains blocked.
2. Keep rivers blocked except bridge centerline crossings.
3. Keep hill plateau and entrance paths connected.
4. Add tests for the exact walkability contract.

## Directly Controlled Commander

The navigation grid is sized for formations: whole cells are blocked around
structures, trees, boulders and ore so that a marching body of troops keeps
clear of them. A directly controlled commander is one person and must not
inherit that margin, so `CommanderControlController` does not path — it moves a
body:

- Terrain walkability (water, mountains, hill access) still comes from the grid
  and is shared with the RTS rules.
- Structures are tested against their real footprint, not their grid padding.
- Scatter props are tested as a circle around the prop rather than as the whole
  cell they occupy, so a person can walk between trees that a formation routes
  around.
- A blocked step is resolved by dropping the blocked axis and keeping the free
  one, so the commander slides along an obstacle instead of stopping in front of
  it with the stick still pushed.

`rpg_obstacle_slide` and `rpg_close_quarters` in the arena catalog are the
regression contracts for this. Anything that makes the commander's clearance
match the formation grid again will fail them.

When changing movement behavior:

1. Use `CommandService` shared queries instead of duplicating terrain/pathfinder checks.
2. Do not write units into the navigation grid.
3. Keep invalid-position recovery narrow.
4. Keep melee lock as a combat-owned movement override.
5. Do not add public tight-passage, group-path, path-request ID, stop-reason, or movement-cooldown concepts.
6. Any "cannot progress forever" mitigation must have a scenario test with a frame budget.
