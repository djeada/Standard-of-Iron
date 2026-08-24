

#include <QBuffer>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLContext>
#include <QPainter>
#include <QPointer>
#include <QQuickWindow>
#include <QSet>
#include <QSize>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>
#include <qbuffer.h>
#include <qcoreapplication.h>
#include <qdir.h>
#include <qevent.h>
#include <qglobal.h>
#include <qimage.h>
#include <qjsonobject.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qpoint.h>
#include <qsize.h>
#include <qstringliteral.h>
#include <qstringview.h>
#include <qtmetamacros.h>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "app/audio/audio_coordinator.h"
#include "app/audio/audio_resource_loader.h"
#include "app/audio/audio_system_proxy.h"
#include "app/audio/weather_audio.h"
#include "app/commander/commander_mode_coordinator.h"
#include "app/commander/commander_status_builder.h"
#include "app/core/frame_ui_coordinator.h"
#include "app/core/game_engine.h"
#include "app/core/game_speed.h"
#include "app/core/user_settings.h"
#include "app/economy/production_manager.h"
#include "app/input/cursor_manager.h"
#include "app/input/cursor_mode.h"
#include "app/input/hover_tracker.h"
#include "app/input/input_command_handler.h"
#include "app/input/rts_camera_controller.h"
#include "app/models/selected_units_model.h"
#include "app/orders/action_vfx.h"
#include "app/orders/command_controller.h"
#include "app/orders/movement_utils.h"
#include "app/orders/order_submission.h"
#include "app/orders/rts_action_model.h"
#include "app/persistence/game_state_restorer.h"
#include "app/persistence/save_load_coordinator.h"
#include "app/session/environment.h"
#include "app/session/level_loader.h"
#include "app/session/loading_progress_tracker.h"
#include "app/session/renderer_bootstrap.h"
#include "app/session/skirmish_loader.h"
#include "app/session/skirmish_runtime_coordinator.h"
#include "app/session/world_bootstrap.h"
#include "app/utils/engine_view_helpers.h"
#include "app/viewmodels/commander_message_view_model.h"
#include "app/viewmodels/save_slots_view_model.h"
#include "app/world/ambient_state_manager.h"
#include "app/world/minimap_manager.h"
#include "app/world/selection_query_service.h"
#include "app/world/unit_queries.h"
#include "app/world/visibility_coordinator.h"
#include "game/audio/audio_cues.h"
#include "game/audio/audio_event_handler.h"
#include "game/audio/audio_system.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/system.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/game_config.h"
#include "game/map/campaign_loader.h"
#include "game/map/map_catalog.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/mission_context.h"
#include "game/map/mission_loader.h"
#include "game/map/render_visibility_rules.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/mission_commander_setup.h"
#include "game/mission/mission_definition_view.h"
#include "game/mission/mission_setup_coordinator.h"
#include "game/mission/mission_waves.h"
#include "game/render_bridge/camera_service.h"
#include "game/render_bridge/minimap/map_preview_generator.h"
#include "game/render_bridge/minimap/minimap_generator.h"
#include "game/render_bridge/minimap/minimap_utils.h"
#include "game/render_bridge/minimap/unit_layer.h"
#include "game/render_bridge/picking_service.h"
#include "game/render_bridge/selection_controller.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/attack_range.h"
#include "game/systems/attack_targeting.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/capture_system.h"
#include "game/systems/cleanup_system.h"
#include "game/systems/combat_rules.h"
#include "game/systems/combat_system.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/guard_system.h"
#include "game/systems/healing_system.h"
#include "game/systems/marketplace_system.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/movement_system.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/patrol_system.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/production_service.h"
#include "game/systems/production_system.h"
#include "game/systems/rain_manager.h"
#include "game/systems/rpg_combat_system/rpg_combat_processor.h"
#include "game/systems/save_load_service.h"
#include "game/systems/selection_system.h"
#include "game/systems/terrain_alignment_system.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/systems/undead_awakening_system.h"
#include "game/systems/victory_service.h"
#include "game/units/commander_catalog.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"
#include "game/units/troop_type.h"
#include "game/util/asset_text.h"
#include "game/util/selection_utils.h"
#include "game/visuals/team_colors.h"
#include "render/camera_visibility.h"
#include "render/geom/stone.h"
#include "render/gl/bootstrap.h"
#include "render/ground/ambient_fog_renderer.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/terrain_surface_manager.h"
#include "render/ground/tree_renderer.h"
#include "render/scene_renderer.h"
#include "render/terrain_scene_proxy.h"
#include "scene/camera.h"
#include "utils/resource_utils.h"

