# How Pathfinding Works

Pathfinding in Standard of Iron is deliberately simple at the core: the game keeps one flat 2D navigation grid, and A* searches that grid. The grid is not a physics simulation, not a unit occupancy map, and not a navmesh. It is a compact answer to one question:

> Can a unit of this radius stand on or move through this cell?

Terrain, buildings, bridges, hills, and resources feed into that answer. Formations, group spacing, combat locks, and invalid-position recovery sit around it. Keeping that separation is what makes the system fast enough to use during normal RTS play without constantly revalidating every unit.

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

| Value | Name | Meaning |
| --- | --- | --- |
| `0` | `Walkable` | Free movement cell |
| `1` | `Blocked` | Mountain, river, bridge edge, building, or generic blocker |
| `2` | `Tree` | Harvestable tree blocker |
| `3` | `Boulder` | Harvestable boulder blocker |
| `4` | `IronOre` | Harvestable iron ore blocker |

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

| Function | Use |
| --- | --- |
| `is_grid_walkable_for_radius(point, radius)` | Test one grid cell using the same radius rule as A*. |
| `is_world_position_walkable_for_radius(position, radius)` | Convert world to grid, then test. |
| `find_nearest_walkable_grid(origin, max_radius, unit_radius)` | Find a nearby valid cell without changing the grid. |
| `snap_to_walkable_ground_for_radius(position, unit_radius)` | Snap orders, exits, formation slots, and delivery positions. |

This boundary matters. `Pathfinding` owns cell values and A*. `CommandService` owns coordinate conversion and high-level navigation queries. Movement, formations, production, resource gathering, and UI helpers call `CommandService`.

## Building The Grid

Every full rebuild starts with an all-free grid, then layers blockers in a fixed order:

```text
1. Start all cells as Walkable
2. Apply static terrain from TerrainService
3. Apply completed/loaded building footprints
4. Apply harvestable resource props
```

The order is important. Terrain decides where the map is physically traversable. Buildings and resources then override terrain by occupying cells.

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

Units are not written into this grid. A unit standing in a cell does not make that cell blocked for other units. Unit-to-unit spacing is handled by formation planning and movement, because writing every unit into the global grid would make group movement unstable and expensive.

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

| Question | System |
| --- | --- |
| Can I path through this terrain cell? | `Pathfinding::NavigationGrid` |
| Is an enemy standing here? | `World` entity queries and `UnitComponent::owner_id` |
| Is that enemy visible to me? | `VisibilityService` |
| Did I right-click an enemy? | `PickingService::pick_unit_first()` |
| Should AI react to this enemy? | `AISnapshotBuilder::visible_enemies` |
| Should the renderer draw this enemy? | `SceneRenderer` + fog visibility |

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

Rivers are blocked except for bridge centerlines. A bridge may be visually wide, but the navigation grid only exposes the center crossing path:

```text
River with bridge deck:

  . . R R R . .
  . . e C e . .
  . . e C e . .
  . . R R R . .

Navigation values:

  . . X X X . .
  . . X . X . .
  . . X . X . .
  . . X X X . .

Legend:
  R = river water
  e = visual bridge edge/deck cell
  C = bridge centerline
  X = Blocked
  . = Walkable
```

This is deliberate. If the entire bridge deck were walkable, groups would drift toward edges, clip rails, or cross at awkward angles. The centerline gives A* a single clean crossing. Later, movement snaps bridge waypoints to `TerrainService::get_bridge_traversal_position()` so units visually enter and exit from the middle.

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

Pathfinding does not invent hill entrances. It consumes the terrain service's walkability mask. That means the terrain builder must guarantee that hill plateau cells are connected through intended entrances and that edge cells remain blocked.

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
  - submit A* request for longer moves
    |
    v
Pathfinding worker thread
  - refresh navigation grid if dirty
  - run A*
  - publish completed path
    |
    v
CommandService::process_path_results()
  - convert grid path to world waypoints
  - snap bridge waypoints to bridge center
    |
    v
MovementSystem
  - follow waypoints
  - check next segment only
  - request recovery/repath if blocked
```

The system does not continuously re-check every whole path. That would be expensive and would make large fights unstable. It checks the order up front, then movement checks the current segment and a few recovery cases.

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

Diagonal movement is allowed only when it does not cut through blocked corners. Radius-aware checks expand the effective footprint for larger units:

```text
Small unit:

  . . .
  . u .
  . . .      ok

Large unit near blocker:

  . X .
  . U .
  . . .      rejected if the blocker intersects the unit radius
```

`Pathfinding::is_walkable_with_radius()` answers that radius question. Bridges are special-cased so units can still cross the centerline even though the visual deck may be narrow.

## Formations And Group Movement

The navigation grid is unit-agnostic. Formations add the unit-specific layer:

```text
Grid says:
  "These cells are traversable."

FormationPlanner says:
  "These specific units can stand here without overlapping."

CommandService::move_group says:
  "The leader/group route can reach the destination."
```

Formation planning uses shared walkability queries for every slot, then checks local spacing between chosen slots:

```text
Target center:

      slot slot slot
        \   |   /
         \  |  /
          center
         /  |  \
      slot slot slot

For each slot:
  1. Snap to radius-valid walkable cell.
  2. Reject if it overlaps an already placed slot.
  3. Search nearby for the closest walkable non-overlapping position.
```

Group movement computes a shared route, then offsets members around it. Bridge traversal suppresses those offsets so the group crosses on the bridge centerline instead of spreading over blocked bridge edges.

```text
Normal ground:

  leader path:  * * * * *
  member paths: offset around the leader path

Bridge:

  river:        X X X X X
  centerline:      * * *
  members:      all align to center while crossing
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

1. A movement order is attempted normally.
2. If no path exists, `CommandService` checks the unit's current cell.
3. If the unit is already invalid, it searches for a nearby walkable recovery cell.
4. During recovery, movement may pass through invalid cells just long enough to get out.
5. Once the unit reaches valid ground, normal rules resume.

This avoids the expensive alternative of constantly revalidating every unit against every dynamic blocker.

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
: Defines `Point`, `DirtyRegion`, `Pathfinding::CellValue`, `Pathfinding::NavigationGrid`, A* requests, and dirty-region state.

`game/systems/pathfinding.cpp`
: Builds and updates the navigation grid, applies terrain/building/resource layers, runs A*, and processes async path requests.

`game/systems/command_service.cpp`
: Owns the pathfinder instance, converts world/grid coordinates, exposes shared navigation queries, issues individual/group movement, and handles invalid-start recovery.

`game/systems/movement_system.cpp`
: Follows waypoints, checks current movement segments, aligns bridge traversal, suppresses movement during melee lock, and queues local recovery or repath requests.

`game/systems/formation_planner.h`
: Computes formation slots using shared walkability plus local spacing checks. It does not write units into the navigation grid.

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

When changing terrain:

1. Keep mountains blocked.
2. Keep rivers blocked except bridge centerline crossings.
3. Keep hill plateau and entrance paths connected.
4. Add tests for the exact walkability contract.

When changing movement behavior:

1. Use `CommandService` shared queries instead of duplicating terrain/pathfinder checks.
2. Do not write units into the navigation grid.
3. Keep invalid-position recovery narrow.
4. Keep melee lock as a combat-owned movement override.
