#include "game_engine.h"

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

#include "../controllers/action_vfx.h"
#include "../controllers/command_controller.h"
#include "../models/audio_system_proxy.h"
#include "../models/cursor_manager.h"
#include "../models/hover_tracker.h"
#include "../models/selected_units_model.h"
#include "../viewmodels/save_slots_view_model.h"
#include "ambient_state_manager.h"
#include "app/core/environment.h"
#include "app/core/skirmish_loader.h"
#include "app/core/world_bootstrap.h"
#include "app/models/cursor_mode.h"
#include "app/utils/engine_view_helpers.h"
#include "app/utils/movement_utils.h"
#include "audio_coordinator.h"
#include "audio_event_handler.h"
#include "audio_resource_loader.h"
#include "camera_controller.h"
#include "campaign_manager.h"
#include "commander_mode_coordinator.h"
#include "commander_status_builder.h"
#include "core/system.h"
#include "frame_ui_coordinator.h"
#include "game/audio/audio_cues.h"
#include "game/audio/audio_system.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/game_config.h"
#include "game/map/campaign_loader.h"
#include "game/map/map_catalog.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/minimap/map_preview_generator.h"
#include "game/map/minimap/minimap_generator.h"
#include "game/map/minimap/minimap_utils.h"
#include "game/map/minimap/unit_layer.h"
#include "game/map/mission_context.h"
#include "game/map/mission_loader.h"
#include "game/map/render_visibility_rules.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/attack_range.h"
#include "game/systems/attack_targeting.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/camera_service.h"
#include "game/systems/capture_system.h"
#include "game/systems/cleanup_system.h"
#include "game/systems/combat_rules.h"
#include "game/systems/combat_system.h"
#include "game/systems/default_content.h"
#include "game/systems/game_state_serializer.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/guard_system.h"
#include "game/systems/healing_system.h"
#include "game/systems/marketplace_system.h"
#include "game/systems/movement_system.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/patrol_system.h"
#include "game/systems/picking_service.h"
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
#include "game/view/selection_controller.h"
#include "game/visuals/team_colors.h"
#include "game_state_restorer.h"
#include "input_command_handler.h"
#include "level_loader.h"
#include "loading_progress_tracker.h"
#include "minimap_manager.h"
#include "mission_commander_setup.h"
#include "mission_definition_view.h"
#include "mission_setup_coordinator.h"
#include "order_service.h"
#include "production_manager.h"
#include "render/camera_visibility.h"
#include "render/geom/stone.h"
#include "render/gl/bootstrap.h"
#include "render/ground/ambient_fog_renderer.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/olive_renderer.h"
#include "render/ground/pine_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/terrain_surface_manager.h"
#include "render/scene_renderer.h"
#include "render/terrain_scene_proxy.h"
#include "renderer_bootstrap.h"
#include "rts_action_model.h"
#include "save_load_coordinator.h"
#include "scene/camera.h"
#include "selection_query_service.h"
#include "skirmish_runtime_coordinator.h"
#include "user_settings.h"
#include "utils/resource_utils.h"
#include "visibility_coordinator.h"
#include "weather_audio.h"

namespace {

auto marketplace_trade_resource_from_key(QStringView key)
    -> std::optional<Game::Systems::ResourceType> {
  if (key == QLatin1String("wood")) {
    return Game::Systems::ResourceType::Wood;
  }
  if (key == QLatin1String("stone")) {
    return Game::Systems::ResourceType::Stone;
  }
  if (key == QLatin1String("iron")) {
    return Game::Systems::ResourceType::Iron;
  }
  return std::nullopt;
}

auto marketplace_trade_resource_label(QStringView key) -> QString {
  if (key == QLatin1String("wood")) {
    return QCoreApplication::translate("GameEngine", "wood");
  }
  if (key == QLatin1String("stone")) {
    return QCoreApplication::translate("GameEngine", "stone");
  }
  if (key == QLatin1String("iron")) {
    return QCoreApplication::translate("GameEngine", "iron");
  }
  return key.toString();
}

auto build_available_commander_entry(const Game::Units::CommanderDefinition& definition,
                                     bool is_default) -> QVariantMap {
  QVariantMap entry;
  entry["id"] = QString::fromStdString(definition.id);
  entry["troop"] =
      QString::fromStdString(Game::Units::troop_typeToString(definition.troop_type));
  entry["display_name"] =
      Game::Util::tr_asset(Game::Util::k_commanders_context, definition.display_name);
  entry["battlefield_role"] = Game::Util::tr_asset(Game::Util::k_commanders_context,
                                                   definition.battlefield_role);
  entry["bonus_summary"] =
      Game::Util::tr_asset(Game::Util::k_commanders_context, definition.bonus_summary);
  entry["passive_aura"] =
      Game::Util::tr_asset(Game::Util::k_commanders_context, definition.passive_aura);
  entry["rally_ability"] =
      Game::Util::tr_asset(Game::Util::k_commanders_context, definition.rally_ability);
  entry["is_default"] = is_default;
  return entry;
}

auto build_resource_map(int owner_id) -> QVariantMap {
  QVariantMap resources;
  Game::Systems::ResourceAmounts const amounts =
      Game::Systems::PlayerResourceRegistry::instance().get_all(owner_id);
  for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
    resources[QLatin1String(Game::Systems::resource_type_key(type))] =
        amounts.get(type);
  }
  return resources;
}

auto build_player_state_map(int owner_id, int population_cap) -> QVariantMap {
  QVariantMap state;
  state["owner_id"] = owner_id;
  state["population"] =
      Game::Systems::TroopCountRegistry::instance().get_troop_count(owner_id);
  state["population_cap"] = population_cap;
  state["resources"] = build_resource_map(owner_id);
  return state;
}

} // namespace

GameEngine::GameEngine(QObject* parent)
    : QObject(parent)
    , m_save_load_service(Game::Systems::SaveLoadService::instance())
    , m_commander_input(this, this)
    , m_selected_units_model(new SelectedUnitsModel(this, this)) {

  m_session = std::make_unique<Game::Session::SessionContext>();
  m_session_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
  m_world = &m_session->world();

  m_save_slots_view_model =
      std::make_unique<App::ViewModels::SaveSlotsViewModel>(m_save_load_service, this);
  connect(m_save_slots_view_model.get(),
          &App::ViewModels::SaveSlotsViewModel::error_occurred,
          this,
          [this](const QString& message) { set_error(message); });
  connect(m_save_slots_view_model.get(),
          &App::ViewModels::SaveSlotsViewModel::save_slots_changed,
          this,
          &GameEngine::save_slots_changed);
  connect(m_save_slots_view_model.get(),
          &App::ViewModels::SaveSlotsViewModel::autosave_interval_changed,
          this,
          [this] { restart_autosave_timer(); });

  App::ViewModels::PlacementHost& placement_host = *this;
  m_placement_view_model =
      std::make_unique<App::ViewModels::PlacementViewModel>(placement_host, this);
  m_wave_view_model = std::make_unique<App::ViewModels::WaveViewModel>(this);
  App::ViewModels::ActivityHost& activity_host = *this;
  m_activity_view_model =
      std::make_unique<App::ViewModels::ActivityViewModel>(&activity_host, this);

  Game::Systems::initialize_default_content(Game::Systems::NationRegistry::instance());
  Game::Systems::TroopCountRegistry::instance().initialize();
  Game::Systems::GlobalStatsRegistry::instance().initialize();

  auto rendering = RendererBootstrap::initialize_rendering();
  m_renderer = std::move(rendering.renderer);
  m_rts_camera = std::move(rendering.camera);
  m_commander_camera = std::make_unique<Render::GL::Camera>(*m_rts_camera);
  set_active_camera(m_rts_camera.get());
  enter_rts_runtime_mode();
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
  m_victory_service = std::make_unique<Game::Systems::VictoryService>();

  connect_save_service_signals();
  m_camera_service = std::make_unique<Game::Systems::CameraService>();
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
            m_available_maps.append(map_data);
            emit available_maps_changed();
          });
  connect(m_map_catalog.get(),
          &Game::Map::MapCatalog::loading_changed,
          this,
          [this](bool loading) {
            m_maps_loading = loading;
            emit maps_loading_changed();
          });
  connect(m_map_catalog.get(), &Game::Map::MapCatalog::all_maps_loaded, this, [this]() {
    emit available_maps_changed();
  });

  if (AudioSystem::get_instance().initialize()) {
    qInfo() << "AudioSystem initialized successfully";
    AudioResourceLoader::load_audio_resources();
    AudioResourceLoader::load_audio_cues();
  } else {
    qWarning() << "Failed to initialize AudioSystem";
  }

  m_audio_systemProxy = std::make_unique<App::Models::AudioSystemProxy>(this);

  m_minimap_manager = std::make_unique<MinimapManager>();
  m_visibility_coordinator = std::make_unique<VisibilityCoordinator>();
  m_visibility_coordinator->set_presenters(m_fog.get(), m_minimap_manager.get());
  m_ambient_state_manager = std::make_unique<AmbientStateManager>();

  m_input_handler = std::make_unique<InputCommandHandler>(m_world,
                                                          m_selection_controller.get(),
                                                          m_command_controller.get(),
                                                          m_cursor_manager.get(),
                                                          m_hover_tracker.get(),
                                                          m_picking_service.get(),
                                                          m_rts_camera.get());

  m_camera_controller = std::make_unique<CameraController>(
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
                reason);
            handle_order_feedback(outcome);
          });
  connect(m_production_manager.get(),
          &ProductionManager::order_feedback,
          this,
          &GameEngine::handle_order_feedback);

  m_campaign_manager = std::make_unique<CampaignManager>(this);
  connect(m_campaign_manager.get(),
          &CampaignManager::available_campaigns_changed,
          this,
          &GameEngine::available_campaigns_changed);

  m_selection_query_service = std::make_unique<SelectionQueryService>(m_world, this);

  m_audio_event_handler = std::make_unique<Game::Audio::AudioEventHandler>(m_world);
  m_audio_coordinator = std::make_unique<AudioCoordinator>(m_audio_event_handler.get());
  m_commander_mode = std::make_unique<App::Core::CommanderModeCoordinator>();
  m_mission_setup = std::make_unique<App::Core::MissionSetupCoordinator>();
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

  connect(m_cursor_manager.get(), &CursorManager::mode_changed, this, [this]() {
    if (m_cursor_manager && (m_window != nullptr)) {
      m_cursor_manager->update_cursor_shape(m_window);
    }
    emit cursor_mode_changed();
  });
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
          [this](bool active) {
            Game::Audio::play_cue(Game::Audio::Cue::k_order_hold);
            emit hold_mode_changed(active);
          });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::gate_mode_changed,
          this,
          [this](const QString& mode) {
            Game::Audio::play_cue(Game::Audio::Cue::k_order_gate_mode);
            emit gate_mode_changed(mode);
          });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::guard_mode_changed,
          this,
          [this](bool active) {
            Game::Audio::play_cue(Game::Audio::Cue::k_order_guard);
            emit guard_mode_changed(active);
          });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::run_mode_changed,
          this,
          []() { Game::Audio::play_cue(Game::Audio::Cue::k_order_run); });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::formation_mode_changed,
          this,
          [this](bool active) {
            Game::Audio::play_cue(Game::Audio::Cue::k_order_formation);
            emit formation_mode_changed(active);
          });
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
            if (m_controlled_commander_id == 0) {
              record_combat_hit(e);
              return;
            }
            if (e.attacker_id != m_controlled_commander_id) {
              return;
            }
            auto* target_ent = m_world->get_entity(e.target_id);
            if (target_ent == nullptr) {
              return;
            }
            auto* tf = target_ent->get_component<Engine::Core::TransformComponent>();
            if (tf == nullptr) {
              return;
            }

            float max_health = 0.0F;
            if (const auto* rpg_health =
                    target_ent->get_component<Engine::Core::RpgHealthComponent>();
                rpg_health != nullptr && rpg_health->rpg_max_hp > 0) {
              max_health = static_cast<float>(rpg_health->rpg_max_hp);
            } else if (const auto* unit =
                           target_ent->get_component<Engine::Core::UnitComponent>();
                       unit != nullptr && unit->max_health > 0) {
              max_health = static_cast<float>(unit->max_health);
            }

            float const damage_ratio =
                max_health > 0.0F
                    ? std::clamp(static_cast<float>(e.damage) / max_health, 0.0F, 1.5F)
                    : 0.0F;
            int const lane = static_cast<int>(m_rpg_damage_event_sequence % 5U) - 2;
            ++m_rpg_damage_event_sequence;

            if (static_cast<int>(m_rpg_damage_events.size()) >=
                k_max_rpg_damage_events) {
              m_rpg_damage_events.erase(m_rpg_damage_events.begin());
            }

            m_rpg_damage_events.push_back({tf->position.x,
                                           tf->position.y + 1.8F,
                                           tf->position.z,
                                           e.damage,
                                           damage_ratio,
                                           lane,
                                           e.is_killing_blow});
          });
}

GameEngine::~GameEngine() {

  m_autosave_timer.stop();
  if (m_save_load_service != nullptr) {

    m_save_load_service->wait_for_pending_saves();
    m_save_load_service->disconnect(this);
    m_save_load_service->shutdown();
  }

  if (m_audio_event_handler) {
    m_audio_event_handler->shutdown();
  }
  AudioSystem::get_instance().shutdown();
  qInfo() << "AudioSystem shut down";
}

void GameEngine::cleanup_opengl_resources() {
  qInfo() << "Cleaning up OpenGL resources...";

  QOpenGLContext* context = QOpenGLContext::currentContext();
  const bool has_valid_context = (context != nullptr);

  if (!has_valid_context) {
    qInfo() << "No valid OpenGL context, skipping OpenGL cleanup";
  }

  if (m_renderer && has_valid_context) {
    m_renderer->shutdown();
    qInfo() << "Renderer shut down";
  }

  m_terrain_scene.reset();

  m_surface.reset();
  m_features.reset();
  m_scatter.reset();
  m_fog.reset();
  m_boundary_fog.reset();
  m_ambient_fog.reset();
  m_rain.reset();
  if (m_weather_audio) {
    m_weather_audio->stop();
  }
  m_weather_audio.reset();
  m_rain_manager.reset();

  m_renderer.reset();
  m_resources.reset();

  qInfo() << "OpenGL resources cleaned up";
}

void GameEngine::on_map_clicked(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->on_map_clicked(sx, sy, m_runtime.local_owner_id, m_viewport);
  }
}

void GameEngine::on_right_click(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->on_right_click(sx, sy, m_runtime.local_owner_id, m_viewport);
  }
}

void GameEngine::on_right_double_click(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();

  if (m_right_mouse_gesture.placement_was_active_on_press) {
    m_right_mouse_gesture.double_click_handled = true;
    return;
  }

  bool const started_formation_placement =
      m_right_mouse_gesture.started_formation_placement && m_input_handler &&
      m_input_handler->is_placing_formation();
  if (started_formation_placement) {
    m_input_handler->on_formation_cancel();
  } else if (m_right_mouse_gesture.suppress_release_click ||
             m_placement_view_model->is_placing_construction()) {
    m_right_mouse_gesture.double_click_handled = true;
    return;
  }

  if (m_input_handler) {
    m_input_handler->on_right_double_click(
        sx, sy, m_runtime.local_owner_id, m_viewport);
  }
  m_right_mouse_gesture.double_click_handled = true;
}

auto GameEngine::on_right_press(qreal sx, qreal sy) -> bool {
  if (m_window == nullptr) {
    return false;
  }
  ensure_initialized();
  m_right_mouse_gesture.reset();
  m_right_mouse_gesture.active = true;
  m_right_mouse_gesture.press_position = QPointF(sx, sy);
  m_right_mouse_gesture.placement_was_active_on_press =
      m_placement_view_model->is_placing_formation() ||
      m_placement_view_model->is_placing_construction();

  if (m_placement_view_model->is_placing_formation()) {
    m_placement_view_model->on_formation_cancel();
    m_right_mouse_gesture.suppress_release_click = true;
    return true;
  }
  if (m_placement_view_model->is_placing_construction()) {
    m_placement_view_model->on_construction_cancel();
    m_right_mouse_gesture.suppress_release_click = true;
    return true;
  }
  if (m_input_handler) {
    m_right_mouse_gesture.suppress_release_click =
        m_input_handler->on_right_press(sx, sy, m_runtime.local_owner_id, m_viewport);
    m_right_mouse_gesture.started_formation_placement =
        !m_right_mouse_gesture.placement_was_active_on_press &&
        m_right_mouse_gesture.suppress_release_click &&
        m_input_handler->is_placing_formation();
    return m_right_mouse_gesture.suppress_release_click;
  }
  return false;
}

void GameEngine::on_right_move(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (!m_right_mouse_gesture.active) {
    return;
  }

  QPointF const delta = QPointF(sx, sy) - m_right_mouse_gesture.press_position;
  if ((delta.x() * delta.x() + delta.y() * delta.y()) > 36.0) {
    m_right_mouse_gesture.dragged = true;
  }

  if (m_right_mouse_gesture.dragged && m_input_handler &&
      m_input_handler->is_placing_formation()) {
    m_input_handler->on_right_drag_orient(sx, sy, m_viewport);
  }
}

