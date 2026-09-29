# WP-E: army formation planner split

`game/formation/army_formation_planner.cpp` was 2,763 lines. `plan` stays an ordered composition:
collect inputs, resolve doctrine, build the local layout, assign members, fit and place on terrain.

## Where things went

| File                                       | Owns                                                                               | Session or world services                                               |
| ------------------------------------------ | ---------------------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| `army_formation_planner.cpp`               | `plan`, `fit_to_ground`, `build_layout`, `place`, `layout_signature`, scatter      | `DoctrineRegistry`, `ArmyFormationRegistry` lookup, nav walkable anchor |
| `army_formation_members.cpp`               | `collect_members`, `measure_footprint`, `shape_member_for_intent`, doctrine vote   | world, nation registry, troop registries                                |
| `army_formation_plan.cpp`                  | `ArmyFormationPlan` accessors, `keeps_shape`                                       | none                                                                    |
| `formation_line_layout.cpp`                | line assignment, centre block and flank rows, jitter, row budget                   | none                                                                    |
| `formation_slot_adjust.cpp`                | overlap resolution, recentre, reserve rows, ranged placement, frontage scale       | none                                                                    |
| `formation_silhouette.cpp` / `_shapes.cpp` | `regularize_silhouette` as `SilhouetteLayout` steps; hollow square and crescent    | none                                                                    |
| `formation_row_split.cpp`                  | row-count arithmetic for every silhouette                                          | none                                                                    |
| `formation_assignment.cpp`                 | Hungarian solver, nearest assignment, fold onto reference, keep previous occupants | none                                                                    |
| `formation_footprint_claims.cpp`           | spatial hash of claimed footprints                                                 | none                                                                    |
| `formation_slot_fitter.cpp`                | terrain fit of one slot (rings, snap to walkable)                                  | `NavGrid`, `Pathfinding`                                                |
| `formation_frame.*`                        | `Bounds`, frame axes, `rotate_offset`, `local_overlap`                             | none                                                                    |

All helpers live in `Game::Formation::planning`. The public `ArmyFormationPlanner` statics did not
change signature; `plan_local_slots`, `min_cost_assignment` and `fold_onto_reference` delegate.

## Local layout versus world placement

Everything up to and including `build_layout` produces `ArmyFormationLayout`: slot offsets in the
formation's own frame, footprints, occupants. Only `place` (and `SlotTerrainFitter`) turns those into
world positions, and only they read the nav grid. The assignment and silhouette code receive plain values
(`SlotExtents`, `NearestAssignmentInput`, slot key vectors) rather than the member map or the plan.

## Exactness

Floating point expressions were moved verbatim, including redundant-looking forms such as
`(bodies + gap * n) - gap * n - outer` in the frontage gap, because the result feeds slot positions that
replays digest. `regularize_silhouette` used a lookup from slot occupant to member; occupants never
change while it runs, so the half extents are precomputed per slot index (default 0.5 when unknown).

## Split of the two long functions

`regularize_silhouette` (330 lines) became `SilhouetteLayout::run` plus partition, sizing, defensive
square, row build, depth fit, row placement and wing placement steps. `plan_local_slots` (190 lines) became
`plan_line_layout` over `assign_lines`, `measure_lines`, `emit_lines` and `shape_and_settle`.

## Verification and the -ffast-math caveat

A scratch test (removed afterwards) dumped hex-float plans and layouts for 1,386 cases: seven intents,
four flank preferences, one to 71 members, mixed and Carthaginian troops, assign-nearest, frontage,
reserve rows, ranged placement, flat ground and a river with a bridge.

- Built with `-fno-fast-math -ffp-contract=off` for the planner files, before and after are byte identical.
  The logic did not change.
- Built with the shipping flags (`-O3 -ffast-math`), 476 cases differ by one ULP in a local offset. Moving
  arithmetic into other functions and translation units changes what the compiler is allowed to
  reassociate. Slot ids, roles, ranks, files, occupants and statuses were not compared separately, but
  `soi_headless bot_skirmish 20 7` still gives digest 16297127701076550023.
- The baseline planner already dies with SIGFPE (integer division by zero) on 16 of those cases
  (Column intent, two Carthaginian members, `frontage_scale` 0.7 and related option mixes). The refactor
  crashes on exactly the same cases; nothing was changed for that.
