# WP-H1: `production_system.cpp` split

`ProductionSystem::update` was one 656-line function holding troop training, builder task state machines, shared construction crews and site ghosts. It is now four calls, each owned by a file with the `production_system_` prefix (the module map matches by stem, so each file is listed in `scripts/module_rules.json`).

| Stage in `update`                                  | Home                                | Owns                                                                                                                                      |
| -------------------------------------------------- | ----------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| `tick_completion_effects`, `advance_unit_training` | `production_system_training.cpp`    | Recruit queue, troop cap, exit position, rally, recruit flare and cue                                                                     |
| `advance_builders`                                 | `production_system_builder.cpp`     | Per-builder tick: fault timer, wall/food refresh, approach vs working vs idle, work timer, dismantle, repair pass, completion bookkeeping |
| `sync_site_ghosts`                                 | `production_system_site_ghosts.cpp` | Ghost previews of shared construction sites (match, refresh, raise, reap)                                                                 |

Builder collaborators (namespace `Game::Systems::ProductionTasks`):

- `production_system_approach.cpp`: walking to a site, arrival test, bypass reach, progress-based give-up (30 s), work-facing.
- `production_system_food.cpp`: grain harvest and sheep slaughter validation, sheep chase and hold, reward, gather order.
- `production_system_wall.cpp`: queued wall site assignment, lost/invalid wall site recovery with refund, wall progress publication.
- `production_system_repair.cpp`: repair tick, repair presentation, repair completion.
- `production_system_shared_site.cpp`: crews sharing one site pool their hands; `advance_shared_sites` returns the finishing lead and crew.
- `production_system_construction.cpp`: `raise_structure` (pose, spawn type, clear-site search, refund, spawn, wall site teardown).
- `production_system_builder_task.cpp`: task-target reset, hauler loading, tree/stone/iron harvest completion.

## Preserved invariants

- Order inside `update` is unchanged: completion timers, training, shared-site pace, builder loop, ghost sync.
- In the builder loop every original `continue` maps to an early `return` of the same stage: food abandon, approach (whether or not a transform exists), idle bookkeeping, out-of-range interrupt, dismantle, repair pass, and `StructureOutcome::Skipped` (invalid wall site, unknown product, no clear site).
- Training: a hit troop cap returns before queue promotion; `produced_count` still advances when the factory registry is missing or spawn fails.
- The finishing-crew list still comes from `advance_shared_sites`, computed before the builder loop.

## Structure notes

- `BuilderTick` in `production_system_builder.cpp` is a per-entity view of already-fetched components, not a container of system state; nothing here mutates it beyond what the original locals did.
- The builder-facing functions take the components they act on; none reaches back into the system.
