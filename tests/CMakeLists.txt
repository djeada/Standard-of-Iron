# Test suite for Standard of Iron
# Uses Google Test framework
#
# The suite is split by the boundary each binary exercises, not by convenience.
# Each one links the narrowest set of production targets its tests need, so the
# link line is a statement about what that domain is allowed to depend on:
#
#   simulation_tests   the kernel. Links engine_core + game_sim and nothing
#                      else, so a gameplay file that starts needing a camera,
#                      the renderer, the AI or the save stack breaks the link
#                      step rather than slipping in unnoticed.
#   combat_balance_tests
#                      combat, formations and the headless battles that drive
#                      the real system registry. Adds soi_runtime; still no AI.
#   ai_tests           the computer opponent. Adds soi_ai.
#   campaign_tests     mission and campaign content, and the runtime that
#                      applies it: what the shipped files say, whether the
#                      loaders agree, and whether setting a mission up in a live
#                      world produces the forces and waves it names. Adds
#                      soi_missions and soi_mission_runtime. No client: no
#                      renderer, no camera, no view models.
#   persistence_tests  the snapshot contract and the save stack. Adds
#                      soi_persistence.
#   render_tests       the renderer and the creature pipeline.
#   app_tests          the application layer: view models, controllers, the
#                      widget shell and the gameplay services that need a
#                      camera or the app layer to be meaningful.
#   arena_tests        the arena harness and its panels.
#   tools_tests        the map editor document model and the balance harness.
#                      Links neither render_gl nor game_view.
#   design_system_qml_tests
#                      the QML design system, driven by QtQuickTest.
#
# Three rules keep this honest:
#
#  - a test binary links production targets; it never re-lists a production
#    `.cpp` in its own source list. A separately compiled copy can pass while
#    the object the game ships is broken.
#  - a test binary links the domains it uses and no more. Reaching for
#    `game_systems` because it is convenient puts the AI and the save database
#    into a binary that was supposed to prove it did not need them.
#  - no test is quarantined out of the default path, and no test source is
#    listed behind an `if(EXISTS)`. If a test cannot build or cannot pass, it is
#    fixed or it is deleted with an issue, not omitted.

# Registers a binary with CTest and parks it beside the other executables.
function(soi_register_test_binary target)
    set_target_properties(
        ${target}
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" AUTOMOC ON
    )
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/game)
    # DISCOVERY_TIMEOUT defaults to 5 seconds, which is the time budget for
    # running the binary once with --gtest_list_tests. That is ample for an
    # optimised build and nowhere near enough for an instrumented one: under
    # AddressSanitizer this suite takes about 23 seconds just to reach main and
    # print its test names. The weekly sanitizer lanes therefore failed during
    # discovery on every run -- "Process terminated due to timeout", before a
    # single test executed. The value is a ceiling, not a delay, so raising it
    # costs a fast build nothing.
    gtest_discover_tests(
        ${target}
        DISCOVERY_MODE PRE_TEST
        DISCOVERY_TIMEOUT 300
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endfunction()

include(GoogleTest)

# One file per suite under suites/, each include()d into this scope. A suite's
# file owns its executable, its sources and its link line; this file owns the
# shared registration function and soi_test_binaries below, which names every
# suite scripts/run-tests.sh runs.
include(${CMAKE_CURRENT_SOURCE_DIR}/suites/simulation_tests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/suites/combat_balance_tests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/suites/ai_tests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/suites/campaign_tests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/suites/persistence_tests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/suites/render_tests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/suites/app_tests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/suites/arena_tests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/suites/tools_tests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/suites/model_tests.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/suites/design_system_qml_tests.cmake)

# Everything `make test` runs. Keep this list and scripts/run-tests.sh in step;
# tests/architecture/module_boundary_test.cpp fails if they drift.
add_custom_target(
    soi_test_binaries
    DEPENDS
        simulation_tests
        combat_balance_tests
        ai_tests
        campaign_tests
        persistence_tests
        render_tests
        app_tests
        arena_tests
        tools_tests
)
add_dependencies(soi_test_binaries translations_qm)
if(TARGET design_system_qml_tests)
    add_dependencies(soi_test_binaries design_system_qml_tests)
endif()

# The gameplay acceptance gate. scripts/run-tests.sh treats a missing binary as
# an error rather than a skip, so it has to be built alongside the unit suites.
if(TARGET battlefield_gameplay_verifier)
    add_dependencies(soi_test_binaries battlefield_gameplay_verifier)
endif()

# The headless replay round trip in scripts/run-tests.sh drives this binary, and
# a missing one is an error there rather than a skip.
if(TARGET soi_headless)
    add_dependencies(soi_test_binaries soi_headless)
endif()