void GameEngine::build_client_and_view_models() {

  m_session = std::make_unique<Game::Session::SessionContext>();
  m_session_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
  m_world = &m_session->world();

  App::Core::ClientHost& host = *this;
  publish_client_context();

  m_camera_view_model =
      std::make_unique<App::ViewModels::CameraViewModel>(m_client, host, this);
  m_match_setup_view_model =
      std::make_unique<App::ViewModels::MatchSetupViewModel>(m_client, host, this);
  m_production_view_model =
      std::make_unique<App::ViewModels::ProductionViewModel>(m_client, host, this);
  m_minimap_view_model = std::make_unique<App::ViewModels::MinimapViewModel>(
      m_client, host, *m_camera_view_model, this);
  m_mission_view_model = std::make_unique<App::ViewModels::MissionViewModel>(
      m_client, host, *m_camera_view_model, this);
  m_placement_view_model =
      std::make_unique<App::ViewModels::PlacementViewModel>(m_client, host, this);
  m_activity_view_model =
      std::make_unique<App::ViewModels::ActivityViewModel>(m_client, host, this);
  m_commander_view_model = std::make_unique<App::ViewModels::CommanderViewModel>(
      m_client, host, *m_camera_view_model, *m_placement_view_model, this);
  m_orders_view_model = std::make_unique<App::ViewModels::OrdersViewModel>(
      m_client, host, *m_placement_view_model, *m_commander_view_model, this);

  connect(m_production_view_model.get(),
          &App::ViewModels::ProductionViewModel::refused,
          this,
          [this](const QString& message) { set_error(message); });
  connect(m_production_view_model.get(),
          &App::ViewModels::ProductionViewModel::player_state_stale,
          this,
          [this] {
            clear_error();
            sync_selected_player_state();
          });
  connect(m_match_setup_view_model.get(),
          &App::ViewModels::MatchSetupViewModel::launch_requested,
          this,
          &GameEngine::launch_match);
  connect(m_match_setup_view_model.get(),
          &App::ViewModels::MatchSetupViewModel::failed,
          this,
          [this](const QString& message) { set_error(message); });
  connect(m_camera_view_model.get(),
          &App::ViewModels::CameraViewModel::moved,
          this,
          [this] { m_tutorial_notes.camera_used = true; });
  connect(m_commander_view_model.get(),
          &App::ViewModels::CommanderViewModel::game_mode_changed,
          this,
          &GameEngine::apply_game_mode_render_policy);
  connect(m_commander_view_model.get(),
          &App::ViewModels::CommanderViewModel::active_camera_requested,
          this,
          &GameEngine::set_active_camera);
  connect(m_commander_view_model.get(),
          &App::ViewModels::CommanderViewModel::rts_selection_restored,
          this,
          [this] {
            sync_selection_flags();
            emit selected_units_changed();
          });
  connect(m_activity_view_model.get(),
          &App::ViewModels::ActivityViewModel::inspect_target_cleared,
          this,
          [this] {
            emit selected_units_changed();
            sync_focus_targets();
          });

  m_save_slots_view_model =
      std::make_unique<App::ViewModels::SaveSlotsViewModel>(m_save_load_service, this);
  connect(m_save_slots_view_model.get(),
          &App::ViewModels::SaveSlotsViewModel::error_occurred,
          this,
          [this](const QString& message) { set_error(message); });
  connect(m_save_slots_view_model.get(),
          &App::ViewModels::SaveSlotsViewModel::autosave_interval_changed,
          this,
          [this] { restart_autosave_timer(); });
  connect(m_save_slots_view_model.get(),
          &App::ViewModels::SaveSlotsViewModel::save_requested,
          this,
          &GameEngine::save_game_to_slot);
  connect(m_save_slots_view_model.get(),
          &App::ViewModels::SaveSlotsViewModel::quicksave_requested,
          this,
          &GameEngine::quicksave);
  connect(m_save_slots_view_model.get(),
          &App::ViewModels::SaveSlotsViewModel::autosave_requested,
          this,
          &GameEngine::autosave);
  connect(m_save_slots_view_model.get(),
          &App::ViewModels::SaveSlotsViewModel::cancel_save_requested,
          this,
          &GameEngine::cancel_active_save);
  connect(m_save_slots_view_model.get(),
          &App::ViewModels::SaveSlotsViewModel::load_requested,
          this,
          &GameEngine::load_game_from_slot);

  m_wave_view_model = std::make_unique<App::ViewModels::WaveViewModel>(this);
  m_commander_message_view_model =
      std::make_unique<App::ViewModels::CommanderMessageViewModel>(this);
  connect(m_commander_message_view_model.get(),
          &App::ViewModels::CommanderMessageViewModel::dismiss_requested,
          this,
          [this]() {
            if (m_commander_message_director.dismiss_active()) {
              publish_commander_message();
            }
          });
  m_economy_view_model = std::make_unique<App::ViewModels::EconomyViewModel>(this);
  m_economy_refresh_timer.start();
  m_tutorial_director = std::make_unique<Game::Mission::TutorialDirector>(this);
  connect(m_tutorial_director.get(),
          &Game::Mission::TutorialDirector::start_requested,
          m_match_setup_view_model.get(),
          &App::ViewModels::MatchSetupViewModel::start_tutorial);

  Game::Systems::initialize_default_content(m_session->nations());
  m_session->troop_counts().initialize();
  m_session->stats().initialize();
}

