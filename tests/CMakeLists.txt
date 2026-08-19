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

# ---- simulation_tests --------------------------------------------------
# The kernel under test with nothing on screen. `simulation_main.cpp` gives it a
# QCoreApplication and the baked animation clips, and no more: a test that
# reaches for a window fails here.
add_executable(
    simulation_tests
    session/session_context_test.cpp
    command/command_pipeline_test.cpp
    command/command_codec_test.cpp
    core/entity_handle_test.cpp
    core/ground_type_test.cpp
    core/building_spawn_setup_test.cpp
    map/terrain_profiles_test.cpp
    map/map_loader_test.cpp
    map/explored_mask_codec_test.cpp
    map/visibility_restore_test.cpp
    map/map_bridge_coverage_test.cpp
    map/map_hill_entrance_smoothness_test.cpp
    map/map_weather_coverage_test.cpp
    map/map_wildlife_config_test.cpp
    map/terrain_topology_audit_test.cpp
    map/map_transformer_test.cpp
    map/hill_crown_geometry_test.cpp
    map/terrain_footprint_test.cpp
    map/undead_shrine_placement_test.cpp
    map/time_of_day_test.cpp
    systems/rain_manager_test.cpp
    wildlife/wildlife_system_test.cpp
    wildlife/bird_flock_test.cpp
    systems/patrol_system_test.cpp
    systems/nation_loader_test.cpp
    systems/troop_catalog_loader_test.cpp
    systems/showcase_routine_system_test.cpp
    systems/home_manpower_system_test.cpp
    systems/building_collision_test.cpp
    systems/pathfinding_test.cpp
    systems/production_system_test.cpp
    systems/construction_cost_catalog_test.cpp
    systems/resource_delivery_system_test.cpp
    systems/gather_loop_system_test.cpp
    systems/food_economy_test.cpp
    systems/auto_gather_test.cpp
    systems/dismantle_test.cpp
    systems/interaction_targeting_test.cpp
    systems/settlement_life_system_test.cpp
    systems/unit_activity_test.cpp
    systems/victory_service_test.cpp
    systems/capture_system_test.cpp
    systems/undead_awakening_system_test.cpp
    architecture/layering_test.cpp
    architecture/module_boundary_test.cpp
    architecture/qml_surface_test.cpp
    architecture/release_contract_test.cpp
    architecture/documentation_accuracy_test.cpp
    simulation_main.cpp
)
target_link_libraries(
    simulation_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        Qt${QT_VERSION_MAJOR}::Core
        engine_core
        game_sim
        # One test here round-trips a standing order through a save. That does
        # not weaken the kernel-only claim: soi_persistence sits *above*
        # game_sim, and a kernel file reaching down into it is caught by
        # scripts/check-modules.py, which fails the build on any
        # simulation -> persistence include.
        soi_persistence
)
soi_register_test_binary(simulation_tests)

# ---- combat_balance_tests ----------------------------------------------
# What a fight does: damage, ranges, engagement slots, formation geometry, and
# the headless battles that run the production system registry end to end.
#
# soi_runtime is the only addition over the kernel. The AI is deliberately not
# linked -- every scenario here is scripted, and if one starts needing an
# opponent to think, that belongs in ai_tests.
add_executable(
    combat_balance_tests
    formation/unit_layout_test.cpp
    formation/army_formation_planner_test.cpp
    formation/formation_planner_cache_test.cpp
    formation/formation_cohesion_test.cpp
    formation/army_formation_registry_test.cpp
    formation/formation_data_loader_test.cpp
    formation/formation_movement_test.cpp
    formation/formation_terrain_navigation_test.cpp
    systems/formation_combat_geometry_test.cpp
    systems/defensive_unit_layout_test.cpp
    systems/structure_combat_test.cpp
    systems/structure_fire_test.cpp
    systems/guard_system_test.cpp
    systems/stamina_system_test.cpp
    systems/attack_range_test.cpp
    systems/attack_targeting_test.cpp
    systems/target_focus_test.cpp
    systems/combat_mode_test.cpp
    systems/hold_mode_test.cpp
    systems/rpg_bow_test.cpp
    systems/rpg_engagement_system_test.cpp
    systems/commander_system_test.cpp
    systems/commander_duel_test.cpp
    systems/melee_engagement_test.cpp
    systems/archer_bonus_test.cpp
    systems/arrow_system_test.cpp
    systems/spent_projectile_test.cpp
    systems/elephant_special_processor_test.cpp
    systems/siege_special_processor_test.cpp
    systems/gate_system_test.cpp
    systems/battle_movement_refactor_test.cpp
    systems/battlefield_capture_test.cpp
    headless/headless_simulation_test.cpp
    headless/gate_traversal_test.cpp
    headless/wall_siege_test.cpp
    headless/tight_gap_navigation_test.cpp
    headless/battle_speed_load_test.cpp
    simulation_main.cpp
)
target_link_libraries(
    combat_balance_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        Qt${QT_VERSION_MAJOR}::Core
        engine_core
        game_sim
        soi_runtime
        # Two tests here assert that gate mode and unit layout survive a save.
        # That does not weaken the boundary: soi_persistence sits *above*
        # game_sim, and a kernel file reaching down into it is caught by
        # scripts/check-modules.py, which fails the build on any
        # simulation -> persistence include.
        soi_persistence
)
soi_register_test_binary(combat_balance_tests)

