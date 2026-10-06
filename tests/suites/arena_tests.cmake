# ---- arena_tests -------------------------------------------------------
# The arena harness: scenario definitions, the runner, frame continuity, the
# promo capture schedule and the two panels whose population rules are asserted.
# All linked from the libraries the arena ships.
add_executable(
    arena_tests
    tools/arena_scenarios_test.cpp
    tools/arena_scenario_selection_test.cpp
    tools/arena_interactive_takeover_test.cpp
    tools/arena_rpg_gate_manifest_test.cpp
    tools/arena_commander_metrics_test.cpp
    tools/arena_scenario_runner_test.cpp
    tools/arena_frame_continuity_test.cpp
    tools/arena_panel_population_test.cpp
    tools/arena_feedback_test.cpp
    tools/arena_unit_spawn_options_test.cpp
    tools/arena_terrain_alignment_test.cpp
    tools/arena_promo_spec_test.cpp
    tools/arena_casting_overlay_test.cpp
    tools/arena_rpg_hud_test.cpp
    tools/arena_matchup_short_test.cpp
    tools/arena_video_encoder_test.cpp
    tools/settlement_layout_test.cpp
    tools/sacred_mountain_slope_test.cpp
    tools/city_map_parity_test.cpp
    test_main.cpp
)
target_link_libraries(
    arena_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        engine_core
        render_gl
        game_sim
        arena_scenario_harness
        arena_panels
        arena_feedback
        arena_casting_overlay
        arena_rpg_hud
)
soi_register_test_binary(arena_tests)
if(TARGET bake_creature_assets)
    add_dependencies(arena_tests bake_creature_assets)
endif()
