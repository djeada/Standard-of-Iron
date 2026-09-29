#include "app/audio/audio_coordinator.h"
#include "app/core/audio_services.h"
#include "app/core/game_engine.h"
#include "app/economy/economy_read_model.h"
#include "app/input/rts_camera_controller.h"
#include "app/mission/commander_message_runtime.h"
#include "app/mission/mission_runtime.h"
#include "app/mission/tutorial_runtime.h"
#include "app/session/environment_runtime.h"
#include "app/session/renderer_bootstrap.h"
#include "app/session/replay_coordinator.h"
#include "app/viewmodels/activity_view_model.h"
#include "app/viewmodels/camera_view_model.h"
#include "app/viewmodels/commander_message_view_model.h"
#include "app/viewmodels/commander_view_model.h"
#include "app/viewmodels/economy_view_model.h"
#include "app/viewmodels/match_setup_view_model.h"
#include "app/viewmodels/minimap_view_model.h"
#include "app/viewmodels/mission_view_model.h"
#include "app/viewmodels/orders_view_model.h"
#include "app/viewmodels/placement_view_model.h"
#include "app/viewmodels/production_view_model.h"
#include "app/viewmodels/save_slots_view_model.h"
#include "app/viewmodels/wave_view_model.h"
#include "app/world/ally_announcements.h"
#include "app/world/battle_stats.h"
#include "app/world/focus_tracker.h"
#include "app/world/targeting_presentation.h"
#include "game/command/command_queue.h"
#include "game/core/world.h"
#include "game/map/map_catalog.h"
#include "game/render_bridge/camera_service.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/session_context.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/persistence/save_load_service.h"
#include "render/ground/ambient_fog_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/map_boundary_fog_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/terrain_surface_manager.h"
#include "render/scene_renderer.h"
#include "render/terrain_scene_proxy.h"
#include "scene/camera.h"

void GameEngine::build_client_and_view_models() {
  create_session();
  create_view_models();
  create_feature_runtimes();
  wire_view_models();

  Game::Systems::initialize_default_content(m_session->nations());
  m_session->stats().initialize();
}

void GameEngine::create_session() {
  m_session = std::make_unique<Game::Session::SessionContext>();
  m_session_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
  m_world = &m_session->world();
  m_world->request_render_snapshots(true);
  m_session->commands().set_rejection_observer(
      [this](const Game::Command::Command& command, Game::Command::Rejection reason) {
        report_late_command_rejection(command, reason);
      });

  publish_client_context();
}

void GameEngine::create_view_models() {
  App::Core::ClientHost& host = *this;
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
  m_save_slots_view_model =
      std::make_unique<App::ViewModels::SaveSlotsViewModel>(m_save_load_service, this);
  m_wave_view_model = std::make_unique<App::ViewModels::WaveViewModel>(this);
  m_commander_message_view_model =
      std::make_unique<App::ViewModels::CommanderMessageViewModel>(this);
  m_economy_view_model = std::make_unique<App::ViewModels::EconomyViewModel>(this);
}

void GameEngine::create_feature_runtimes() {
  m_economy = std::make_unique<App::Core::EconomyReadModel>(m_economy_view_model.get());
  m_tutorial = std::make_unique<App::Mission::TutorialRuntime>(this);
  m_mission = std::make_unique<App::Mission::MissionRuntime>(
      m_mission_view_model.get(), m_wave_view_model.get(), [this](const QString& text) {
        emit mission_announcement(text);
      });
  m_commander_messages = std::make_unique<App::Mission::CommanderMessageRuntime>(
      m_commander_message_view_model.get());
  m_battle_stats = std::make_unique<App::World::BattleStats>();
  m_targeting = std::make_unique<App::World::TargetingPresentation>();
  m_focus = std::make_unique<App::World::FocusTracker>();
  m_replay = std::make_unique<App::Session::ReplayCoordinator>();
  m_environment = std::make_unique<App::Session::EnvironmentRuntime>();
  m_ally_announcements = std::make_unique<App::World::AllyAnnouncementPresenter>(
      App::World::AllyAnnouncementSink{
          .exchange = [this](const QString& text,
                             bool positive) { emit ally_exchange(text, positive); },
          .appeal_opened =
              [this](const QVariantMap& card) { emit ally_appeal_opened(card); },
          .appeal_closed =
              [this](quint32 appeal_id) { emit ally_appeal_closed(appeal_id); },
          .commander_fact =
              [this](const Game::Mission::CommanderMessageFact& fact) {
                m_commander_messages->director().notify_fact(fact);
              }});
}

void GameEngine::wire_view_models() {
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
          [this] { m_tutorial->notes().camera_used = true; });
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
            m_focus->sync_focus_targets(focus_inputs());
          });
  connect(m_commander_message_view_model.get(),
          &App::ViewModels::CommanderMessageViewModel::dismiss_requested,
          this,
          [this]() { m_commander_messages->dismiss_active(commander_binding()); });
  connect(m_tutorial->director(),
          &Game::Mission::TutorialDirector::start_requested,
          m_match_setup_view_model.get(),
          &App::ViewModels::MatchSetupViewModel::start_tutorial);
}

void GameEngine::build_services_and_controllers() {
  create_render_services();
  wire_victory_service();
  wire_loading_and_saves();
  wire_world_services();
  wire_map_catalog();
  wire_audio();
  wire_production_and_placement();
  wire_command_controller();
  wire_selection_and_cursor();
  wire_event_subscriptions();

  publish_client_context();
}

void GameEngine::create_render_services() {
  auto rendering = RendererBootstrap::initialize_rendering();
  m_renderer = std::move(rendering.renderer);
  m_rts_camera = std::move(rendering.camera);
  m_commander_camera = std::make_unique<Render::GL::Camera>(*m_rts_camera);
  m_commander_camera->set_rts_constraints(false);
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
}