# ---- ai_tests ----------------------------------------------------------
# The computer opponent, and the two scenarios that need one to be meaningful:
# a wall assault it has to path around, and a mission wave it has to press.
add_executable(
    ai_tests
    systems/ai_system_test.cpp
    systems/wall_system_test.cpp
    headless/mission_wave_assault_test.cpp
    simulation_main.cpp
)
target_link_libraries(
    ai_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        Qt${QT_VERSION_MAJOR}::Core
        engine_core
        game_sim
        soi_ai
        soi_runtime
)
soi_register_test_binary(ai_tests)

# ---- campaign_tests ----------------------------------------------------
# Mission and campaign content -- what the shipped files say, and whether the
# loaders agree with them -- and the runtime that applies that content to a live
# world.
#
# It links no client on purpose: no renderer, no camera, no view models. Setting
# a mission up, spawning the forces it names, stepping the tutorial and firing
# its waves are world manipulation, and this link line is what says so. Those
# tests used to sit in app_tests only because the code did.
#
# soi_ai and soi_persistence arrive through soi_mission_runtime, which needs
# them: a definition names its opponents with a strategy, and a campaign records
# which missions are complete. Reading content still needs neither -- that half
# is soi_missions, and it links neither.
add_executable(
    campaign_tests
    map/mission_loader_test.cpp
    map/mission_asset_rules_test.cpp
    map/mission_stage_tracker_test.cpp
    map/commander_message_director_test.cpp
    map/mission_victory_rules_test.cpp
    map/wave_archetype_catalog_test.cpp
    map/map_catalog_test.cpp
    map/campaign_loader_test.cpp
    map/campaign_roster_test.cpp
    map/campaign_content_integration_test.cpp
    map/campaign_province_data_test.cpp
    # The mission runtime, checked without a client. These were in app_tests
    # while the code they exercise sat in app/mission/; running them here is what
    # proves the move -- setting a mission up, spawning its forces, stepping the
    # tutorial and firing its waves need no renderer and no view model.
    core/campaign_manager_test.cpp
    core/mission_commander_setup_test.cpp
    core/mission_definition_view_test.cpp
    core/mission_events_test.cpp
    core/mission_wave_director_test.cpp
    core/tutorial_director_test.cpp
    simulation_main.cpp
)
target_link_libraries(
    campaign_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        Qt${QT_VERSION_MAJOR}::Core
        engine_core
        game_sim
        soi_missions
        soi_mission_runtime
)
soi_register_test_binary(campaign_tests)

# ---- persistence_tests -------------------------------------------------
# What a save is, and whether the thing that writes it agrees. No runtime and no
# AI, because a mission snapshot must be complete without either.
add_executable(
    persistence_tests
    save/snapshot_contract_test.cpp
    core/serialization_test.cpp
    db/save_storage_test.cpp
    db/save_format_test.cpp
    db/save_load_service_test.cpp
    db/mission_progress_test.cpp
    db/campaign_progression_test.cpp
    db/campaign_end_to_end_test.cpp
    headless/headless_main.cpp
)
target_link_libraries(
    persistence_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        Qt${QT_VERSION_MAJOR}::Core
        engine_core
        game_sim
        soi_persistence
        soi_missions
)
soi_register_test_binary(persistence_tests)

