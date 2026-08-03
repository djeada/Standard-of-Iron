# Core ECS library (moved from engine/core to game/core)
add_library(
    engine_core
    STATIC
    core/entity.cpp
    core/component.cpp
    core/system.cpp
    core/world.cpp
    core/event_manager.cpp
    core/simulation_timing.cpp
    core/serialization.cpp
)

target_include_directories(engine_core PUBLIC .)
target_link_libraries(
    engine_core
    PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui animation_core
)

# Accessibility settings the simulation and the renderer have to honour.
#
# It is its own target because both ends of the stack need it and neither may
# depend on the other: the simulation assigns owner colours from it and the
# renderer picks ring shapes from it, while the UI preferences push the
# player's choices into it. A leaf with no dependencies of its own is the only
# thing both can safely share.
add_library(
    accessibility_runtime
    STATIC
    accessibility/team_identity.cpp
    accessibility/motion_settings.cpp
)
target_include_directories(accessibility_runtime PUBLIC ..)

# The simulation kernel.
#
# Everything a match needs to run with nothing on screen: the ECS systems, the
# session, the command pipeline, terrain, units and world serialisation.
#
# It deliberately does NOT link scene_core or render_gl. That is the boundary
# the build enforces: if a gameplay file starts needing a camera, this target
# stops linking rather than the dependency slipping in unnoticed. Headless
# balance runs, deterministic replay and a dedicated server all consume this
# target alone.
#
# Qt::Gui is present only for the value types (QVector3D, QImage); it pulls in
# no windowing or GL.
add_library(
    game_sim
    STATIC
    command/command.cpp
    command/command_dispatcher.cpp
    command/command_queue.cpp
    command/command_system.cpp
    command/command_validator.cpp
    save/snapshot_contract.cpp
    util/asset_text.cpp
    util/json_vec_utils.cpp
    session/session_context.cpp
    session/simulation_clock.cpp
    systems/player_resource_registry.cpp
    systems/runtime_system_registry.cpp
    systems/battlefield_capture.cpp
    systems/order_service.cpp
    systems/movement_system.cpp
    systems/combat_system.cpp
    systems/combat_system/combat_utils.cpp
    systems/combat_system/structure_combat.cpp
    systems/combat_system/structure_fire.cpp
    systems/combat_system/damage_application.cpp
    systems/combat_system/damage_processor.cpp
    systems/combat_system/combat_mode_processor.cpp
    systems/combat_system/formation_contact_processor.cpp
    systems/combat_system/auto_engagement.cpp
    systems/combat_system/hit_feedback_processor.cpp
    systems/combat_system/combat_hit_resolver.cpp
    systems/combat_system/combat_action_processor.cpp
    systems/combat_system/spear_brace_processor.cpp
    systems/combat_system/mounted_charge_processor.cpp
    systems/combat_system/combat_status_effect_processor.cpp
    systems/combat_system/combat_state_processor.cpp
    systems/combat_system/combat_animation_timing.cpp
    systems/combat_system/attack_processor.cpp
    systems/combat_system/elephant_special_processor.cpp
    systems/combat_system/siege_special_processor.cpp
    systems/combat_actions/body_impact.cpp
    systems/combat_actions/combat_action_definition.cpp
    systems/combat_actions/combat_action_events.cpp
    systems/combat_actions/combat_action_service.cpp
    systems/combat_actions/projectile_release.cpp
    systems/combat_actions/weapon_trace.cpp
    systems/cleanup_system.cpp
    systems/rpg_combat_system/rpg_commander_damage.cpp
    systems/rpg_combat_system/rpg_damage_resolver.cpp
    systems/rpg_combat_system/rpg_combat_processor.cpp
    systems/rpg_combat_system/rpg_targeting.cpp
    systems/ai_system.cpp
    systems/ai_system/ai_command_applier.cpp
    systems/ai_system/ai_command_filter.cpp
    systems/ai_system/ai_base_manager.cpp
    systems/ai_system/ai_snapshot_builder.cpp
    systems/ai_system/ai_reasoner.cpp
    systems/ai_system/ai_executor.cpp
    systems/ai_system/ai_worker.cpp
    systems/ai_system/ai_formation.cpp
    systems/ai_system/ai_tactical.cpp
    systems/ai_system/ai_strategy.cpp
    systems/ai_system/behaviors/production_behavior.cpp
    systems/ai_system/behaviors/gather_behavior.cpp
    systems/ai_system/behaviors/harass_behavior.cpp
    systems/ai_system/behaviors/assault_behavior.cpp
    systems/ai_system/behaviors/attack_behavior.cpp
    systems/ai_system/behaviors/chase_behavior.cpp
    systems/ai_system/behaviors/defend_behavior.cpp
    systems/ai_system/behaviors/retreat_behavior.cpp
    systems/ai_system/behaviors/expand_behavior.cpp
    systems/ai_system/behaviors/builder_behavior.cpp
    systems/ai_system/behaviors/commander_behavior.cpp
    systems/patrol_system.cpp
    systems/guard_system.cpp
    systems/pathfinding.cpp
    systems/spatial_grid.cpp
    systems/local_avoidance_system.cpp
    systems/engagement_slot_system.cpp
    systems/target_commitment_system.cpp
    systems/cohort_system.cpp
    systems/building_collision_registry.cpp
    systems/wall_network_service.cpp
    systems/gate_service.cpp
    systems/gate_system.cpp
    systems/selection_system.cpp
    systems/arrow_system.cpp
    systems/arrow_visual_profile.cpp
    systems/combat_status_effect_system.cpp
    systems/projectile_system.cpp
    systems/arrow_projectile.cpp
    systems/stone_projectile.cpp
    systems/command_service.cpp
    systems/movement_orders.cpp
    systems/commander_system.cpp
    systems/construction_cost_catalog.cpp
    systems/production_service.cpp
    systems/production_system.cpp
    systems/unit_activity.cpp
    systems/civilian_delivery_system.cpp
    systems/capture_system.cpp
    systems/terrain_alignment_system.cpp
    systems/healing_rules.cpp
    systems/healing_system.cpp
    systems/healing_beam.cpp
    systems/healing_beam_system.cpp
    systems/owner_registry.cpp
    systems/troop_count_registry.cpp
    systems/marketplace_system.cpp
    systems/global_stats_registry.cpp
    systems/victory_service.cpp
    systems/undead_awakening_system.cpp
    systems/save_format.cpp
    systems/save_load_service.cpp
    systems/save_storage.cpp
    systems/nation_registry.cpp
    systems/nation_loader.cpp
    systems/defense_formation_service.cpp
    systems/formation_combat_geometry.cpp
    formation/formation_roles.cpp
    formation/army_formation_types.cpp
    formation/troop_role_registry.cpp
    formation/formation_doctrine.cpp
    formation/unit_layout.cpp
    formation/unit_layout_resolver.cpp
    formation/army_formation_planner.cpp
    formation/army_formation_registry.cpp
    formation/army_formation_service.cpp
    formation/formation_data_loader.cpp
    formation/unit_layout_state_system.cpp
    systems/troop_profile_service.cpp
    systems/rain_manager.cpp
    systems/showcase_routine_system.cpp
    systems/stamina_system.cpp
    map/map_loader.cpp
    map/mission_loader.cpp
    map/wave_archetype_catalog.cpp
    map/mission_victory_rules.cpp
    map/campaign_loader.cpp
    map/procedural_tree_generation.cpp
    map/map_transformer.cpp
    map/environment_lighting.cpp
    map/scatter/spawn_validator.cpp
    map/terrain.cpp
    map/terrain_landform.cpp
    map/terrain_service.cpp
    map/undead_shrine_placement.cpp
    map/terrain_topology_audit.cpp
    map/visibility_service.cpp
    map/explored_mask_codec.cpp
    map/map_catalog.cpp
    visuals/building_asset_key.cpp
    visuals/visual_catalog.cpp
    units/unit.cpp
    units/archer.cpp
    units/swordsman.cpp
    units/skeleton_swordsman.cpp
    units/skeleton_archer.cpp
    units/grave_priest.cpp
    units/horse_swordsman.cpp
    units/horse_archer.cpp
    units/horse_spearman.cpp
    units/spearman.cpp
    units/healer.cpp
    units/builder.cpp
    units/catapult.cpp
    units/ballista.cpp
    units/elephant.cpp
    units/civilian.cpp
    units/commander_catalog.cpp
    units/troop_catalog.cpp
    units/troop_catalog_loader.cpp
    units/building_spawn_setup.cpp
    units/factory.cpp
    units/barracks.cpp
    units/defense_tower.cpp
    units/home.cpp
    units/marketplace.cpp
    units/temple.cpp
    units/wall_segment.cpp
    units/wall_gate.cpp
    systems/home_system.cpp
    systems/settlement_life_system.cpp
)

