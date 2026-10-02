#include "app/audio/audio_coordinator.h"
#include "app/core/audio_services.h"
#include "app/core/game_engine.h"
#include "app/core/order_feedback_presenter.h"
#include "app/economy/production_manager.h"
#include "app/input/cursor_manager.h"
#include "app/input/hover_tracker.h"
#include "app/input/input_command_handler.h"
#include "app/input/rts_camera_controller.h"
#include "app/mission/commander_message_runtime.h"
#include "app/mission/mission_runtime.h"
#include "app/models/selected_units_model.h"
#include "app/orders/command_controller.h"
#include "app/orders/order_feedback.h"
#include "app/orders/order_submission.h"
#include "app/persistence/save_load_coordinator.h"
#include "app/persistence/save_slot_controller.h"
#include "app/session/loading_progress_tracker.h"
#include "app/session/skirmish_runtime_coordinator.h"
#include "app/viewmodels/activity_view_model.h"
#include "app/viewmodels/commander_view_model.h"
#include "app/viewmodels/match_setup_view_model.h"
#include "app/viewmodels/minimap_view_model.h"
#include "app/viewmodels/placement_view_model.h"
#include "app/viewmodels/save_slots_view_model.h"
#include "app/world/ally_announcements.h"
#include "app/world/battle_stats.h"
#include "app/world/minimap_events.h"
#include "app/world/minimap_manager.h"
#include "app/world/selection_query_service.h"
#include "app/world/visibility_coordinator.h"
#include "game/audio/audio_cues.h"
#include "game/core/component_core.h"
#include "game/core/local_audience.h"
#include "game/core/world.h"
#include "game/map/map_catalog.h"
#include "game/map/render_visibility_rules.h"
#include "game/mission/campaign_manager.h"
#include "game/render_bridge/camera_service.h"
#include "game/render_bridge/picking_service.h"
#include "game/render_bridge/selection_controller.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/session/session_snapshot.h"
#include "game/systems/owner_registry.h"
#include "game/systems/persistence/save_load_service.h"
#include "game/systems/victory_service.h"
#include "render/ground/ambient_fog_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/map_boundary_fog_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/terrain_surface_manager.h"
#include "render/terrain_scene_proxy.h"

void GameEngine::wire_victory_service() {
  auto& session = *m_session;
  m_victory_service = std::make_unique<Game::Systems::VictoryService>(
      Game::Systems::VictoryService::Services{.stats = session.stats(),
                                              .owners = session.owners(),
                                              .nations = session.nations(),
                                              .economy = session.economy()});

  Game::Session::SessionSnapshot::register_contributor(
      {.key = "victory",
       .capture = [service = m_victory_service.get()](
                      const Game::Session::SnapshotScope&) -> QJsonValue {
         return service != nullptr ? QJsonValue(service->serialize_state())
                                   : QJsonValue();
       },
       .restore =
           [service = m_victory_service.get()](const Game::Session::SnapshotScope&,
                                               const QJsonValue& value) {
             if (service != nullptr) {
               service->restore_state(value.toObject());
             }
           }});

  m_victory_service->set_objectives_changed_callback(
      [this]() { m_mission->publish_stages(mission_binding()); });

  m_victory_service->set_victory_callback(
      [this](const QString& state) { on_match_outcome(state); });
}

void GameEngine::on_match_outcome(const QString& state) {
  if (m_runtime.victory_state == state) {
    return;
  }
  m_audio->coordinator().ensure_result_audio_ready(state, m_runtime.local_owner_id);
  if (state == "defeat") {
    Game::Audio::play_cue(Game::Audio::Cue::k_alert_objective_failed);
  }

  m_commander_messages->notify_outcome(state);
  m_runtime.victory_state = state;
  m_runtime.defeat_reason =
      state == "defeat" ? m_victory_service->get_defeat_description() : QString();
  emit victory_state_changed();

  if (!state.isEmpty() && m_commander_view_model->active()) {
    QMetaObject::invokeMethod(
        this,
        [this]() {
          if (!m_runtime.victory_state.isEmpty() && m_commander_view_model->active()) {
            m_commander_view_model->exit_mode();
          }
        },
        Qt::QueuedConnection);
  }

  if (state == "victory" &&
      m_campaign_manager->current_mission_context().has_mission()) {
    m_match_setup_view_model->mark_current_mission_completed();
  }
}

