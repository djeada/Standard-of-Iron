# Headless balance harness. Deliberately links no GUI/QML module so it can run
# in CI without a display server.
#
# game_sim rather than game_systems: it ticks the combat, movement and healing
# systems directly and never asks for a camera, an opponent or a save. Linking
# the facade would have hidden that, and would have put the renderer-facing
# game_view into every binary that consumes this harness.
add_library(balance_sim_harness STATIC balance_fixture.cpp battle_simulation.cpp balance_report.cpp)

target_include_directories(balance_sim_harness PUBLIC "${CMAKE_SOURCE_DIR}")
target_link_libraries(balance_sim_harness PUBLIC Qt${QT_VERSION_MAJOR}::Core engine_core game_sim)

add_executable(balance_sim main.cpp)
target_include_directories(balance_sim PRIVATE "${CMAKE_SOURCE_DIR}")
target_link_libraries(balance_sim PRIVATE Qt${QT_VERSION_MAJOR}::Core balance_sim_harness)

set_target_properties(
    balance_sim
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
)
