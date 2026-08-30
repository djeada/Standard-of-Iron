# The simulation, one static library per domain.
#
# Every target below is a layer, and the link lines are the layering: a target
# links the domains it is allowed to use and nothing else, so a file that starts
# including a header from a domain above it fails to link instead of quietly
# adding an edge. scripts/module_rules.json is the same map at header
# granularity -- it also separates the modules that still share one archive
# (unit_catalog / registries / world in soi_world; simulation / session in
# game_sim) -- and scripts/check-modules.py enforces it on every build.
#
# Bottom-up:
#
#   engine_core       ECS, the vocabulary types, the ambient session binding
#   soi_world         what a unit type is, the registries, terrain and maps
#   soi_navigation    nav grid, pathfinding, spatial index, walls and gates
#   soi_units         the unit and building factories, the map-to-world spawner
#   soi_formations    unit layouts, army formations, structure/formation geometry
#   soi_movement      movement, local avoidance, order and command services
#   soi_economy       production, gathering, delivery, capture, the marketplace
#   soi_combat        every damage pipeline, projectiles, healing, guard/patrol
#   soi_wildlife      the ambient creatures
#   game_sim          the session, the command pipeline, and the systems that
#                     compose a match (victory, cleanup, selection, settlement)
#
# game_sim links all of them PUBLIC, so linking `game_sim` still means "the
# whole simulation kernel" -- the name every test and tool already spells.

# ---- engine_core: the ECS and everything every layer names -------------------
#
# The vocabulary types (formation roles, army formation types) and the
# simulation clock live here because every layer above names them and none of
# them names anything above. The ambient session binding is here for the same
# reason: the registries define their `instance()` accessors against it, and
# the session module fills it in.
add_library(
    engine_core
    STATIC
    core/entity.cpp
    core/registry.cpp
    core/component.cpp
    core/system.cpp
    core/world.cpp
    core/world_spatial_index.cpp
    core/system_profiler.cpp
    core/system_schedule.cpp
    core/deferred_mutations.cpp
    core/ambient_session.cpp
    core/event_manager.cpp
    core/simulation_timing.cpp
    core/movement_facts.cpp
    core/movement_trace.cpp
    core/movement_trace_analysis.cpp
    session/simulation_clock.cpp
    systems/unit_activity.cpp
    util/asset_text.cpp
    util/json_vec_utils.cpp
    formation/formation_roles.cpp
    formation/army_formation_types.cpp
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
    accessibility/commander_input_settings.cpp
    accessibility/motion_settings.cpp
)
target_include_directories(accessibility_runtime PUBLIC ..)

# ---- soi_world: catalogues, registries, terrain -------------------------------
#
# What a unit type *is*, who owns what, and the ground it stands on. Three
# modules share this archive -- unit_catalog, registries, world -- because
# nothing above needs them apart; check-modules.py keeps their internal order.
#
# Qt::Gui is present only for the value types (QVector3D, QImage); it pulls in
# no windowing or GL. Qt::Sql is not linked here or anywhere below
# soi_persistence.
add_library(
    soi_world
    STATIC
    units/commander_catalog.cpp
    units/troop_catalog.cpp
    units/troop_catalog_loader.cpp
    visuals/building_asset_key.cpp
    systems/building_collision_registry.cpp
    systems/building_line_of_sight.cpp
    systems/economy_feedback.cpp
    systems/global_stats_registry.cpp
    systems/nation_loader.cpp
    systems/nation_registry.cpp
    systems/owner_queries.cpp
    systems/owner_registry.cpp
    systems/player_resource_registry.cpp
    systems/run_stamina.cpp
    systems/troop_count_registry.cpp
    systems/troop_profile_service.cpp
    map/base_options.cpp
    map/environment_lighting.cpp
    map/explored_mask_codec.cpp
    map/map_loader.cpp
    map/procedural_tree_generation.cpp
    map/scatter/spawn_validator.cpp
    map/scatter/tree_scatter_walk.cpp
    map/scatter/world_prop_clearance_index.cpp
    map/terrain.cpp
    map/terrain_landform.cpp
    map/terrain_service.cpp
    map/terrain_topology_audit.cpp
    map/undead_shrine_placement.cpp
    map/visibility_service.cpp
    wildlife/wildlife_config.cpp
    wildlife/wildlife_placement.cpp
)
target_include_directories(soi_world PUBLIC .)
target_link_libraries(
    soi_world
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        engine_core
        animation_core
        accessibility_runtime
)