void GameEngine::on_right_release(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    m_right_mouse_gesture.reset();
    return;
  }
  ensure_initialized();

  if (m_right_mouse_gesture.double_click_handled) {
    m_right_mouse_gesture.reset();
    return;
  }

  if (m_input_handler && m_input_handler->is_placing_formation()) {
    m_input_handler->on_formation_confirm();
    m_right_mouse_gesture.reset();
    return;
  }

  if (!m_right_mouse_gesture.suppress_release_click && m_input_handler) {
    m_input_handler->on_right_click(sx, sy, m_runtime.local_owner_id, m_viewport);
  }

  m_right_mouse_gesture.reset();
}

void GameEngine::on_right_drag_orient(qreal sx, qreal sy) {
  on_right_move(sx, sy);
}

void GameEngine::on_attack_click(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->on_attack_click(sx, sy, m_viewport);
  }
}

void GameEngine::on_stop_command() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_stop_command();
}

void GameEngine::on_hold_command() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_hold_command();
}

void GameEngine::on_gate_command() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_gate_command();
}

void GameEngine::toggle_auto_gather(const QString& priority_product_type) {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_auto_gather_command(priority_product_type);
}

void GameEngine::on_guard_command() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_guard_command();
}

void GameEngine::on_run_command() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_run_command();
}

void GameEngine::on_heal_command() {
  if (!m_cursor_manager) {
    return;
  }
  ensure_initialized();
  if (!is_action_enabled(QStringLiteral("heal"))) {
    return;
  }
  m_cursor_manager->set_mode(CursorMode::Heal);
}

void GameEngine::on_build_command() {
  if (!m_cursor_manager) {
    return;
  }
  ensure_initialized();
  if (!is_action_enabled(QStringLiteral("build"))) {
    return;
  }
  m_cursor_manager->set_mode(CursorMode::Build);
}

void GameEngine::on_civilian_delivery_click(qreal sx, qreal sy) {
  if (!m_input_handler || (m_camera == nullptr)) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_civilian_delivery_click(
      sx, sy, m_runtime.local_owner_id, m_viewport);
}

void GameEngine::confirm_repair_at(qreal sx, qreal sy) {
  if (!m_input_handler || (m_camera == nullptr)) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_builder_repair_click(
      sx, sy, m_runtime.local_owner_id, m_viewport);
}

void GameEngine::toggle_repair_order() {
  ensure_initialized();
  if (m_cursor_manager == nullptr) {
    return;
  }
  m_cursor_manager->set_mode(m_cursor_manager->mode() == CursorMode::Repair
                                 ? CursorMode::Normal
                                 : CursorMode::Repair);
}

void GameEngine::on_guard_click(qreal sx, qreal sy) {
  if (!m_input_handler || (m_camera == nullptr)) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_guard_click(sx, sy, m_viewport);
}

auto GameEngine::any_selected_in_hold_mode() const -> bool {
  if (!m_input_handler) {
    return false;
  }
  return m_input_handler->any_selected_in_hold_mode();
}

auto GameEngine::any_selected_in_guard_mode() const -> bool {
  if (!m_input_handler) {
    return false;
  }
  return m_input_handler->any_selected_in_guard_mode();
}

auto GameEngine::any_selected_in_run_mode() const -> bool {
  if (!m_input_handler) {
    return false;
  }
  return m_input_handler->any_selected_in_run_mode();
}

bool GameEngine::is_campaign_mission() const {
  if (!m_campaign_manager) {
    return false;
  }
  return m_campaign_manager->current_mission_context().is_campaign();
}

bool GameEngine::release_self_test_mission_ready() const {
  return m_runtime.initialized && !is_loading() && is_campaign_mission() &&
         m_world != nullptr && m_world->entity_count() > 0U &&
         m_runtime.last_error.isEmpty();
}

QString GameEngine::release_self_test_pending_reason() const {
  QStringList pending;
  if (!m_runtime.initialized) {
    pending << QStringLiteral("engine not initialized");
  }

  if (m_runtime.loading) {
    pending << QStringLiteral("still loading (stage: %1, progress %2%)")
                   .arg(loading_stage_text())
                   .arg(static_cast<int>(loading_progress() * 100.0F));
  }
  if (m_loading_overlay_active) {

    pending << QStringLiteral(
                   "loading overlay still up (frames remaining %1, elapsed %2ms, "
                   "renderer %3, gpu resources %4, waiting for first frame %5)")
                   .arg(m_loading_overlay_frames_remaining)
                   .arg(m_loading_overlay_timer.isValid()
                            ? m_loading_overlay_timer.elapsed()
                            : -1)
                   .arg(m_renderer ? QStringLiteral("up") : QStringLiteral("null"))
                   .arg((m_renderer && m_renderer->resources() != nullptr)
                            ? QStringLiteral("up")
                            : QStringLiteral("null"))
                   .arg(m_loading_overlay_wait_for_first_frame.load(
                            std::memory_order_acquire)
                            ? QStringLiteral("yes")
                            : QStringLiteral("no"));
  }
  if (!is_campaign_mission()) {
    pending << QStringLiteral("mission context is not a campaign mission");
  }
  if (m_world == nullptr) {
    pending << QStringLiteral("no world");
  } else if (m_world->entity_count() == 0U) {
    pending << QStringLiteral("world has no entities");
  }
  if (!m_runtime.last_error.isEmpty()) {
    pending << QStringLiteral("error: %1").arg(m_runtime.last_error);
  }
  if (pending.isEmpty()) {
    return QStringLiteral("nothing pending");
  }
  return pending.join(QStringLiteral("; "));
}

bool GameEngine::campaign_completed() const {
  if (!m_campaign_manager) {
    return false;
  }
  const QString campaign_id = m_campaign_manager->current_campaign_id();
  if (campaign_id.isEmpty()) {
    return false;
  }

  if (m_campaign_manager->campaign_completed()) {
    return true;
  }

  for (const QVariant& entry : m_campaign_manager->available_campaigns()) {
    const QVariantMap campaign = entry.toMap();
    if (campaign.value(QStringLiteral("campaign_id")).toString() == campaign_id ||
        campaign.value(QStringLiteral("id")).toString() == campaign_id) {
      return campaign.value(QStringLiteral("completed")).toBool();
    }
  }
  return false;
}

void GameEngine::on_patrol_click(qreal sx, qreal sy) {
  if (!m_input_handler || (m_camera == nullptr)) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_patrol_click(sx, sy, m_viewport);
}

auto GameEngine::control_mode() const -> QString {
  return m_control_mode == PlayerControlMode::Commander ? QStringLiteral("commander")
                                                        : QStringLiteral("rts");
}

auto GameEngine::game_mode() const -> QString {
  return m_game_mode == GameMode::Rpg ? QStringLiteral("rpg") : QStringLiteral("rts");
}

void GameEngine::apply_game_mode_render_policy() {
  if (m_renderer != nullptr) {
    m_renderer->set_world_render_mode(m_game_mode == GameMode::Rpg
                                          ? Render::GL::Renderer::WorldRenderMode::Rpg
                                          : Render::GL::Renderer::WorldRenderMode::Rts);
    m_renderer->set_rpg_camera_focus(
        m_game_mode == GameMode::Rpg ? m_controlled_commander_id : 0);
  }
  if (m_fog != nullptr) {
    m_fog->set_soft_reveal_enabled(m_game_mode == GameMode::Rpg);
  }
}

auto GameEngine::commander_control_available() const -> bool {
  return find_local_commander() != nullptr;
}

auto GameEngine::commander_input() -> QObject* {
  return &m_commander_input;
}

auto GameEngine::find_local_commander() const -> Engine::Core::Entity* {
  if (m_world == nullptr) {
    return nullptr;
  }

  for (auto* entity : m_world->get_entities_with<Engine::Core::CommanderComponent>()) {
    if (entity == nullptr) {
      continue;
    }
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr) {
      continue;
    }
    if (unit->owner_id == m_runtime.local_owner_id && unit->health > 0) {
      return entity;
    }
  }
  return nullptr;
}

auto GameEngine::controlled_commander_entity() -> Engine::Core::Entity* {
  if (m_world == nullptr || m_controlled_commander_id == 0) {
    return nullptr;
  }
  return m_world->get_entity(m_controlled_commander_id);
}

void GameEngine::store_rts_selection() {
  m_saved_rts_selection_ids =
      m_commander_mode
          ->store_rts_selection({.selection_controller = m_selection_controller.get()})
          .saved_rts_selection_ids;
}

void GameEngine::select_controlled_commander() {
  m_commander_mode->select_controlled_commander(
      {.selection_controller = m_selection_controller.get(),
       .controlled_commander_id = m_controlled_commander_id,
       .local_owner_id = m_runtime.local_owner_id});
}

void GameEngine::restore_rts_selection() {
  auto const effects = m_commander_mode->restore_rts_selection(
      {.world = m_world,
       .local_owner_id = m_runtime.local_owner_id,
       .saved_rts_selection_ids = &m_saved_rts_selection_ids});
  if (effects.sync_selection_flags) {
    sync_selection_flags();
  }
  if (effects.emit_selected_units_changed) {
    emit selected_units_changed();
  }
  if (effects.clear_saved_rts_selection_ids) {
    m_saved_rts_selection_ids.clear();
  }
}

void GameEngine::set_active_camera(Render::GL::Camera* camera) {
  m_camera = camera;
  if (m_renderer != nullptr) {
    m_renderer->set_camera(m_camera);
    if (m_viewport.width > 0 && m_viewport.height > 0) {
      m_renderer->set_viewport(m_viewport.width, m_viewport.height);
    }
  }
  Render::GL::CameraVisibility::instance().set_camera(m_camera);
}

void GameEngine::request_enter_commander_control_mode() {
  ensure_initialized();
  auto* commander = find_local_commander();
  if (m_level.is_spectator_mode || commander == nullptr ||
      m_commander_camera == nullptr) {
    return;
  }

  if (m_placement_view_model->is_placing_formation()) {
    m_placement_view_model->on_formation_cancel();
  }
  if (m_placement_view_model->is_placing_construction()) {
    m_placement_view_model->on_construction_cancel();
  }
  set_cursor_mode(CursorMode::Normal);

  store_rts_selection();
  auto const effects = m_commander_mode->enter_commander_control_mode(
      {.world = m_world,
       .commander = commander,
       .commander_camera = m_commander_camera.get(),
       .commander_control = &m_commander_control,
       .local_owner_id = m_runtime.local_owner_id,
       .is_spectator_mode = m_level.is_spectator_mode,
       .follow_selection_enabled = m_follow_selection_enabled});
  if (!effects.entered) {
    return;
  }

  if (effects.save_follow_selection_snapshot) {
    m_rts_camera_snapshot.follow_selection = effects.saved_follow_selection_enabled;
    m_rts_camera_snapshot.valid = true;
  }
  m_follow_selection_enabled = false;
  if (m_camera_controller) {
    m_camera_controller->follow_selection(false);
  }

  m_controlled_commander_id = effects.controlled_commander_id;
  if (effects.commander_view_yaw.has_value()) {
    m_commander_control.set_view_yaw(*effects.commander_view_yaw);
  }
  m_commander_control.set_view_pitch(k_commander_rest_view_pitch_degrees);
  reset_commander_input();
  set_active_camera(m_commander_camera.get());

  enter_commander_runtime_mode();

  emit game_mode_changed();
  if (m_world != nullptr && m_commander_camera != nullptr) {
    (void)m_commander_control.update(*m_world,
                                     m_controlled_commander_id,
                                     m_runtime.local_owner_id,
                                     *m_commander_camera,
                                     0.0F);
  }
  select_controlled_commander();
  Game::Audio::play_cue(Game::Audio::Cue::k_state_commander_enter);
  emit control_mode_changed();
}

void GameEngine::request_exit_commander_control_mode() {
  const bool was_in_commander_mode = m_control_mode == PlayerControlMode::Commander;
  exit_commander_runtime_mode();
  reset_commander_input();
  auto const effects = m_commander_mode->exit_commander_control_mode(
      {.world = m_world,
       .controlled_commander_id = m_controlled_commander_id,
       .rts_follow_selection_snapshot_valid = m_rts_camera_snapshot.valid,
       .rts_follow_selection_snapshot = m_rts_camera_snapshot.follow_selection});
  enter_rts_runtime_mode();
  m_controlled_commander_id = effects.controlled_commander_id;

  set_active_camera(m_rts_camera.get());
  if (effects.restored_follow_selection_enabled.has_value()) {
    m_follow_selection_enabled = *effects.restored_follow_selection_enabled;
    if (m_camera_controller) {
      m_camera_controller->follow_selection(m_follow_selection_enabled);
    }
  }
  restore_rts_selection();

  if (was_in_commander_mode) {
    Game::Audio::play_cue(Game::Audio::Cue::k_state_commander_exit);
  }

  emit game_mode_changed();
  emit control_mode_changed();
}

void GameEngine::enter_rts_runtime_mode() {
  m_control_mode = PlayerControlMode::Rts;
  m_game_mode = GameMode::Rts;
  apply_game_mode_render_policy();
}

void GameEngine::enter_commander_runtime_mode() {
  m_control_mode = PlayerControlMode::Commander;
  m_game_mode = GameMode::Rpg;
  apply_game_mode_render_policy();
}

void GameEngine::exit_commander_runtime_mode() {
  m_rpg_hit_stop_timer = 0.0F;
  m_rpg_telegraphs.clear();
}

void GameEngine::toggle_commander_control_mode() {
  if (m_control_mode == PlayerControlMode::Commander) {
    request_exit_commander_control_mode();
    return;
  }
  request_enter_commander_control_mode();
}

void GameEngine::commander_key_down(int key, int modifiers) {
  (void)modifiers;
  m_commander_control.key_down(key);
}

void GameEngine::commander_key_up(int key, int modifiers) {
  (void)modifiers;
  m_commander_control.key_up(key);
}

void GameEngine::reset_commander_input() {
  m_commander_control.reset();
}

void GameEngine::commander_primary_action() {
  if (m_world == nullptr) {
    return;
  }

  if (!m_commander_control.primary_action(
          *m_world, m_controlled_commander_id, m_runtime.local_owner_id)) {
    request_exit_commander_control_mode();
  }
}

void GameEngine::commander_primary_action_down() {
  m_commander_control.primary_action_down();
  commander_primary_action();
}

void GameEngine::commander_primary_action_up() {
  m_commander_control.primary_action_up();
}

void GameEngine::commander_secondary_action_down() {
  m_commander_control.secondary_action_down();
}

void GameEngine::commander_secondary_action_up() {
  m_commander_control.secondary_action_up();
  if (m_world != nullptr) {
    m_commander_control.release_guard(
        *m_world, m_controlled_commander_id, m_runtime.local_owner_id);
  }
}

void GameEngine::commander_trigger_aura() {
  if (m_world == nullptr) {
    return;
  }

  Engine::Core::Entity* commander_entity = nullptr;
  if (m_control_mode == PlayerControlMode::Commander) {
    commander_entity = controlled_commander_entity();
  } else if (auto* selection = m_world->get_system<Game::Systems::SelectionSystem>()) {
    for (const auto entity_id : selection->get_selected_units()) {
      auto* candidate = m_world->get_entity(entity_id);
      const auto* unit = candidate != nullptr
                             ? candidate->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
      if (candidate != nullptr && unit != nullptr &&
          unit->owner_id == m_runtime.local_owner_id && unit->health > 0 &&
          candidate->get_component<Engine::Core::CommanderComponent>() != nullptr) {
        commander_entity = candidate;
        break;
      }
    }
  }

  if (commander_entity == nullptr) {
    return;
  }
  Game::Command::submit(*m_world,
                        Game::Command::Source::LocalPlayer,
                        m_runtime.local_owner_id,
                        Game::Command::UseCommanderAbility{
                            .commander = commander_entity->get_id(),
                            .ability = Game::Command::CommanderAbility::Aura});
}

void GameEngine::commander_trigger_rally() {
  if (m_control_mode != PlayerControlMode::Commander) {
    return;
  }
  begin_commander_flag_rally();
}

void GameEngine::begin_commander_flag_rally() {
  if (m_cursor_manager == nullptr) {
    return;
  }

  auto const effects = m_commander_mode->begin_commander_flag_rally(
      {.world = m_world,
       .local_commander = find_local_commander(),
       .controlled_commander = controlled_commander_entity(),
       .local_owner_id = m_runtime.local_owner_id,
       .commander_mode_active = m_control_mode == PlayerControlMode::Commander,
       .cursor_mode = m_cursor_manager->mode()});
  if (effects.should_exit_commander_mode) {
    request_exit_commander_control_mode();
    return;
  }
  if (effects.reset_commander_input) {
    reset_commander_input();
  }
  if (effects.clear_rally_preview) {
    m_commander_rally_preview_pos = std::nullopt;
  }
  if (effects.cursor_mode.has_value()) {
    set_cursor_mode(*effects.cursor_mode);
  }
  if (effects.seed_preview_from_view_center) {
    seed_commander_rally_preview_from_view_center();
  }
}