# ---- render_tests ------------------------------------------------------
add_executable(
    render_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/test_assets.qrc
    render/unit_primitive_convention_test.cpp
    render/instance_draw_guard_test.cpp
    render/pose_controller_test.cpp
    render/wildlife_action_pose_test.cpp
    render/sheep_rig_integrity_test.cpp
    render/wolf_rig_integrity_test.cpp
    render/pose_controller_compatibility_test.cpp
    render/mounted_pose_controller_test.cpp
    render/body_frames_test.cpp
    render/grip_axis_test.cpp
    render/equipment_registry_test.cpp
    render/equipment_loadout_catalog_test.cpp
    render/helmet_renderers_test.cpp
    render/armor_renderer_test.cpp
    render/carthage_armor_bounds_test.cpp
    render/rider_proportions_test.cpp
    render/infantry_proportions_test.cpp
    render/horse_equipment_renderers_test.cpp
    render/spawn_validator_test.cpp
    render/battle_render_optimizer_test.cpp
    render/barracks_flag_renderer_test.cpp
    render/building_archetype_desc_test.cpp
    render/building_render_common_test.cpp
    render/mode_indicator_test.cpp
    render/mode_indicator_placement_test.cpp
    render/shader_source_test.cpp
    render/visibility_mask_encoder_test.cpp
    render/fog_renderer_test.cpp
    render/render_view_state_test.cpp
    render/renderer_visibility_policy_test.cpp
    render/unit_render_cache_test.cpp
    render/persistent_render_registry_test.cpp
    render/render_archetype_test.cpp
    render/draw_queue_sort_order_test.cpp
    render/effect_batch_dispatch_test.cpp
    render/spark_direction_test.cpp
    render/linear_feature_geometry_test.cpp
    render/road_network_geometry_test.cpp
    render/linear_feature_visibility_test.cpp
    render/ground_utils_test.cpp
    render/dead_tree_mesh_test.cpp
    render/local_lighting_test.cpp
    render/contact_shadow_test.cpp
    render/weather_particle_budget_test.cpp
    render/graphics_lighting_settings_test.cpp
    render/scatter_composition_test.cpp
    render/scatter_runtime_test.cpp
    render/terrain_scene_proxy_test.cpp
    render/render_thread_boundary_test.cpp
    render/scene_walk_entity_handle_test.cpp
    render/map_boundary_fog_renderer_test.cpp
    render/oriented_parts_test.cpp
    render/draw_part_material_test.cpp
    render/roman_light_helmet_mesh_test.cpp
    render/rig_dsl_interpreter_test.cpp
    render/rigged_pipeline_test.cpp
    render/pipeline/lod_selector_test.cpp
    render/pipeline/creature_lod_decision_test.cpp
    render/pipeline/creature_asset_registry_test.cpp
    render/pipeline/instance_coalescer_test.cpp
    render/pass/frame_pass_runner_test.cpp
    render/rig_dsl/barracks_rig_test.cpp
    render/bpat/bpat_format_test.cpp
    render/bpat/bpat_registry_test.cpp
    render/humanoid/skeleton_test.cpp
    render/humanoid/humanoid_spec_test.cpp
    render/horse/mounted_seat_frame_test.cpp
    render/horse/mounted_rider_seat_test.cpp
    render/horse/horse_spec_test.cpp
    render/horse/horse_anatomy_test.cpp
    render/elephant/elephant_spec_test.cpp
    render/elephant/elephant_source_asset_test.cpp
    render/stateless_weapon_renderers_test.cpp
    render/creature/skeleton_test.cpp
    render/creature/species_manifest_parity_test.cpp
    render/creature/part_graph_test.cpp
    render/creature/creature_visual_definition_test.cpp
    render/creature/rigged_mesh_bake_test.cpp
    render/creature/rigged_mesh_cache_test.cpp
    render/creature/static_attachment_bake_test.cpp
    render/creature/quadruped_topology_test.cpp
    render/creature/quadruped_gait_test.cpp
    render/creature/combat_visual_state_test.cpp
    render/creature/soldier_turn_smoothing_test.cpp
    render/creature/humanoid_prepare_test.cpp
    render/creature/death_collapse_test.cpp
    render/creature/horse_prepare_test.cpp
    render/creature/elephant_prepare_test.cpp
    render/creature/quadruped_clip_set_test.cpp
    render/creature/quadruped_mesh_graph_test.cpp
    render/creature/mounted_prepare_test.cpp
    render/creature/creature_render_graph_test.cpp
    render/creature/shadow_batch_test.cpp
    render/creature/render_request_test.cpp
    render/selection_ring_layout_test.cpp
    render/ground_marker_pattern_test.cpp
    render/arrow_cloud_render_test.cpp
    render/projectile_relation_test.cpp
    render/orientation_arrow_mesh_test.cpp
    render/creature/rigged_mesh_asset_test.cpp
    render/creature/snapshot_mesh_asset_test.cpp
    render/creature/snapshot_mesh_cache_test.cpp
    render/creature/static_attachment_bake_test.cpp
    render/creature/per_unit_archetype_test.cpp
    render/creature/riding_clip_test.cpp
    render/creature/baked_attachment_world_position_test.cpp
    render/creature/baked_horse_attachment_world_position_test.cpp
    render/creature/render_request_batch_parity_test.cpp
    render/creature/creature_bone_probe_test.cpp
    render/creature/template_prewarm_regression_test.cpp
    render/software/software_rasterizer_test.cpp
    render/software/software_backend_integration_test.cpp
    render/software/render_backend_factory_test.cpp
    render/profiling/frame_profile_test.cpp
    test_main.cpp
)
target_link_libraries(
    render_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        engine_core
        render_gl
        game_sim
)
set_target_properties(render_tests PROPERTIES AUTORCC ON)
soi_register_test_binary(render_tests)
if(TARGET bake_creature_assets)
    add_dependencies(render_tests bake_creature_assets)
