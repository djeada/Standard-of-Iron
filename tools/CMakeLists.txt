add_subdirectory(arena)
add_subdirectory(balance_sim)
add_subdirectory(map_editor)
add_subdirectory(content_validator)
add_subdirectory(terrain_probe)
add_subdirectory(movement_trace_report)
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
    # Sixty seconds, not fifteen. The `invisible_damage` defect only appeared
    # after fifteen to twenty seconds of sustained contact, so a fifteen second
    # gate passed while the sixty second soak `docs/GAMEPLAY_VERIFICATION.md`
    # asks for before a release candidate failed every scenario it ran. A gate
    # shorter than the bug is not a gate. Sixty seconds costs about five.
    add_test(
        NAME battlefield_gameplay_quality
        COMMAND battlefield_gameplay_verifier --all --seconds 60
    )
    set_tests_properties(
        battlefield_gameplay_quality
        PROPERTIES LABELS "gameplay;soak" TIMEOUT 120 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    # Every scenario, run twice from the same seed, must produce the same world
    # digest on every tick. A failure here names the tick and the entity that
    # went a different way -- the bug a lockstep multiplayer game would
    # otherwise show as two players seeing different matches.
    add_test(
        NAME simulation_determinism
        COMMAND battlefield_gameplay_verifier --all --seconds 15 --determinism-runs 2
    )
    set_tests_properties(
        simulation_determinism
        PROPERTIES LABELS "gameplay;determinism" TIMEOUT 120 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()

# ---- sim_benchmark: the repeatable large-battle workload --------------------
#
# The fixed scenario optimisation work is measured against: 1k, 5k and 10k units
# ticked a fixed number of times, reporting simulation ms, peak RSS, the
# per-system timing table and the query counters. It links the same simulation
# kernel the game does and nothing else, so a number it produces is a statement
# about the simulation and not about the renderer.
add_executable(sim_benchmark sim_benchmark/main.cpp)
target_link_libraries(sim_benchmark PRIVATE Qt${QT_VERSION_MAJOR}::Core soi_runtime soi_ai game_sim)
target_include_directories(sim_benchmark PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(sim_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---- soi_headless: the simulation with no window ---------------------------
#
# A session, the runtime system registry, the AI and a fixed-step loop, linked
# without Qt Quick or the renderer. The dedicated-server shape of the game;
# also the headless way to record and verify a replay.
add_executable(soi_headless headless/main.cpp)
target_link_libraries(soi_headless PRIVATE Qt${QT_VERSION_MAJOR}::Core soi_runtime soi_ai game_sim)
target_include_directories(soi_headless PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(soi_headless PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if(BUILD_TESTING)
    # Record a bot skirmish headless, replay it, and require every digest to
    # match: the round trip a networked match depends on, with no display.
    add_test(
        NAME headless_replay_round_trip
        COMMAND
            ${CMAKE_COMMAND} -E env SOI_HEADLESS=$<TARGET_FILE:soi_headless>
            ${CMAKE_SOURCE_DIR}/scripts/check-headless-replay.sh
    )
    set_tests_properties(
        headless_replay_round_trip
        PROPERTIES LABELS "gameplay;determinism" TIMEOUT 120 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()