void GameEngine::confirm_commander_flag_rally(qreal sx, qreal sy) {
  if (m_cursor_manager == nullptr) {
    return;
  }

  auto const effects = m_commander_mode->confirm_commander_flag_rally(
      {.world = m_world,
       .local_commander = find_local_commander(),
       .controlled_commander = controlled_commander_entity(),
       .picking_service = m_picking_service.get(),
       .camera = m_camera,
       .viewport_width = m_viewport.width,
       .viewport_height = m_viewport.height,
       .screen_x = sx,
       .screen_y = sy,
       .local_owner_id = m_runtime.local_owner_id,
       .commander_mode_active = m_control_mode == PlayerControlMode::Commander,
       .cursor_mode = m_cursor_manager->mode()});
  if (effects.reset_commander_input) {
    reset_commander_input();
  }
  if (effects.clear_rally_preview) {
    m_commander_rally_preview_pos = std::nullopt;
  }
  if (effects.cursor_mode.has_value()) {
    set_cursor_mode(*effects.cursor_mode);
  }
}

void GameEngine::cancel_commander_flag_rally() {
  auto const effects = m_commander_mode->cancel_commander_flag_rally(
      m_cursor_manager ? m_cursor_manager->mode() : CursorMode::Normal);
  if (effects.clear_rally_preview) {
    m_commander_rally_preview_pos = std::nullopt;
  }
  if (effects.cursor_mode.has_value()) {
    set_cursor_mode(*effects.cursor_mode);
  }
}

auto GameEngine::is_placing_commander_rally() const -> bool {
  return m_cursor_manager &&
         m_cursor_manager->mode() == CursorMode::PlaceCommanderRally;
}

auto GameEngine::has_commander_rally_preview() const -> bool {
  return m_commander_rally_preview_pos.has_value();
}

auto GameEngine::get_commander_rally_preview() const -> QVector3D {
  return m_commander_rally_preview_pos.value_or(QVector3D{});
}

void GameEngine::begin_barracks_rally_placement() {
  ensure_initialized();
  auto const effects = m_commander_mode->begin_barracks_rally_placement(
      {.world = m_world, .local_owner_id = m_runtime.local_owner_id});
  if (effects.clear_rally_preview) {
    m_commander_rally_preview_pos = std::nullopt;
  }
  if (effects.cursor_mode.has_value()) {
    set_cursor_mode(*effects.cursor_mode);
  }
  if (effects.rally_preview.has_value()) {
    m_commander_rally_preview_pos = effects.rally_preview;
  }
}

void GameEngine::confirm_barracks_rally_placement(qreal sx, qreal sy) {
  auto const effects = m_commander_mode->confirm_barracks_rally_placement(
      {.world = m_world,
       .production_manager = m_production_manager.get(),
       .viewport = &m_viewport,
       .local_owner_id = m_runtime.local_owner_id,
       .screen_x = sx,
       .screen_y = sy,
       .cursor_mode =
           m_cursor_manager ? m_cursor_manager->mode() : CursorMode::Normal});
  if (effects.clear_rally_preview) {
    m_commander_rally_preview_pos = std::nullopt;
  }
  if (effects.cursor_mode.has_value()) {
    set_cursor_mode(*effects.cursor_mode);
  }
}

void GameEngine::cancel_barracks_rally_placement() {
  auto const effects = m_commander_mode->cancel_barracks_rally_placement(
      m_cursor_manager ? m_cursor_manager->mode() : CursorMode::Normal);
  if (effects.clear_rally_preview) {
    m_commander_rally_preview_pos = std::nullopt;
  }
  if (effects.cursor_mode.has_value()) {
    set_cursor_mode(*effects.cursor_mode);
  }
}

auto GameEngine::has_commander_rally_flag() const -> bool {
  if (m_world == nullptr) {
    return false;
  }
  for (auto* entity : m_world->get_entities_with<Engine::Core::CommanderComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != m_runtime.local_owner_id) {
      continue;
    }
    auto* commander_data = entity->get_component<Engine::Core::CommanderComponent>();
    if (commander_data != nullptr && commander_data->flag_rally_flag_active) {
      return true;
    }
  }
  return false;
}

auto GameEngine::get_commander_rally_flag_position() const -> QVector3D {
  if (m_world == nullptr) {
    return {};
  }
  for (auto* entity : m_world->get_entities_with<Engine::Core::CommanderComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != m_runtime.local_owner_id) {
      continue;
    }
    auto* commander_data = entity->get_component<Engine::Core::CommanderComponent>();
    if (commander_data != nullptr && commander_data->flag_rally_flag_active) {
      return {
          commander_data->flag_rally_flag_x, 0.0F, commander_data->flag_rally_flag_z};
    }
  }
  return {};
}

void GameEngine::seed_commander_rally_preview_from_view_center() {
  if (m_picking_service == nullptr || m_camera == nullptr || m_viewport.width <= 0 ||
      m_viewport.height <= 0) {
    return;
  }

  QVector3D hit;
  if (!m_picking_service->screen_to_ground(
          QPointF(m_viewport.width * 0.5, m_viewport.height * 0.5),
          *m_camera,
          m_viewport.width,
          m_viewport.height,
          hit)) {
    return;
  }
  m_commander_rally_preview_pos = Game::Systems::NavGrid::snap_to_walkable_ground(hit);
}

void GameEngine::seed_barracks_rally_preview_from_selection() {
  if (auto preview = m_commander_mode->seed_barracks_rally_preview_from_selection(
          {.world = m_world, .local_owner_id = m_runtime.local_owner_id});
      preview.has_value()) {
    m_commander_rally_preview_pos = *preview;
  }
}

void GameEngine::restore_controlled_commander_direct_control_if_ready() {
  auto const effects =
      m_commander_mode->restore_controlled_commander_direct_control_if_ready(
          {.world = m_world,
           .controlled_commander_id = m_controlled_commander_id,
           .commander_mode_active = m_control_mode == PlayerControlMode::Commander,
           .placing_commander_rally = is_placing_commander_rally()});
  if (effects.reset_commander_input) {
    reset_commander_input();
  }
}

void GameEngine::commander_dodge() {
  if (m_control_mode == PlayerControlMode::Commander) {
    m_commander_control.request_dodge();
  }
}

void GameEngine::commander_jump() {
  if (m_control_mode == PlayerControlMode::Commander) {
    m_commander_control.request_jump();
  }
}

void GameEngine::commander_cycle_lock_on() {
  if (m_control_mode != PlayerControlMode::Commander || m_world == nullptr) {
    return;
  }
  m_commander_control.cycle_lock_on_target(
      *m_world, m_controlled_commander_id, m_runtime.local_owner_id);
}

void GameEngine::commander_special_action() {
  if (m_control_mode == PlayerControlMode::Commander) {
    m_commander_control.special_action();
  }
}

void GameEngine::commander_vanguard_rush() {
  if (m_control_mode == PlayerControlMode::Commander) {
    m_commander_control.request_vanguard_rush();
  }
}

void GameEngine::commander_second_wind() {
  if (m_control_mode == PlayerControlMode::Commander) {
    m_commander_control.request_second_wind();
  }
}

void GameEngine::commander_toggle_camera_mode() {
  if (m_control_mode != PlayerControlMode::Commander || m_world == nullptr) {
    return;
  }
  m_commander_control.toggle_close_camera_mode(
      *m_world, m_controlled_commander_id, m_runtime.local_owner_id);
}

void GameEngine::commander_toggle_weapon_stance() {
  if (m_control_mode != PlayerControlMode::Commander || m_world == nullptr) {
    return;
  }
  m_commander_control.toggle_weapon_stance(
      *m_world, m_controlled_commander_id, m_runtime.local_owner_id);
}

void GameEngine::commander_mouse_move(qreal dx, qreal dy) {
  m_commander_control.mouse_move(dx, dy);
}

void GameEngine::commander_mouse_look_at(qreal sx,
                                         qreal sy,
                                         qreal center_sx,
                                         qreal center_sy) {
  m_commander_control.mouse_look_at(sx, sy, center_sx, center_sy, m_window);
}

void GameEngine::commander_center_mouse(qreal center_sx, qreal center_sy) {
  m_commander_control.center_mouse(center_sx, center_sy, m_window);
}

void GameEngine::poll_commander_mouse_look() {
  m_commander_control.poll_mouse_look(m_window);
}

void GameEngine::update_cursor(Qt::CursorShape new_cursor) {
  if (m_window == nullptr) {
    return;
  }
  if (m_runtime.current_cursor != new_cursor) {
    m_runtime.current_cursor = new_cursor;
    QPointer<QQuickWindow> const safe_window(m_window);
    QMetaObject::invokeMethod(
        m_window,
        [safe_window, new_cursor]() {
          if (safe_window != nullptr) {
            safe_window->setCursor(new_cursor);
          }
        },
        Qt::AutoConnection);
  }
}

void GameEngine::set_error(const QString& error_message) {
  if (m_runtime.last_error != error_message) {
    m_runtime.last_error = error_message;
    qCritical() << "GameEngine error:" << error_message;
    emit last_error_changed();
  }
}

void GameEngine::set_cursor_mode(CursorMode mode) {
  if (!m_cursor_manager) {
    return;
  }
  m_cursor_manager->set_mode(mode);
  m_cursor_manager->update_cursor_shape(m_window);
}

void GameEngine::set_cursor_mode(const QString& mode) {
  set_cursor_mode(CursorModeUtils::fromString(mode));
}

auto GameEngine::cursor_mode() const -> QString {
  if (!m_cursor_manager) {
    return "normal";
  }
  return m_cursor_manager->mode_string();
}

auto GameEngine::global_cursor_x() const -> qreal {
  if (!m_cursor_manager) {
    return 0;
  }
  return m_cursor_manager->global_cursor_x(m_window);
}

auto GameEngine::global_cursor_y() const -> qreal {
  if (!m_cursor_manager) {
    return 0;
  }
  return m_cursor_manager->global_cursor_y(m_window);
}

void GameEngine::set_hover_at_screen(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->set_hover_at_screen(sx, sy, m_viewport);
  }
  const bool civilian_delivery_available =
      App::Core::FrameUiCoordinator::civilian_delivery_available(
          {.world = m_world,
           .hover_tracker = m_hover_tracker.get(),
           .local_owner_id = m_runtime.local_owner_id});
  if (m_civilian_delivery_available != civilian_delivery_available) {
    m_civilian_delivery_available = civilian_delivery_available;
    emit civilian_delivery_available_changed();
  }

  if (m_cursor_manager == nullptr || m_picking_service == nullptr ||
      m_camera == nullptr) {
    return;
  }

  QVector3D hit;
  if (m_cursor_manager->mode() == CursorMode::PlaceCommanderRally &&
      m_picking_service->screen_to_ground(
          QPointF(sx, sy), *m_camera, m_viewport.width, m_viewport.height, hit)) {
    m_commander_rally_preview_pos =
        Game::Systems::NavGrid::snap_to_walkable_ground(hit);
  } else if (m_cursor_manager->mode() == CursorMode::PlaceBarracksRally &&
             m_picking_service->screen_to_surface(QPointF(sx, sy),
                                                  *m_camera,
                                                  m_viewport.width,
                                                  m_viewport.height,
                                                  hit)) {
    m_commander_rally_preview_pos = hit;
  }
}

void GameEngine::on_click_select(qreal sx, qreal sy, bool additive) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->on_click_select(
        sx, sy, additive, m_runtime.local_owner_id, m_viewport);
  }
}

void GameEngine::on_area_selected(
    qreal x1, qreal y1, qreal x2, qreal y2, bool additive) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->on_area_selected(
        x1, y1, x2, y2, additive, m_runtime.local_owner_id, m_viewport);
  }
}

void GameEngine::select_all_troops() {
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->select_all_troops(m_runtime.local_owner_id);
  }
}

void GameEngine::select_unit_by_id(qulonglong unit_id) {
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->select_unit_by_id(static_cast<Engine::Core::EntityID>(unit_id),
                                       m_runtime.local_owner_id);
  }
}

void GameEngine::select_selected_units_by_type(const QString& unit_type) {
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->select_selected_units_by_type(unit_type, m_runtime.local_owner_id);
  }
}

void GameEngine::ensure_initialized() {
  const bool was_initialized = m_runtime.initialized;
  QString error;
  App::Core::WorldBootstrap::ensure_initialized(m_runtime.initialized,
                                                *m_renderer,
                                                *m_camera,
                                                m_surface ? m_surface->ground()
                                                          : nullptr,
                                                &error);
  if (!error.isEmpty()) {
    set_error(error);
  }
  if (!was_initialized && m_runtime.initialized) {
    emit renderer_initialized_changed();
  }
}

auto GameEngine::enemy_troops_defeated() const -> int {
  return m_enemy_troops_defeated;
}

auto GameEngine::selected_player_state() const -> QVariantMap {
  return m_selected_player_state;
}

void GameEngine::set_selected_player_id(int id) {
  if (m_selected_player_id == id) {
    return;
  }
  m_selected_player_id = id;
  sync_selected_player_state();
  emit selected_player_id_changed();
}

auto GameEngine::scene_context() const -> AppSceneContext {
  return AppSceneContext{.session = m_session.get(),
                         .world = m_world,
                         .renderer = m_renderer.get(),
                         .active_camera = m_camera,
                         .ground = m_surface ? m_surface->ground() : nullptr,
                         .terrain = m_surface ? m_surface->terrain() : nullptr,
                         .features = m_features.get(),
                         .scatter = m_scatter.get(),
                         .fog = m_fog.get(),
                         .boundary_fog = m_boundary_fog.get(),
                         .ambient_fog = m_ambient_fog.get(),
                         .rain = m_rain.get(),
                         .minimap_manager = m_minimap_manager.get(),
                         .visibility_coordinator = m_visibility_coordinator.get(),
                         .victory_service = m_victory_service.get(),
                         .rain_manager = m_rain_manager.get(),
                         .weather_audio = m_weather_audio.get(),
                         .environment_clock = m_environment_clock.get()};
}

auto GameEngine::get_player_stats(int owner_id) -> QVariantMap {
  QVariantMap result;

  auto& stats_registry = Game::Systems::GlobalStatsRegistry::instance();
  const auto* stats = stats_registry.get_stats(owner_id);

  if (stats != nullptr) {
    result["troopsRecruited"] = stats->troops_recruited;
    result["enemiesKilled"] = stats->enemies_killed;
    result["losses"] = stats->losses;
    result["barracksOwned"] = stats->barracks_owned;
    result["playTimeSec"] = stats->play_time_sec;
    result["gameEnded"] = stats->game_ended;
  } else {
    result["troopsRecruited"] = 0;
    result["enemiesKilled"] = 0;
    result["losses"] = 0;
    result["barracksOwned"] = 0;
    result["playTimeSec"] = 0.0F;
    result["gameEnded"] = false;
  }

  return result;
}

void GameEngine::update_rts_control_mode(float dt) {
  (void)dt;
  if (m_camera_controller) {
    m_camera_controller->update_follow(m_follow_selection_enabled);
  }
}

void GameEngine::clear_controlled_commander_state() {
  m_commander_mode->clear_controlled_commander_state(
      {.world = m_world, .controlled_commander_id = m_controlled_commander_id});
}

void GameEngine::update_commander_control_mode(float dt) {
  poll_commander_mouse_look();
  auto const effects = m_commander_mode->update_commander_control_mode(
      {.world = m_world,
       .commander_camera = m_commander_camera.get(),
       .commander_control = &m_commander_control,
       .controlled_commander_id = m_controlled_commander_id,
       .local_owner_id = m_runtime.local_owner_id,
       .dt = dt});
  if (effects.should_exit_commander_mode) {
    request_exit_commander_control_mode();
    return;
  }
  if (effects.hit_stop_duration.has_value()) {
    m_rpg_hit_stop_timer = *effects.hit_stop_duration;
    m_rpg_hit_stop_total = *effects.hit_stop_duration;
  }
}

auto GameEngine::apply_runtime_time_effects(float dt) -> float {
  if (m_control_mode != PlayerControlMode::Commander) {
    return dt;
  }

  if (!m_runtime.paused && m_rpg_hit_stop_timer > 0.0F) {
    m_rpg_hit_stop_timer -= dt;
    if (m_rpg_hit_stop_timer < 0.0F) {
      m_rpg_hit_stop_timer = 0.0F;
    } else {

      const float progress =
          1.0F - std::clamp(m_rpg_hit_stop_timer / m_rpg_hit_stop_total, 0.0F, 1.0F);
      const float time_scale =
          progress < 0.5F ? 0.04F : (0.04F + 0.96F * (progress - 0.5F) * 2.0F);
      dt *= time_scale;
    }
  }
  return dt;
}

void GameEngine::update_active_runtime_simulation(float dt) {
  if (m_world == nullptr) {
    return;
  }

  if (m_control_mode == PlayerControlMode::Commander) {
    update_commander_control_mode(dt);
    m_world->update(dt);
    restore_controlled_commander_direct_control_if_ready();
    return;
  }

  m_world->update(dt);
  update_rts_control_mode(dt);
  finish_replay_verification_if_done();
}

