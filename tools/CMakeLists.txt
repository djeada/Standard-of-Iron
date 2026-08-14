add_subdirectory(arena)
add_subdirectory(balance_sim)
add_subdirectory(map_editor)
add_subdirectory(content_validator)
add_subdirectory(bpat_baker)
add_subdirectory(audio_master)
add_subdirectory(audio_synth)

# Arena scenarios inspect the baked clips, so an Arena-only build must never
# run against assets produced by an older pose controller.
add_dependencies(arena_app bake_creature_assets)
add_subdirectory(mesh_preview)
add_subdirectory(building_preview)
add_subdirectory(humanoid_preview)
add_subdirectory(wildlife_preview)

add_executable(battlefield_gameplay_verifier battlefield_capture_main.cpp)
# soi_runtime is what this drives: the production system registry, one tick at a
# time, asserting on movement, combat, responsiveness and AI. render_gl calls
# back into map visibility owned by the kernel, so game_sim is repeated after the
# renderer for static-link builds to resolve that without depending on
# executable-specific object order.
target_link_libraries(battlefield_gameplay_verifier PRIVATE soi_runtime render_gl game_sim)
target_include_directories(battlefield_gameplay_verifier PRIVATE ${CMAKE_SOURCE_DIR}/game)
set_target_properties(
    battlefield_gameplay_verifier
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
if(BUILD_TESTING)
    add_test(
        NAME battlefield_gameplay_quality
        COMMAND battlefield_gameplay_verifier --all --seconds 15
    )
    set_tests_properties(
        battlefield_gameplay_quality
        PROPERTIES LABELS "gameplay;soak" TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()
