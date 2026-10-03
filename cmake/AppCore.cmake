# ---- Application layer ----
# The composition root, view models and input adapters, built once as a library
# so the executable and the test suite consume the same binary instead of each
# recompiling the same translation units with their own flags. Tests that
# recompiled these files could disagree with the shipped build; linking one
# target removes that whole class of drift.
add_library(
    app_core
    STATIC
    # app/core -- the composition root and the frame it drives.
    app/core/benchmark_action_fixture.cpp
    app/core/audio_services.cpp
    app/core/entity_cache.cpp
    app/core/order_feedback_presenter.cpp
    app/core/film_action_dispatch.cpp
    app/core/film_recorder.cpp
    app/core/frame_ui_coordinator.cpp
    app/core/game_engine.cpp
    app/core/game_engine_composition.cpp
    app/core/game_engine_wiring.cpp
    app/core/language_manager.cpp
    app/core/match_presentation_sync.cpp
    app/core/player_feedback.cpp
    app/core/frame_snapshot.cpp
    app/core/runtime_frame_orchestrator.cpp
    app/core/simulation_lifecycle.cpp
    # app/mission -- the client's half of running a mission. The mission logic
    # itself is game/mission (soi_mission_runtime); what is left here is reading
    # the frame the tutorial director is about to be advanced through.
    app/mission/tutorial_observation.cpp
    app/mission/commander_message_runtime.cpp
    app/mission/mission_runtime.cpp
    app/mission/tutorial_runtime.cpp
    # app/session -- bringing one match up: world, renderer, level, skirmish.
    app/session/environment.cpp
    app/session/environment_runtime.cpp
    app/session/level_loader.cpp
    app/session/level_orchestrator.cpp
    app/session/loading_overlay.cpp
    app/session/loading_progress_tracker.cpp
    app/session/renderer_bootstrap.cpp
    app/session/replay_coordinator.cpp
    app/session/skirmish_loader.cpp
    app/session/skirmish_runtime_coordinator.cpp
    app/session/world_bootstrap.cpp
    # app/commander -- the first-person control mode and its camera.
    app/commander/commander_abilities.cpp
    app/commander/commander_body_facing.cpp
    app/commander/commander_camera_rig.cpp
    app/commander/commander_control_controller.cpp
    app/commander/commander_defence.cpp
    app/commander/commander_entity_access.cpp
    app/commander/commander_input_port.cpp
    app/commander/commander_locomotion.cpp
    app/commander/commander_look.cpp
    app/commander/commander_lunge.cpp
    app/commander/commander_motor.cpp
    app/commander/commander_latency_probe.cpp
    app/commander/commander_mode_coordinator.cpp
    app/commander/commander_presentation.cpp
    app/commander/commander_primary_scan.cpp
    app/commander/commander_status_builder.cpp
    app/commander/commander_strike.cpp
    app/commander/commander_targeting.cpp
    app/commander/commander_tick_trace.cpp
    app/commander/rts_camera_bookmark.cpp
    # app/input -- pointer and key gestures, before they become orders.
    app/input/cursor_manager.cpp
    app/input/context_interaction_resolver.cpp
    app/input/formation_pointer_adapter.cpp
    app/input/hover_tracker.cpp
    app/input/input_command_handler.cpp
    app/input/rts_camera_controller.cpp
    # app/orders -- gestures turned into Game::Command payloads, and the
    # feedback the player gets back for them.
    app/orders/action_vfx.cpp
    app/orders/army_formation_controller.cpp
    app/orders/command_controller.cpp
    app/orders/context_intent.cpp
    app/orders/formation_options_model.cpp
    app/orders/formation_placement.cpp
    app/orders/formation_preview_cache.cpp
    app/orders/formation_readout.cpp
    app/orders/local_command.cpp
    app/orders/mode_toggle_commands.cpp
    app/orders/movement_commands.cpp
    app/orders/movement_utils.cpp
    app/orders/order_cues.cpp
    app/orders/order_feedback.cpp
    app/orders/order_issuer.cpp
    app/orders/order_markers.cpp
    app/orders/order_submission.cpp
    app/orders/patrol_commands.cpp
    app/orders/roster_commands.cpp
    app/orders/rts_action_model.cpp
    app/orders/worker_commands.cpp
    app/orders/worker_orders.cpp
    # app/economy -- production, trade and the resource coach.
    app/economy/economy_overview.cpp
    app/economy/economy_read_model.cpp
    app/economy/harvest_targeting.cpp
    app/economy/construction_pointer.cpp
    app/economy/construction_preview.cpp
    app/economy/placement_orders.cpp
    app/economy/placement_refusal.cpp
    app/economy/placement_session.cpp
    app/economy/production_manager.cpp
    app/economy/wall_placement_session.cpp
    app/economy/production_readouts.cpp
    app/economy/resource_text.cpp
    app/economy/unit_profile.cpp
    # app/persistence -- saving and restoring a match.
    app/persistence/game_state_restorer.cpp
    app/persistence/save_load_coordinator.cpp
    app/persistence/save_orchestrator.cpp
    app/persistence/save_slot_controller.cpp
    # app/world -- world state read back as client presentation state.
    app/world/ambient_state_manager.cpp
    app/world/ally_announcements.cpp
    app/world/battle_stats.cpp
    app/world/focus_target.cpp
    app/world/focus_tracker.cpp
    app/world/minimap_manager.cpp
    app/world/minimap_events.cpp
    app/world/targeting_presentation.cpp
    app/world/player_defeat_watcher.cpp
    app/world/selection_query_service.cpp
    app/world/unit_queries.cpp
    app/world/visibility_coordinator.cpp
    app/world/world_feedback.cpp
    app/world/world_feedback_events.cpp
    # app/audio -- cue routing and the QML-facing audio proxy.
    app/audio/audio_coordinator.cpp
    app/audio/audio_resource_loader.cpp
    app/audio/audio_status_hud.cpp
    app/audio/audio_system_proxy.cpp
    app/audio/weather_audio.cpp
    # app/viewmodels -- the QML API, one coherent slice per model.
    app/viewmodels/activity_view_model.cpp
    app/viewmodels/camera_view_model.cpp
    app/viewmodels/commander_message_view_model.cpp
    app/viewmodels/commander_view_model.cpp
    app/viewmodels/economy_view_model.cpp
    app/viewmodels/match_setup_view_model.cpp
    app/viewmodels/minimap_view_model.cpp
    app/viewmodels/mission_view_model.cpp
    app/viewmodels/orders_view_model.cpp
    app/viewmodels/placement_view_model.cpp
    app/viewmodels/production_view_model.cpp
    app/viewmodels/save_slots_view_model.cpp
    app/viewmodels/wave_view_model.cpp
    # app/models -- Qt item models and image providers.
    app/models/graphics_settings_proxy.cpp
    app/models/loading_tips.cpp
    app/models/map_preview_image_provider.cpp
    app/models/minimap_image_provider.cpp
    app/models/selected_units_model.cpp
    app/models/selection_grouping.cpp
)

target_include_directories(app_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    app_core
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::Sql
        $<$<TARGET_EXISTS:Qt${QT_VERSION_MAJOR}::Multimedia>:Qt${QT_VERSION_MAJOR}::Multimedia>
        engine_core
        render_gl
        game_sim
        game_view
        # The application is the layer that legitimately needs all of gameplay:
        # it starts missions, restores saves, installs the system registry and
        # hands the AI its opponents. Named one by one rather than through the
        # game_systems facade so this list stays a true statement of what the
        # composition root touches -- and so removing a responsibility from it
        # shows up here.
        soi_ai
        soi_runtime
        soi_missions
        soi_mission_runtime
        soi_campaign
        soi_persistence
        audio_system
)
set_target_properties(app_core PROPERTIES AUTOMOC ON)

# The name docs/ARCHITECTURE.md gives this layer. See the alias note in
# game/CMakeLists.txt.
add_library(soi_app ALIAS app_core)