void GameEngine::finish_replay_verification_if_done() {
  if (!m_replay_verify_exit || m_session == nullptr) {
    return;
  }
  auto* player = m_session->replay_player();
  if (player == nullptr) {
    return;
  }
  const auto& file = player->file();
  const std::uint64_t last_recorded_tick = std::max<std::uint64_t>(
      file.last_tick(), file.digests.empty() ? 0U : file.digests.back().tick);
  if (!player->finished() || m_session->clock().tick() <= last_recorded_tick) {
    return;
  }
  if (const auto& divergence = player->divergence(); divergence.has_value()) {
    qCritical() << "SOI_REPLAY_VERIFY: FAIL - diverged at tick" << divergence->tick
                << "(recorded" << divergence->recorded << ", observed"
                << divergence->observed << ")";
    QCoreApplication::exit(12);
  } else {
    qInfo() << "SOI_REPLAY_VERIFY: PASS -" << player->fed_count() << "commands,"
            << player->checked_count() << "digests matched";
    QCoreApplication::exit(0);
  }
  m_replay_verify_exit = false;
}

auto GameEngine::should_render_selected_entity(Engine::Core::EntityID id) const
    -> bool {
  return m_control_mode != PlayerControlMode::Commander ||
         id != m_controlled_commander_id;
}

void GameEngine::render_runtime_mode_effects() {
  if (m_control_mode != PlayerControlMode::Commander || m_world == nullptr ||
      m_renderer == nullptr || m_controlled_commander_id == 0) {
    return;
  }

  Engine::Core::EntityID locked_target_id = m_commander_control.locked_target_id();
  if (auto* commander = m_world->get_entity(m_controlled_commander_id)) {
    if (auto* rpg_targets =
            commander->get_component<Engine::Core::RpgCommanderTargetComponent>()) {
      locked_target_id = rpg_targets->explicit_lock_target_id;
    }
  }

  m_rpg_telegraphs.render(m_renderer.get(),
                          m_world,
                          m_controlled_commander_id,
                          locked_target_id,
                          m_renderer->get_animation_time());
}

void GameEngine::update(float dt) {
  if (m_runtime.loading) {
    return;
  }

  m_order_markers.update(dt, m_world);
  m_combat_feedback.update(dt);

  if (m_runtime.paused) {
    dt = 0.0F;
  } else {
    dt *= m_runtime.time_scale;
    dt = apply_runtime_time_effects(dt);
  }

  update_mission_waves(dt);

  RuntimeFrameState frame_state{
      .local_owner_id = m_runtime.local_owner_id,
      .spectator_mode = m_level.is_spectator_mode,
      .viewport_width = m_viewport.width,
      .viewport_height = m_viewport.height,
      .selection_refresh_enabled = (m_selected_units_model != nullptr),
      .selection_refresh_counter = m_runtime.selection_refresh_counter,
      .minimap_unit_update_accumulator = m_runtime.minimap_unit_update_accumulator};
  const FrameUpdateCallbacks callbacks{
      .on_minimap_image_changed = [this]() { emit minimap_image_changed(); },
      .on_selected_units_data_changed =
          [this]() {
            emit selected_units_data_changed();
          }};

  m_frame_orchestrator.update(
      scene_context(),
      frame_state,
      m_entity_cache,
      (!m_runtime.paused && !m_runtime.loading) ? m_ambient_state_manager.get()
                                                : nullptr,
      m_runtime.victory_state,
      dt,
      callbacks,
      [this](float step_dt) { update_active_runtime_simulation(step_dt); });
  m_runtime.selection_refresh_counter = frame_state.selection_refresh_counter;
  m_runtime.minimap_unit_update_accumulator =
      frame_state.minimap_unit_update_accumulator;
  sync_scatter_world_props();
  sync_selected_player_state();
  sync_attack_targeting();
  sync_attack_range_rings();
  sync_focus_targets();
  sync_target_focus_markers();
}

void GameEngine::render(int pixel_width, int pixel_height) {
  if (!m_renderer || !m_world || !m_runtime.initialized || m_runtime.loading) {
    return;
  }

  Render::GL::CameraVisibility::instance().set_camera(m_camera);

  if (pixel_width > 0 && pixel_height > 0) {
    m_viewport.width = pixel_width;
    m_viewport.height = pixel_height;
    m_renderer->set_viewport(pixel_width, pixel_height);
  }

  if (auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>()) {
    const auto& sel = selection_system->get_selected_units();
    std::vector<Engine::Core::EntityID> ids;
    ids.reserve(sel.size());
    for (const auto id : sel) {
      if (!should_render_selected_entity(id)) {
        continue;
      }
      ids.push_back(id);
    }
    m_renderer->set_selected_entities(ids);
  }

  m_renderer->set_world_view(m_session != nullptr
                                 ? Render::WorldView::of(*m_session)
                                 : Render::WorldView::of_active_session());

  m_renderer->begin_frame();

  if (m_terrain_scene) {
    m_terrain_scene->submit(*m_renderer, m_renderer->resources());
  }

  if (m_renderer && m_hover_tracker) {
    m_renderer->set_hovered_entity_id(m_hover_tracker->get_last_hovered_entity());
  }
  if (m_renderer) {
    m_renderer->set_local_owner_id(m_runtime.local_owner_id);
    m_renderer->set_order_marker_spectator_mode(m_level.is_spectator_mode);
  }

  m_renderer->render_world(m_world);
  App::Core::FrameUiCoordinator::render_effects(
      {.renderer = m_renderer.get(),
       .world = m_world,
       .command_controller = m_command_controller.get(),
       .local_owner_id = m_runtime.local_owner_id,
       .commander_rally_preview_pos = m_commander_rally_preview_pos,
       .attack_targeting = &m_attack_targeting,
       .attack_range_rings = &m_attack_range_rings,
       .order_markers = &m_order_markers.markers(),
       .target_focus = &m_target_focus},
      [this]() { render_runtime_mode_effects(); });
  m_renderer->end_frame();

  update_loading_overlay();
  update_cursor_position();
}

void GameEngine::set_input_viewport_size(qreal width, qreal height) {
  if (width > 0.0 && height > 0.0) {
    m_viewport.input_width = width;
    m_viewport.input_height = height;
  }
}

void GameEngine::update_loading_overlay() {
  if (!m_loading_overlay_wait_for_first_frame.load(std::memory_order_acquire)) {
    return;
  }

  if (QThread::currentThread() != thread()) {
    QMetaObject::invokeMethod(
        this, [this]() { update_loading_overlay(); }, Qt::QueuedConnection);
    return;
  }

  if (!m_renderer || (m_renderer->resources() == nullptr)) {
    m_loading_overlay_frames_remaining = 5;
    m_loading_overlay_last_frame_ms = 0;
    m_loading_overlay_timer.restart();
    return;
  }

  if (m_loading_overlay_frames_remaining > 0) {
    m_loading_overlay_frames_remaining--;

    const qint64 now_ms =
        m_loading_overlay_timer.isValid() ? m_loading_overlay_timer.elapsed() : 0;
    qInfo().noquote() << QStringLiteral(
                             "SOI_LOADING_OVERLAY: frame %1 of 5 presented at %2ms "
                             "(+%3ms since the previous one)")
                             .arg(5 - m_loading_overlay_frames_remaining)
                             .arg(now_ms)
                             .arg(now_ms - m_loading_overlay_last_frame_ms);
    m_loading_overlay_last_frame_ms = now_ms;
  }

  constexpr qint64 k_loading_overlay_max_wait_ms = 15000;
  const qint64 elapsed_ms =
      m_loading_overlay_timer.isValid() ? m_loading_overlay_timer.elapsed() : 0;
  const bool enough_time = m_loading_overlay_timer.isValid() &&
                           (elapsed_ms >= m_loading_overlay_min_duration_ms);
  const bool exceeded_max_wait = m_loading_overlay_timer.isValid() &&
                                 (elapsed_ms >= k_loading_overlay_max_wait_ms);

  QStringList pending_components;
  const bool scatter_ready = !m_scatter || m_scatter->is_gpu_ready();

  if (!scatter_ready) {
    pending_components << QStringLiteral("terrain scatter");
  }

  const bool biome_gpu_ready = pending_components.isEmpty();

  if (enough_time && m_loading_overlay_frames_remaining <= 0 &&
      (biome_gpu_ready || exceeded_max_wait)) {
    if (exceeded_max_wait && !biome_gpu_ready) {
      qWarning() << "Loading overlay timed out waiting for GPU readiness"
                 << pending_components.join(", ");
    }
    m_loading_overlay_wait_for_first_frame.store(false, std::memory_order_release);
    m_loading_overlay_active = false;
    if (m_finalize_progress_after_overlay && m_loading_progress_tracker) {
      m_loading_progress_tracker->set_stage(
          LoadingProgressTracker::LoadingStage::COMPLETED);
    }
    m_finalize_progress_after_overlay = false;
    emit is_loading_changed();

    if (m_show_objectives_after_loading) {
      m_show_objectives_after_loading = false;
      emit campaign_mission_changed();
    }
  }
}

void GameEngine::update_cursor_position() {
  if (QThread::currentThread() != thread()) {
    QMetaObject::invokeMethod(
        this, [this]() { update_cursor_position(); }, Qt::QueuedConnection);
    return;
  }
  qreal const current_x = global_cursor_x();
  qreal const current_y = global_cursor_y();
  if (current_x != m_runtime.last_cursor_x || current_y != m_runtime.last_cursor_y) {
    m_runtime.last_cursor_x = current_x;
    m_runtime.last_cursor_y = current_y;
    emit global_cursor_changed();
  }
}

auto GameEngine::screen_to_ground(const QPointF& screen_pt,
                                  QVector3D& out_world) -> bool {
  return App::Utils::screen_to_ground(m_picking_service.get(),
                                      m_camera,
                                      m_window,
                                      m_viewport.width,
                                      m_viewport.height,
                                      screen_pt,
                                      out_world);
}

auto GameEngine::world_to_screen(const QVector3D& world,
                                 QPointF& out_screen) const -> bool {
  return App::Utils::world_to_screen(m_picking_service.get(),
                                     m_camera,
                                     m_window,
                                     m_viewport.width,
                                     m_viewport.height,
                                     world,
                                     out_screen);
}

auto GameEngine::map_input_to_viewport(qreal sx, qreal sy) const -> QPointF {
  if (m_viewport.width <= 0 || m_viewport.height <= 0 ||
      m_viewport.input_width <= 0.0 || m_viewport.input_height <= 0.0) {
    return {sx, sy};
  }

  qreal const scale_x = qreal(m_viewport.width) / m_viewport.input_width;
  qreal const scale_y = qreal(m_viewport.height) / m_viewport.input_height;
  return {sx * scale_x, sy * scale_y};
}

void GameEngine::sync_selection_flags() {
  if (!m_world) {
    return;
  }
  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return;
  }

  Game::Selection::sanitize_selection(m_world, selection_system);
  const auto prune_effects =
      App::Core::FrameUiCoordinator::prune_selection_action_context(
          {.world = m_world,
           .cursor_manager = m_cursor_manager.get(),
           .production_manager = m_production_manager.get(),
           .command_controller = m_command_controller.get(),
           .local_owner_id = m_runtime.local_owner_id,
           .hud_action_states = get_hud_action_states()});
  if (prune_effects.cancel_construction) {
    m_placement_view_model->on_construction_cancel();
  }
  if (prune_effects.cancel_formation) {
    m_placement_view_model->on_formation_cancel();
  }
  switch (prune_effects.cursor_resolution) {
  case App::Core::FrameUiCoordinator::CursorResolution::CancelBarracksRallyPlacement:
    cancel_barracks_rally_placement();
    break;
  case App::Core::FrameUiCoordinator::CursorResolution::CancelCommanderFlagRally:
    cancel_commander_flag_rally();
    break;
  case App::Core::FrameUiCoordinator::CursorResolution::ResetToNormal:
    if (prune_effects.clear_patrol_first_waypoint && m_command_controller) {
      m_command_controller->clear_patrol_first_waypoint();
    }
    set_cursor_mode(CursorMode::Normal);
    break;
  case App::Core::FrameUiCoordinator::CursorResolution::None:
    break;
  }

  emit hold_mode_changed(any_selected_in_hold_mode());
  emit guard_mode_changed(any_selected_in_guard_mode());
  emit formation_mode_changed(m_placement_view_model->any_selected_in_formation_mode());
  emit run_mode_changed(any_selected_in_run_mode());
  const bool civilian_delivery_available =
      App::Core::FrameUiCoordinator::civilian_delivery_available(
          {.world = m_world,
           .hover_tracker = m_hover_tracker.get(),
           .local_owner_id = m_runtime.local_owner_id});
  if (m_civilian_delivery_available != civilian_delivery_available) {
    m_civilian_delivery_available = civilian_delivery_available;
    emit civilian_delivery_available_changed();
  }
}

void GameEngine::sync_attack_range_rings() {
  std::vector<Game::Systems::AttackRangeRing> rings;

  if ((m_world != nullptr) && !m_level.is_spectator_mode) {
    if (auto* selection_system =
            m_world->get_system<Game::Systems::SelectionSystem>()) {
      const auto& selected = selection_system->get_selected_units();
      const auto hovered =
          m_hover_tracker ? m_hover_tracker->get_last_hovered_entity() : 0;

      Game::Systems::AttackRangeRingRequest request;
      request.world = m_world;
      request.local_owner_id = m_runtime.local_owner_id;
      request.selection = selected;
      request.max_rings = Game::Systems::k_attack_range_max_rings;
      if (std::find(selected.begin(), selected.end(), hovered) != selected.end()) {
        request.focus_entity_id = hovered;
      }
      rings = Game::Systems::collect_attack_range_rings(request);
    }
  }

  m_attack_range_rings = std::move(rings);
}

namespace {

auto accepted_order_cue(App::Core::OrderKind kind) -> const char* {
  switch (kind) {
  case App::Core::OrderKind::Move:
  case App::Core::OrderKind::Deliver:
  case App::Core::OrderKind::Repair:
    return Game::Audio::Cue::k_order_move;
  case App::Core::OrderKind::Attack:
    return Game::Audio::Cue::k_order_attack;
  case App::Core::OrderKind::Patrol:
    return Game::Audio::Cue::k_order_patrol;
  case App::Core::OrderKind::Stop:
    return Game::Audio::Cue::k_order_stop;
  case App::Core::OrderKind::Rally:
    return Game::Audio::Cue::k_order_rally_set;
  case App::Core::OrderKind::Build:
  case App::Core::OrderKind::Gather:
    return Game::Audio::Cue::k_build_placement_confirmed;
  case App::Core::OrderKind::Guard:
  case App::Core::OrderKind::Hold:
  case App::Core::OrderKind::Formation:
  case App::Core::OrderKind::None:
    break;
  }
  return nullptr;
}

} // namespace

void GameEngine::handle_order_feedback(const App::Core::OrderOutcome& outcome) {
  if (!outcome.issued()) {
    return;
  }

  m_order_markers.push(outcome, m_world);

  QString message;
  if (outcome.accepted()) {
    if (const char* cue = accepted_order_cue(outcome.kind)) {
      Game::Audio::play_cue(cue);
    }
    if (outcome.kind == App::Core::OrderKind::Attack && outcome.target != 0) {
      App::Controllers::ActionVFX::spawn_attack_arrow(m_world, outcome.target);
    }
    message = App::Core::accepted_order_message(outcome);
  } else {
    Game::Audio::play_cue(Game::Audio::Cue::k_ui_error);
    message = outcome.reason;
  }

  emit order_feedback(QString::fromLatin1(App::Core::order_kind_name(outcome.kind)),
                      outcome.accepted(),
                      message);
}

