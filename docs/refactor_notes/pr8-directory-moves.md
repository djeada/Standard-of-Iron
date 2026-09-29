# PR 8: game/systems directory moves

Mechanical moves only (git mv plus path rewrites); no behaviour change. Membership follows module ownership in `scripts/module_rules.json`, so each directory is exactly the file set of the CMake domain of the same name (`soi_navigation`, `soi_movement`, `soi_economy`, `soi_persistence`). Include roots are unchanged (`systems/<domain>/x.h`); no forwarding headers were left behind.

Not moved: `combat_system/`, `combat_actions/`, `rpg_combat_system/`, `ai_system/`, and everything owned by other modules (domain_types, registries, world, simulation, mission_runtime, runtime). For example `building_collision_registry` and `nav_grid_types` stay at the top level because `module_rules.json` gives them to `registries` and `domain_types`.

Budgets keyed by directory (`entity_access`, `world_scan`, `ambient_instance`) bucket by the first two path components, so `game/systems` already covers the new subdirectories; the numbers are unchanged and need no rekeying.

Map/content versus mission runtime is documented in `game/README.md` (`game/map/` vs `game/mission/`).

## navigation/ (21 files)

| Old                                         | New                                                    |
| ------------------------------------------- | ------------------------------------------------------ |
| `game/systems/gate_service.h`               | `game/systems/navigation/gate_service.h`               |
| `game/systems/gate_service.cpp`             | `game/systems/navigation/gate_service.cpp`             |
| `game/systems/nav_grid.h`                   | `game/systems/navigation/nav_grid.h`                   |
| `game/systems/nav_grid.cpp`                 | `game/systems/navigation/nav_grid.cpp`                 |
| `game/systems/navigation_service.h`         | `game/systems/navigation/navigation_service.h`         |
| `game/systems/navigation_service.cpp`       | `game/systems/navigation/navigation_service.cpp`       |
| `game/systems/pathfinding.h`                | `game/systems/navigation/pathfinding.h`                |
| `game/systems/pathfinding.cpp`              | `game/systems/navigation/pathfinding.cpp`              |
| `game/systems/pathfinding_astar.cpp`        | `game/systems/navigation/pathfinding_astar.cpp`        |
| `game/systems/pathfinding_clearance.cpp`    | `game/systems/navigation/pathfinding_clearance.cpp`    |
| `game/systems/pathfinding_dirty.cpp`        | `game/systems/navigation/pathfinding_dirty.cpp`        |
| `game/systems/pathfinding_grid_build.cpp`   | `game/systems/navigation/pathfinding_grid_build.cpp`   |
| `game/systems/pathfinding_regions.cpp`      | `game/systems/navigation/pathfinding_regions.cpp`      |
| `game/systems/route_corridor_planner.h`     | `game/systems/navigation/route_corridor_planner.h`     |
| `game/systems/route_corridor_planner.cpp`   | `game/systems/navigation/route_corridor_planner.cpp`   |
| `game/systems/terrain_alignment_system.h`   | `game/systems/navigation/terrain_alignment_system.h`   |
| `game/systems/terrain_alignment_system.cpp` | `game/systems/navigation/terrain_alignment_system.cpp` |
| `game/systems/walkability.h`                | `game/systems/navigation/walkability.h`                |
| `game/systems/walkability.cpp`              | `game/systems/navigation/walkability.cpp`              |
| `game/systems/wall_network_service.h`       | `game/systems/navigation/wall_network_service.h`       |
| `game/systems/wall_network_service.cpp`     | `game/systems/navigation/wall_network_service.cpp`     |

## movement/ (54 files)