# ---- soi_navigation: where a unit can stand and how it gets there -----------
#
# The grid, the search, the spatial index, and the walls and gates that carve
# the grid. It knows terrain and the collision registry; it does not know units
# or formations, which is why the unit factories can register walls with it.
add_library(
    soi_navigation
    STATIC
    systems/nav_grid.cpp
    systems/pathfinding.cpp
    systems/walkability.cpp
    systems/route_corridor_planner.cpp
    systems/gate_service.cpp
    systems/wall_network_service.cpp
    systems/terrain_alignment_system.cpp
)
target_include_directories(soi_navigation PUBLIC .)
target_link_libraries(soi_navigation PUBLIC soi_world)

# ---- soi_units: the factories ------------------------------------------------
#
# How each unit and building comes into the world, and the spawner that turns a
# loaded map into entities. A factory places things; it does not order them
# about, so nothing here links movement or combat.
add_library(
    soi_units
    STATIC
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
    units/building_spawn_setup.cpp
    units/factory.cpp
    units/barracks.cpp
    units/defense_tower.cpp
    units/home.cpp
    units/marketplace.cpp
    units/temple.cpp
    units/farm.cpp
    units/wall_segment.cpp
    units/wall_gate.cpp
    units/sheep.cpp
    units/wolf.cpp
    units/wildlife_unit_common.cpp
    map/map_transformer.cpp
)
target_include_directories(soi_units PUBLIC .)
target_link_libraries(soi_units PUBLIC soi_navigation)

# ---- soi_formations: layouts and army formations -----------------------------
#
# Soldier offsets inside a troop, multi-unit deployment, and the geometry that
# combat and movement both read (formation frontage, structure contact). The
# formation runtime *plans*; when a plan needs units moved it raises a flag the
# movement layer consumes, so nothing here links movement.
add_library(
    soi_formations
    STATIC
    formation/troop_role_registry.cpp
    formation/formation_doctrine.cpp
    formation/unit_layout.cpp
    formation/unit_layout_resolver.cpp
    formation/army_formation_planner.cpp
    formation/army_formation_registry.cpp
    formation/army_formation_service.cpp
    formation/formation_data_loader.cpp
    formation/unit_layout_state_system.cpp
    systems/cohort_system.cpp
    systems/defensive_unit_layout_service.cpp
    systems/formation_combat_geometry.cpp
    systems/combat_system/structure_combat.cpp
)
target_include_directories(soi_formations PUBLIC .)
target_link_libraries(soi_formations PUBLIC soi_units)

# ---- soi_movement: moving units and issuing them orders ----------------------
add_library(
    soi_movement
    STATIC
    systems/order_service.cpp
    systems/command_service.cpp
    systems/movement_route.cpp
    systems/route_follow_system.cpp
    systems/movement_system.cpp
    systems/unit_traversal_layout_system.cpp
    systems/movement_pipeline.cpp
    systems/movement_orders.cpp
    systems/local_avoidance_system.cpp
    systems/body_contact_system.cpp
    systems/formation_move_dispatch_system.cpp
)
target_include_directories(soi_movement PUBLIC .)
target_link_libraries(soi_movement PUBLIC soi_formations)

# ---- soi_economy: production, gathering, delivery, capture, trade -----------
add_library(
    soi_economy
    STATIC
    systems/build_site.cpp
    systems/construction_cost_catalog.cpp
    systems/production_service.cpp
    systems/production_system.cpp
    systems/civilian_delivery_system.cpp
    systems/resource_delivery_system.cpp
    systems/gather_loop_system.cpp
    systems/farm_system.cpp
    systems/food_targets.cpp
    systems/capture_system.cpp
    systems/marketplace_system.cpp
    systems/nation_collapse_service.cpp
)
target_include_directories(soi_economy PUBLIC .)
target_link_libraries(soi_economy PUBLIC soi_movement)