void GameEngine::wire_loading_and_saves() {
  m_saves = std::make_unique<App::Core::SaveSlotController>(
      m_save_load_service,
      m_save_slots_view_model.get(),
      m_world,
      App::Core::SaveSlotHooks{
          .simulation_running = [this]() { return m_lifecycle.running(); },
          .capture =
              [this](const QString& slot,
                     Game::Systems::Save::SlotKind kind,
                     int autosave_retention) {
                return capture_save_to_slot(slot, kind, autosave_retention);
              },
          .lock_frame = [this]() { return lock_frame(); },
          .report_error = [this](const QString& message) { set_error(message); },
          .leave_commander_mode =
              [this]() {
                if (m_commander_view_model->active()) {
                  m_commander_view_model->exit_mode();
                }
              },
          .autosave_allowed =
              [this]() {
                return m_runtime.initialized && !m_runtime.loading &&
                       !m_level.map_path.isEmpty() && m_runtime.victory_state.isEmpty();
              }},
      this);
  m_saves->connect_view_model();
  connect(m_save_slots_view_model.get(),
          &App::ViewModels::SaveSlotsViewModel::load_requested,
          this,
          &GameEngine::load_game_from_slot);
  m_saves->connect_service_signals();

  m_save_load_coordinator = std::make_unique<App::Core::SaveLoadCoordinator>();
  m_skirmish_runtime = std::make_unique<App::Core::SkirmishRuntimeCoordinator>();

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
}

void GameEngine::wire_world_services() {
  Game::Systems::PickingService::bind_surface(&m_session->terrain(), m_world);
  m_camera_service = std::make_unique<Game::Systems::CameraService>(
      m_session->visibility(), m_session->terrain());

  auto* selection_system = &Game::Session::session_for(*m_world).selection();
  m_selection_controller = std::make_unique<Game::Systems::SelectionController>(
      m_world, selection_system, m_picking_service.get());
  m_selection_controller->set_inspect_filter(
      [this](Engine::Core::EntityID id) { return can_inspect_entity(id); });
  m_command_controller = std::make_unique<App::Controllers::CommandController>(
      m_world, selection_system, m_picking_service.get());

  m_cursor_manager = std::make_unique<CursorManager>();
  m_hover_tracker = std::make_unique<HoverTracker>(m_picking_service.get());

  m_minimap_manager = std::make_unique<MinimapManager>();
  m_visibility_coordinator =
      std::make_unique<VisibilityCoordinator>(m_session->visibility());
  m_visibility_coordinator->set_presenters(m_fog.get(), m_minimap_manager.get());

  m_input_handler = std::make_unique<InputCommandHandler>(m_world,
                                                          m_selection_controller.get(),
                                                          m_command_controller.get(),
                                                          m_cursor_manager.get(),
                                                          m_hover_tracker.get(),
                                                          m_picking_service.get(),
                                                          m_rts_camera.get());

  m_camera_controller = std::make_unique<RtsCameraController>(
      m_rts_camera.get(), m_camera_service.get(), m_world);
  m_order_feedback = std::make_unique<App::Core::OrderFeedbackPresenter>(
      App::Core::OrderFeedbackSources{.world = m_world,
                                      .minimap = m_minimap_view_model.get(),
                                      .feedback = &m_player_feedback,
                                      .tutorial = m_tutorial.get(),
                                      .selection = m_selection_controller.get(),
                                      .local_owner_id = &m_runtime.local_owner_id});

  m_campaign_manager = std::make_unique<CampaignManager>(this);
  connect(m_campaign_manager.get(),
          &CampaignManager::available_campaigns_changed,
          m_match_setup_view_model.get(),
          &App::ViewModels::MatchSetupViewModel::notify_campaigns_changed);
}

