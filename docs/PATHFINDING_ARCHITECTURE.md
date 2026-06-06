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
| `is_grid_walkable(point)` | Test one grid cell against the current navigation grid. |
| `is_world_position_walkable(position)` | Convert world to grid, then test. |
| `find_nearest_walkable_grid(origin, max_search_radius)` | Find a nearby valid cell without changing the grid. |
| `snap_to_walkable_ground(position, max_search_radius)` | Snap orders, exits, formation slots, and delivery positions. |

This boundary matters. `Pathfinding` owns cell values and A*. `CommandService` owns coordinate conversion and high-level navigation queries. Movement, formations, production, resource gathering, and UI helpers call `CommandService`.

## Building The Grid

Every full rebuild starts with an all-free grid, then layers blockers in a fixed order:

```text
1. Start all cells as Walkable
2. Apply static terrain from TerrainService
3. Apply completed/loaded building footprints
4. Apply harvestable resource props
5. Force mandatory traversal cells back to Walkable
```

The order is important. Terrain decides where the map is physically traversable. Buildings and resources then override terrain by occupying cells. Mandatory traversal cells are applied last so bridge crossings and hill entrances cannot be accidentally blocked by broad terrain/resource/building masks.

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

| Fact | Owner |
| --- | --- |
| Unit has active movement target/path | `MovementComponent` |
| Unit has non-zero velocity | `MovementSystem` |
| Unit actually changed world position this frame | `World` motion presentation snapshot |
| Unit is on a walkable cell | `CommandService` / `Pathfinding` |
| Unit is visually playing walk animation | render motion presentation |

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

When changing movement behavior:

1. Use `CommandService` shared queries instead of duplicating terrain/pathfinder checks.
2. Do not write units into the navigation grid.
3. Keep invalid-position recovery narrow.
4. Keep melee lock as a combat-owned movement override.
5. Do not add public tight-passage, group-path, path-request ID, stop-reason, or movement-cooldown concepts.
6. Any "cannot progress forever" mitigation must have a scenario test with a frame budget.