# ---- soi_combat: every way a unit hurts or heals another --------------------
add_library(
    soi_combat
    STATIC
    systems/combat_system.cpp
    systems/combat_system/combat_utils.cpp
    systems/combat_system/structure_fire.cpp
    systems/combat_system/damage_application.cpp
    systems/combat_system/damage_processor.cpp
    systems/combat_system/combat_mode_processor.cpp
    systems/combat_system/formation_contact_processor.cpp
    systems/combat_system/auto_engagement.cpp
    systems/combat_system/threat_alert.cpp
    systems/combat_system/hit_feedback_processor.cpp
    systems/combat_system/combat_hit_resolver.cpp
    systems/combat_system/combat_action_processor.cpp
    systems/combat_system/spear_brace_processor.cpp
    systems/combat_system/mounted_charge_processor.cpp
    systems/combat_system/combat_status_effect_processor.cpp
    systems/combat_system/combat_state_processor.cpp
    systems/combat_system/attack_processor.cpp
    systems/combat_system/melee_exchange.cpp
    systems/combat_system/elephant_special_processor.cpp
    systems/combat_system/siege_special_processor.cpp
    systems/combat_actions/body_impact.cpp
    systems/combat_actions/combat_action_definition.cpp
    systems/combat_actions/combat_action_events.cpp
    systems/combat_actions/combat_action_service.cpp
    systems/combat_actions/melee_intent_solver.cpp
    systems/combat_actions/projectile_release.cpp
    systems/combat_actions/weapon_trace.cpp
    systems/rpg_combat_system/rpg_bow_aim.cpp
    systems/rpg_combat_system/rpg_bow_draw.cpp
    systems/rpg_combat_system/rpg_bow_shot.cpp
    systems/rpg_combat_system/rpg_commander_damage.cpp
    systems/rpg_combat_system/rpg_damage_resolver.cpp
    systems/rpg_combat_system/rpg_combat_processor.cpp
    systems/rpg_combat_system/rpg_engagement_system.cpp
    systems/rpg_combat_system/rpg_targeting.cpp
    systems/patrol_system.cpp
    systems/guard_system.cpp
    systems/engagement_slot_system.cpp
    systems/target_commitment_system.cpp
    systems/attack_range.cpp
    systems/attack_targeting.cpp
    systems/target_focus.cpp
    systems/arrow_system.cpp
    systems/arrow_visual_profile.cpp
    systems/combat_status_effect_system.cpp
    systems/projectile_system.cpp
    systems/arrow_projectile.cpp
    systems/stone_projectile.cpp
    systems/commander_system.cpp
    systems/healing_rules.cpp
    systems/healing_system.cpp
    systems/healing_beam.cpp
    systems/healing_beam_system.cpp
    systems/stamina_system.cpp
)
target_include_directories(soi_combat PUBLIC .)
target_link_libraries(soi_combat PUBLIC soi_economy)

# ---- soi_wildlife: the ambient creatures -------------------------------------
add_library(
    soi_wildlife
    STATIC
    wildlife/wildlife_terrain_probe.cpp
    wildlife/wildlife_threats.cpp
    wildlife/bird_flock.cpp
    wildlife/wildlife_system.cpp
    wildlife/nature_ai.cpp
)
target_include_directories(soi_wildlife PUBLIC .)
target_link_libraries(soi_wildlife PUBLIC soi_combat)

# ---- game_sim: the session and the systems that compose a match --------------
#
# The top of the kernel: the session context that owns every per-match service,
# the command pipeline that every order -- player, AI, script -- goes through,
# and the systems that are about the match rather than any one domain. It links
# everything below, so `game_sim` still means "the whole simulation kernel".
#
# It deliberately does NOT link scene_core or render_gl. Headless balance runs,
# deterministic replay and a dedicated server all consume this target alone.
# Nor does it contain the AI, the mission and campaign loaders, the save stack
# or the system composition root: those are separate targets below, stacked on
# top of this one, so the kernel cannot call into them.
add_library(
    game_sim
    STATIC
    command/command.cpp
    command/command_codec.cpp
    command/command_dispatcher.cpp
    command/command_queue.cpp
    command/command_system.cpp
    command/command_validator.cpp
    command/replay.cpp
    session/session_context.cpp
    session/world_digest.cpp
    systems/default_content.cpp
    systems/dismantle_system.cpp
    systems/interaction_targeting.cpp
    systems/squad_service.cpp
    systems/structure_placement_service.cpp
    systems/wall_plan_service.cpp
    systems/cleanup_system.cpp
    systems/gate_system.cpp
    systems/home_system.cpp
    systems/rain_manager.cpp
    systems/selection_system.cpp
    systems/settlement_life_system.cpp
    systems/showcase_routine_system.cpp
    systems/cursed_gold_vein_system.cpp
    systems/undead_awakening_system.cpp
    systems/victory_service.cpp
    # Rebuilding the registries that are derived from world contents rather than
    # stored with them. It was in app/persistence, which meant the mission code
    # that needs it depended on the client -- the one edge that kept
    # app/mission/ out of game/. It is not in the save stack either: a mission
    # setup needs it and never touches a save file.
    systems/world_restore.cpp
)

target_include_directories(game_sim PUBLIC .)
target_link_libraries(game_sim PUBLIC soi_wildlife soi_combat soi_economy soi_movement)