auto GameEngine::can_inspect_entity(Engine::Core::EntityID id) const -> bool {
  if (m_world == nullptr || m_visibility_coordinator == nullptr) {
    return true;
  }
  const auto* transform = m_world->try_get<Engine::Core::TransformComponent>(id);
  if (transform == nullptr) {
    return false;
  }
  if (const auto* cover = m_world->try_get<Engine::Core::ForestCoverComponent>(id);
      cover != nullptr &&
      cover->hidden_from(
          Game::Session::session_for(*m_world).owners().get_local_player_id())) {
    return false;
  }
  const auto snapshot = m_visibility_coordinator->current_snapshot();
  if (snapshot == nullptr || !snapshot->initialized) {
    return true;
  }

  if (m_world->has<Engine::Core::BuildingComponent>(id)) {
    return Game::Map::classify_world_visibility(
               *snapshot, transform->position.x, transform->position.z) !=
           Game::Map::RenderVisibilityState::Hidden;
  }
  return Game::Map::should_render_non_local_unit(
      *snapshot, transform->position.x, transform->position.z);
}

void GameEngine::wire_map_catalog() {
  m_map_catalog = std::make_unique<Game::Map::MapCatalog>(this);
  connect(m_map_catalog.get(),
          &Game::Map::MapCatalog::map_loaded,
          this,
          [this](const QVariantMap& map_data) {
            m_match_setup_view_model->append_map(map_data);
          });
  connect(
      m_map_catalog.get(),
      &Game::Map::MapCatalog::loading_changed,
      this,
      [this](bool loading) { m_match_setup_view_model->set_maps_loading(loading); });
}

void GameEngine::wire_audio() {
  m_audio = std::make_unique<App::Core::AudioServices>();
  m_audio->initialize_audio_system(this);
  m_audio->create_event_handling(m_world, m_session->nations());
  m_audio->initialize_event_handler(m_runtime.local_owner_id);
}

void GameEngine::wire_production_and_placement() {
  m_production_manager = std::make_unique<ProductionManager>(
      m_world, m_picking_service.get(), m_rts_camera.get(), this);
  using PlacementVm = App::ViewModels::PlacementViewModel;
  connect(m_production_manager.get(),
          &ProductionManager::placing_construction_changed,
          m_placement_view_model.get(),
          &PlacementVm::placing_construction_changed);
  connect(m_production_manager.get(),
          &ProductionManager::construction_preview_active_changed,
          m_placement_view_model.get(),
          &PlacementVm::construction_preview_active_changed);
  connect(m_production_manager.get(),
          &ProductionManager::construction_preview_valid_changed,
          m_placement_view_model.get(),
          &PlacementVm::construction_preview_valid_changed);
  connect(m_production_manager.get(),
          &ProductionManager::construction_preview_reason_changed,
          m_placement_view_model.get(),
          &PlacementVm::construction_preview_reason_changed);
  connect(m_production_manager.get(),
          &ProductionManager::construction_preview_summary_changed,
          m_placement_view_model.get(),
          &PlacementVm::construction_preview_summary_changed);
  connect(m_production_manager.get(),
          &ProductionManager::construction_placement_rejected,
          this,
          [this](const QString& reason) {
            m_order_feedback->warn(Game::Audio::Cue::k_build_placement_rejected);
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
            if (const auto clicked = m_production_manager->release_position()) {
              outcome.has_destination = true;
              outcome.destination = *clicked;
            }
            handle_order_feedback(outcome);
          });
  connect(m_production_manager.get(),
          &ProductionManager::order_feedback,
          this,
          &GameEngine::handle_order_feedback);

  m_selection_query_service = std::make_unique<SelectionQueryService>(m_world, this);
}