void GameEngine::sync_attack_targeting() {
  Game::Systems::AttackTargetingHighlights highlights;
  QVariantMap hint;
  hint[QStringLiteral("state")] = QStringLiteral("none");
  hint[QStringLiteral("name")] = QString();
  hint[QStringLiteral("range")] = QStringLiteral("none");

  const bool attack_mode_active =
      (m_cursor_manager != nullptr) && m_cursor_manager->mode() == CursorMode::Attack;

  if (attack_mode_active && (m_world != nullptr) && !m_level.is_spectator_mode) {
    std::vector<Engine::Core::EntityID> attackers;
    if (auto* selection_system =
            m_world->get_system<Game::Systems::SelectionSystem>()) {
      attackers = App::Core::filter_selected_units_for_action(
          m_world, selection_system->get_selected_units(), QStringLiteral("attack"));
    }

    auto& visibility = Game::Map::VisibilityService::instance();
    const auto snapshot =
        visibility.is_initialized() ? visibility.snapshot_ptr() : nullptr;

    Game::Systems::AttackTargetingRequest request;
    request.world = m_world;
    request.local_owner_id = m_runtime.local_owner_id;
    request.has_attackers = !attackers.empty();
    request.hovered_entity_id =
        m_hover_tracker ? m_hover_tracker->get_last_hovered_entity() : 0;
    if (m_camera != nullptr) {
      const QVector3D anchor = m_camera->get_target();
      request.anchor_x = anchor.x();
      request.anchor_z = anchor.z();
      request.max_distance = Game::Systems::k_attack_highlight_max_distance;
    }
    request.max_markers = Game::Systems::k_attack_highlight_max_markers;
    request.visibility = snapshot.get();

    highlights = Game::Systems::collect_attack_target_highlights(request);

    const auto verdict_key =
        Game::Systems::attack_target_verdict_key(highlights.hovered_verdict);
    hint[QStringLiteral("state")] = QString::fromLatin1(
        verdict_key.data(), static_cast<qsizetype>(verdict_key.size()));
    if (highlights.hovered_verdict == Game::Systems::AttackTargetVerdict::Valid) {
      QString name;
      QString nation;
      int health = 0;
      int max_health = 0;
      bool is_building = false;
      bool alive = false;
      if (get_unit_info(highlights.hovered_entity_id,
                        name,
                        health,
                        max_health,
                        is_building,
                        alive,
                        nation)) {
        hint[QStringLiteral("name")] = name;
      }

      const auto range_verdict = Game::Systems::classify_range_to_target(
          m_world, attackers, highlights.hovered_entity_id);
      const auto range_key = Game::Systems::range_verdict_key(range_verdict);
      hint[QStringLiteral("range")] = QString::fromLatin1(
          range_key.data(), static_cast<qsizetype>(range_key.size()));
    }
  }

  m_attack_targeting = std::move(highlights);

  if ((m_activity_view_model == nullptr) || (m_attack_target_hint == hint)) {
    return;
  }
  m_attack_target_hint = hint;
  QMetaObject::invokeMethod(
      m_activity_view_model.get(),
      [view_model = m_activity_view_model.get(), hint]() {
        view_model->set_attack_target_hint(hint);
      },
      Qt::QueuedConnection);
}

auto GameEngine::is_action_enabled(const QString& action_id) const -> bool {
  return get_hud_action_states()
      .value(action_id)
      .toMap()
      .value(QStringLiteral("enabled"))
      .toBool();
}

void GameEngine::camera_move(float dx, float dz) {
  ensure_initialized();
  if (m_camera_controller) {
    m_camera_controller->move(dx, dz);
  }
}

void GameEngine::camera_elevate(float dy) {
  ensure_initialized();
  if (m_camera_controller) {
    m_camera_controller->elevate(dy);
  }
}

void GameEngine::reset_camera() {
  ensure_initialized();
  if (m_camera_controller) {
    m_camera_controller->reset(m_runtime.local_owner_id, m_level);
  }
}

void GameEngine::camera_zoom(float delta) {
  ensure_initialized();
  if (m_camera_controller) {
    m_camera_controller->zoom(delta);
  }
}

auto GameEngine::camera_distance() const -> float {
  if (m_camera_controller) {
    return m_camera_controller->distance();
  }
  return 0.0F;
}

void GameEngine::camera_yaw(float degrees) {
  ensure_initialized();
  if (m_camera_controller) {
    m_camera_controller->yaw(degrees);
  }
}

void GameEngine::camera_orbit(float yaw_deg, float pitch_deg) {
  ensure_initialized();
  if (m_camera_controller) {
    m_camera_controller->orbit(yaw_deg, pitch_deg);
  }
}

void GameEngine::camera_orbit_direction(int direction, bool shift) {
  if (m_camera_controller) {
    m_camera_controller->orbit_direction(direction, shift);
  }
}

void GameEngine::camera_follow_selection(bool enable) {
  ensure_initialized();
  m_follow_selection_enabled = enable;
  if (m_camera_controller) {
    m_camera_controller->follow_selection(enable);
  }
}

void GameEngine::camera_set_follow_lerp(float alpha) {
  ensure_initialized();
  if (m_camera_controller) {
    m_camera_controller->set_follow_lerp(alpha);
  }
}

void GameEngine::on_minimap_left_click(qreal mx,
                                       qreal my,
                                       qreal minimap_width,
                                       qreal minimap_height) {
  ensure_initialized();
  if ((m_camera == nullptr) || !m_minimap_manager ||
      !m_minimap_manager->has_minimap()) {
    return;
  }

  const QImage& minimap_img = m_minimap_manager->get_image();
  if (minimap_img.isNull()) {
    return;
  }

  const auto img_width = static_cast<float>(minimap_img.width());
  const auto img_height = static_cast<float>(minimap_img.height());

  const float px =
      (static_cast<float>(mx) / static_cast<float>(minimap_width)) * img_width;
  const float py =
      (static_cast<float>(my) / static_cast<float>(minimap_height)) * img_height;

  const auto [world_x, world_z] =
      Game::Map::Minimap::pixel_to_world(px,
                                         py,
                                         m_minimap_manager->get_world_width(),
                                         m_minimap_manager->get_world_height(),
                                         img_width,
                                         img_height,
                                         m_minimap_manager->get_tile_size());

  if (m_camera != nullptr) {
    const QVector3D new_target(world_x, 0.0F, world_z);
    const QVector3D current_target = m_camera->get_target();
    const QVector3D current_position = m_camera->get_position();

    const QVector3D offset = current_position - current_target;

    m_camera->look_at(new_target + offset, new_target, m_camera->get_up_vector());
  }

  m_follow_selection_enabled = false;
  if (m_camera_controller) {
    m_camera_controller->follow_selection(false);
  }
}

void GameEngine::on_minimap_right_click(qreal mx,
                                        qreal my,
                                        qreal minimap_width,
                                        qreal minimap_height) {
  ensure_initialized();
  if (!m_minimap_manager || !m_minimap_manager->has_minimap()) {
    return;
  }

  const QImage& minimap_img = m_minimap_manager->get_image();
  if (minimap_img.isNull()) {
    return;
  }

  const auto img_width = static_cast<float>(minimap_img.width());
  const auto img_height = static_cast<float>(minimap_img.height());

  const float px =
      (static_cast<float>(mx) / static_cast<float>(minimap_width)) * img_width;
  const float py =
      (static_cast<float>(my) / static_cast<float>(minimap_height)) * img_height;

  const auto [world_x, world_z] =
      Game::Map::Minimap::pixel_to_world(px,
                                         py,
                                         m_minimap_manager->get_world_width(),
                                         m_minimap_manager->get_world_height(),
                                         img_width,
                                         img_height,
                                         m_minimap_manager->get_tile_size());

  if (m_input_handler) {
    m_input_handler->on_minimap_right_click(QVector3D(world_x, 0.0F, world_z),
                                            m_runtime.local_owner_id);
  }
}

auto GameEngine::selected_units_model() -> QAbstractItemModel* {
  return m_selected_units_model;
}

auto GameEngine::audio_system() -> QObject* {
  return m_audio_systemProxy.get();
}

void GameEngine::set_audio_frontend_context(const QString& context) {
  const QString normalized = context.trimmed().toLower();
  if (m_audio_frontend_context == normalized) {
    return;
  }

  m_audio_frontend_context = normalized;
  m_audio_coordinator->apply_frontend_music_context(normalized);
}

void GameEngine::set_paused(bool paused) {
  if (m_runtime.paused == paused) {
    return;
  }
  m_runtime.paused = paused;
  if (m_runtime.loading) {
    return;
  }
  Game::Audio::play_cue(paused ? Game::Audio::Cue::k_state_pause
                               : Game::Audio::Cue::k_state_resume);
}

void GameEngine::set_game_speed(float speed) {
  const float clamped = std::max(0.0F, speed);
  if (qFuzzyCompare(m_runtime.time_scale, clamped)) {
    return;
  }
  m_runtime.time_scale = clamped;
  Game::Audio::play_cue(Game::Audio::Cue::k_state_speed_change);
}

auto GameEngine::has_units_selected() const -> bool {
  if (!m_selection_controller) {
    return false;
  }
  return m_selection_controller->has_units_selected();
}

auto GameEngine::player_troop_count() const -> int {
  return m_entity_cache.player_troop_count;
}

auto GameEngine::has_selected_type(const QString& type) const -> bool {
  if (!m_selection_controller) {
    return false;
  }
  return m_selection_controller->has_selected_type(type);
}

void GameEngine::recruit_near_selected(const QString& unit_type) {
  ensure_initialized();
  if (!m_command_controller) {
    return;
  }
  m_command_controller->recruit_near_selected(unit_type, m_runtime.local_owner_id);
}

auto GameEngine::get_selected_production_state() const -> QVariantMap {
  return m_production_manager ? m_production_manager->get_selected_production_state(
                                    m_runtime.local_owner_id)
                              : QVariantMap();
}

auto GameEngine::get_selected_home_production_state() const -> QVariantMap {
  return m_production_manager
             ? m_production_manager->get_selected_home_production_state(
                   m_runtime.local_owner_id)
             : QVariantMap();
}

auto GameEngine::get_unit_production_info(
    const QString& unit_type, const QString& nation_id) const -> QVariantMap {
  return m_production_manager
             ? m_production_manager->get_unit_production_info(unit_type, nation_id)
             : QVariantMap();
}

auto GameEngine::get_selected_marketplace_state() const -> QVariantMap {
  return m_production_manager ? m_production_manager->get_selected_marketplace_state(
                                    m_runtime.local_owner_id)
                              : QVariantMap();
}

auto GameEngine::get_selected_builder_production_state() const -> QVariantMap {
  return m_production_manager
             ? m_production_manager->get_selected_builder_production_state()
             : QVariantMap();
}

bool GameEngine::marketplace_buy_resource(const QString& resource_key) {
  return marketplace_trade(resource_key, Game::Command::TradeDirection::Buy);
}

bool GameEngine::marketplace_sell_resource(const QString& resource_key) {
  return marketplace_trade(resource_key, Game::Command::TradeDirection::Sell);
}

auto GameEngine::marketplace_trade(const QString& resource_key,
                                   Game::Command::TradeDirection direction) -> bool {
  ensure_initialized();

  QVariantMap const market_state = get_selected_marketplace_state();
  if (!market_state.value("has_marketplace").toBool()) {
    set_error(tr("Select your marketplace to trade."));
    return false;
  }

  auto const resource_type = marketplace_trade_resource_from_key(resource_key);
  if (!resource_type.has_value()) {
    set_error(tr("Marketplace can trade only wood, stone, or iron."));
    return false;
  }

  const bool buying = direction == Game::Command::TradeDirection::Buy;
  auto& marketplace = m_session->marketplace();
  const bool allowed =
      buying ? marketplace.can_buy(*m_world, m_runtime.local_owner_id, *resource_type)
             : marketplace.can_sell(*m_world, m_runtime.local_owner_id, *resource_type);
  if (!allowed) {
    set_error(buying ? tr("Not enough gold to buy %1.")
                           .arg(marketplace_trade_resource_label(resource_key))
                     : tr("Not enough %1 to sell.")
                           .arg(marketplace_trade_resource_label(resource_key)));
    return false;
  }

  Game::Command::submit(
      *m_world,
      Game::Command::Source::LocalPlayer,
      m_runtime.local_owner_id,
      Game::Command::Trade{.resource = *resource_type, .direction = direction});

  clear_error();
  sync_selected_player_state();
  return true;
}

auto GameEngine::get_controlled_commander_status() const -> QVariantMap {
  App::Core::CommanderStatusInput input;
  input.world = m_world;
  input.controlled_commander_id = m_controlled_commander_id;
  input.dodge_active = m_commander_control.is_dodge_rolling();
  input.locked_target_id = m_commander_control.locked_target_id();
  input.rally_placing = is_placing_commander_rally();
  input.get_unit_info = [this](Engine::Core::EntityID id,
                               QString& name,
                               int& health,
                               int& max_health,
                               bool& is_building,
                               bool& alive,
                               QString& nation) {
    return get_unit_info(id, name, health, max_health, is_building, alive, nation);
  };
  input.get_unit_stamina_info = [this](Engine::Core::EntityID id,
                                       float& stamina_ratio,
                                       bool& is_running,
                                       bool& can_run) {
    return get_unit_stamina_info(id, stamina_ratio, is_running, can_run);
  };
  return App::Core::build_controlled_commander_status(input);
}

void GameEngine::record_combat_hit(const Engine::Core::CombatHitEvent& e) {
  if (m_world == nullptr || m_level.is_spectator_mode) {
    return;
  }
  auto* target_ent = m_world->get_entity(e.target_id);
  if (target_ent == nullptr) {
    return;
  }
  const auto* tf = target_ent->get_component<Engine::Core::TransformComponent>();
  const auto* target_unit = target_ent->get_component<Engine::Core::UnitComponent>();
  if (tf == nullptr || target_unit == nullptr) {
    return;
  }
  const bool target_visible = [&]() {
    if (target_unit->owner_id == m_runtime.local_owner_id ||
        m_visibility_coordinator == nullptr) {
      return true;
    }
    const auto snapshot = m_visibility_coordinator->current_snapshot();
    if (snapshot == nullptr || !snapshot->initialized) {
      return true;
    }
    return Game::Map::should_render_non_local_unit(
        *snapshot, tf->position.x, tf->position.z);
  }();
  if (!target_visible) {
    return;
  }

  int attacker_owner = 0;
  if (auto* attacker_ent = m_world->get_entity(e.attacker_id)) {
    if (const auto* attacker_unit =
            attacker_ent->get_component<Engine::Core::UnitComponent>()) {
      attacker_owner = attacker_unit->owner_id;
    }
  }

  App::Core::CombatHitFeedback hit;
  hit.target = e.target_id;
  const bool is_building = target_ent->has_component<Engine::Core::BuildingComponent>();
  hit.x = tf->position.x;
  hit.y = tf->position.y + (is_building ? std::max(2.4F, tf->scale.y * 0.9F) : 1.8F);
  hit.z = tf->position.z;
  hit.damage = e.damage;
  hit.damage_ratio = target_unit->max_health > 0
                         ? std::clamp(static_cast<float>(e.damage) /
                                          static_cast<float>(target_unit->max_health),
                                      0.0F,
                                      1.5F)
                         : 0.0F;
  hit.killing_blow = e.is_killing_blow;
  hit.incoming = target_unit->owner_id == m_runtime.local_owner_id;
  hit.outgoing = attacker_owner == m_runtime.local_owner_id;
  if (!hit.incoming && !hit.outgoing) {
    return;
  }
  if (auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>()) {
    const auto& selected = selection_system->get_selected_units();
    hit.focused =
        selection_system->inspected_entity() == e.target_id ||
        std::find(selected.begin(), selected.end(), e.target_id) != selected.end() ||
        App::Core::primary_attack_target(m_world, selected) == e.target_id;
  }
  m_combat_feedback.push(hit);
}

auto GameEngine::pop_combat_damage_events() -> QVariantList {
  return App::Core::CombatFeedbackStore::to_variant(m_combat_feedback.pop_ready());
}

auto GameEngine::pop_rpg_damage_events() -> QVariantList {
  QVariantList list;
  list.reserve(static_cast<int>(m_rpg_damage_events.size()));
  for (const auto& ev : m_rpg_damage_events) {
    QVariantMap m;
    m["x"] = static_cast<double>(ev.wx);
    m["y"] = static_cast<double>(ev.wy);
    m["z"] = static_cast<double>(ev.wz);
    m["damage"] = ev.damage;
    m["damageRatio"] = static_cast<double>(ev.damage_ratio);
    m["lane"] = ev.lane;
    m["killingBlow"] = ev.killing_blow;
    list.append(m);
  }
  m_rpg_damage_events.clear();
  return list;
}

auto GameEngine::rpg_project_world(float x, float y, float z) const -> QVariantMap {
  QVariantMap result;
  result["valid"] = false;
  result["x"] = 0.0;
  result["y"] = 0.0;
  QPointF screen;
  if (world_to_screen(QVector3D(x, y, z), screen)) {
    result["valid"] = true;
    result["x"] = screen.x();
    result["y"] = screen.y();
  }
  return result;
}

auto GameEngine::get_selected_units_command_mode() const -> QString {
  App::Core::ActionContext context;
  context.world = m_world;
  context.cursor_mode =
      m_cursor_manager != nullptr ? m_cursor_manager->mode() : CursorMode::Normal;
  context.placing_construction = m_production_manager != nullptr &&
                                 m_production_manager->is_placing_construction();
  context.pending_builder_construction_type =
      m_production_manager != nullptr
          ? m_production_manager->pending_builder_construction_type()
          : QString();
  context.placing_formation =
      m_command_controller != nullptr && m_command_controller->is_placing_formation();
  context.has_patrol_first_waypoint = m_command_controller != nullptr &&
                                      m_command_controller->has_patrol_first_waypoint();
  return App::Core::get_current_action_mode(context);
}

auto GameEngine::get_selected_units_toggle_state(const QString& mode) const -> QString {
  return App::Core::get_toggle_state(m_world, mode);
}

auto GameEngine::get_selected_units_mode_availability() const -> QVariantMap {
  return App::Core::get_mode_availability(m_world);
}

