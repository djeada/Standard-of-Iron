# Writes assets/maps/map_aurelia_magna.json from the authored city plan.
#
# The city is laid out in code -- rings, districts, a planner that rejects
# overlapping plots -- and that plan is the *authoring* source, in the same way
# scripts/generate-map-settlements.py is the authoring source for the campaign
# settlements. What the arena and the mission both *load* is the JSON this
# writes, so the promo scenario and the playable siege can never drift apart.
add_executable(city_export main.cpp aurelia_magna_plan.cpp)

target_link_libraries(
    city_export
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        arena_scenario_harness
        soi_missions
        game_sim
)

target_include_directories(city_export PRIVATE ${CMAKE_SOURCE_DIR})

set_target_properties(city_export PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