# engine_core once linked game_sim as well, in a declared cycle.
#
# core/world.cpp called the owner, building-collision and troop-count
# registries; core/serialization.cpp called terrain, the terrain service and the
# formation registry. Both archives genuinely needed each other, resolution
# depended on which one the linker reached first, and persistence_tests failed
# under Ninja/Debug while Make/Release linked.
#
# None of it was the ECS's work:
#
#   * the ownership queries became Game::Systems::allied_units and friends
#     (game/systems/owner_queries.h);
#   * releasing a destroyed building's footprint became an entity-destroyed
#     observer that SessionContext installs -- World already had the mechanism;
#   * serialization moved to soi_persistence, which is what it always was.
#
# The same shape came back once more when the registries were split from the
# session: their `instance()` accessors were defined in game/session/, so the
# registries archive needed the session archive to link. The accessors now
# resolve through game/core/ambient_session.h, a leaf the session fills in.
# `nm -u` on any archive below game_sim names nothing from the archives above.

# ---------------------------------------------------------------------------
# Domains stacked on the kernel.
#
# None of these is reachable from `game_sim`: the kernel does not include a
# single one of their headers, and does not link them, so the day a kernel file
# calls the AI or the save stack, the kernel stops linking.
#
# They stack, they do not interleave:
#
#     game_sim  <-  soi_campaign  <-  soi_missions
#           ^-----  soi_persistence
#           ^-----  soi_ai  <-  soi_runtime
#           ^-----  game_view
# ---------------------------------------------------------------------------

# The campaign definition: which missions exist, in what order, and what a
# province is worth. Read before a match starts and by the save database, never
# during one.
add_library(soi_campaign STATIC map/campaign_loader.cpp)
target_include_directories(soi_campaign PUBLIC .)
target_link_libraries(soi_campaign PUBLIC game_sim)

# ---- soi_missions: reading mission content ------------------------------------
#
# The mission file, its victory rules, its wave archetypes, the catalogue that
# indexes maps, missions and campaigns together, the commander each side is
# given, and the view of a definition the menus read. The catalogue is what
# makes this depend on soi_campaign.
#
# It links no AI and no save database on purpose. Reading a mission must not
# require a match to be running or a save slot to exist, and this link line is
# what says so -- see the campaign_tests comment in tests/CMakeLists.txt.
add_library(
    soi_missions
    STATIC
    map/mission_loader.cpp
    map/mission_stage_tracker.cpp
    map/mission_victory_rules.cpp
    map/wave_archetype_catalog.cpp
    map/map_catalog.cpp
    mission/mission_commander_setup.cpp
    mission/mission_definition_view.cpp
    mission/tutorial_director.cpp
)
target_include_directories(soi_missions PUBLIC .)
target_link_libraries(soi_missions PUBLIC game_sim soi_campaign)
# TutorialDirector reports step changes with signals.
set_target_properties(soi_missions PROPERTIES AUTOMOC ON)

# The save stack: what a snapshot contains, how it is encoded, and the SQL
# database it lands in. It reads the campaign to record progress.
#
# Nothing in the kernel may reach back into this. A save is written *about* the
# simulation, not *by* it.
add_library(
    soi_persistence
    STATIC
    save/snapshot_contract.cpp
    # Serialization used to sit in engine_core. It writes the owner registry,
    # the formation registry and the terrain into a save file, so having it
    # there meant the ECS core reached into three gameplay modules -- most of
    # what made engine_core and game_sim a declared static-library cycle. It was
    # never the ECS's job: nothing in game/core/ calls it.
    save/serialization.cpp
    systems/save_format.cpp
    systems/save_load_service.cpp
    systems/save_storage.cpp
)
target_include_directories(soi_persistence PUBLIC .)
# The save database is the only thing in the simulation stack that speaks SQL.
target_link_libraries(soi_persistence PUBLIC Qt${QT_VERSION_MAJOR}::Sql game_sim soi_campaign)

# The computer opponent. It reads a snapshot of the world and emits commands
# through the same queue a player's clicks go through, which is why it can sit
# strictly above the kernel: it has no privileged entry point.
add_library(
    soi_ai
    STATIC
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
    systems/ai_system/ai_commander_doctrine.cpp
    systems/ai_system/ai_doctrine_catalog.cpp
    systems/ai_system/ai_attack_wave.cpp
    systems/ai_system/behaviors/production_behavior.cpp
    systems/ai_system/behaviors/gather_behavior.cpp
    systems/ai_system/behaviors/harass_behavior.cpp
    systems/ai_system/behaviors/assault_behavior.cpp
    systems/ai_system/behaviors/attack_behavior.cpp
    systems/ai_system/behaviors/local_engagement_behavior.cpp
    systems/ai_system/behaviors/defend_behavior.cpp
    systems/ai_system/behaviors/retreat_behavior.cpp
    systems/ai_system/behaviors/expand_behavior.cpp
    systems/ai_system/behaviors/builder_behavior.cpp
    systems/ai_system/behaviors/economy_behavior.cpp
    systems/ai_system/behaviors/squad_discipline_behavior.cpp
    systems/ai_system/behaviors/commander_behavior.cpp
)
target_include_directories(soi_ai PUBLIC .)
target_link_libraries(soi_ai PUBLIC game_sim)

