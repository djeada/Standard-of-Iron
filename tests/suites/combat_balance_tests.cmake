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
    formation/formation_drag_shape_test.cpp
    formation/formation_squad_extent_probe_test.cpp
    formation/army_formation_planner_test.cpp
    formation/army_formation_integrity_test.cpp
    formation/army_formation_membership_test.cpp
    formation/formation_planner_cache_test.cpp
    formation/formation_cohesion_test.cpp
    formation/formation_phase_vocabulary_test.cpp
    formation/army_formation_registry_test.cpp
    formation/formation_data_loader_test.cpp
    formation/formation_movement_test.cpp
    formation/formation_terrain_navigation_test.cpp
    formation/battle_order_layout_test.cpp
    map/map_crossing_traversal_test.cpp
    core/system_access_verification_test.cpp
    systems/runtime_phase_order_test.cpp
    systems/movement_stage_ownership_test.cpp
    systems/movement_route_test.cpp
    systems/movement_orders_test.cpp
    systems/formation_combat_geometry_test.cpp
    systems/formation_march_keepup_test.cpp
    systems/defensive_unit_layout_test.cpp
    systems/structure_combat_test.cpp
    systems/event_audience_test.cpp
    systems/structure_fire_test.cpp
    systems/guard_system_test.cpp
    systems/skirmish_screen_system_test.cpp
    systems/stamina_system_test.cpp
    systems/attack_range_test.cpp
    systems/attack_targeting_test.cpp
    systems/target_rules_test.cpp
    systems/target_focus_test.cpp
    systems/combat_mode_test.cpp
    systems/death_sequence_test.cpp
    systems/hold_mode_test.cpp
    systems/rpg_bow_test.cpp
    systems/rpg_engagement_system_test.cpp
    systems/commander_system_test.cpp
    systems/commander_duel_test.cpp
    systems/melee_engagement_test.cpp
    systems/melee_exchange_test.cpp
    systems/melee_intent_test.cpp
    systems/weapon_trace_test.cpp
    systems/archer_bonus_test.cpp
    systems/arrow_system_test.cpp
    systems/spent_projectile_test.cpp
    systems/elephant_special_processor_test.cpp
    systems/siege_special_processor_test.cpp
    systems/squad_alert_test.cpp
    systems/gate_system_test.cpp
    systems/siege_tower_system_test.cpp
    systems/battle_movement_refactor_test.cpp
    systems/battlefield_capture_test.cpp
    headless/headless_simulation_test.cpp
    headless/gate_traversal_test.cpp
    headless/wall_siege_test.cpp
    headless/builder_crews_test.cpp
    headless/tight_gap_navigation_test.cpp
    headless/stuck_recovery_test.cpp
    headless/movement_pace_test.cpp
    headless/army_formation_march_test.cpp
    headless/battle_order_behaviour_test.cpp
    headless/formation_ux_lab_test.cpp
    headless/army_command_test.cpp
    headless/hill_containment_test.cpp
    headless/formation_prop_clearance_test.cpp
    headless/hill_traversal_test.cpp
    headless/fortress_siege_traversal_test.cpp
    headless/playable_ground_test.cpp
    headless/forest_cover_test.cpp
    headless/wildlife_fight_motion_test.cpp
    headless/battle_speed_load_test.cpp
    headless/selected_unit_readout_test.cpp
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
