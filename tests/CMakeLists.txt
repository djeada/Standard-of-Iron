# Test suite for Standard of Iron
# Uses Google Test framework
#
# The suite is split by the boundary each binary exercises, not by convenience:
#
#   simulation_tests   the kernel. Links engine_core + game_sim and nothing
#                      else, so a gameplay file that starts needing a camera,
#                      the renderer or the application breaks the link step
#                      rather than slipping in unnoticed.
#   persistence_tests  the snapshot contract and the save stack. Kernel-only
#                      link as well.
#   render_tests       the renderer and the creature pipeline.
#   app_tests          the application layer: view models, controllers, the
#                      widget shell and the gameplay services that need a
#                      camera or the app layer to be meaningful.
#   tools_tests        the map editor, the arena and the balance harness.
#   design_system_qml_tests
#                      the QML design system, driven by QtQuickTest.
#
# Two rules keep this honest:
#
#  - a test binary links production targets; it never re-lists a production
#    `.cpp` in its own source list. A separately compiled copy can pass while
#    the object the game ships is broken.
#  - no test is quarantined out of the default path. If a test cannot build or
#    cannot pass, it is fixed or it is deleted with an issue, not omitted.

set(_soi_optional_test_sources)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/render/creature/combat_visual_state_test.cpp")
    list(APPEND _soi_optional_test_sources render/creature/combat_visual_state_test.cpp)
endif()

# Registers a binary with CTest and parks it beside the other executables.
function(soi_register_test_binary target)
    set_target_properties(
        ${target}
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" AUTOMOC ON
    )
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/game)
    gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
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
    core/entity_handle_test.cpp
    core/serialization_test.cpp
    core/ground_type_test.cpp
    core/building_spawn_setup_test.cpp
    map/terrain_profiles_test.cpp
    map/map_loader_test.cpp
    map/explored_mask_codec_test.cpp
    map/visibility_restore_test.cpp
    map/map_catalog_test.cpp
    map/map_weather_coverage_test.cpp
    map/terrain_topology_audit_test.cpp
    map/map_transformer_test.cpp
    map/mission_loader_test.cpp
    map/mission_asset_rules_test.cpp
    map/hill_crown_geometry_test.cpp
    map/terrain_footprint_test.cpp
    map/mission_victory_rules_test.cpp
    map/campaign_loader_test.cpp
    map/time_of_day_test.cpp
    systems/rain_manager_test.cpp
    systems/patrol_system_test.cpp
    systems/formation_system_test.cpp
    systems/formation_combat_geometry_test.cpp
    systems/structure_combat_test.cpp
    systems/guard_system_test.cpp
    systems/nation_loader_test.cpp
    systems/troop_catalog_loader_test.cpp
    systems/stamina_system_test.cpp
    systems/home_manpower_system_test.cpp
    systems/building_collision_test.cpp
    systems/combat_mode_test.cpp
    systems/hold_mode_test.cpp
    systems/defense_formation_test.cpp
    systems/rpg_engagement_system_test.cpp
    systems/commander_system_test.cpp
    systems/pathfinding_test.cpp
    systems/production_system_test.cpp
    systems/victory_service_test.cpp
    systems/undead_awakening_system_test.cpp
    systems/archer_bonus_test.cpp
    systems/arrow_system_test.cpp
    systems/ai_system_test.cpp
    systems/elephant_special_processor_test.cpp
    systems/siege_special_processor_test.cpp
    systems/wall_system_test.cpp
    systems/gate_system_test.cpp
    systems/battle_movement_refactor_test.cpp
    systems/battlefield_capture_test.cpp
    headless/headless_simulation_test.cpp
    headless/gate_traversal_test.cpp
    architecture/layering_test.cpp
    architecture/qml_surface_test.cpp
    architecture/documentation_accuracy_test.cpp
    simulation_main.cpp
)
target_link_libraries(
    simulation_tests
    PRIVATE GTest::gtest GTest::gmock Qt${QT_VERSION_MAJOR}::Core engine_core game_sim
)
soi_register_test_binary(simulation_tests)

# ---- persistence_tests -------------------------------------------------
# What a save is, and whether the thing that writes it agrees. Kernel-only
# link, because a mission snapshot must be complete without the application.
add_executable(
    persistence_tests
    save/snapshot_contract_test.cpp
    db/save_storage_test.cpp
    db/save_format_test.cpp
    db/save_load_service_test.cpp
    db/mission_progress_test.cpp
    headless/headless_main.cpp
)
target_link_libraries(
    persistence_tests
    PRIVATE GTest::gtest GTest::gmock Qt${QT_VERSION_MAJOR}::Core engine_core game_sim
)
soi_register_test_binary(persistence_tests)