void GameEngine::wire_command_controller() {
  using Commands = App::Controllers::CommandController;
  using PlacementVm = App::ViewModels::PlacementViewModel;
  connect(m_command_controller.get(),
          &Commands::order_feedback,
          this,
          &GameEngine::handle_order_feedback);
  connect(m_command_controller.get(),
          &Commands::formation_placement_rejected,
          this,
          [this](const QString& reason) {
            m_order_feedback->warn(Game::Audio::Cue::k_ui_error);
            if (!reason.isEmpty()) {
              set_error(reason);
            }
          });
  connect(m_command_controller.get(), &Commands::gate_mode_changed, this, []() {
    Game::Audio::play_cue(Game::Audio::Cue::k_order_gate_mode);
  });
  connect(
      m_command_controller.get(), &Commands::run_mode_changed, this, [](bool active) {
        if (!active) {
          Game::Audio::play_cue(Game::Audio::Cue::k_order_run);
        }
      });
  connect(m_command_controller.get(),
          &Commands::formation_placement_started,
          this,
          []() { Game::Audio::play_cue(Game::Audio::Cue::k_order_formation); });
  connect(m_command_controller.get(),
          &Commands::formation_placement_started,
          m_placement_view_model.get(),
          &PlacementVm::placing_formation_changed);
  connect(m_command_controller.get(),
          &Commands::formation_placement_ended,
          m_placement_view_model.get(),
          &PlacementVm::placing_formation_changed);
  connect(m_command_controller.get(),
          &Commands::formation_preview_changed,
          m_placement_view_model.get(),
          &PlacementVm::formation_options_changed);
  connect(m_command_controller.get(),
          &Commands::formation_deployed,
          m_placement_view_model.get(),
          &PlacementVm::formation_deployed);
}

void GameEngine::wire_selection_and_cursor() {
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

  connect(
      this, SIGNAL(selected_units_changed()), m_selected_units_model, SLOT(refresh()));
  connect(this,
          SIGNAL(selected_units_data_changed()),
          m_selected_units_model,
          SLOT(refresh()));

  emit selected_units_changed();
}

void GameEngine::wire_event_subscriptions() {
  m_unit_died_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const Engine::Core::UnitDiedEvent& e) {
            on_unit_died(e);
            if (m_battle_stats->note_unit_died(e, m_world, m_runtime.local_owner_id)) {
              emit enemy_troops_defeated_changed();
            }
          });

  m_unit_spawned_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>(
          [this](const Engine::Core::UnitSpawnedEvent& e) { on_unit_spawned(e); });

  m_mission_announcement_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::MissionAnnouncementEvent>(
          [this](const Engine::Core::MissionAnnouncementEvent& e) {
            if (e.text.isEmpty() ||
                !Engine::Core::LocalAudience{m_runtime.local_owner_id}.includes(
                    e.owner_id)) {
              return;
            }
            m_mission->queue_announcement(e.text);
          });

  m_combat_hit_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent>(
          [this](const Engine::Core::CombatHitEvent& e) { route_combat_hit(e); });

  m_world_feedback_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::WorldFeedbackEvent>(
          [this](const Engine::Core::WorldFeedbackEvent& e) {
            if (m_world == nullptr) {
              return;
            }
            m_activity_view_model->record_world_feedback(e);
          });

  m_minimap_events = std::make_unique<App::World::MinimapEvents>(
      App::World::MinimapEventSources{.manager = m_minimap_manager.get(),
                                      .view_model = m_minimap_view_model.get(),
                                      .world = m_world,
                                      .session = m_session.get(),
                                      .local_owner_id = &m_runtime.local_owner_id});
}

void GameEngine::route_combat_hit(const Engine::Core::CombatHitEvent& event) {
  if (m_world == nullptr) {
    return;
  }
  using HitRouting = App::ViewModels::CommanderViewModel::HitRouting;
  switch (m_commander_view_model->classify_hit(event)) {
  case HitRouting::Rts:
    m_activity_view_model->record_hit(event, App::Core::FeedbackStyle::Tick);
    break;
  case HitRouting::CommanderBurst:
    m_activity_view_model->record_hit(event, App::Core::FeedbackStyle::Burst);
    break;
  case HitRouting::Suppressed:
    break;
  }
  m_minimap_events->note_combat_hit(event);
}