endif()

# ---- app_tests ---------------------------------------------------------
# View models, controllers, the widget shell, and the gameplay services that
# only mean something with a camera or the application around them.
add_executable(
    app_tests
    ${SOI_TRANSLATIONS_QRC}
    core/audio_system_test.cpp
    core/audio_cues_test.cpp
    core/audio_mastering_test.cpp
    core/loop_seam_test.cpp
    core/resampler_test.cpp
    core/ambience_assets_test.cpp
    core/weather_audio_test.cpp
    core/audio_command_ring_test.cpp
    core/audio_backend_test.cpp
    db/save_preview_test.cpp
    core/settings_persistence_test.cpp
    core/minimap_manager_test.cpp
    core/commander_control_regression_test.cpp
    core/commander_camera_rig_test.cpp
    core/commander_control_controller_test.cpp
    core/input_command_handler_test.cpp
    core/tutorial_mission_test.cpp
    core/campaign_wave_assault_test.cpp
    core/production_manager_test.cpp
    core/economy_overview_test.cpp
    core/unit_profile_test.cpp
    core/selection_query_service_test.cpp
    core/combat_feedback_test.cpp
    core/order_feedback_test.cpp
    core/rts_action_model_test.cpp
    core/runtime_frame_orchestrator_test.cpp
    core/game_speed_test.cpp
    core/camera_framing_test.cpp
    core/mission_objective_markers_test.cpp
    controllers/command_controller_test.cpp
    ui/preferences_test.cpp
    ui/input_bindings_test.cpp
    ui/edge_scroll_test.cpp
    ui/team_identity_test.cpp
    ui/selection_grouping_test.cpp
    ui/selected_units_model_test.cpp
    ui/icon_art_test.cpp
    ui/icon_resources_test.cpp
    ui/widget_theme_test.cpp
    map/minimap_generator_test.cpp
    map/minimap_utils_test.cpp
    map/terrain_service_test.cpp
    map/iron_sepulcher_skirmish_test.cpp
    systems/civilian_delivery_system_test.cpp
    systems/building_obstruction_lifecycle_test.cpp
    systems/command_service_test.cpp
    test_main.cpp
)
target_link_libraries(
    app_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::Widgets
        engine_core
        render_gl
        game_systems
        app_core
        ui_shell
        audio_system
)
set_target_properties(app_tests PROPERTIES AUTORCC ON)
soi_register_test_binary(app_tests)
add_dependencies(app_tests translations_qm)
if(TARGET bake_creature_assets)
    add_dependencies(app_tests bake_creature_assets)
endif()

# ---- arena_tests -------------------------------------------------------
# The arena harness: scenario definitions, the runner, frame continuity, the
# promo capture schedule and the two panels whose population rules are asserted.
# All linked from the libraries the arena ships.
add_executable(
    arena_tests
    tools/arena_scenarios_test.cpp
    tools/arena_scenario_runner_test.cpp
    tools/arena_frame_continuity_test.cpp
    tools/arena_panel_population_test.cpp
    tools/arena_unit_spawn_options_test.cpp
    tools/arena_terrain_alignment_test.cpp
    tools/arena_promo_spec_test.cpp
    tools/settlement_layout_test.cpp
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
)
soi_register_test_binary(arena_tests)
if(TARGET bake_creature_assets)
    add_dependencies(arena_tests bake_creature_assets)
