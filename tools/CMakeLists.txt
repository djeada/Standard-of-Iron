add_subdirectory(arena)
add_subdirectory(map_editor)
add_subdirectory(content_validator)
add_subdirectory(bpat_baker)

# Arena scenarios inspect the baked clips, so an Arena-only build must never
# run against assets produced by an older pose controller.
add_dependencies(arena_app bake_creature_assets)
add_subdirectory(mesh_preview)
add_subdirectory(building_preview)

add_executable(battlefield_gameplay_verifier battlefield_capture_main.cpp)
# render_gl currently calls back into map visibility owned by game_systems. Keep
# game_systems after the renderer as well so static-link builds resolve that
# dependency instead of depending on executable-specific object order.
target_link_libraries(battlefield_gameplay_verifier
    PRIVATE game_systems render_gl game_systems
)
target_include_directories(battlefield_gameplay_verifier PRIVATE
    ${CMAKE_SOURCE_DIR}/game
)
set_target_properties(battlefield_gameplay_verifier PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
add_test(NAME battlefield_gameplay_quality
    COMMAND battlefield_gameplay_verifier --all --seconds 15
)
set_tests_properties(battlefield_gameplay_quality PROPERTIES
    LABELS "gameplay;soak"
    TIMEOUT 30
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