| Old                                               | New                                                        |
| ------------------------------------------------- | ---------------------------------------------------------- |
| `game/systems/body_contact_system.h`              | `game/systems/movement/body_contact_system.h`              |
| `game/systems/body_contact_system.cpp`            | `game/systems/movement/body_contact_system.cpp`            |
| `game/systems/body_profile.h`                     | `game/systems/movement/body_profile.h`                     |
| `game/systems/body_profile.cpp`                   | `game/systems/movement/body_profile.cpp`                   |
| `game/systems/command_service.h`                  | `game/systems/movement/command_service.h`                  |
| `game/systems/command_service.cpp`                | `game/systems/movement/command_service.cpp`                |
| `game/systems/formation_move_dispatch_system.h`   | `game/systems/movement/formation_move_dispatch_system.h`   |
| `game/systems/formation_move_dispatch_system.cpp` | `game/systems/movement/formation_move_dispatch_system.cpp` |
| `game/systems/local_avoidance_system.h`           | `game/systems/movement/local_avoidance_system.h`           |
| `game/systems/local_avoidance_system.cpp`         | `game/systems/movement/local_avoidance_system.cpp`         |
| `game/systems/movement_orders.cpp`                | `game/systems/movement/movement_orders.cpp`                |
| `game/systems/movement_orders_assignment.h`       | `game/systems/movement/movement_orders_assignment.h`       |
| `game/systems/movement_orders_assignment.cpp`     | `game/systems/movement/movement_orders_assignment.cpp`     |
| `game/systems/movement_orders_group.h`            | `game/systems/movement/movement_orders_group.h`            |
| `game/systems/movement_orders_group.cpp`          | `game/systems/movement/movement_orders_group.cpp`          |
| `game/systems/movement_orders_prepared.h`         | `game/systems/movement/movement_orders_prepared.h`         |
| `game/systems/movement_orders_targets.h`          | `game/systems/movement/movement_orders_targets.h`          |
| `game/systems/movement_orders_targets.cpp`        | `game/systems/movement/movement_orders_targets.cpp`        |
| `game/systems/movement_pipeline.h`                | `game/systems/movement/movement_pipeline.h`                |
| `game/systems/movement_pipeline.cpp`              | `game/systems/movement/movement_pipeline.cpp`              |
| `game/systems/movement_route.h`                   | `game/systems/movement/movement_route.h`                   |
| `game/systems/movement_route.cpp`                 | `game/systems/movement/movement_route.cpp`                 |
| `game/systems/movement_system.h`                  | `game/systems/movement/movement_system.h`                  |
| `game/systems/movement_system.cpp`                | `game/systems/movement/movement_system.cpp`                |
| `game/systems/movement_system_collision.h`        | `game/systems/movement/movement_system_collision.h`        |
| `game/systems/movement_system_collision.cpp`      | `game/systems/movement/movement_system_collision.cpp`      |
| `game/systems/movement_system_duel_footwork.h`    | `game/systems/movement/movement_system_duel_footwork.h`    |
| `game/systems/movement_system_duel_footwork.cpp`  | `game/systems/movement/movement_system_duel_footwork.cpp`  |
| `game/systems/movement_system_gates.h`            | `game/systems/movement/movement_system_gates.h`            |
| `game/systems/movement_system_gates.cpp`          | `game/systems/movement/movement_system_gates.cpp`          |
| `game/systems/movement_system_heading.h`          | `game/systems/movement/movement_system_heading.h`          |
| `game/systems/movement_system_heading.cpp`        | `game/systems/movement/movement_system_heading.cpp`        |
| `game/systems/movement_system_motor.h`            | `game/systems/movement/movement_system_motor.h`            |
| `game/systems/movement_system_motor.cpp`          | `game/systems/movement/movement_system_motor.cpp`          |
| `game/systems/movement_system_mover.h`            | `game/systems/movement/movement_system_mover.h`            |
| `game/systems/movement_system_path_requests.h`    | `game/systems/movement/movement_system_path_requests.h`    |
| `game/systems/movement_system_path_requests.cpp`  | `game/systems/movement/movement_system_path_requests.cpp`  |
| `game/systems/order_service.h`                    | `game/systems/movement/order_service.h`                    |
| `game/systems/order_service.cpp`                  | `game/systems/movement/order_service.cpp`                  |
| `game/systems/route_follow_system.h`              | `game/systems/movement/route_follow_system.h`              |
| `game/systems/route_follow_system.cpp`            | `game/systems/movement/route_follow_system.cpp`            |
| `game/systems/route_follow_system_arrival.h`      | `game/systems/movement/route_follow_system_arrival.h`      |
| `game/systems/route_follow_system_arrival.cpp`    | `game/systems/movement/route_follow_system_arrival.cpp`    |
| `game/systems/route_follow_system_frame.h`        | `game/systems/movement/route_follow_system_frame.h`        |
| `game/systems/route_follow_system_gate.h`         | `game/systems/movement/route_follow_system_gate.h`         |
| `game/systems/route_follow_system_gate.cpp`       | `game/systems/movement/route_follow_system_gate.cpp`       |
| `game/systems/route_follow_system_progress.h`     | `game/systems/movement/route_follow_system_progress.h`     |
| `game/systems/route_follow_system_progress.cpp`   | `game/systems/movement/route_follow_system_progress.cpp`   |
| `game/systems/route_follow_system_stall.h`        | `game/systems/movement/route_follow_system_stall.h`        |
| `game/systems/route_follow_system_stall.cpp`      | `game/systems/movement/route_follow_system_stall.cpp`      |
| `game/systems/route_follow_system_steering.h`     | `game/systems/movement/route_follow_system_steering.h`     |
| `game/systems/route_follow_system_steering.cpp`   | `game/systems/movement/route_follow_system_steering.cpp`   |
| `game/systems/unit_traversal_layout_system.h`     | `game/systems/movement/unit_traversal_layout_system.h`     |
| `game/systems/unit_traversal_layout_system.cpp`   | `game/systems/movement/unit_traversal_layout_system.cpp`   |