target_include_directories(game_sim PUBLIC .)
target_link_libraries(
    game_sim
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Sql
        engine_core
        animation_core
        accessibility_runtime
)

# The kernel and the simulation are mutually dependent, and the link line has
# to say so.
#
# core/world.cpp calls the owner, building-collision and troop-count registries;
# core/serialization.cpp calls terrain, the terrain service and the formation
# registry. All of those live in game_sim, so libengine_core.a genuinely needs
# libgame_sim.a as well as the other way round.
#
# Without this, resolution depends on which archive happens to pull world.cpp.o
# first. A binary that links "engine_core game_sim" and only reaches world.cpp.o
# through game_sim picks it up on the repeat pass, after game_sim has already
# gone by, and every registry call is left undefined -- which is why
# persistence_tests failed under Ninja/Debug while Make/Release linked. CMake
# resolves declared static-library cycles by repeating both archives.
#
# This is a statement of the current fact, not an endorsement: the kernel should
# not be reaching into the simulation. Pulling those calls behind hooks the
# simulation installs would let this line go away.
target_link_libraries(engine_core PUBLIC game_sim)

# Presentation-facing gameplay services.
#
# These need a camera to do their job -- unprojecting a click, framing the
# minimap, writing the camera into a save. They consume the kernel; the kernel
# never consumes them.
add_library(
    game_view
    STATIC
    view/selection_controller.cpp
    systems/camera_controller.cpp
    systems/camera_follow_system.cpp
    systems/camera_service.cpp
    systems/camera_visibility_service.cpp
    systems/picking_service.cpp
    systems/game_state_serializer.cpp
    map/minimap/minimap_generator.cpp
    map/minimap/map_preview_generator.cpp
    map/minimap/minimap_fog_compositor.cpp
    map/minimap/unit_layer.cpp
    map/minimap/camera_viewport_layer.cpp
)

target_include_directories(game_view PUBLIC .)
target_link_libraries(game_view PUBLIC game_sim scene_core)

# Compatibility facade for consumers that just want "all of gameplay". New
# targets should depend on game_sim or game_view directly so the boundary keeps
# meaning something.
add_library(game_systems INTERFACE)
target_link_libraries(game_systems INTERFACE game_sim game_view)

add_subdirectory(audio)