auto GameEngine::get_hud_action_states() const -> QVariantMap {
  App::Core::ActionContext context;
  context.world = m_world;
  context.cursor_mode =
      m_cursor_manager != nullptr ? m_cursor_manager->mode() : CursorMode::Normal;
  context.placing_construction = m_production_manager != nullptr &&
                                 m_production_manager->is_placing_construction();
  context.pending_builder_construction_type =
      m_production_manager != nullptr
          ? m_production_manager->pending_builder_construction_type()
          : QString();
  context.placing_formation =
      m_command_controller != nullptr && m_command_controller->is_placing_formation();
  context.has_patrol_first_waypoint = m_command_controller != nullptr &&
                                      m_command_controller->has_patrol_first_waypoint();
  return App::Core::get_action_states(context);
}

auto GameEngine::get_unit_activity_state(Engine::Core::EntityID id) const
    -> Game::Systems::UnitActivity {
  if (m_world == nullptr) {
    return {};
  }
  return Game::Systems::classify_unit_activity(*m_world, id);
}

namespace {

auto activity_to_variant(const Game::Systems::UnitActivity& activity) -> QVariantMap {
  const auto kind = Game::Systems::activity_kind_id(activity.kind);
  const auto state = Game::Systems::activity_state_id(activity.state);
  QVariantMap result;
  result[QStringLiteral("activity")] =
      QString::fromUtf8(kind.data(), static_cast<int>(kind.size()));
  result[QStringLiteral("state")] =
      QString::fromUtf8(state.data(), static_cast<int>(state.size()));
  result[QStringLiteral("queued")] = activity.queued_orders;
  return result;
}

} // namespace

auto GameEngine::unit_activity(qulonglong unit_id) const -> QVariantMap {
  return activity_to_variant(
      get_unit_activity_state(static_cast<Engine::Core::EntityID>(unit_id)));
}

auto GameEngine::selection_activity_summary() const -> QVariantMap {
  QVariantMap summary;
  summary[QStringLiteral("activity")] = QStringLiteral("idle");
  summary[QStringLiteral("state")] = QStringLiteral("active");
  summary[QStringLiteral("count")] = 0;
  summary[QStringLiteral("total")] = 0;
  summary[QStringLiteral("mixed")] = false;
  if (m_world == nullptr) {
    return summary;
  }

  std::vector<Engine::Core::EntityID> selected;
  get_selected_unit_ids(selected);
  if (selected.empty()) {
    return summary;
  }

  std::map<std::pair<QString, QString>, int> tally;
  for (const auto id : selected) {
    const auto entry = activity_to_variant(get_unit_activity_state(id));
    tally[{entry[QStringLiteral("activity")].toString(),
           entry[QStringLiteral("state")].toString()}] += 1;
  }

  auto best = tally.begin();
  for (auto it = tally.begin(); it != tally.end(); ++it) {
    if (it->second > best->second) {
      best = it;
    }
  }

  summary[QStringLiteral("activity")] = best->first.first;
  summary[QStringLiteral("state")] = best->first.second;
  summary[QStringLiteral("count")] = best->second;
  summary[QStringLiteral("total")] = static_cast<int>(selected.size());
  summary[QStringLiteral("mixed")] = tally.size() > 1;
  return summary;
}

void GameEngine::set_rally_at_screen(qreal sx, qreal sy) {
  ensure_initialized();
  if (m_production_manager) {
    m_production_manager->set_rally_at_screen(
        sx, sy, m_runtime.local_owner_id, m_viewport);
  }
}

void GameEngine::start_loading_maps() {
  m_available_maps.clear();
  if (m_map_catalog) {
    m_map_catalog->load_maps_async();
  }
  load_campaigns();
}

auto GameEngine::available_maps() const -> QVariantList {
  return m_available_maps;
}

auto GameEngine::available_nations() const -> QVariantList {
  QVariantList nations;
  const auto& registry = Game::Systems::NationRegistry::instance();
  const auto& all = registry.get_all_nations();
  QList<QVariantMap> ordered;
  ordered.reserve(static_cast<int>(all.size()));
  for (const auto& nation : all) {
    if (!nation.playable || !nation.selectable_in_skirmish) {
      continue;
    }
    QVariantMap entry;
    entry.insert(QStringLiteral("id"),
                 QString::fromStdString(Game::Systems::nation_id_to_string(nation.id)));
    entry.insert(
        QStringLiteral("name"),
        Game::Util::tr_asset(Game::Util::k_nations_context, nation.display_name));
    ordered.append(entry);
  }
  std::sort(
      ordered.begin(), ordered.end(), [](const QVariantMap& a, const QVariantMap& b) {
        return a.value(QStringLiteral("name"))
                   .toString()
                   .localeAwareCompare(b.value(QStringLiteral("name")).toString()) < 0;
      });
  for (const auto& entry : ordered) {
    nations.append(entry);
  }
  return nations;
}

auto GameEngine::available_commanders(const QString& nation_id) const -> QVariantList {
  QVariantList commanders;
  const auto parsed_nation =
      Game::Systems::nation_id_from_string(nation_id.toStdString());
  const auto nation = parsed_nation.value_or(
      Game::Systems::NationRegistry::instance().default_nation_id());
  const QString default_troop =
      App::Core::resolve_commander_troop(nation_id, std::nullopt);
  auto definitions = Game::Units::commander_definitions_for_nation(nation);
  std::stable_sort(
      definitions.begin(),
      definitions.end(),
      [&default_troop](const Game::Units::CommanderDefinition* lhs,
                       const Game::Units::CommanderDefinition* rhs) {
        const bool lhs_default =
            lhs != nullptr &&
            QString::fromStdString(Game::Units::troop_typeToString(lhs->troop_type)) ==
                default_troop;
        const bool rhs_default =
            rhs != nullptr &&
            QString::fromStdString(Game::Units::troop_typeToString(rhs->troop_type)) ==
                default_troop;
        return lhs_default && !rhs_default;
      });
  for (const auto* definition : definitions) {
    if (definition == nullptr) {
      continue;
    }
    const QString troop =
        QString::fromStdString(Game::Units::troop_typeToString(definition->troop_type));
    commanders.append(build_available_commander_entry(
        *definition, troop.compare(default_troop, Qt::CaseInsensitive) == 0));
  }
  return commanders;
}

auto GameEngine::available_campaigns() const -> QVariantList {
  return m_campaign_manager ? m_campaign_manager->available_campaigns()
                            : QVariantList();
}

void GameEngine::load_campaigns() {
  if (m_save_load_service == nullptr) {
    return;
  }

  QString error;
  auto campaigns = m_save_load_service->list_campaigns(&error);
  if (!error.isEmpty()) {
    qWarning() << "Failed to load campaigns:" << error;
    return;
  }

  if (m_campaign_manager) {
    m_campaign_manager->set_available_campaigns(campaigns);
  }
}

void GameEngine::start_campaign_mission(const QString& mission_path) {
  clear_error();

  if (!m_campaign_manager) {
    set_error(tr("Campaign manager not initialized"));
    return;
  }

  m_selected_player_id = 1;

  m_campaign_manager->start_campaign_mission(mission_path, m_selected_player_id);

  if (!m_campaign_manager->current_mission_definition().has_value()) {
    set_error(tr("Failed to load mission"));
    return;
  }

  const auto& mission = *m_campaign_manager->current_mission_definition();
  m_replay_launch = {QStringLiteral("campaign-mission"), mission_path, {}};
  start_skirmish_internal(
      mission.map_path, build_campaign_player_configs(mission), false);
}

void GameEngine::start_mission_file(const QString& file_path) {
  clear_error();
  if (!m_campaign_manager) {
    set_error(tr("Campaign manager not initialized"));
    return;
  }

  QString error;
  if (!m_campaign_manager->start_mission_file(
          file_path, m_selected_player_id, &error)) {
    set_error(tr("Failed to load mission preview: %1").arg(error));
    return;
  }

  const auto& mission = *m_campaign_manager->current_mission_definition();
  m_replay_launch = {QStringLiteral("mission-file"), file_path, {}};
  start_skirmish_internal(
      mission.map_path, build_campaign_player_configs(mission), false);
}

void GameEngine::mark_current_mission_completed() {
  if (!m_campaign_manager) {
    return;
  }

  if (m_campaign_manager->current_campaign_id().isEmpty()) {
    qWarning() << "No active campaign mission to mark as completed";
    return;
  }

  if (m_save_load_service == nullptr) {
    qWarning() << "Save/Load service not initialized";
    return;
  }

  m_campaign_manager->mark_current_mission_completed();
  load_campaigns();
}

QVariantMap GameEngine::get_current_mission_objectives() const {
  if (!m_campaign_manager) {
    return {};
  }

  const auto& mission_def = m_campaign_manager->current_mission_definition();
  if (!mission_def.has_value()) {
    return {};
  }

  return build_mission_objectives_map(*mission_def);
}

QVariantMap GameEngine::get_mission_definition(const QString& mission_id) const {
  return load_mission_definition_map(mission_id);
}

void GameEngine::start_skirmish(const QString& map_path,
                                const QVariantList& player_configs) {
  m_replay_launch = {QStringLiteral("skirmish"), map_path, player_configs};
  start_skirmish_internal(map_path, player_configs, true);
}

void GameEngine::set_replay_record_path(const QString& path) {
  m_replay_record_path = path;
}

auto GameEngine::replay_playing() const -> bool {
  return m_session != nullptr && m_session->replay_player() != nullptr;
}

auto GameEngine::start_replay(const QString& path) -> bool {
  QString error;
  auto file = Game::Command::ReplayFile::load(path, &error);
  if (!file.has_value()) {
    set_error(tr("Cannot play replay: %1").arg(error));
    return false;
  }
  const Game::Command::ReplayHeader header = file->header;
  m_pending_replay = std::move(file);
  if (header.kind == QLatin1String("campaign-mission")) {
    start_campaign_mission(header.reference);
  } else if (header.kind == QLatin1String("mission-file")) {
    start_mission_file(header.reference);
  } else if (header.kind == QLatin1String("skirmish")) {
    start_skirmish(
        header.reference,
        header.launch.value(QLatin1String("player_configs")).toArray().toVariantList());
  } else {
    m_pending_replay.reset();
    set_error(tr("Cannot play replay: unknown launch kind '%1'").arg(header.kind));
    return false;
  }
  return true;
}

void GameEngine::arm_replay_for_started_match() {
  if (m_session == nullptr) {
    return;
  }
  if (m_pending_replay.has_value()) {
    auto file = std::move(*m_pending_replay);
    m_pending_replay.reset();
    qInfo() << "Replay: driving" << m_replay_launch.kind << m_replay_launch.reference
            << "from" << file.commands.size() << "commands, last tick"
            << file.last_tick();
    m_session->set_replay_player(
        std::make_unique<Game::Command::ReplayPlayer>(std::move(file)));
    return;
  }
  if (m_replay_record_path.isEmpty()) {
    return;
  }
  Game::Command::ReplayHeader header;
  header.kind = m_replay_launch.kind;
  header.reference = m_replay_launch.reference;
  header.launch["player_configs"] =
      QJsonArray::fromVariantList(m_replay_launch.player_configs);
  header.tick_seconds = m_session->clock().tick_seconds();
  header.rng_seed = m_session->rng_seed();
  auto recorder = std::make_unique<Game::Command::ReplayRecorder>();
  if (!recorder->begin(m_replay_record_path, header, m_session->commands())) {
    qWarning() << "Replay: cannot write" << m_replay_record_path;
    return;
  }
  qInfo() << "Replay: recording to" << m_replay_record_path;
  m_session->set_replay_recorder(std::move(recorder));
}

void GameEngine::start_skirmish_internal(const QString& map_path,
                                         const QVariantList& player_configs,
                                         bool set_skirmish_context) {

  clear_error();
  reset_preload_interaction_state();
  reset_mission_runtime_state();

  m_level.map_path = map_path;
  m_level.map_name = map_path;

  if (m_campaign_manager && set_skirmish_context) {
    m_campaign_manager->set_skirmish_context(map_path);
  }

  if (!m_runtime.victory_state.isEmpty()) {
    m_runtime.victory_state = "";
    emit victory_state_changed();
  }
  if (m_victory_service) {
    m_victory_service->reset();
  }
  m_enemy_troops_defeated = 0;

  if (!m_runtime.initialized) {
    ensure_initialized();
  }

  if (!m_world || !m_renderer || (m_camera == nullptr) || !m_skirmish_runtime) {
    set_error(tr("Cannot start skirmish: renderer not initialized"));
    return;
  }

  m_finalize_progress_after_overlay = false;
  m_loading_overlay_active = true;
  m_runtime.loading = true;
  emit is_loading_changed();

  if (m_loading_progress_tracker) {
    m_loading_progress_tracker->start_loading();
  }

  QCoreApplication::processEvents(QEventLoop::AllEvents);
  if (m_release_self_test_mode) {

    qInfo() << "SOI_AUDIO_SELF_TEST: mission preload skipped after manifest "
               "validation";
  } else {
    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);
  }
  QTimer::singleShot(50, this, [this, map_path, player_configs]() {
    if (!m_world || !m_renderer || (m_camera == nullptr) || !m_skirmish_runtime) {
      set_error(tr("Cannot start skirmish: renderer not initialized"));
      m_runtime.loading = false;
      emit is_loading_changed();
      return;
    }

    if (m_hover_tracker) {
      m_hover_tracker->update_hover(-1, -1, *m_world, *m_camera, 0, 0);
    }

    const bool allow_default_player_barracks =
        !m_campaign_manager ||
        !m_campaign_manager->current_mission_context().is_campaign();
    const auto load_effects =
        m_skirmish_runtime->perform_load({*m_world,
                                          m_level,
                                          m_entity_cache,
                                          map_path,
                                          player_configs,
                                          m_selected_player_id,
                                          scene_context(),
                                          m_victory_service.get(),
                                          m_minimap_manager.get(),
                                          m_visibility_coordinator.get(),
                                          allow_default_player_barracks,
                                          m_loading_progress_tracker.get(),
                                          [this]() {
                                            emit owner_info_changed();
                                          }});

    if (load_effects.selected_player_changed) {
      m_selected_player_id = load_effects.updated_player_id;
      emit selected_player_id_changed();
    }

    if (!load_effects.success) {
      set_error(load_effects.error);
      m_runtime.loading = false;
      m_loading_overlay_active = false;
      m_loading_overlay_wait_for_first_frame.store(false, std::memory_order_release);
      m_finalize_progress_after_overlay = false;
      m_show_objectives_after_loading = false;
      emit is_loading_changed();
      return;
    }

    m_runtime.local_owner_id = load_effects.updated_player_id;
    m_audio_coordinator->configure_audio_manifest_mappings(m_runtime.local_owner_id);
    const Game::Mission::MissionDefinition* mission_def = nullptr;
    if (m_campaign_manager &&
        m_campaign_manager->current_mission_definition().has_value()) {
      mission_def = &*m_campaign_manager->current_mission_definition();
    }
    const Game::Mission::MissionDefinition* campaign_mission_def = nullptr;
    if (m_campaign_manager &&
        m_campaign_manager->current_mission_context().is_campaign() &&
        m_campaign_manager->current_mission_definition().has_value()) {
      campaign_mission_def = &*m_campaign_manager->current_mission_definition();
    }
    m_audio_coordinator->apply_mission_ambience(
        mission_def, map_path, m_runtime.local_owner_id);

    apply_skirmish_commander_setup(player_configs);
    apply_mission_setup();
    m_skirmish_runtime->initialize_player_resources(
        {m_level, m_runtime.local_owner_id, campaign_mission_def});
    configure_mission_victory_conditions();
    configure_rain_system();
    if (m_environment_clock) {
      m_environment_clock->reset(m_level.environment);
    }

    const auto finalize_effects =
        m_skirmish_runtime->finalize_load({m_runtime.loading,
                                           m_loading_overlay_wait_for_first_frame,
                                           m_loading_overlay_frames_remaining,
                                           m_loading_overlay_min_duration_ms,
                                           m_loading_overlay_timer,
                                           m_finalize_progress_after_overlay,
                                           m_show_objectives_after_loading,
                                           is_campaign_mission()});

    if (finalize_effects.emit_is_loading_changed) {
      emit is_loading_changed();
    }
    if (finalize_effects.rebuild_entity_cache) {
      GameStateRestorer::rebuild_entity_cache(
          m_world, m_entity_cache, m_runtime.local_owner_id);
    }
    if (finalize_effects.emit_troop_count_changed) {
      emit troop_count_changed();
    }
    if (finalize_effects.sync_scatter_world_props) {
      sync_scatter_world_props();
    }
    if (finalize_effects.sync_selected_player_state) {
      sync_selected_player_state();
    }
    if (finalize_effects.reset_ambient_state) {
      m_ambient_state_manager = std::make_unique<AmbientStateManager>();
      Engine::Core::EventManager::instance().publish(
          Engine::Core::AmbientStateChangedEvent(Engine::Core::AmbientState::PEACEFUL,
                                                 Engine::Core::AmbientState::PEACEFUL));
    }
    if (finalize_effects.apply_spectator_mode && m_input_handler) {
      m_input_handler->set_spectator_mode(m_level.is_spectator_mode);
    }
    if (finalize_effects.emit_owner_info_changed) {
      emit owner_info_changed();
    }
    if (finalize_effects.emit_spectator_mode_changed) {
      emit spectator_mode_changed();
    }
    arm_replay_for_started_match();
  });
}

