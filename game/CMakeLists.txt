# Core ECS library (moved from engine/core to game/core)
add_library(engine_core STATIC
    core/entity.cpp
    core/component.cpp
    core/system.cpp
    core/world.cpp
    core/event_manager.cpp
    core/serialization.cpp
)

target_include_directories(engine_core PUBLIC .)
target_link_libraries(engine_core PUBLIC Qt${QT_VERSION_MAJOR}::Core)

add_library(game_systems STATIC
    systems/movement_system.cpp
    systems/combat_system.cpp
    systems/ai_system.cpp
    systems/ai_system/ai_command_applier.cpp
    systems/ai_system/ai_command_filter.cpp
    systems/ai_system/ai_snapshot_builder.cpp
    systems/ai_system/ai_reasoner.cpp
    systems/ai_system/ai_executor.cpp
    systems/ai_system/ai_worker.cpp
    systems/ai_system/ai_tactical.cpp
    systems/ai_system/behaviors/production_behavior.cpp
    systems/ai_system/behaviors/gather_behavior.cpp
    systems/ai_system/behaviors/attack_behavior.cpp
    systems/ai_system/behaviors/defend_behavior.cpp
    systems/ai_system/behaviors/retreat_behavior.cpp
    systems/patrol_system.cpp
    systems/guard_system.cpp
    systems/pathfinding.cpp
    systems/building_collision_registry.cpp
    systems/selection_system.cpp
    systems/arrow_system.cpp
    systems/camera_follow_system.cpp
    systems/camera_controller.cpp
    systems/camera_service.cpp
    systems/picking_service.cpp
    systems/command_service.cpp
    systems/production_service.cpp
    systems/production_system.cpp
    systems/terrain_alignment_system.cpp
    systems/owner_registry.cpp
    systems/troop_count_registry.cpp
    systems/victory_service.cpp
    systems/nation_registry.cpp
    systems/formation_system.cpp
    map/map_loader.cpp
    map/level_loader.cpp
    map/map_transformer.cpp
    map/environment.cpp
    map/terrain.cpp
    map/terrain_service.cpp
    map/visibility_service.cpp
    map/world_bootstrap.cpp
    map/map_catalog.cpp
    map/skirmish_loader.cpp
    visuals/visual_catalog.cpp
    units/unit.cpp
    units/archer.cpp
    units/factory.cpp
    units/barracks.cpp
)

target_include_directories(game_systems PUBLIC .)
target_link_libraries(game_systems PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui engine_core render_gl)