# ---- render_tests ------------------------------------------------------
add_executable(
    render_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/test_assets.qrc
    render/pose_controller_test.cpp
    render/pose_controller_compatibility_test.cpp
    render/mounted_pose_controller_test.cpp
    render/body_frames_test.cpp
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
    render/renderer_visibility_policy_test.cpp
    render/unit_render_cache_test.cpp
    render/persistent_render_registry_test.cpp
    render/render_archetype_test.cpp
    render/draw_queue_sort_order_test.cpp
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
    render/rigged_pipeline_instanced_test.cpp
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
    render/stateless_weapon_renderers_test.cpp
    render/creature/skeleton_test.cpp
    render/creature/species_manifest_parity_test.cpp
    render/creature/part_graph_test.cpp
    render/creature/creature_visual_definition_test.cpp
    render/creature/rigged_mesh_bake_test.cpp
    render/creature/rigged_mesh_cache_test.cpp
    render/creature/quadruped_topology_test.cpp
    render/creature/quadruped_gait_test.cpp
    ${_soi_optional_test_sources}
    render/creature/humanoid_prepare_test.cpp
    render/creature/horse_prepare_test.cpp
    render/creature/elephant_prepare_test.cpp
    render/creature/quadruped_clip_set_test.cpp
    render/creature/quadruped_mesh_graph_test.cpp
    render/creature/mounted_prepare_test.cpp
    render/creature/creature_render_graph_test.cpp
    render/creature/shadow_batch_test.cpp
    render/creature/render_request_test.cpp
    render/selection_ring_layout_test.cpp
    render/selection_ring_mesh_test.cpp
    render/arrow_cloud_render_test.cpp
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
        game_systems
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
    ${CMAKE_SOURCE_DIR}/translations.qrc
    core/audio_system_test.cpp
    db/save_preview_test.cpp
    core/settings_persistence_test.cpp
    core/minimap_manager_test.cpp
    core/commander_control_regression_test.cpp
    core/commander_control_controller_test.cpp
    core/input_command_handler_test.cpp
    core/mission_commander_setup_test.cpp
    core/mission_definition_view_test.cpp
    core/production_manager_test.cpp
    core/selection_query_service_test.cpp
    core/rts_action_model_test.cpp
    core/runtime_frame_orchestrator_test.cpp
    controllers/command_controller_test.cpp
    ui/preferences_test.cpp
    ui/selection_grouping_test.cpp
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
if(TARGET bake_creature_assets)
    add_dependencies(app_tests bake_creature_assets)
endif()

# ---- tools_tests -------------------------------------------------------
# The map editor document model, the arena harness and the balance runner --
# all linked from the libraries those tools ship.
add_executable(
    tools_tests
    tools/map_editor_map_data_test.cpp
    tools/map_editor_mission_data_test.cpp
    tools/map_editor_canvas_transform_test.cpp
    tools/map_editor_element_ops_test.cpp
    tools/map_editor_json_schema_test.cpp
    tools/hill_projection_model_test.cpp
    tools/map_editor_commander_preview_test.cpp
    tools/arena_scenarios_test.cpp
    tools/arena_scenario_runner_test.cpp
    tools/arena_frame_continuity_test.cpp
    tools/arena_panel_population_test.cpp
    tools/arena_unit_spawn_options_test.cpp
    tools/arena_terrain_alignment_test.cpp
    tools/balance_sim_test.cpp
    test_main.cpp
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
        render_gl
        game_systems
        map_editor_core
        arena_scenario_harness
        arena_panels
        balance_sim_harness
)
soi_register_test_binary(tools_tests)
if(TARGET bake_creature_assets)
    add_dependencies(tools_tests bake_creature_assets)
endif()

# Focused horse contract binary. It stays runnable when an unrelated test
# translation unit in the render suite has interface drift.
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
        game_systems
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
        game_systems
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

# Everything `make test` runs. Keep this list and scripts/run-tests.sh in step.
add_custom_target(
    soi_test_binaries
    DEPENDS simulation_tests persistence_tests render_tests app_tests tools_tests
)
if(TARGET design_system_qml_tests)
    add_dependencies(soi_test_binaries design_system_qml_tests)
endif()