## economy/ (44 files)

| Old                                               | New                                                       |
| ------------------------------------------------- | --------------------------------------------------------- |
| `game/systems/build_site.h`                       | `game/systems/economy/build_site.h`                       |
| `game/systems/build_site.cpp`                     | `game/systems/economy/build_site.cpp`                     |
| `game/systems/capture_system.h`                   | `game/systems/economy/capture_system.h`                   |
| `game/systems/capture_system.cpp`                 | `game/systems/economy/capture_system.cpp`                 |
| `game/systems/civilian_delivery_system.h`         | `game/systems/economy/civilian_delivery_system.h`         |
| `game/systems/civilian_delivery_system.cpp`       | `game/systems/economy/civilian_delivery_system.cpp`       |
| `game/systems/construction_cost_catalog.h`        | `game/systems/economy/construction_cost_catalog.h`        |
| `game/systems/construction_cost_catalog.cpp`      | `game/systems/economy/construction_cost_catalog.cpp`      |
| `game/systems/farm_system.h`                      | `game/systems/economy/farm_system.h`                      |
| `game/systems/farm_system.cpp`                    | `game/systems/economy/farm_system.cpp`                    |
| `game/systems/food_targets.h`                     | `game/systems/economy/food_targets.h`                     |
| `game/systems/food_targets.cpp`                   | `game/systems/economy/food_targets.cpp`                   |
| `game/systems/gather_loop_system.h`               | `game/systems/economy/gather_loop_system.h`               |
| `game/systems/gather_loop_system.cpp`             | `game/systems/economy/gather_loop_system.cpp`             |
| `game/systems/marketplace_system.h`               | `game/systems/economy/marketplace_system.h`               |
| `game/systems/marketplace_system.cpp`             | `game/systems/economy/marketplace_system.cpp`             |
| `game/systems/nation_collapse_service.h`          | `game/systems/economy/nation_collapse_service.h`          |
| `game/systems/nation_collapse_service.cpp`        | `game/systems/economy/nation_collapse_service.cpp`        |
| `game/systems/production_service.h`               | `game/systems/economy/production_service.h`               |
| `game/systems/production_service.cpp`             | `game/systems/economy/production_service.cpp`             |
| `game/systems/production_system.h`                | `game/systems/economy/production_system.h`                |
| `game/systems/production_system.cpp`              | `game/systems/economy/production_system.cpp`              |
| `game/systems/production_system_approach.h`       | `game/systems/economy/production_system_approach.h`       |
| `game/systems/production_system_approach.cpp`     | `game/systems/economy/production_system_approach.cpp`     |
| `game/systems/production_system_builder.h`        | `game/systems/economy/production_system_builder.h`        |
| `game/systems/production_system_builder.cpp`      | `game/systems/economy/production_system_builder.cpp`      |
| `game/systems/production_system_builder_task.h`   | `game/systems/economy/production_system_builder_task.h`   |
| `game/systems/production_system_builder_task.cpp` | `game/systems/economy/production_system_builder_task.cpp` |
| `game/systems/production_system_construction.h`   | `game/systems/economy/production_system_construction.h`   |
| `game/systems/production_system_construction.cpp` | `game/systems/economy/production_system_construction.cpp` |
| `game/systems/production_system_food.h`           | `game/systems/economy/production_system_food.h`           |
| `game/systems/production_system_food.cpp`         | `game/systems/economy/production_system_food.cpp`         |
| `game/systems/production_system_repair.h`         | `game/systems/economy/production_system_repair.h`         |
| `game/systems/production_system_repair.cpp`       | `game/systems/economy/production_system_repair.cpp`       |
| `game/systems/production_system_shared_site.h`    | `game/systems/economy/production_system_shared_site.h`    |
| `game/systems/production_system_shared_site.cpp`  | `game/systems/economy/production_system_shared_site.cpp`  |
| `game/systems/production_system_site_ghosts.h`    | `game/systems/economy/production_system_site_ghosts.h`    |
| `game/systems/production_system_site_ghosts.cpp`  | `game/systems/economy/production_system_site_ghosts.cpp`  |
| `game/systems/production_system_training.h`       | `game/systems/economy/production_system_training.h`       |
| `game/systems/production_system_training.cpp`     | `game/systems/economy/production_system_training.cpp`     |
| `game/systems/production_system_wall.h`           | `game/systems/economy/production_system_wall.h`           |
| `game/systems/production_system_wall.cpp`         | `game/systems/economy/production_system_wall.cpp`         |
| `game/systems/resource_delivery_system.h`         | `game/systems/economy/resource_delivery_system.h`         |
| `game/systems/resource_delivery_system.cpp`       | `game/systems/economy/resource_delivery_system.cpp`       |

