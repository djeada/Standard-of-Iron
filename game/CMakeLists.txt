add_library(game_systems STATIC
    systems/movement_system.cpp
    systems/combat_system.cpp
    systems/ai_system.cpp
    systems/pathfinding.cpp
    systems/selection_system.cpp
)

target_include_directories(game_systems PUBLIC .)
target_link_libraries(game_systems PUBLIC Qt6::Core engine_core)