void GameEngine::build_services_and_controllers() {

  auto rendering = RendererBootstrap::initialize_rendering();
  m_renderer = std::move(rendering.renderer);
  m_rts_camera = std::move(rendering.camera);
  m_commander_camera = std::make_unique<Render::GL::Camera>(*m_rts_camera);
  set_active_camera(m_rts_camera.get());
  apply_game_mode_render_policy();
  m_terrain_scene = std::move(rendering.terrain_scene);
  m_surface = std::move(rendering.surface);
  m_features = std::move(rendering.features);
  m_scatter = std::move(rendering.scatter);
  m_fog = std::move(rendering.fog);
  m_boundary_fog = std::move(rendering.boundary_fog);
  m_ambient_fog = std::move(rendering.ambient_fog);
  m_rain = std::move(rendering.rain);

  RendererBootstrap::initialize_world_systems(*m_world);

  m_picking_service = std::make_unique<Game::Systems::PickingService>();
  auto& session = *m_session;
  m_victory_service = std::make_unique<Game::Systems::VictoryService>(
      Game::Systems::VictoryService::Services{.stats = session.stats(),
                                              .owners = session.owners(),
                                              .nations = session.nations(),
                                              .economy = session.economy()});

  connect_save_service_signals();
  m_camera_service =
      std::make_unique<Game::Systems::CameraService>(m_session->visibility());
  m_rain_manager = std::make_unique<Game::Systems::RainManager>();
  m_weather_audio = std::make_unique<App::Core::WeatherAudio>();
  m_environment_clock = std::make_unique<Game::Map::EnvironmentClock>();

  m_loading_progress_tracker = std::make_unique<LoadingProgressTracker>(this);
  connect(m_loading_progress_tracker.get(),
          &LoadingProgressTracker::progress_changed,
          this,
          [this](float progress) { emit loading_progress_changed(progress); });
  connect(m_loading_progress_tracker.get(),
          &LoadingProgressTracker::stage_changed,
          this,
          [this](LoadingProgressTracker::LoadingStage, QString detail) {
            emit loading_stage_changed(std::move(detail));
          });

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  m_selection_controller = std::make_unique<Game::Systems::SelectionController>(
      m_world, selection_system, m_picking_service.get());
  m_selection_controller->set_inspect_filter([this](Engine::Core::EntityID id) {
    if (m_world == nullptr || m_visibility_coordinator == nullptr) {
      return true;
    }
    auto* entity = m_world->get_entity(id);
    const auto* transform =
        entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (transform == nullptr) {
      return false;
    }
    const auto snapshot = m_visibility_coordinator->current_snapshot();
    if (snapshot == nullptr || !snapshot->initialized) {
      return true;
    }
    return Game::Map::should_render_non_local_unit(
        *snapshot, transform->position.x, transform->position.z);
  });
  m_command_controller = std::make_unique<App::Controllers::CommandController>(
      m_world, selection_system, m_picking_service.get());

  m_cursor_manager = std::make_unique<CursorManager>();
  m_hover_tracker = std::make_unique<HoverTracker>(m_picking_service.get());

  m_map_catalog = std::make_unique<Game::Map::MapCatalog>(this);
  connect(m_map_catalog.get(),
          &Game::Map::MapCatalog::map_loaded,
          this,
          [this](const QVariantMap& map_data) {
            m_catalogued_maps.append(map_data);
            m_match_setup_view_model->set_maps(m_catalogued_maps);
          });
  connect(
      m_map_catalog.get(),
      &Game::Map::MapCatalog::loading_changed,
      this,
      [this](bool loading) { m_match_setup_view_model->set_maps_loading(loading); });

  if (AudioSystem::get_instance().initialize()) {
    qInfo() << "AudioSystem initialized successfully";
    AudioResourceLoader::load_audio_resources();
    AudioResourceLoader::load_audio_cues();
  } else {
    qWarning() << "Failed to initialize AudioSystem";
  }

  m_audio_systemProxy = std::make_unique<App::Models::AudioSystemProxy>(this);

  m_minimap_manager = std::make_unique<MinimapManager>();
  m_visibility_coordinator =
      std::make_unique<VisibilityCoordinator>(m_session->visibility());
  m_visibility_coordinator->set_presenters(m_fog.get(), m_minimap_manager.get());
  m_ambient_state_manager = std::make_unique<AmbientStateManager>();

  m_input_handler = std::make_unique<InputCommandHandler>(m_world,
                                                          m_selection_controller.get(),
                                                          m_command_controller.get(),
                                                          m_cursor_manager.get(),
                                                          m_hover_tracker.get(),
                                                          m_picking_service.get(),
                                                          m_rts_camera.get());

  m_camera_controller = std::make_unique<RtsCameraController>(
      m_rts_camera.get(), m_camera_service.get(), m_world);

  m_production_manager = std::make_unique<ProductionManager>(
      m_world, m_picking_service.get(), m_rts_camera.get(), this);
  connect(m_production_manager.get(),
          &ProductionManager::placing_construction_changed,
          m_placement_view_model.get(),
          &App::ViewModels::PlacementViewModel::placing_construction_changed);
  connect(m_production_manager.get(),
          &ProductionManager::construction_preview_active_changed,
          m_placement_view_model.get(),
          &App::ViewModels::PlacementViewModel::construction_preview_active_changed);
  connect(m_production_manager.get(),
          &ProductionManager::construction_preview_valid_changed,
          m_placement_view_model.get(),
          &App::ViewModels::PlacementViewModel::construction_preview_valid_changed);
  connect(m_production_manager.get(),
          &ProductionManager::construction_preview_summary_changed,
          m_placement_view_model.get(),
          &App::ViewModels::PlacementViewModel::construction_preview_summary_changed);
  connect(m_production_manager.get(),
          &ProductionManager::construction_placement_rejected,
          this,
          [this](const QString& reason) {
            Game::Audio::play_cue(Game::Audio::Cue::k_build_placement_rejected);
            if (reason.isEmpty()) {
              return;
            }
            const bool gathering =
                m_production_manager->pending_builder_construction_type() ==
                QStringLiteral("collect");
            auto outcome = App::Core::rejected_order(
                gathering ? App::Core::OrderKind::Gather : App::Core::OrderKind::Build,
                App::Core::OrderRefusal{App::Core::OrderFailure::CommandUnavailable,
                                        reason});
            handle_order_feedback(outcome);
          });
  connect(m_production_manager.get(),
          &ProductionManager::order_feedback,
          this,
          &GameEngine::handle_order_feedback);

  m_campaign_manager = std::make_unique<CampaignManager>(this);
  connect(m_campaign_manager.get(),
          &CampaignManager::available_campaigns_changed,
          m_match_setup_view_model.get(),
          &App::ViewModels::MatchSetupViewModel::notify_campaigns_changed);

  m_selection_query_service = std::make_unique<SelectionQueryService>(m_world, this);

  m_audio_event_handler = std::make_unique<Game::Audio::AudioEventHandler>(m_world);
  m_audio_coordinator = std::make_unique<AudioCoordinator>(m_audio_event_handler.get(),
                                                           m_session->nations());
  m_mission_setup = std::make_unique<Game::Mission::MissionSetupCoordinator>();
  m_save_load_coordinator = std::make_unique<App::Core::SaveLoadCoordinator>();
  m_skirmish_runtime = std::make_unique<App::Core::SkirmishRuntimeCoordinator>();
  if (m_audio_event_handler->initialize()) {
    qInfo() << "AudioEventHandler initialized successfully";
    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Screen);
    m_audio_coordinator->configure_audio_manifest_mappings(m_runtime.local_owner_id);

    qInfo() << "Audio mappings configured";
  } else {
    qWarning() << "Failed to initialize AudioEventHandler";
  }

  connect(m_cursor_manager.get(),
          &CursorManager::mode_changed,
          this,
          &GameEngine::cursor_mode_changed);
  connect(m_cursor_manager.get(),
          &CursorManager::global_cursor_changed,
          this,
          &GameEngine::global_cursor_changed);

  connect(m_selection_controller.get(),
          &Game::Systems::SelectionController::selection_changed,
          this,
          &GameEngine::selected_units_changed);
  connect(m_selection_controller.get(),
          &Game::Systems::SelectionController::selection_changed,
          this,
          &GameEngine::sync_selection_flags);
  connect(m_selection_controller.get(),
          &Game::Systems::SelectionController::selection_model_refresh_requested,
          this,
          &GameEngine::selected_units_data_changed);
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::order_feedback,
          this,
          &GameEngine::handle_order_feedback);

  connect(m_command_controller.get(),
          &App::Controllers::CommandController::troop_limit_reached,
          [this]() {
            Game::Audio::play_cue(Game::Audio::Cue::k_alert_population_limit);
            set_error(tr("Maximum troop limit reached. Cannot produce more units."));
          });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::insufficient_manpower,
          [this]() {
            Game::Audio::play_cue(Game::Audio::Cue::k_alert_low_resources);
            set_error(tr("Not enough manpower. Build homes or wait for families."));
          });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::insufficient_resources,
          [this](const QString& message) {
            Game::Audio::play_cue(Game::Audio::Cue::k_alert_low_resources);
            set_error(message);
          });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::formation_placement_rejected,
          this,
          [this](const QString& reason) {
            Game::Audio::play_cue(Game::Audio::Cue::k_ui_error);
            if (!reason.isEmpty()) {
              set_error(reason);
            }
          });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::hold_mode_changed,
          this,
          []() { Game::Audio::play_cue(Game::Audio::Cue::k_order_hold); });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::gate_mode_changed,
          this,
          []() { Game::Audio::play_cue(Game::Audio::Cue::k_order_gate_mode); });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::guard_mode_changed,
          this,
          []() { Game::Audio::play_cue(Game::Audio::Cue::k_order_guard); });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::run_mode_changed,
          this,
          []() { Game::Audio::play_cue(Game::Audio::Cue::k_order_run); });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::formation_mode_changed,
          this,
          []() { Game::Audio::play_cue(Game::Audio::Cue::k_order_formation); });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::formation_placement_ended,
          this,
          []() { Game::Audio::play_cue(Game::Audio::Cue::k_order_formation_placed); });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::formation_placement_started,
          m_placement_view_model.get(),
          &App::ViewModels::PlacementViewModel::placing_formation_changed);
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::formation_placement_ended,
          m_placement_view_model.get(),
          &App::ViewModels::PlacementViewModel::placing_formation_changed);
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::formation_preview_changed,
          m_placement_view_model.get(),
          &App::ViewModels::PlacementViewModel::formation_options_changed);

  connect(
      this, SIGNAL(selected_units_changed()), m_selected_units_model, SLOT(refresh()));
  connect(this,
          SIGNAL(selected_units_data_changed()),
          m_selected_units_model,
          SLOT(refresh()));

  emit selected_units_changed();

  m_unit_died_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const Engine::Core::UnitDiedEvent& e) {
            on_unit_died(e);

            if (Game::Units::is_troop_spawn(e.spawn_type) &&
                e.owner_id != m_runtime.local_owner_id &&
                e.killer_owner_id == m_runtime.local_owner_id) {

              int const production_cost =
                  Game::Units::TroopConfig::instance().get_production_cost(
                      e.spawn_type);
              m_enemy_troops_defeated += production_cost;
              emit enemy_troops_defeated_changed();
            }
          });

  m_unit_spawned_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>(
          [this](const Engine::Core::UnitSpawnedEvent& e) { on_unit_spawned(e); });

  m_mission_announcement_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::MissionAnnouncementEvent>(
          [this](const Engine::Core::MissionAnnouncementEvent& e) {
            if (!e.text.isEmpty()) {
              emit mission_announcement(e.text);
            }
          });

  m_combat_hit_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent>(
          [this](const Engine::Core::CombatHitEvent& e) {
            if (m_world == nullptr) {
              return;
            }
            if (!m_commander_view_model->record_rpg_hit(e)) {
              m_activity_view_model->record_hit(e);
            }
          });

  publish_client_context();
}
