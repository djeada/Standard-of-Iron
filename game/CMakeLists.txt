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
target_link_libraries(engine_core PUBLIC Qt6::Core)

add_library(game_systems STATIC
    systems/movement_system.cpp
    systems/combat_system.cpp
    systems/ai_system.cpp
    systems/pathfinding.cpp
    systems/selection_system.cpp
    systems/arrow_system.cpp
    systems/production_system.cpp
    map/map_loader.cpp
    map/map_transformer.cpp
    map/environment.cpp
    visuals/visual_catalog.cpp
    units/unit.cpp
    units/archer.cpp
    units/factory.cpp
    units/barracks.cpp
)

target_include_directories(game_systems PUBLIC .)
target_link_libraries(game_systems PUBLIC Qt6::Core Qt6::Gui engine_core render_gl)