# ---- soi_mission_runtime: running a mission in a live world -------------------
#
# Spawning the forces a definition names, configuring its AI opponents, building
# and firing its attack waves, and recording campaign progress.
#
# This was app/mission/ until the layering audit. Nothing in it names a
# renderer, a camera, a QML type or a view model: it is world manipulation, and
# the two edges that held it behind the client boundary were a HUD counter
# struct it wrote directly and the registry rebuild in app/persistence. Both
# were inverted first, so the move needed no new abstraction.
#
# soi_persistence because a campaign records which missions are complete, and
# soi_ai because a definition names its opponents with a strategy and a
# personality. Neither names a mission, so both edges are one-way.
add_library(
    soi_mission_runtime
    STATIC
    mission/campaign_manager.cpp
    mission/commander_message_director.cpp
    mission/mission_setup_coordinator.cpp
    mission/mission_wave_director.cpp
    mission/mission_waves.cpp
)
target_include_directories(soi_mission_runtime PUBLIC .)
target_link_libraries(soi_mission_runtime PUBLIC soi_missions soi_persistence soi_ai)
# CampaignManager reports campaign and mission changes with signals.
set_target_properties(soi_mission_runtime PROPERTIES AUTOMOC ON)

# The composition root for the ECS systems: which systems a running match owns
# and in what order they tick, plus the headless capture harness that drives
# them.
#
# This is the one place that legitimately knows about every system including the
# AI, and it is the reason `soi_ai` could be lifted out at all -- these two files
# were the kernel's only route into it. They belong above the things they
# assemble, not beside them.
add_library(soi_runtime STATIC systems/runtime_system_registry.cpp systems/battlefield_capture.cpp)
target_include_directories(soi_runtime PUBLIC .)
target_link_libraries(soi_runtime PUBLIC game_sim soi_ai)

# ---- game_view: the render bridge --------------------------------------------
#
# Presentation-facing gameplay services: the ones that need a camera to do their
# job -- unprojecting a click, framing the minimap, writing the camera into a
# save. They consume the kernel; the kernel never consumes them, and neither
# does render_gl.
#
# The sources were spread across game/view/, five loose files in game/systems/
# and game/map/minimap/, so the only place the module was visible as one thing
# was this list and a hand-kept set in tests/architecture/layering_test.cpp.
# They are one directory now, named after the module -- which is also the name
# scripts/module_rules.json and docs/ARCHITECTURE.md already used for it.
add_library(
    game_view
    STATIC
    render_bridge/selection_controller.cpp
    render_bridge/camera_controller.cpp
    render_bridge/camera_follow_system.cpp
    render_bridge/camera_service.cpp
    render_bridge/picking_service.cpp
    render_bridge/game_state_serializer.cpp
    render_bridge/minimap/minimap_generator.cpp
    render_bridge/minimap/map_preview_generator.cpp
    render_bridge/minimap/minimap_fog_compositor.cpp
    render_bridge/minimap/unit_layer.cpp
    render_bridge/minimap/camera_viewport_layer.cpp
)

target_include_directories(game_view PUBLIC .)
target_link_libraries(game_view PUBLIC game_sim scene_core)

# Compatibility facade for consumers that just want "all of gameplay". New
# targets should depend on the specific domains they use so the boundary keeps
# meaning something: linking this pulls the AI and the save stack into a binary
# that may want neither.
add_library(game_systems INTERFACE)
target_link_libraries(
    game_systems
    INTERFACE
        game_sim
        game_view
        soi_ai
        soi_runtime
        soi_missions
        soi_mission_runtime
        soi_campaign
        soi_persistence
)

# The names the architecture is described by, mapped onto the targets that
# already carried those responsibilities before the split. Aliases rather than
# renames: `engine_core` and `game_sim` are spelled out in a dozen CMakeLists,
# in docs/ARCHITECTURE.md and in the tests that read it, and churning all of
# that would buy no enforcement. New code may use either spelling.
add_library(soi_components ALIAS engine_core)
add_library(soi_simulation ALIAS game_sim)
add_library(soi_render_bridge ALIAS game_view)

add_subdirectory(audio)