void GameEngine::apply_mission_setup() {
  if (!m_world || !m_campaign_manager || !m_mission_setup || !m_skirmish_runtime) {
    return;
  }

  auto effects = m_mission_setup->apply_mission_setup({*m_world,
                                                       *m_campaign_manager,
                                                       m_level,
                                                       m_selected_player_id,
                                                       m_runtime.local_owner_id,
                                                       m_pending_mission_waves,
                                                       m_entity_cache});
  m_mission_wave_director.bind(&m_pending_mission_waves, m_world);
  m_mission_wave_director.set_elapsed(m_campaign_mission_elapsed);
  if (m_campaign_manager->current_mission_definition().has_value()) {
    m_pending_mission_events = App::Core::build_pending_mission_events(
        *m_campaign_manager->current_mission_definition());
  }
  if (m_victory_service) {
    m_victory_service->set_mission_wave_query(&m_mission_wave_director);
  }
  if (effects.selected_player_changed) {
    emit selected_player_id_changed();
  }
  if (effects.center_camera_on_local_forces) {
    m_skirmish_runtime->center_camera_on_local_forces(
        {m_world, m_camera, m_runtime.local_owner_id});
  }
  if (effects.troop_count_changed) {
    emit troop_count_changed();
  }
  if (effects.owner_info_changed) {
    emit owner_info_changed();
  }
}

void GameEngine::configure_mission_victory_conditions() {
  if (!m_campaign_manager || !m_victory_service) {
    return;
  }

  m_campaign_manager->configure_mission_victory_conditions(m_victory_service.get(),
                                                           m_runtime.local_owner_id);

  m_victory_service->set_victory_callback([this](const QString& state) {
    if (m_runtime.victory_state != state) {
      m_audio_coordinator->ensure_result_audio_ready(state, m_runtime.local_owner_id);
      if (state == "defeat") {
        Game::Audio::play_cue(Game::Audio::Cue::k_alert_objective_failed);
      }
      m_runtime.victory_state = state;
      emit victory_state_changed();

      if (state == "victory" && !m_campaign_manager->current_campaign_id().isEmpty()) {
        mark_current_mission_completed();
      }
    }
  });
}

void GameEngine::configure_rain_system() {
  if (m_rain_manager) {
    m_rain_manager->configure(m_level.rain, m_level.biome_seed);
  }

  if (m_weather_audio) {
    m_weather_audio->stop();
    if (m_level.rain.enabled) {
      m_weather_audio->preload(m_level.rain.type);
    }
  }

  if (!m_rain) {
    return;
  }

  const float world_width = static_cast<float>(m_level.grid_width) * m_level.tile_size;
  const float world_height =
      static_cast<float>(m_level.grid_height) * m_level.tile_size;
  m_rain->configure(world_width, world_height, m_level.biome_seed, m_level.rain.type);
  m_rain->set_enabled(m_level.rain.enabled);
  m_rain->set_wind_strength(m_level.rain.wind_strength);
  m_rain->set_wind_direction_deg(m_level.rain.wind_direction_deg);

  const float initial_intensity =
      m_rain_manager ? m_rain_manager->get_intensity()
                     : (m_level.rain.enabled ? m_level.rain.intensity : 0.0F);
  m_rain->set_intensity(initial_intensity);
}

void GameEngine::reset_preload_interaction_state() {
  m_saved_rts_selection_ids.clear();
  if (m_command_controller) {
    m_command_controller->reset_transient_state();
  }

  if (m_production_manager) {
    m_production_manager->reset_transient_state();
  }

  if (m_world) {
    if (auto* selection_system =
            m_world->get_system<Game::Systems::SelectionSystem>()) {
      selection_system->clear_selection();
    }
  }

  if (m_renderer) {
    m_renderer->set_selected_entities({});
    m_renderer->set_hovered_entity_id(0);
  }

  if (m_hover_tracker && m_world && (m_camera != nullptr)) {
    m_hover_tracker->update_hover(-1, -1, *m_world, *m_camera, 0, 0);
  }

  if (m_cursor_manager && m_cursor_manager->mode() != CursorMode::Normal) {
    set_cursor_mode(CursorMode::Normal);
  }

  m_follow_selection_enabled = false;
  m_runtime.selection_refresh_counter = 0;
  m_runtime.minimap_unit_update_accumulator = 0.0F;

  emit selected_units_changed();
}

void GameEngine::reset_mission_runtime_state() {
  m_campaign_mission_elapsed = 0.0F;
  m_runtime.minimap_unit_update_accumulator = 0.0F;
  m_pending_mission_waves.clear();
  m_pending_mission_events.clear();
  m_mission_wave_director.reset();
  if (m_wave_view_model) {
    m_wave_view_model->clear();
  }
  Game::Systems::PlayerResourceRegistry::instance().clear();
  sync_selected_player_state();
  m_audio_coordinator->stop_mission_ambience();
  AudioSystem::get_instance().stop_music();
  AudioResourceLoader::unload_audio_resources(AudioLoadPolicy::Mission);
  AudioResourceLoader::unload_audio_resources(AudioLoadPolicy::Lazy);
}

void GameEngine::update_mission_events() {
  if (m_pending_mission_events.empty()) {
    return;
  }
  for (auto& event : m_pending_mission_events) {
    if (event.fired || m_campaign_mission_elapsed < event.trigger_time) {
      continue;
    }
    event.fired = true;
    emit mission_announcement(
        Game::Util::tr_asset(Game::Util::k_missions_context, event.text));
  }
}

void GameEngine::update_mission_waves(float dt) {
  if (dt <= 0.0F || !m_world || !m_mission_setup ||
      !m_runtime.victory_state.isEmpty()) {
    return;
  }

  m_campaign_mission_elapsed += dt;
  update_mission_events();

  if (m_pending_mission_waves.empty()) {
    return;
  }

  m_mission_wave_director.set_elapsed(m_campaign_mission_elapsed);
  const auto effects = m_mission_wave_director.advance();

  bool spawned_any = false;
  for (const auto index : effects.waves_to_spawn) {
    const auto spawn_effects =
        m_mission_setup->spawn_wave({*m_world, m_level, m_campaign_mission_elapsed},
                                    m_pending_mission_waves[index]);
    for (const auto& announcement : spawn_effects.mission_announcements) {
      emit mission_announcement(announcement);
    }
    m_mission_wave_director.note_spawned(index, spawn_effects.spawned_entity_ids);
    spawned_any = true;
  }

  for (const auto& announcement : effects.announcements) {
    emit mission_announcement(announcement);
  }
  for (const auto& cue : effects.audio_cues) {
    Game::Audio::play_cue(cue.toStdString());
  }

  if (!effects.reward.empty()) {
    auto& resources = Game::Systems::PlayerResourceRegistry::instance();
    for (const auto type : Game::Systems::k_all_resource_types) {
      const int amount = effects.reward.get(type);
      if (amount > 0) {
        resources.add(m_runtime.local_owner_id, type, amount);
      }
    }
    sync_selected_player_state();
  }

  if (effects.status_changed || spawned_any) {
    publish_wave_status();
  }
  if (spawned_any) {
    emit owner_info_changed();
  }
}

void GameEngine::restore_mission_waves(const QJsonObject& wave_state) {
  m_pending_mission_waves.clear();
  m_mission_wave_director.reset();

  if (!m_world || !m_campaign_manager ||
      !m_campaign_manager->current_mission_definition().has_value()) {
    return;
  }

  const auto& mission = *m_campaign_manager->current_mission_definition();

  const QVector3D defense_reference =
      App::Core::resolve_defense_reference(*m_world, m_runtime.local_owner_id);

  m_pending_mission_waves = App::Core::build_pending_mission_waves(
      {.mission = mission,
       .mission_difficulty = m_campaign_manager->current_mission_context().difficulty,
       .level = m_level,
       .defense_reference_world_position = defense_reference});

  m_pending_mission_events = App::Core::build_pending_mission_events(mission);

  m_mission_wave_director.bind(&m_pending_mission_waves, m_world);
  m_mission_wave_director.restore(wave_state);
  m_campaign_mission_elapsed = m_mission_wave_director.elapsed();
  for (auto& event : m_pending_mission_events) {
    event.fired = m_campaign_mission_elapsed >= event.trigger_time;
  }

  if (m_victory_service) {
    m_victory_service->set_mission_wave_query(&m_mission_wave_director);
  }
  publish_wave_status();
}

void GameEngine::publish_wave_status() {
  if (!m_wave_view_model) {
    return;
  }

  QVariantMap status = m_mission_wave_director.status();
  QVariantList alerts = status.value("alerts").toList();
  if (!alerts.isEmpty() && m_minimap_manager && m_minimap_manager->has_minimap()) {
    const float world_width = m_minimap_manager->get_world_width();
    const float world_height = m_minimap_manager->get_world_height();
    QVariantList normalized;
    for (const auto& value : alerts) {
      QVariantMap alert = value.toMap();
      const auto [nx, ny] =
          Game::Map::Minimap::world_to_pixel(alert.value("x").toFloat(),
                                             alert.value("z").toFloat(),
                                             world_width,
                                             world_height,
                                             1.0F,
                                             1.0F);
      alert["nx"] = std::clamp(nx, 0.0F, 1.0F);
      alert["ny"] = std::clamp(ny, 0.0F, 1.0F);
      normalized.append(alert);
    }
    status["alerts"] = normalized;
  }

  m_wave_view_model->set_status(status);
}

void GameEngine::apply_skirmish_commander_setup(const QVariantList& player_configs) {
  if (!m_world || !m_mission_setup) {
    return;
  }

  const auto effects = m_mission_setup->apply_skirmish_commander_setup(
      {*m_world, m_campaign_manager.get(), m_level, m_runtime.local_owner_id},
      player_configs);
  for (const auto& announcement : effects.mission_announcements) {
    emit mission_announcement(announcement);
  }
}

void GameEngine::open_settings() {
  if (m_save_load_service != nullptr) {
    Game::Systems::SaveLoadService::open_settings();
  }
}

void GameEngine::connect_save_service_signals() {
  if (m_save_load_service == nullptr) {
    return;
  }

  connect(
      m_save_load_service,
      &Game::Systems::SaveLoadService::save_progress,
      this,
      [this](
          quint64 job_id, const QString& slot_name, int percent, const QString& stage) {
        if (job_id != m_active_save_job) {
          return;
        }
        m_save_progress_slot = slot_name;
        m_save_progress_percent = percent;
        m_save_progress_stage = stage;
        emit save_progress_changed();
      });

  connect(m_save_load_service,
          &Game::Systems::SaveLoadService::save_finished,
          this,
          [this](quint64 job_id,
                 const QString& slot_name,
                 bool success,
                 const QString& error) {
            if (job_id == m_active_save_job) {
              m_active_save_job = 0;
              m_save_progress_percent = success ? 100 : 0;
              m_save_progress_stage.clear();
              emit save_progress_changed();
            }
            if (!success) {
              set_error(error);
              Game::Audio::play_cue(Game::Audio::Cue::k_ui_error);
            } else {
              Game::Audio::play_cue(Game::Audio::Cue::k_state_save_complete);
            }
            emit save_completed(slot_name, success, error);
          });

  connect(m_save_load_service,
          &Game::Systems::SaveLoadService::save_slots_changed,
          this,
          &GameEngine::save_slots_changed);

  connect(&m_autosave_timer, &QTimer::timeout, this, &GameEngine::autosave);
  restart_autosave_timer();
}

void GameEngine::restart_autosave_timer() {
  const int minutes = m_save_slots_view_model->autosave_interval_minutes();
  if (minutes <= 0) {
    m_autosave_timer.stop();
    return;
  }
  m_autosave_timer.setInterval(minutes * 60 * 1000);
  m_autosave_timer.start();
}

void GameEngine::begin_save(const QString& slot_name,
                            Game::Systems::Save::SlotKind kind,
                            int autosave_retention) {
  if ((m_save_load_service == nullptr) || !m_world) {
    set_error(tr("Save: not initialized"));
    return;
  }

  if (m_active_save_job != 0) {
    set_error(tr("A save is already in progress"));
    return;
  }

  if (m_control_mode == PlayerControlMode::Commander) {
    request_exit_commander_control_mode();
  }
  const Game::Systems::RuntimeSnapshot runtime_snapshot = to_runtime_snapshot();
  Game::Systems::LevelSnapshot level_snapshot = m_level;
  if (m_environment_clock) {
    level_snapshot.environment = m_environment_clock->definition();
    level_snapshot.environment_clock = m_environment_clock->snapshot();
  }
  if (m_rain_manager) {
    level_snapshot.weather_runtime = m_rain_manager->snapshot();
  }
  std::optional<Game::Mission::MissionContext> mission_context;
  if (m_campaign_manager) {
    mission_context = m_campaign_manager->current_mission_context();
  }

  const App::Core::SaveToSlotEffects effects =
      m_save_load_coordinator->begin_save_to_slot(
          {.world = *m_world,
           .save_load_service = *m_save_load_service,
           .camera = m_camera,
           .level = level_snapshot,
           .runtime_snapshot = runtime_snapshot,
           .slot = slot_name,
           .title = slot_name,
           .map_name = m_level.map_name,
           .mission_context = std::move(mission_context),
           .kind = kind,
           .play_time_seconds = m_campaign_mission_elapsed,
           .autosave_retention = autosave_retention,
           .mission_wave_state = m_mission_wave_director.serialize()});
  if (!effects.queued) {
    set_error(effects.error);
    return;
  }

  m_active_save_job = effects.job_id;
  m_save_progress_slot = slot_name;
  m_save_progress_percent = 0;
  m_save_progress_stage = tr("Queued");
  emit save_progress_changed();

  m_screenshot_target_slot = slot_name;
  m_screenshot_requested.store(true, std::memory_order_release);
}

void GameEngine::save_game_to_slot(const QString& slot_name) {
  begin_save(slot_name, Game::Systems::Save::SlotKind::Manual, 0);
}

void GameEngine::quicksave() {
  begin_save(QStringLiteral("quicksave"), Game::Systems::Save::SlotKind::Quicksave, 0);
}

void GameEngine::autosave() {

  if ((m_save_load_service == nullptr) || !m_world || !m_runtime.initialized ||
      m_runtime.loading || m_level.map_path.isEmpty() ||
      !m_runtime.victory_state.isEmpty() || m_active_save_job != 0) {
    return;
  }

  const int retention = m_save_slots_view_model->autosave_slot_count();
  begin_save(m_save_load_service->next_autosave_slot(retention),
             Game::Systems::Save::SlotKind::Autosave,
             retention);
}

void GameEngine::cancel_active_save() {
  if (m_active_save_job == 0 || (m_save_load_service == nullptr)) {
    return;
  }
  m_save_load_service->cancel_save(m_active_save_job);
  m_save_progress_stage = tr("Cancelling...");
  emit save_progress_changed();
}

void GameEngine::load_game_from_slot(const QString& slot_name) {
  if ((m_save_load_service == nullptr) || !m_world) {
    set_error(tr("Load: not initialized"));
    return;
  }

  if (m_control_mode == PlayerControlMode::Commander) {
    request_exit_commander_control_mode();
  }

  reset_preload_interaction_state();
  reset_mission_runtime_state();

  m_finalize_progress_after_overlay = false;
  m_loading_overlay_active = true;
  m_runtime.loading = true;
  emit is_loading_changed();

  Game::Systems::RuntimeSnapshot runtime_snapshot = to_runtime_snapshot();
  const App::Core::LoadFromSlotEffects effects =
      m_save_load_coordinator->load_from_slot(
          {.world = *m_world,
           .save_load_service = *m_save_load_service,
           .slot = slot_name,
           .campaign_manager = m_campaign_manager.get(),
           .level = m_level,
           .camera = m_camera,
           .viewport_width = m_viewport.width,
           .viewport_height = m_viewport.height,
           .runtime_snapshot = runtime_snapshot,
           .apply_runtime_snapshot =
               [this](const Game::Systems::RuntimeSnapshot& snapshot) {
                 apply_runtime_snapshot(snapshot);
               },
           .selected_player_id = m_selected_player_id,
           .scene = scene_context(),
           .entity_cache = m_entity_cache,
           .audio_coordinator = m_audio_coordinator.get(),
           .victory_service = m_victory_service.get(),
           .emit_troop_count_changed = [this]() { emit troop_count_changed(); },
           .restore_mission_waves =
               [this](const QJsonObject& wave_state) {
                 restore_mission_waves(wave_state);
               }});
  if (!effects.success) {
    set_error(effects.error);
    m_runtime.loading = false;
    m_loading_overlay_active = false;
    m_loading_overlay_wait_for_first_frame.store(false, std::memory_order_release);
    m_finalize_progress_after_overlay = false;
    m_show_objectives_after_loading = false;
    emit is_loading_changed();
    return;
  }
  if (m_environment_clock) {
    m_environment_clock->restore(m_level.environment, m_level.environment_clock);
    if (m_renderer) {
      m_renderer->set_environment_lighting(m_environment_clock->lighting());
    }
  }

  if (m_rain_manager) {

    configure_rain_system();
    m_rain_manager->restore(m_level.weather_runtime);
    if (m_rain) {
      m_rain->set_intensity(m_rain_manager->get_intensity());
    }
  }

  sync_scatter_world_props();

  if (m_camera_controller) {
    m_camera_controller->sync_map_bounds();
  }

  m_runtime.loading = false;
  m_loading_overlay_wait_for_first_frame.store(true, std::memory_order_release);
  m_loading_overlay_frames_remaining = 5;
  m_loading_overlay_last_frame_ms = 0;
  m_loading_overlay_min_duration_ms = 1000;
  m_loading_overlay_timer.restart();
  m_finalize_progress_after_overlay = true;
  emit is_loading_changed();
  qInfo() << "Game load complete, victory/defeat checks re-enabled";
  Game::Audio::play_cue(Game::Audio::Cue::k_state_load_complete);

  emit minimap_image_changed();

  if (effects.emit_selected_units_changed) {
    emit selected_units_changed();
  }
  if (effects.emit_owner_info_changed) {
    emit owner_info_changed();
  }
}