## persistence/ (12 files)

| Old                                       | New                                                   |
| ----------------------------------------- | ----------------------------------------------------- |
| `game/systems/save_campaign_catalog.h`    | `game/systems/persistence/save_campaign_catalog.h`    |
| `game/systems/save_campaign_catalog.cpp`  | `game/systems/persistence/save_campaign_catalog.cpp`  |
| `game/systems/save_campaign_progress.cpp` | `game/systems/persistence/save_campaign_progress.cpp` |
| `game/systems/save_format.h`              | `game/systems/persistence/save_format.h`              |
| `game/systems/save_format.cpp`            | `game/systems/persistence/save_format.cpp`            |
| `game/systems/save_load_service.h`        | `game/systems/persistence/save_load_service.h`        |
| `game/systems/save_load_service.cpp`      | `game/systems/persistence/save_load_service.cpp`      |
| `game/systems/save_schema.cpp`            | `game/systems/persistence/save_schema.cpp`            |
| `game/systems/save_slots.cpp`             | `game/systems/persistence/save_slots.cpp`             |
| `game/systems/save_sql.h`                 | `game/systems/persistence/save_sql.h`                 |
| `game/systems/save_storage.h`             | `game/systems/persistence/save_storage.h`             |
| `game/systems/save_storage.cpp`           | `game/systems/persistence/save_storage.cpp`           |