endif()

# ---- tools_tests -------------------------------------------------------
# The map editor document model and the balance runner. Neither needs a
# renderer, so this binary links neither render_gl nor game_view -- an editor
# change that starts reaching for the scene graph fails here.
add_executable(
    tools_tests
    tools/map_editor_map_data_test.cpp
    tools/map_editor_mission_data_test.cpp
    tools/map_editor_canvas_transform_test.cpp
    tools/map_editor_canvas_input_test.cpp
    tools/map_editor_wall_geometry_test.cpp
    tools/map_editor_element_ops_test.cpp
    tools/map_editor_wildlife_test.cpp
    tools/map_editor_json_schema_test.cpp
    tools/map_editor_commander_preview_test.cpp
    tools/hill_projection_model_test.cpp
    tools/balance_sim_test.cpp
    widget_main.cpp
)
target_link_libraries(
    tools_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        engine_core
        game_sim
        map_editor_core
        balance_sim_harness
)
soi_register_test_binary(tools_tests)
if(TARGET bake_creature_assets)
    add_dependencies(tools_tests bake_creature_assets)
endif()

# Focused model binaries. They stay runnable when an unrelated test translation
# unit in the render suite has interface drift. Both test sources are also
# compiled into render_tests, which is where they run on the default path --
# these are a debugging convenience, not a second verification route.
# docs/HORSE_MODEL_ARCHITECTURE.md points at them.
add_executable(horse_model_tests render/horse/horse_anatomy_test.cpp test_main.cpp)
target_link_libraries(
    horse_model_tests
    PRIVATE
        GTest::gtest
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        engine_core
        render_gl
        game_sim
)
soi_register_test_binary(horse_model_tests)
if(TARGET bake_creature_assets)
    add_dependencies(horse_model_tests bake_creature_assets)
endif()

add_executable(elephant_model_tests render/elephant/elephant_source_asset_test.cpp test_main.cpp)
target_link_libraries(
    elephant_model_tests
    PRIVATE
        GTest::gtest
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        engine_core
        render_gl
        game_sim
)
soi_register_test_binary(elephant_model_tests)
if(TARGET bake_creature_assets)
    add_dependencies(elephant_model_tests bake_creature_assets)
endif()

# ---- QML design system regression tests ----
# QtQuickTest drives the real StandardOfIron.Design module, so token drift or a
# renamed control fails here rather than in a screen. Skipped when the Qt Quick
# test module is unavailable so a minimal Qt install still builds the suite.
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS QuickTest)
if(TARGET Qt${QT_VERSION_MAJOR}::QuickTest)
    add_executable(
        design_system_qml_tests
        ui/design_system_qml_test.cpp
        ${CMAKE_SOURCE_DIR}/design_resources.qrc
        ${CMAKE_SOURCE_DIR}/assets.qrc
    )
    target_link_libraries(
        design_system_qml_tests
        PRIVATE
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Gui
            Qt${QT_VERSION_MAJOR}::Qml
            Qt${QT_VERSION_MAJOR}::Quick
            Qt${QT_VERSION_MAJOR}::QuickTest
            ui_shell
    )
    target_include_directories(design_system_qml_tests PRIVATE ${CMAKE_SOURCE_DIR})
    set_target_properties(
        design_system_qml_tests
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" AUTOMOC ON AUTORCC ON
    )
    add_test(
        NAME design_system_qml
        COMMAND design_system_qml_tests -input ${CMAKE_CURRENT_SOURCE_DIR}/ui/qml
    )
    # The design system is presentation-only; a real GPU adds nothing to assert.
    set_tests_properties(design_system_qml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

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

# AmbienceAssetsTest reads the shipped beds, which the build renders rather than
# git storing. Without this the suite would skip them on a clean tree.
if(TARGET synthesize_ambience_assets)
    add_dependencies(soi_test_binaries synthesize_ambience_assets)
endif()

# The gameplay acceptance gate. scripts/run-tests.sh treats a missing binary as
# an error rather than a skip, so it has to be built alongside the unit suites.
if(TARGET battlefield_gameplay_verifier)
    add_dependencies(soi_test_binaries battlefield_gameplay_verifier)
endif()