auto GameEngine::to_runtime_snapshot() const -> Game::Systems::RuntimeSnapshot {
  return m_save_load_coordinator->to_runtime_snapshot(
      {.paused = m_runtime.paused,
       .time_scale = m_runtime.time_scale,
       .local_owner_id = m_runtime.local_owner_id,
       .victory_state = m_runtime.victory_state,
       .cursor_mode = m_runtime.cursor_mode,
       .selected_player_id = m_selected_player_id,
       .follow_selection = m_follow_selection_enabled});
}

void GameEngine::apply_runtime_snapshot(
    const Game::Systems::RuntimeSnapshot& snapshot) {
  m_save_load_coordinator->apply_runtime_snapshot(
      snapshot,
      {.paused = m_runtime.paused,
       .time_scale = m_runtime.time_scale,
       .local_owner_id = m_runtime.local_owner_id,
       .victory_state = m_runtime.victory_state,
       .cursor_mode = m_runtime.cursor_mode,
       .selected_player_id = m_selected_player_id,
       .follow_selection = m_follow_selection_enabled});
  if (m_cursor_manager) {
    m_cursor_manager->set_mode(m_runtime.cursor_mode);
  }
  sync_selected_player_state();
}

auto GameEngine::describe_focus_entity(Engine::Core::EntityID id) const
    -> App::Core::FocusTargetInfo {
  App::Core::FocusTargetInfo info;
  if (m_world == nullptr || id == Engine::Core::NULL_ENTITY) {
    return info;
  }
  auto* entity = m_world->get_entity(id);
  const auto* unit = entity != nullptr
                         ? entity->get_component<Engine::Core::UnitComponent>()
                         : nullptr;
  if (unit == nullptr || unit->health <= 0) {
    return info;
  }

  QString name;
  QString nation;
  int health = 0;
  int max_health = 0;
  bool is_building = false;
  bool alive = false;
  if (!get_unit_info(id, name, health, max_health, is_building, alive, nation) ||
      !alive) {
    return info;
  }
  if (is_building) {
    const QString pretty = App::Core::building_display_name(unit->spawn_type);
    if (!pretty.isEmpty()) {
      name = pretty;
    }
  }

  info.valid = true;
  info.id = id;
  info.name = name;
  info.nation = nation;
  get_unit_type_key(id, info.type_key);
  info.owner_id = unit->owner_id;
  info.is_building = is_building;
  info.is_own = unit->owner_id == m_runtime.local_owner_id;
  info.is_enemy =
      !info.is_own &&
      (m_session != nullptr
           ? m_session->owners().are_enemies(m_runtime.local_owner_id, unit->owner_id)
           : true);
  info.health = health;
  info.max_health = max_health;
  info.health_ratio = max_health > 0
                          ? static_cast<double>(std::clamp(health, 0, max_health)) /
                                static_cast<double>(max_health)
                          : 0.0;
  const QVariantMap activity = activity_to_variant(get_unit_activity_state(id));
  info.activity = activity.value(QStringLiteral("activity")).toString();
  info.activity_state = activity.value(QStringLiteral("state")).toString();

  std::vector<Engine::Core::EntityID> selection;
  get_selected_unit_ids(selection);
  info.attacked_by_selection =
      App::Core::count_selection_attacking(m_world, selection, id);
  info.attacked_by_local =
      App::Core::count_units_attacking(m_world, id, m_runtime.local_owner_id);
  info.attackers_incoming =
      App::Core::count_enemies_attacking(m_world, id, m_runtime.local_owner_id);
  return info;
}

void GameEngine::sync_focus_targets() {
  QVariantMap inspect;
  QVariantMap target;
  if (m_world != nullptr) {
    auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
    if (selection_system != nullptr) {
      const auto& selection = selection_system->get_selected_units();
      const auto inspected = selection_system->inspected_entity();
      const auto focus = App::Core::resolve_focus_entity(
          m_world, selection, inspected, m_runtime.local_owner_id);
      if (inspected != Engine::Core::NULL_ENTITY && focus != inspected) {
        selection_system->clear_inspected_entity();
      }
      const auto inspect_info = describe_focus_entity(focus);
      if (inspect_info.valid) {
        inspect = App::Core::focus_target_to_variant(inspect_info);
      }
      const auto primary = App::Core::primary_attack_target(m_world, selection);
      const auto target_info = describe_focus_entity(primary);
      if (target_info.valid) {
        target = App::Core::focus_target_to_variant(target_info);
      }
    }
  }
  if (inspect == m_inspect_target && target == m_selection_target) {
    return;
  }
  m_inspect_target = std::move(inspect);
  m_selection_target = std::move(target);
  if (m_activity_view_model != nullptr) {
    m_activity_view_model->set_focus_targets(m_inspect_target, m_selection_target);
  }
}

void GameEngine::sync_target_focus_markers() {
  m_target_focus.clear();
  if (m_world == nullptr || m_level.is_spectator_mode) {
    return;
  }
  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return;
  }
  const auto snapshot = m_visibility_coordinator != nullptr
                            ? m_visibility_coordinator->current_snapshot()
                            : nullptr;
  Game::Systems::TargetFocusRequest request;
  request.world = m_world;
  request.local_owner_id = m_runtime.local_owner_id;
  request.selection = &selection_system->get_selected_units();
  request.inspected = selection_system->inspected_entity();
  request.max_locked_targets = Game::Systems::k_target_focus_max_locked;
  request.max_incoming_attackers = Game::Systems::k_target_focus_max_incoming;
  request.visibility =
      (snapshot != nullptr && snapshot->initialized) ? snapshot.get() : nullptr;
  request.owners = m_session != nullptr ? &m_session->owners() : nullptr;
  m_target_focus = Game::Systems::collect_target_focus_markers(request);
}

void GameEngine::clear_inspect_target() {
  if (m_world == nullptr) {
    return;
  }
  if (auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>()) {
    if (selection_system->inspected_entity() != Engine::Core::NULL_ENTITY) {
      selection_system->clear_inspected_entity();
      emit selected_units_changed();
    }
  }
  sync_focus_targets();
}

void GameEngine::sync_selected_player_state() {
  int const owner_id =
      m_selected_player_id > 0 ? m_selected_player_id : m_runtime.local_owner_id;
  QVariantMap const next_state =
      build_player_state_map(owner_id, m_level.max_troops_per_player);
  if (m_selected_player_state == next_state) {
    return;
  }
  m_selected_player_state = next_state;
  emit selected_player_state_changed();
  emit owner_info_changed();
}

void GameEngine::sync_scatter_world_props() {
  auto& terrain_service = Game::Map::TerrainService::instance();
  if (m_scatter == nullptr || !terrain_service.is_initialized() ||
      terrain_service.get_height_map() == nullptr) {
    return;
  }

  auto const revision = terrain_service.world_props_revision();
  if (revision == m_last_world_props_revision) {
    return;
  }

  m_scatter->refresh_runtime_world_props(terrain_service.world_props());
  m_last_world_props_revision = revision;
}

auto GameEngine::consume_screenshot_request() -> bool {
  return m_screenshot_requested.exchange(false, std::memory_order_acq_rel);
}

void GameEngine::submit_frame_image(const QImage& image) {
  if (image.isNull()) {
    return;
  }

  QMetaObject::invokeMethod(
      this, [this, image]() { on_frame_image_captured(image); }, Qt::QueuedConnection);
}

void GameEngine::on_frame_image_captured(const QImage& image) {
  const QString slot_name = m_screenshot_target_slot;
  m_screenshot_target_slot.clear();
  if (slot_name.isEmpty() || (m_save_load_service == nullptr) || image.isNull()) {
    return;
  }

  const QByteArray png = Game::Systems::Save::encode_preview(image);
  if (png.isEmpty()) {
    qWarning() << "GameEngine: failed to encode save preview for" << slot_name;
    return;
  }

  m_save_load_service->attach_screenshot(slot_name, png);
}

void GameEngine::exit_game() {
  if (m_save_load_service != nullptr) {
    Game::Systems::SaveLoadService::exit_game();
  }
}

auto GameEngine::get_owner_info() const -> QVariantList {
  QVariantList result;
  const auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  const auto& owners = owner_registry.get_all_owners();

  for (const auto& owner : owners) {
    QVariantMap owner_map;
    owner_map["id"] = owner.owner_id;
    owner_map["name"] = QString::fromStdString(owner.name);
    owner_map["team_id"] = owner.team_id;

    QString type_str;
    switch (owner.type) {
    case Game::Systems::OwnerType::Player:
      type_str = "Player";
      break;
    case Game::Systems::OwnerType::AI:
      type_str = "AI";
      break;
    case Game::Systems::OwnerType::Neutral:
      type_str = "Neutral";
      break;
    }
    owner_map["type"] = type_str;
    owner_map["isLocal"] = (owner.owner_id == m_runtime.local_owner_id);
    owner_map["state"] =
        build_player_state_map(owner.owner_id, m_level.max_troops_per_player);

    result.append(owner_map);
  }

  return result;
}

auto GameEngine::local_player_nation() const -> QString {
  const auto* nation = Game::Systems::NationRegistry::instance().get_nation_for_player(
      m_runtime.local_owner_id);
  if (nation == nullptr) {
    return {};
  }
  return QString::fromStdString(Game::Systems::nation_id_to_string(nation->id));
}

void GameEngine::get_selected_unit_ids(std::vector<Engine::Core::EntityID>& out) const {
  out.clear();
  if (!m_selection_controller) {
    return;
  }
  m_selection_controller->get_selected_unit_ids(out);
}

auto GameEngine::get_unit_type_key(Engine::Core::EntityID id,
                                   QString& type_key) const -> bool {
  type_key.clear();
  if (!m_world) {
    return false;
  }
  auto* e = m_world->get_entity(id);
  if (e == nullptr) {
    return false;
  }
  if (auto* u = e->get_component<Engine::Core::UnitComponent>()) {
    type_key = Game::Units::spawn_typeToQString(u->spawn_type);
    return true;
  }
  return false;
}

auto GameEngine::get_unit_info(Engine::Core::EntityID id,
                               QString& name,
                               int& health,
                               int& max_health,
                               bool& is_building,
                               bool& alive,
                               QString& nation) const -> bool {
  if (!m_world) {
    return false;
  }
  auto* e = m_world->get_entity(id);
  if (e == nullptr) {
    return false;
  }
  is_building = e->has_component<Engine::Core::BuildingComponent>();
  if (auto* u = e->get_component<Engine::Core::UnitComponent>()) {

    auto troop_type_opt = Game::Units::spawn_typeToTroopType(u->spawn_type);
    if (troop_type_opt.has_value()) {
      auto profile = Game::Systems::TroopProfileService::instance().get_profile(
          u->nation_id, *troop_type_opt);
      name = Game::Util::tr_asset(Game::Util::k_units_context, profile.display_name);
    } else {

      name = QString::fromStdString(Game::Units::spawn_typeToString(u->spawn_type));
    }
    health = u->health;
    max_health = u->max_health;
    alive = (u->health > 0);
    nation = Game::Systems::nation_id_to_qstring(u->nation_id);
    return true;
  }
  name = QStringLiteral("Entity");
  health = max_health = 0;
  alive = true;
  nation = QStringLiteral("");
  return true;
}

auto GameEngine::get_unit_stamina_info(Engine::Core::EntityID id,
                                       float& stamina_ratio,
                                       bool& is_running,
                                       bool& can_run) const -> bool {
  stamina_ratio = 1.0F;
  is_running = false;
  can_run = false;

  if (!m_world) {
    return false;
  }
  auto* e = m_world->get_entity(id);
  if (e == nullptr) {
    return false;
  }

  auto* unit = e->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return false;
  }

  can_run = Game::Units::can_use_run_mode(unit->spawn_type);

  auto* stamina = e->get_component<Engine::Core::StaminaComponent>();
  if (stamina != nullptr) {
    stamina_ratio = stamina->get_stamina_ratio();
    is_running = stamina->is_running;
  }

  return true;
}

void GameEngine::on_unit_spawned(const Engine::Core::UnitSpawnedEvent& event) {
  auto& owners = Game::Systems::OwnerRegistry::instance();

  if (event.owner_id == m_runtime.local_owner_id) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.player_barracks_alive = true;
    } else {
      int const production_cost =
          Game::Units::TroopConfig::instance().get_production_cost(event.spawn_type);
      m_entity_cache.player_troop_count += production_cost;
    }
  } else if (owners.is_ai(event.owner_id)) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.enemy_barracks_count++;
      m_entity_cache.enemy_barracks_alive = true;
    }
  }

  auto emit_if_changed = [&] {
    if (m_entity_cache.player_troop_count != m_runtime.last_troop_count) {
      m_runtime.last_troop_count = m_entity_cache.player_troop_count;
      emit troop_count_changed();
    }
  };
  emit_if_changed();
  if (event.owner_id == m_runtime.local_owner_id) {
    const auto troop_type = Game::Units::spawn_typeToTroopType(event.spawn_type);
    if (troop_type.has_value() && Game::Units::is_commander_troop(*troop_type)) {
      emit commander_control_available_changed();
    }
  }
}

void GameEngine::on_unit_died(const Engine::Core::UnitDiedEvent& event) {
  auto& owners = Game::Systems::OwnerRegistry::instance();

  if (event.owner_id == m_runtime.local_owner_id) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.player_barracks_alive = false;
    } else {
      int const production_cost =
          Game::Units::TroopConfig::instance().get_production_cost(event.spawn_type);
      m_entity_cache.player_troop_count -= production_cost;
      m_entity_cache.player_troop_count =
          std::max(0, m_entity_cache.player_troop_count);
    }
  } else if (owners.is_ai(event.owner_id)) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.enemy_barracks_count--;
      m_entity_cache.enemy_barracks_count =
          std::max(0, m_entity_cache.enemy_barracks_count);
      m_entity_cache.enemy_barracks_alive = (m_entity_cache.enemy_barracks_count > 0);
    }
  }
  if (event.owner_id == m_runtime.local_owner_id) {
    const auto troop_type = Game::Units::spawn_typeToTroopType(event.spawn_type);
    if (troop_type.has_value() && Game::Units::is_commander_troop(*troop_type)) {
      if (m_controlled_commander_id == event.unit_id) {
        request_exit_commander_control_mode();
      }
      emit commander_control_available_changed();
    }
  }
}

auto GameEngine::minimap_image() const -> QImage {
  if (m_minimap_manager) {
    return m_minimap_manager->get_image();
  }
  return {};
}

auto GameEngine::generate_map_preview(
    const QString& map_path, const QVariantList& player_configs) const -> QImage {
  Game::Map::Minimap::MapPreviewGenerator generator;
  return generator.generate_preview(map_path, player_configs);
}

float GameEngine::loading_progress() const {
  if (m_loading_progress_tracker) {
    return m_loading_progress_tracker->progress();
  }
  return 0.0F;
}

QString GameEngine::loading_stage_text() const {
  if (m_loading_progress_tracker) {
    auto stage = m_loading_progress_tracker->current_stage();
    auto stage_name = m_loading_progress_tracker->stage_name(stage);
    auto detail = m_loading_progress_tracker->current_detail();
    if (!detail.isEmpty()) {
      return stage_name + " - " + detail;
    }
    return stage_name;
  }
  return {};
}

auto GameEngine::save_slots_view_model() const -> QObject* {
  return m_save_slots_view_model.get();
}

auto GameEngine::placement_view_model() const -> QObject* {
  return m_placement_view_model.get();
}

auto GameEngine::wave_view_model() const -> QObject* {
  return m_wave_view_model.get();
}

auto GameEngine::activity_view_model() const -> QObject* {
  return m_activity_view_model.get();
}

auto GameEngine::input_handler() const -> InputCommandHandler* {
  return m_input_handler.get();
}

auto GameEngine::command_controller() const -> App::Controllers::CommandController* {
  return m_command_controller.get();
}

auto GameEngine::production_manager() const -> ProductionManager* {
  return m_production_manager.get();
}
