#include "game_engine.h"

#include "../controllers/action_vfx.h"
#include "../controllers/command_controller.h"
#include "../models/audio_system_proxy.h"
#include "../models/cursor_manager.h"
#include "../models/hover_tracker.h"
#include "ambient_state_manager.h"
#include "app/models/cursor_mode.h"
#include "app/utils/engine_view_helpers.h"
#include "app/utils/movement_utils.h"
#include "app/utils/selection_utils.h"
#include "audio_event_handler.h"
#include "audio_resource_loader.h"
#include "camera_controller.h"
#include "campaign_manager.h"
#include "core/system.h"
#include "game/audio/audio_system.h"
#include "game/map/mission_context.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game_state_restorer.h"
#include "input_command_handler.h"
#include "level_orchestrator.h"
#include "loading_progress_tracker.h"
#include "minimap_manager.h"
#include "production_manager.h"
#include "renderer_bootstrap.h"
#include "selection_query_service.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QOpenGLContext>
#include <QPainter>
#include <QQuickWindow>
#include <QSize>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>
#include <cmath>
#include <memory>
#include <optional>
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
#include <unordered_set>

#include "../models/selected_units_model.h"
#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/map/campaign_loader.h"
#include "game/map/environment.h"
#include "game/map/level_loader.h"
#include "game/map/map_catalog.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/minimap/map_preview_generator.h"
#include "game/map/minimap/minimap_generator.h"
#include "game/map/minimap/minimap_utils.h"
#include "game/map/minimap/unit_layer.h"
#include "game/map/mission_loader.h"
#include "game/map/skirmish_loader.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/map/world_bootstrap.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/arrow_system.h"
#include "game/systems/ballista_attack_system.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/camera_service.h"
#include "game/systems/camera_visibility_service.h"
#include "game/systems/capture_system.h"
#include "game/systems/catapult_attack_system.h"
#include "game/systems/cleanup_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/command_service.h"
#include "game/systems/formation_planner.h"
#include "game/systems/game_state_serializer.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/guard_system.h"
#include "game/systems/healing_beam_system.h"
#include "game/systems/healing_system.h"
#include "game/systems/movement_system.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/patrol_system.h"
#include "game/systems/picking_service.h"
#include "game/systems/production_service.h"
#include "game/systems/production_system.h"
#include "game/systems/projectile_system.h"
#include "game/systems/rain_manager.h"
#include "game/systems/save_load_service.h"
#include "game/systems/selection_system.h"
#include "game/systems/terrain_alignment_system.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/systems/victory_service.h"
#include "game/units/factory.h"
#include "game/units/troop_config.h"
#include "game/visuals/team_colors.h"
#include "render/entity/combat_dust_renderer.h"
#include "render/entity/healer_aura_renderer.h"
#include "render/entity/healing_beam_renderer.h"
#include "render/entity/healing_waves_renderer.h"
#include "render/geom/arrow.h"
#include "render/geom/formation_arrow.h"
#include "render/geom/patrol_flags.h"
#include "render/geom/stone.h"
#include "render/gl/bootstrap.h"
#include "render/gl/camera.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/bridge_renderer.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/olive_renderer.h"
#include "render/ground/pine_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/river_renderer.h"
#include "render/ground/riverbank_renderer.h"
#include "render/ground/road_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_renderer.h"
#include "render/scene_renderer.h"
#include "utils/resource_utils.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

GameEngine::GameEngine(QObject *parent)
    : QObject(parent),
      m_selectedUnitsModel(new SelectedUnitsModel(this, this)) {

  Game::Systems::NationRegistry::instance().initialize_defaults();
  Game::Systems::TroopCountRegistry::instance().initialize();
  Game::Systems::GlobalStatsRegistry::instance().initialize();

  m_world = std::make_unique<Engine::Core::World>();

  auto rendering = RendererBootstrap::initialize_rendering();
  m_renderer = std::move(rendering.renderer);
  m_camera = std::move(rendering.camera);
  m_ground = std::move(rendering.ground);
  m_terrain = std::move(rendering.terrain);
  m_biome = std::move(rendering.biome);
  m_river = std::move(rendering.river);
  m_road = std::move(rendering.road);
  m_riverbank = std::move(rendering.riverbank);
  m_bridge = std::move(rendering.bridge);
  m_fog = std::move(rendering.fog);
  m_stone = std::move(rendering.stone);
  m_plant = std::move(rendering.plant);
  m_pine = std::move(rendering.pine);
  m_olive = std::move(rendering.olive);
  m_firecamp = std::move(rendering.firecamp);
  m_rain = std::move(rendering.rain);
  m_passes = std::move(rendering.passes);

  RendererBootstrap::initialize_world_systems(*m_world);

  m_pickingService = std::make_unique<Game::Systems::PickingService>();
  m_victoryService = std::make_unique<Game::Systems::VictoryService>();
  m_saveLoadService = std::make_unique<Game::Systems::SaveLoadService>();
  m_cameraService = std::make_unique<Game::Systems::CameraService>();
  m_rainManager = std::make_unique<Game::Systems::RainManager>();

  m_loading_progress_tracker = std::make_unique<LoadingProgressTracker>(this);
  connect(m_loading_progress_tracker.get(),
          &LoadingProgressTracker::progress_changed, this,
          [this](float progress) { emit loading_progress_changed(progress); });
  connect(m_loading_progress_tracker.get(),
          &LoadingProgressTracker::stage_changed, this,
          [this](LoadingProgressTracker::LoadingStage, QString detail) {
            emit loading_stage_changed(detail);
          });

  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  m_selectionController = std::make_unique<Game::Systems::SelectionController>(
      m_world.get(), selection_system, m_pickingService.get());
  m_commandController = std::make_unique<App::Controllers::CommandController>(
      m_world.get(), selection_system, m_pickingService.get());

  m_cursor_manager = std::make_unique<CursorManager>();
  m_hoverTracker = std::make_unique<HoverTracker>(m_pickingService.get());

  m_mapCatalog = std::make_unique<Game::Map::MapCatalog>(this);
  connect(m_mapCatalog.get(), &Game::Map::MapCatalog::map_loaded, this,
          [this](const QVariantMap &mapData) {
            m_available_maps.append(mapData);
            emit available_maps_changed();
          });
  connect(m_mapCatalog.get(), &Game::Map::MapCatalog::loading_changed, this,
          [this](bool loading) {
            m_maps_loading = loading;
            emit maps_loading_changed();
          });
  connect(m_mapCatalog.get(), &Game::Map::MapCatalog::all_maps_loaded, this,
          [this]() { emit available_maps_changed(); });

  if (AudioSystem::getInstance().initialize()) {
    qInfo() << "AudioSystem initialized successfully";
    AudioResourceLoader::load_audio_resources();
  } else {
    qWarning() << "Failed to initialize AudioSystem";
  }

  m_audio_systemProxy = std::make_unique<App::Models::AudioSystemProxy>(this);

  m_minimap_manager = std::make_unique<MinimapManager>();
  m_ambient_state_manager = std::make_unique<AmbientStateManager>();

  m_input_handler = std::make_unique<InputCommandHandler>(
      m_world.get(), m_selectionController.get(), m_commandController.get(),
      m_cursor_manager.get(), m_hoverTracker.get(), m_pickingService.get(),
      m_camera.get());

  m_camera_controller = std::make_unique<CameraController>(
      m_camera.get(), m_cameraService.get(), m_world.get());

  m_production_manager = std::make_unique<ProductionManager>(
      m_world.get(), m_pickingService.get(), m_camera.get(), this);
  connect(m_production_manager.get(),
          &ProductionManager::placing_construction_changed, this,
          &GameEngine::placing_construction_changed);

  m_campaign_manager = std::make_unique<CampaignManager>(this);
  connect(m_campaign_manager.get(),
          &CampaignManager::available_campaigns_changed, this,
          &GameEngine::available_campaigns_changed);

  m_selection_query_service =
      std::make_unique<SelectionQueryService>(m_world.get(), this);

  m_audioEventHandler =
      std::make_unique<Game::Audio::AudioEventHandler>(m_world.get());
  if (m_audioEventHandler->initialize()) {
    qInfo() << "AudioEventHandler initialized successfully";

    m_audioEventHandler->loadUnitVoiceMapping("archer", "archer_voice");
    m_audioEventHandler->loadUnitVoiceMapping("swordsman", "swordsman_voice");
    m_audioEventHandler->loadUnitVoiceMapping("swordsman", "swordsman_voice");
    m_audioEventHandler->loadUnitVoiceMapping("spearman", "spearman_voice");

    m_audioEventHandler->loadAmbientMusic(Engine::Core::AmbientState::PEACEFUL,
                                          "music_peaceful");
    m_audioEventHandler->loadAmbientMusic(Engine::Core::AmbientState::TENSE,
                                          "music_tense");
    m_audioEventHandler->loadAmbientMusic(Engine::Core::AmbientState::COMBAT,
                                          "music_combat");
    m_audioEventHandler->loadAmbientMusic(Engine::Core::AmbientState::VICTORY,
                                          "music_victory");
    m_audioEventHandler->loadAmbientMusic(Engine::Core::AmbientState::DEFEAT,
                                          "music_defeat");

    qInfo() << "Audio mappings configured";
  } else {
    qWarning() << "Failed to initialize AudioEventHandler";
  }

  connect(m_cursor_manager.get(), &CursorManager::mode_changed, this, [this]() {
    if (m_cursor_manager && m_window) {
      m_cursor_manager->update_cursor_shape(m_window);
    }
    emit cursor_mode_changed();
  });
  connect(m_cursor_manager.get(), &CursorManager::global_cursor_changed, this,
          &GameEngine::global_cursor_changed);

  connect(m_selectionController.get(),
          &Game::Systems::SelectionController::selection_changed, this,
          &GameEngine::selected_units_changed);
  connect(m_selectionController.get(),
          &Game::Systems::SelectionController::selection_changed, this,
          &GameEngine::sync_selection_flags);
  connect(
      m_selectionController.get(),
      &Game::Systems::SelectionController::selection_model_refresh_requested,
      this, &GameEngine::selected_units_data_changed);
  connect(m_commandController.get(),
          &App::Controllers::CommandController::attack_target_selected,
          [this]() {
            if (auto *sel_sys =
                    m_world->get_system<Game::Systems::SelectionSystem>()) {
              const auto &sel = sel_sys->get_selected_units();
              if (!sel.empty()) {
                auto *cam = m_camera.get();
                auto *picking = m_pickingService.get();
                if ((cam != nullptr) && (picking != nullptr)) {
                  Engine::Core::EntityID const target_id =
                      Game::Systems::PickingService::pick_unit_first(
                          0.0F, 0.0F, *m_world, *cam, m_viewport.width,
                          m_viewport.height, 0);
                  if (target_id != 0) {
                    App::Controllers::ActionVFX::spawnAttackArrow(m_world.get(),
                                                                  target_id);
                  }
                }
              }
            }
          });

  connect(m_commandController.get(),
          &App::Controllers::CommandController::troop_limit_reached, [this]() {
            set_error(
                "Maximum troop limit reached. Cannot produce more units.");
          });
  connect(m_commandController.get(),
          &App::Controllers::CommandController::hold_mode_changed, this,
          &GameEngine::hold_mode_changed);
  connect(m_commandController.get(),
          &App::Controllers::CommandController::guard_mode_changed, this,
          &GameEngine::guard_mode_changed);
  connect(m_commandController.get(),
          &App::Controllers::CommandController::formation_mode_changed, this,
          &GameEngine::formation_mode_changed);
  connect(m_commandController.get(),
          &App::Controllers::CommandController::formation_placement_started,
          [this]() { emit placing_formation_changed(); });
  connect(m_commandController.get(),
          &App::Controllers::CommandController::formation_placement_ended,
          [this]() { emit placing_formation_changed(); });

  connect(this, SIGNAL(selected_units_changed()), m_selectedUnitsModel,
          SLOT(refresh()));
  connect(this, SIGNAL(selected_units_data_changed()), m_selectedUnitsModel,
          SLOT(refresh()));

  emit selected_units_changed();

  m_unit_died_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const Engine::Core::UnitDiedEvent &e) {
            on_unit_died(e);

            if (Game::Units::isTroopSpawn(e.spawn_type) &&
                e.owner_id != m_runtime.local_owner_id &&
                e.killer_owner_id == m_runtime.local_owner_id) {

              int const production_cost =
                  Game::Units::TroopConfig::instance().getProductionCost(
                      e.spawn_type);
              m_enemyTroopsDefeated += production_cost;
              emit enemy_troops_defeated_changed();
            }
          });

  m_unit_spawned_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>(
          [this](const Engine::Core::UnitSpawnedEvent &e) {
            on_unit_spawned(e);
          });
}

GameEngine::~GameEngine() {

  if (m_audioEventHandler) {
    m_audioEventHandler->shutdown();
  }
  AudioSystem::getInstance().shutdown();
  qInfo() << "AudioSystem shut down";
}

void GameEngine::cleanup_opengl_resources() {
  qInfo() << "Cleaning up OpenGL resources...";

  QOpenGLContext *context = QOpenGLContext::currentContext();
  const bool has_valid_context = (context != nullptr);

  if (!has_valid_context) {
    qInfo() << "No valid OpenGL context, skipping OpenGL cleanup";
  }

  if (m_renderer && has_valid_context) {
    m_renderer->shutdown();
    qInfo() << "Renderer shut down";
  }

  m_passes.clear();

  m_ground.reset();
  m_terrain.reset();
  m_biome.reset();
  m_river.reset();
  m_road.reset();
  m_riverbank.reset();
  m_bridge.reset();
  m_fog.reset();
  m_stone.reset();
  m_plant.reset();
  m_pine.reset();
  m_olive.reset();
  m_firecamp.reset();
  m_rain.reset();
  m_rainManager.reset();

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
    m_input_handler->on_map_clicked(sx, sy, m_runtime.local_owner_id,
                                    m_viewport);
  }
}

void GameEngine::on_right_click(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->on_right_click(sx, sy, m_runtime.local_owner_id,
                                    m_viewport);
  }
}

void GameEngine::on_right_double_click(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->on_right_double_click(sx, sy, m_runtime.local_owner_id,
                                           m_viewport);
  }
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

void GameEngine::reset_movement(Engine::Core::Entity *entity) {
  InputCommandHandler::reset_movement(entity);
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

void GameEngine::on_guard_command() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_guard_command();
}

void GameEngine::on_formation_command() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_formation_command();
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
  m_cursor_manager->set_mode(CursorMode::Heal);
}

void GameEngine::on_build_command() {
  if (!m_cursor_manager) {
    return;
  }
  ensure_initialized();
  m_cursor_manager->set_mode(CursorMode::Build);
}

void GameEngine::on_guard_click(qreal sx, qreal sy) {
  if (!m_input_handler || !m_camera) {
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

auto GameEngine::any_selected_in_formation_mode() const -> bool {
  if (!m_input_handler) {
    return false;
  }
  return m_input_handler->any_selected_in_formation_mode();
}

auto GameEngine::any_selected_in_run_mode() const -> bool {
  if (!m_input_handler) {
    return false;
  }
  return m_input_handler->any_selected_in_run_mode();
}

auto GameEngine::is_placing_formation() const -> bool {
  if (m_commandController) {
    return m_commandController->is_placing_formation();
  }
  return false;
}

bool GameEngine::is_campaign_mission() const {
  if (!m_campaign_manager) {
    return false;
  }
  return m_campaign_manager->current_mission_context().is_campaign();
}

void GameEngine::on_formation_mouse_move(qreal sx, qreal sy) {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_formation_mouse_move(sx, sy, m_viewport);
}

void GameEngine::on_formation_scroll(float delta) {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_formation_scroll(delta);
}

void GameEngine::on_formation_confirm() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_formation_confirm();
}

void GameEngine::on_formation_cancel() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_formation_cancel();
}

auto GameEngine::is_placing_construction() const -> bool {
  return m_production_manager ? m_production_manager->is_placing_construction()
                              : false;
}

void GameEngine::on_construction_mouse_move(qreal sx, qreal sy) {
  ensure_initialized();
  if (m_production_manager) {
    m_production_manager->on_construction_mouse_move(sx, sy, m_viewport);
  }
}

void GameEngine::on_construction_confirm() {
  ensure_initialized();
  if (m_production_manager) {
    m_production_manager->on_construction_confirm();
  }
}

void GameEngine::on_construction_cancel() {
  if (m_production_manager) {
    m_production_manager->on_construction_cancel();
  }
}

void GameEngine::on_patrol_click(qreal sx, qreal sy) {
  if (!m_input_handler || !m_camera) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_patrol_click(sx, sy, m_viewport);
}

void GameEngine::update_cursor(Qt::CursorShape newCursor) {
  if (m_window == nullptr) {
    return;
  }
  if (m_runtime.current_cursor != newCursor) {
    m_runtime.current_cursor = newCursor;
    m_window->setCursor(newCursor);
  }
}

void GameEngine::set_error(const QString &errorMessage) {
  if (m_runtime.last_error != errorMessage) {
    m_runtime.last_error = errorMessage;
    qCritical() << "GameEngine error:" << errorMessage;
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

void GameEngine::set_cursor_mode(const QString &mode) {
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
}

void GameEngine::on_click_select(qreal sx, qreal sy, bool additive) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->on_click_select(sx, sy, additive, m_runtime.local_owner_id,
                                     m_viewport);
  }
}

void GameEngine::on_area_selected(qreal x1, qreal y1, qreal x2, qreal y2,
                                  bool additive) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->on_area_selected(x1, y1, x2, y2, additive,
                                      m_runtime.local_owner_id, m_viewport);
  }
}

void GameEngine::select_all_troops() {
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->select_all_troops(m_runtime.local_owner_id);
  }
}

void GameEngine::select_unit_by_id(int unitId) {
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->select_unit_by_id(unitId, m_runtime.local_owner_id);
  }
}

void GameEngine::ensure_initialized() {
  QString error;
  Game::Map::WorldBootstrap::ensure_initialized(
      m_runtime.initialized, *m_renderer, *m_camera, m_ground.get(), &error);
  if (!error.isEmpty()) {
    set_error(error);
  }
}

auto GameEngine::enemy_troops_defeated() const -> int {
  return m_enemyTroopsDefeated;
}

auto GameEngine::get_player_stats(int owner_id) -> QVariantMap {
  QVariantMap result;

  auto &stats_registry = Game::Systems::GlobalStatsRegistry::instance();
  const auto *stats = stats_registry.get_stats(owner_id);

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

void GameEngine::update(float dt) {

  if (m_runtime.loading) {
    return;
  }

  if (m_runtime.paused) {
    dt = 0.0F;
  } else {
    dt *= m_runtime.time_scale;
  }

  if (!m_runtime.paused && !m_runtime.loading) {
    if (m_ambient_state_manager) {
      m_ambient_state_manager->update(dt, m_world.get(),
                                      m_runtime.local_owner_id, m_entity_cache,
                                      m_runtime.victory_state);
    }
  }

  if (m_renderer) {
    m_renderer->update_animation_time(dt);
  }

  if (m_camera) {
    m_camera->update(dt);
  }

  if (m_world) {
    m_world->update(dt);

    auto &visibility_service = Game::Map::VisibilityService::instance();
    if (visibility_service.is_initialized() && !m_level.is_spectator_mode) {
      m_runtime.visibility_update_accumulator += dt;
      const float visibility_update_interval =
          Game::GameConfig::instance().gameplay().visibility_update_interval;
      if (m_runtime.visibility_update_accumulator >=
          visibility_update_interval) {
        m_runtime.visibility_update_accumulator = 0.0F;
        visibility_service.update(*m_world, m_runtime.local_owner_id);
      }

      const auto new_version = visibility_service.version();
      if (new_version != m_runtime.visibility_version) {
        if (m_fog) {
          m_fog->update_mask(visibility_service.getWidth(),
                             visibility_service.getHeight(),
                             visibility_service.getTileSize(),
                             visibility_service.snapshotCells());
        }
        m_runtime.visibility_version = new_version;
      }
    }

    if (m_minimap_manager) {
      if (!m_level.is_spectator_mode) {
        m_minimap_manager->update_fog(dt, m_runtime.local_owner_id);
      }
      auto *selection_system =
          m_world->get_system<Game::Systems::SelectionSystem>();
      m_minimap_manager->update_units(m_world.get(), selection_system,
                                      m_runtime.local_owner_id);
      m_minimap_manager->update_camera_viewport(
          m_camera.get(), static_cast<float>(m_viewport.width),
          static_cast<float>(m_viewport.height));

      if (m_minimap_manager->consume_dirty_flag()) {
        emit minimap_image_changed();
      }
    }
  }

  if (m_rainManager) {
    m_rainManager->update(dt);
    if (m_rain) {
      m_rain->set_enabled(m_rainManager->is_enabled());
      m_rain->set_intensity(m_rainManager->get_intensity());
      m_rain->set_weather_type(m_rainManager->get_weather_type());
      m_rain->set_wind_strength(m_rainManager->get_wind_strength());
      if (m_camera) {
        m_rain->set_camera_position(m_camera->get_position());
      }
    }
  }

  if (m_victoryService && m_world) {
    m_victoryService->update(*m_world, dt);
  }

  if (m_camera_controller) {
    m_camera_controller->update_follow(m_followSelectionEnabled);
  }

  if (m_selectedUnitsModel != nullptr) {
    auto *selection_system =
        m_world->get_system<Game::Systems::SelectionSystem>();
    if ((selection_system != nullptr) &&
        !selection_system->get_selected_units().empty()) {
      m_runtime.selection_refresh_counter++;
      if (m_runtime.selection_refresh_counter >= 15) {
        m_runtime.selection_refresh_counter = 0;
        emit selected_units_data_changed();
      }
    }
  }
}

void GameEngine::render(int pixelWidth, int pixelHeight) {
  if (!m_renderer || !m_world || !m_runtime.initialized || m_runtime.loading) {
    return;
  }

  Game::Systems::CameraVisibilityService::instance().set_camera(m_camera.get());

  if (pixelWidth > 0 && pixelHeight > 0) {
    m_viewport.width = pixelWidth;
    m_viewport.height = pixelHeight;
    m_renderer->set_viewport(pixelWidth, pixelHeight);
  }

  if (auto *selection_system =
          m_world->get_system<Game::Systems::SelectionSystem>()) {
    const auto &sel = selection_system->get_selected_units();
    std::vector<unsigned int> const ids(sel.begin(), sel.end());
    m_renderer->set_selected_entities(ids);
  }

  m_renderer->begin_frame();

  if (auto *res = m_renderer->resources()) {
    for (auto *pass : m_passes) {
      if (pass != nullptr) {
        pass->submit(*m_renderer, res);
      }
    }
  }

  if (m_renderer && m_hoverTracker) {
    m_renderer->set_hovered_entity_id(m_hoverTracker->getLastHoveredEntity());
  }
  if (m_renderer) {
    m_renderer->set_local_owner_id(m_runtime.local_owner_id);
  }

  m_renderer->render_world(m_world.get());
  render_game_effects();
  m_renderer->end_frame();

  update_loading_overlay();
  update_cursor_position();
}

void GameEngine::render_game_effects() {
  auto *res = m_renderer->resources();
  if (!res) {
    return;
  }

  if (auto *arrow_system = m_world->get_system<Game::Systems::ArrowSystem>()) {
    Render::GL::render_arrows(m_renderer.get(), res, *arrow_system);
  }

  if (auto *projectile_system =
          m_world->get_system<Game::Systems::ProjectileSystem>()) {
    Render::GL::render_projectiles(m_renderer.get(), res, *projectile_system);
  }

  if (auto *healing_beam_system =
          m_world->get_system<Game::Systems::HealingBeamSystem>()) {
    if (auto *res = m_renderer->resources()) {
      Render::GL::render_healing_beams(m_renderer.get(), res,
                                       *healing_beam_system);
      Render::GL::render_healing_waves(m_renderer.get(), res,
                                       *healing_beam_system);
    }
  }

  Render::GL::render_healer_auras(m_renderer.get(), res, m_world.get());
  Render::GL::render_combat_dust(m_renderer.get(), res, m_world.get());

  std::optional<QVector3D> preview_waypoint;
  if (m_commandController && m_commandController->has_patrol_first_waypoint()) {
    preview_waypoint = m_commandController->get_patrol_first_waypoint();
  }
  Render::GL::render_patrol_flags(m_renderer.get(), res, *m_world,
                                  preview_waypoint);

  if (m_commandController && m_commandController->is_placing_formation()) {
    Render::GL::FormationPlacementInfo placement;
    placement.position =
        m_commandController->get_formation_placement_position();
    placement.angle_degrees =
        m_commandController->get_formation_placement_angle();
    placement.active = true;
    Render::GL::render_formation_arrow(m_renderer.get(), res, placement);
  }
}

void GameEngine::update_loading_overlay() {
  if (!m_loading_overlay_wait_for_first_frame) {
    return;
  }

  if (!m_renderer || !m_renderer->resources()) {
    m_loading_overlay_frames_remaining = 5;
    m_loading_overlay_timer.restart();
    return;
  }

  if (m_loading_overlay_frames_remaining > 0) {
    m_loading_overlay_frames_remaining--;
  }

  constexpr qint64 k_loading_overlay_max_wait_ms = 15000;
  const qint64 elapsed_ms =
      m_loading_overlay_timer.isValid() ? m_loading_overlay_timer.elapsed() : 0;
  const bool enough_time = m_loading_overlay_timer.isValid() &&
                           (elapsed_ms >= m_loading_overlay_min_duration_ms);
  const bool exceeded_max_wait = m_loading_overlay_timer.isValid() &&
                                 (elapsed_ms >= k_loading_overlay_max_wait_ms);

  QStringList pending_components;
  const bool biome_ready = !m_biome || m_biome->is_gpu_ready();
  const bool pine_ready = !m_pine || m_pine->is_gpu_ready();
  const bool olive_ready = !m_olive || m_olive->is_gpu_ready();
  const bool plant_ready = !m_plant || m_plant->is_gpu_ready();
  const bool stone_ready = !m_stone || m_stone->is_gpu_ready();
  const bool firecamp_ready = !m_firecamp || m_firecamp->is_gpu_ready();

  if (!biome_ready) {
    pending_components << QStringLiteral("biome");
  }
  if (!pine_ready) {
    pending_components << QStringLiteral("pine");
  }
  if (!olive_ready) {
    pending_components << QStringLiteral("olive");
  }
  if (!plant_ready) {
    pending_components << QStringLiteral("plant");
  }
  if (!stone_ready) {
    pending_components << QStringLiteral("stone");
  }
  if (!firecamp_ready) {
    pending_components << QStringLiteral("firecamp");
  }

  const bool biome_gpu_ready = pending_components.isEmpty();

  if (enough_time && m_loading_overlay_frames_remaining <= 0 &&
      (biome_gpu_ready || exceeded_max_wait)) {
    if (exceeded_max_wait && !biome_gpu_ready) {
      qWarning() << "Loading overlay timed out waiting for GPU readiness"
                 << pending_components.join(", ");
    }
    m_loading_overlay_wait_for_first_frame = false;
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
  qreal const current_x = global_cursor_x();
  qreal const current_y = global_cursor_y();
  if (current_x != m_runtime.last_cursor_x ||
      current_y != m_runtime.last_cursor_y) {
    m_runtime.last_cursor_x = current_x;
    m_runtime.last_cursor_y = current_y;
    emit global_cursor_changed();
  }
}

auto GameEngine::screen_to_ground(const QPointF &screenPt,
                                  QVector3D &outWorld) -> bool {
  return App::Utils::screen_to_ground(m_pickingService.get(), m_camera.get(),
                                      m_window, m_viewport.width,
                                      m_viewport.height, screenPt, outWorld);
}

auto GameEngine::world_to_screen(const QVector3D &world,
                                 QPointF &outScreen) const -> bool {
  return App::Utils::world_to_screen(m_pickingService.get(), m_camera.get(),
                                     m_window, m_viewport.width,
                                     m_viewport.height, world, outScreen);
}

void GameEngine::sync_selection_flags() {
  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  if (!m_world || (selection_system == nullptr)) {
    return;
  }

  App::Utils::sanitize_selection(m_world.get(), selection_system);

  if (selection_system->get_selected_units().empty()) {
    if (m_cursor_manager && m_cursor_manager->mode() != CursorMode::Normal) {
      set_cursor_mode(CursorMode::Normal);
    }
  }
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
  m_followSelectionEnabled = enable;
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

void GameEngine::on_minimap_left_click(qreal mx, qreal my, qreal minimap_width,
                                       qreal minimap_height) {
  ensure_initialized();
  if (!m_camera || !m_minimap_manager || !m_minimap_manager->has_minimap()) {
    return;
  }

  const QImage &minimap_img = m_minimap_manager->get_image();
  if (minimap_img.isNull()) {
    return;
  }

  const float img_width = static_cast<float>(minimap_img.width());
  const float img_height = static_cast<float>(minimap_img.height());

  const float px =
      (static_cast<float>(mx) / static_cast<float>(minimap_width)) * img_width;
  const float py =
      (static_cast<float>(my) / static_cast<float>(minimap_height)) *
      img_height;

  const auto [world_x, world_z] = Game::Map::Minimap::pixel_to_world(
      px, py, m_minimap_manager->get_world_width(),
      m_minimap_manager->get_world_height(), img_width, img_height,
      m_minimap_manager->get_tile_size());

  if (m_camera) {
    const QVector3D new_target(world_x, 0.0F, world_z);
    const QVector3D current_target = m_camera->get_target();
    const QVector3D current_position = m_camera->get_position();

    const QVector3D offset = current_position - current_target;

    m_camera->look_at(new_target + offset, new_target,
                      m_camera->get_up_vector());
  }

  m_followSelectionEnabled = false;
  if (m_camera_controller) {
    m_camera_controller->follow_selection(false);
  }
}

void GameEngine::on_minimap_right_click(qreal mx, qreal my, qreal minimap_width,
                                        qreal minimap_height) {
  ensure_initialized();
  if (m_level.is_spectator_mode || !m_world || !m_minimap_manager ||
      !m_minimap_manager->has_minimap()) {
    return;
  }

  const QImage &minimap_img = m_minimap_manager->get_image();
  if (minimap_img.isNull()) {
    return;
  }

  const float img_width = static_cast<float>(minimap_img.width());
  const float img_height = static_cast<float>(minimap_img.height());

  const float px =
      (static_cast<float>(mx) / static_cast<float>(minimap_width)) * img_width;
  const float py =
      (static_cast<float>(my) / static_cast<float>(minimap_height)) *
      img_height;

  const auto [world_x, world_z] = Game::Map::Minimap::pixel_to_world(
      px, py, m_minimap_manager->get_world_width(),
      m_minimap_manager->get_world_height(), img_width, img_height,
      m_minimap_manager->get_tile_size());

  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  if (!selection_system) {
    return;
  }

  const auto &selected = selection_system->get_selected_units();
  if (selected.empty()) {
    return;
  }

  const QVector3D target_pos(world_x, 0.0F, world_z);
  auto targets = Game::Systems::FormationPlanner::spread_formation_by_nation(
      *m_world, selected, target_pos,
      Game::GameConfig::instance().gameplay().formation_spacing_default);

  Game::Systems::CommandService::MoveOptions opts;
  opts.group_move = selected.size() > 1;
  Game::Systems::CommandService::move_units(*m_world, selected, targets, opts);
}

auto GameEngine::selected_units_model() -> QAbstractItemModel * {
  return m_selectedUnitsModel;
}

auto GameEngine::audio_system() -> QObject * {
  return m_audio_systemProxy.get();
}

auto GameEngine::has_units_selected() const -> bool {
  if (!m_selectionController) {
    return false;
  }
  return m_selectionController->has_units_selected();
}

auto GameEngine::player_troop_count() const -> int {
  return m_entity_cache.player_troop_count;
}

auto GameEngine::has_selected_type(const QString &type) const -> bool {
  if (!m_selectionController) {
    return false;
  }
  return m_selectionController->has_selected_type(type);
}

void GameEngine::recruit_near_selected(const QString &unit_type) {
  ensure_initialized();
  if (!m_commandController) {
    return;
  }
  m_commandController->recruit_near_selected(unit_type,
                                             m_runtime.local_owner_id);
}

void GameEngine::start_building_placement(const QString &building_type) {
  ensure_initialized();
  if (m_production_manager) {
    m_production_manager->start_building_placement(building_type);
    set_cursor_mode(CursorMode::PlaceBuilding);
  }
}

void GameEngine::place_building_at_screen(qreal sx, qreal sy) {
  ensure_initialized();
  if (m_production_manager) {
    m_production_manager->place_building_at_screen(
        sx, sy, m_runtime.local_owner_id, m_viewport);
    set_cursor_mode(CursorMode::Normal);
  }
}

void GameEngine::cancel_building_placement() {
  if (m_production_manager) {
    m_production_manager->cancel_building_placement();
  }
  set_cursor_mode(CursorMode::Normal);
}

auto GameEngine::pending_building_type() const -> QString {
  return m_production_manager ? m_production_manager->pending_building_type()
                              : QString();
}

auto GameEngine::get_selected_production_state() const -> QVariantMap {
  return m_production_manager
             ? m_production_manager->get_selected_production_state(
                   m_runtime.local_owner_id)
             : QVariantMap();
}

auto GameEngine::get_unit_production_info(
    const QString &unit_type, const QString &nation_id) const -> QVariantMap {
  return m_production_manager ? m_production_manager->get_unit_production_info(
                                    unit_type, nation_id)
                              : QVariantMap();
}

auto GameEngine::get_selected_builder_production_state() const -> QVariantMap {
  return m_production_manager
             ? m_production_manager->get_selected_builder_production_state()
             : QVariantMap();
}

void GameEngine::start_builder_construction(const QString &item_type) {
  if (m_production_manager) {
    m_production_manager->start_builder_construction(item_type);
  }
}

auto GameEngine::get_selected_units_command_mode() const -> QString {
  return m_selection_query_service
             ? m_selection_query_service->get_selected_units_command_mode()
             : "normal";
}

auto GameEngine::get_selected_units_mode_availability() const -> QVariantMap {
  return m_selection_query_service
             ? m_selection_query_service->get_selected_units_mode_availability()
             : QVariantMap();
}

void GameEngine::set_rally_at_screen(qreal sx, qreal sy) {
  ensure_initialized();
  if (m_production_manager) {
    m_production_manager->set_rally_at_screen(sx, sy, m_runtime.local_owner_id,
                                              m_viewport);
  }
}

void GameEngine::start_loading_maps() {
  m_available_maps.clear();
  if (m_mapCatalog) {
    m_mapCatalog->load_maps_async();
  }
  load_campaigns();
}

auto GameEngine::available_maps() const -> QVariantList {
  return m_available_maps;
}

auto GameEngine::available_nations() const -> QVariantList {
  QVariantList nations;
  const auto &registry = Game::Systems::NationRegistry::instance();
  const auto &all = registry.get_all_nations();
  QList<QVariantMap> ordered;
  ordered.reserve(static_cast<int>(all.size()));
  for (const auto &nation : all) {
    QVariantMap entry;
    entry.insert(
        QStringLiteral("id"),
        QString::fromStdString(Game::Systems::nation_id_to_string(nation.id)));
    entry.insert(QStringLiteral("name"),
                 QString::fromStdString(nation.display_name));
    ordered.append(entry);
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const QVariantMap &a, const QVariantMap &b) {
              return a.value(QStringLiteral("name"))
                         .toString()
                         .localeAwareCompare(
                             b.value(QStringLiteral("name")).toString()) < 0;
            });
  for (const auto &entry : ordered) {
    nations.append(entry);
  }
  return nations;
}

auto GameEngine::available_campaigns() const -> QVariantList {
  return m_campaign_manager ? m_campaign_manager->available_campaigns()
                            : QVariantList();
}

void GameEngine::load_campaigns() {
  if (!m_saveLoadService) {
    return;
  }

  QString error;
  auto campaigns = m_saveLoadService->list_campaigns(&error);
  if (!error.isEmpty()) {
    qWarning() << "Failed to load campaigns:" << error;
    return;
  }

  if (m_campaign_manager) {
    m_campaign_manager->set_available_campaigns(campaigns);
  }
}

void GameEngine::start_campaign_mission(const QString &mission_path) {
  clear_error();

  if (!m_campaign_manager) {
    set_error("Campaign manager not initialized");
    return;
  }

  m_selected_player_id = 1;

  m_campaign_manager->start_campaign_mission(mission_path,
                                             m_selected_player_id);

  if (!m_campaign_manager->current_mission_definition().has_value()) {
    set_error("Failed to load mission");
    return;
  }

  const auto &mission = *m_campaign_manager->current_mission_definition();

  QVariantList playerConfigs;

  QVariantMap player1;
  player1.insert("player_id", 1);
  player1.insert("playerName", mission.player_setup.nation);
  player1.insert("colorIndex", 0);
  player1.insert("team_id", 0);
  player1.insert("nationId", mission.player_setup.nation);
  player1.insert("isHuman", true);
  playerConfigs.append(player1);

  int player_id = 2;
  int default_team_id = 1;
  for (const auto &ai_setup : mission.ai_setups) {
    QVariantMap ai_player;
    ai_player.insert("player_id", player_id);
    ai_player.insert("playerName", ai_setup.nation);
    ai_player.insert("colorIndex", player_id - 1);

    int team_id;
    if (ai_setup.team_id.has_value()) {
      team_id = ai_setup.team_id.value();
    } else {
      team_id = default_team_id++;
    }

    ai_player.insert("team_id", team_id);
    ai_player.insert("nationId", ai_setup.nation);
    ai_player.insert("isHuman", false);
    playerConfigs.append(ai_player);
    player_id++;
  }

  start_skirmish_internal(mission.map_path, playerConfigs, false);
}

void GameEngine::mark_current_mission_completed() {
  if (!m_campaign_manager) {
    return;
  }

  const QString campaign_id = m_campaign_manager->current_campaign_id();
  if (campaign_id.isEmpty()) {
    qWarning() << "No active campaign mission to mark as completed";
    return;
  }

  if (!m_saveLoadService) {
    qWarning() << "Save/Load service not initialized";
    return;
  }

  QString error;
  bool success =
      m_saveLoadService->mark_campaign_completed(campaign_id, &error);
  if (!success) {
    qWarning() << "Failed to mark campaign as completed:" << error;
  } else {
    m_campaign_manager->mark_current_mission_completed();
    load_campaigns();
  }
}

QVariantMap GameEngine::get_current_mission_objectives() const {
  QVariantMap result;

  if (!m_campaign_manager) {
    return result;
  }

  const auto &mission_def = m_campaign_manager->current_mission_definition();
  if (!mission_def.has_value()) {
    return result;
  }

  const auto &mission = *mission_def;

  result["title"] = mission.title;
  result["summary"] = mission.summary;

  QVariantList victory_conditions;
  for (const auto &condition : mission.victory_conditions) {
    QVariantMap cond;
    cond["type"] = condition.type;
    cond["description"] = condition.description;
    if (condition.duration.has_value()) {
      cond["duration"] = condition.duration.value();
    }
    victory_conditions.append(cond);
  }
  result["victory_conditions"] = victory_conditions;

  QVariantList defeat_conditions;
  for (const auto &condition : mission.defeat_conditions) {
    QVariantMap cond;
    cond["type"] = condition.type;
    cond["description"] = condition.description;
    defeat_conditions.append(cond);
  }
  result["defeat_conditions"] = defeat_conditions;

  QVariantList optional_objectives;
  for (const auto &condition : mission.optional_objectives) {
    QVariantMap cond;
    cond["type"] = condition.type;
    cond["description"] = condition.description;
    optional_objectives.append(cond);
  }
  result["optional_objectives"] = optional_objectives;

  return result;
}

void GameEngine::start_skirmish(const QString &map_path,
                                const QVariantList &playerConfigs) {
  start_skirmish_internal(map_path, playerConfigs, true);
}

void GameEngine::start_skirmish_internal(const QString &map_path,
                                         const QVariantList &playerConfigs,
                                         bool set_skirmish_context) {

  clear_error();

  m_level.map_path = map_path;
  m_level.map_name = map_path;

  if (m_campaign_manager && set_skirmish_context) {
    m_campaign_manager->set_skirmish_context(map_path);
  }

  if (!m_runtime.victory_state.isEmpty()) {
    m_runtime.victory_state = "";
    emit victory_state_changed();
  }
  if (m_victoryService) {
    m_victoryService->reset();
  }
  m_enemyTroopsDefeated = 0;

  if (!m_runtime.initialized) {
    ensure_initialized();
  }

  if (!m_world || !m_renderer || !m_camera) {
    set_error("Cannot start skirmish: renderer not initialized");
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
  QTimer::singleShot(50, this, [this, map_path, playerConfigs]() {
    perform_skirmish_load(map_path, playerConfigs);
  });
}

void GameEngine::perform_skirmish_load(const QString &map_path,
                                       const QVariantList &playerConfigs) {

  if (!(m_world && m_renderer && m_camera)) {
    set_error("Cannot start skirmish: renderer not initialized");
    m_runtime.loading = false;
    emit is_loading_changed();
    return;
  }

  if (m_hoverTracker) {
    m_hoverTracker->update_hover(-1, -1, *m_world, *m_camera, 0, 0);
  }

  LevelOrchestrator orchestrator;
  LevelOrchestrator::RendererRefs renderers{
      m_renderer.get(), m_camera.get(), m_ground.get(),   m_terrain.get(),
      m_biome.get(),    m_river.get(),  m_road.get(),     m_riverbank.get(),
      m_bridge.get(),   m_fog.get(),    m_stone.get(),    m_plant.get(),
      m_pine.get(),     m_olive.get(),  m_firecamp.get(), m_rain.get()};

  auto visibility_ready = [this]() {
    m_runtime.visibility_version =
        Game::Map::VisibilityService::instance().version();
    m_runtime.visibility_update_accumulator = 0.0F;
  };

  auto owner_update = [this]() { emit owner_info_changed(); };

  const bool allow_default_player_barracks =
      !m_campaign_manager ||
      !m_campaign_manager->current_mission_context().is_campaign();

  auto load_result = orchestrator.load_skirmish(
      map_path, playerConfigs, m_selected_player_id, *m_world, renderers,
      m_level, m_entity_cache, m_victoryService.get(), m_minimap_manager.get(),
      visibility_ready, owner_update, allow_default_player_barracks,
      m_loading_progress_tracker.get());

  if (load_result.updated_player_id != m_selected_player_id) {
    m_selected_player_id = load_result.updated_player_id;
    emit selected_player_id_changed();
  }

  if (!load_result.success) {
    set_error(load_result.error_message);
    m_runtime.loading = false;
    m_loading_overlay_active = false;
    m_loading_overlay_wait_for_first_frame = false;
    m_finalize_progress_after_overlay = false;
    m_show_objectives_after_loading = false;
    emit is_loading_changed();
    return;
  }

  m_runtime.local_owner_id = load_result.updated_player_id;

  apply_mission_setup();
  configure_mission_victory_conditions();
  configure_rain_system();
  finalize_skirmish_load();
}

void GameEngine::apply_mission_setup() {
  if (!m_world || !m_campaign_manager) {
    return;
  }

  if (!m_campaign_manager->current_mission_context().is_campaign()) {
    return;
  }

  if (!m_campaign_manager->current_mission_definition().has_value()) {
    return;
  }

  auto reg = Game::Map::MapTransformer::getFactoryRegistry();
  if (!reg) {
    qWarning() << "Mission setup skipped: unit factory registry missing";
    return;
  }

  const auto &mission = *m_campaign_manager->current_mission_definition();
  auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  auto &nation_registry = Game::Systems::NationRegistry::instance();

  Game::Map::MapDefinition map_def;
  QString map_error;
  const QString resolved_map_path =
      Utils::Resources::resolveResourcePath(m_level.map_path);
  bool map_loaded = Game::Map::MapLoader::loadFromJsonFile(resolved_map_path,
                                                           map_def, &map_error);
  if (!map_loaded) {
    qWarning() << "Mission setup: failed to load map definition for"
               << m_level.map_path << "resolved to" << resolved_map_path << "-"
               << map_error;
  }

  bool has_map_spawns = true;
  if (map_loaded) {
    has_map_spawns = !map_def.spawns.empty();
  }

  bool has_mission_spawns = !mission.player_setup.starting_units.empty() ||
                            !mission.player_setup.starting_buildings.empty();
  for (const auto &ai_setup : mission.ai_setups) {
    if (!ai_setup.starting_units.empty() ||
        !ai_setup.starting_buildings.empty()) {
      has_mission_spawns = true;
      break;
    }
  }

  if (has_mission_spawns && !has_map_spawns) {
    std::vector<Engine::Core::EntityID> to_remove;
    auto entities = m_world->get_entities_with<Engine::Core::UnitComponent>();
    to_remove.reserve(entities.size());
    for (auto *entity : entities) {
      if (entity != nullptr) {
        to_remove.push_back(entity->get_id());
      }
    }
    for (const auto id : to_remove) {
      m_world->destroy_entity(id);
    }
  }

  auto resolve_nation_id = [&nation_registry](const QString &nation_str) {
    const auto parsed =
        Game::Systems::nation_id_from_string(nation_str.toStdString());
    if (parsed.has_value()) {
      return parsed.value();
    }
    return nation_registry.default_nation_id();
  };

  auto mission_position_to_world = [&](const Game::Mission::Position &pos) {
    float world_x = pos.x;
    float world_z = pos.z;
    if (map_loaded && map_def.coordSystem == Game::Map::CoordSystem::Grid) {
      const float tile = std::max(0.0001F, map_def.grid.tile_size);
      world_x = (pos.x - (map_def.grid.width * 0.5F - 0.5F)) * tile;
      world_z = (pos.z - (map_def.grid.height * 0.5F - 0.5F)) * tile;
    } else if (!map_loaded) {
      const float tile = std::max(0.0001F, m_level.tile_size);
      world_x = (pos.x - (m_level.grid_width * 0.5F - 0.5F)) * tile;
      world_z = (pos.z - (m_level.grid_height * 0.5F - 0.5F)) * tile;
    }
    return QVector3D(world_x, 0.0F, world_z);
  };

  auto apply_team_color = [&](Engine::Core::Entity *entity, int owner_id) {
    if (entity == nullptr) {
      return;
    }
    auto *renderable =
        entity->get_component<Engine::Core::RenderableComponent>();
    if (renderable == nullptr) {
      return;
    }
    const QVector3D team_color = Game::Visuals::team_colorForOwner(owner_id);
    renderable->color[0] = team_color.x();
    renderable->color[1] = team_color.y();
    renderable->color[2] = team_color.z();
  };

  auto parse_color = [](const QString &color_name,
                        std::array<float, 3> &out) -> bool {
    if (color_name.isEmpty()) {
      return false;
    }

    const QString trimmed = color_name.trimmed();
    if (trimmed.startsWith('#') && trimmed.length() == 7) {
      bool ok = false;
      int r = trimmed.mid(1, 2).toInt(&ok, 16);
      if (!ok) {
        return false;
      }
      int g = trimmed.mid(3, 2).toInt(&ok, 16);
      if (!ok) {
        return false;
      }
      int b = trimmed.mid(5, 2).toInt(&ok, 16);
      if (!ok) {
        return false;
      }
      constexpr float scale = 255.0F;
      out = {r / scale, g / scale, b / scale};
      return true;
    }

    const QString lowered = trimmed.toLower();
    if (lowered == "red") {
      out = {1.00F, 0.30F, 0.30F};
      return true;
    }
    if (lowered == "brown") {
      out = {0.55F, 0.36F, 0.18F};
      return true;
    }
    if (lowered == "blue") {
      out = {0.20F, 0.55F, 1.00F};
      return true;
    }
    if (lowered == "green") {
      out = {0.20F, 0.80F, 0.40F};
      return true;
    }
    if (lowered == "yellow") {
      out = {1.00F, 0.80F, 0.20F};
      return true;
    }
    if (lowered == "orange") {
      out = {0.95F, 0.55F, 0.10F};
      return true;
    }
    if (lowered == "white") {
      out = {0.95F, 0.95F, 0.95F};
      return true;
    }
    if (lowered == "black") {
      out = {0.15F, 0.15F, 0.15F};
      return true;
    }

    return false;
  };

  auto apply_owner_color = [&](int owner_id, const QString &color_name) {
    std::array<float, 3> color;
    if (!parse_color(color_name, color)) {
      return;
    }
    owner_registry.set_owner_color(owner_id, color[0], color[1], color[2]);
  };

  auto spawn_units_for_owner =
      [&](int owner_id, const Game::Systems::NationID nation_id,
          const std::vector<Game::Mission::UnitSetup> &units) {
        const bool ai_controlled = owner_registry.is_ai(owner_id);
        for (const auto &unit_setup : units) {
          const auto spawn_type =
              Game::Units::spawn_typeFromString(unit_setup.type.toStdString());
          if (!spawn_type.has_value()) {
            qWarning() << "Mission setup: unknown unit type" << unit_setup.type;
            continue;
          }

          const int count = std::max(1, unit_setup.count);
          const int grid =
              static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
          const QVector3D base_pos =
              mission_position_to_world(unit_setup.position);
          float base_tile_size = m_level.tile_size;
          if (map_loaded &&
              map_def.coordSystem == Game::Map::CoordSystem::Grid) {
            base_tile_size = map_def.grid.tile_size;
          }
          const float spacing = std::max(0.5F, base_tile_size * 1.2F);

          for (int i = 0; i < count; ++i) {
            const int row = i / grid;
            const int col = i % grid;
            const float offset_x = (float(col) - (grid - 1) * 0.5F) * spacing;
            const float offset_z = (float(row) - (grid - 1) * 0.5F) * spacing;
            const QVector3D pos = QVector3D(
                base_pos.x() + offset_x, base_pos.y(), base_pos.z() + offset_z);

            Game::Units::SpawnParams sp;
            sp.position = pos;
            sp.player_id = owner_id;
            sp.spawn_type = spawn_type.value();
            sp.ai_controlled = ai_controlled;
            sp.nation_id = nation_id;

            auto unit = reg->create(sp.spawn_type, *m_world, sp);
            if (!unit) {
              qWarning() << "Mission setup: failed to spawn unit"
                         << unit_setup.type << "for owner" << owner_id;
              continue;
            }
            apply_team_color(m_world->get_entity(unit->id()), owner_id);
          }
        }
      };

  auto spawn_buildings_for_owner =
      [&](int owner_id, const Game::Systems::NationID nation_id,
          const std::vector<Game::Mission::BuildingSetup> &buildings) {
        const bool ai_controlled = owner_registry.is_ai(owner_id);
        for (const auto &building_setup : buildings) {
          const auto spawn_type = Game::Units::spawn_typeFromString(
              building_setup.type.toStdString());
          if (!spawn_type.has_value()) {
            qWarning() << "Mission setup: unknown building type"
                       << building_setup.type;
            continue;
          }

          const QVector3D pos =
              mission_position_to_world(building_setup.position);

          Game::Units::SpawnParams sp;
          sp.position = pos;
          sp.player_id = owner_id;
          sp.spawn_type = spawn_type.value();
          sp.ai_controlled = ai_controlled;
          sp.nation_id = nation_id;
          sp.max_population = building_setup.max_population;

          auto unit = reg->create(sp.spawn_type, *m_world, sp);
          if (!unit) {
            qWarning() << "Mission setup: failed to spawn building"
                       << building_setup.type << "for owner" << owner_id;
            continue;
          }
          apply_team_color(m_world->get_entity(unit->id()), owner_id);
        }
      };

  const int local_owner_id = m_runtime.local_owner_id;
  if (owner_registry.get_owner_type(local_owner_id) ==
      Game::Systems::OwnerType::Neutral) {
    owner_registry.register_owner_with_id(
        local_owner_id, Game::Systems::OwnerType::Player,
        "Player " + std::to_string(local_owner_id));
  }
  owner_registry.set_owner_team(local_owner_id, 0);

  const auto player_nation_id = resolve_nation_id(mission.player_setup.nation);
  nation_registry.set_player_nation(local_owner_id, player_nation_id);
  apply_owner_color(local_owner_id, mission.player_setup.color);

  spawn_units_for_owner(local_owner_id, player_nation_id,
                        mission.player_setup.starting_units);
  spawn_buildings_for_owner(local_owner_id, player_nation_id,
                            mission.player_setup.starting_buildings);

  int ai_owner_id = 2;
  int default_team_id = 1;
  for (const auto &ai_setup : mission.ai_setups) {
    if (owner_registry.get_owner_type(ai_owner_id) ==
        Game::Systems::OwnerType::Neutral) {
      owner_registry.register_owner_with_id(
          ai_owner_id, Game::Systems::OwnerType::AI,
          "AI Player " + std::to_string(ai_owner_id));
    }

    int team_id;
    if (ai_setup.team_id.has_value()) {
      team_id = ai_setup.team_id.value();
    } else {
      team_id = default_team_id++;
    }

    owner_registry.set_owner_team(ai_owner_id, team_id);

    const auto ai_nation_id = resolve_nation_id(ai_setup.nation);
    nation_registry.set_player_nation(ai_owner_id, ai_nation_id);
    apply_owner_color(ai_owner_id, ai_setup.color);

    spawn_units_for_owner(ai_owner_id, ai_nation_id, ai_setup.starting_units);
    spawn_buildings_for_owner(ai_owner_id, ai_nation_id,
                              ai_setup.starting_buildings);
    ai_owner_id++;
  }

  auto entities = m_world->get_entities_with<Engine::Core::UnitComponent>();
  for (auto *entity : entities) {
    if (entity == nullptr) {
      continue;
    }
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    apply_team_color(entity, unit->owner_id);
  }

  if (auto *ai_system = m_world->get_system<Game::Systems::AISystem>()) {
    ai_system->reinitialize();

    int ai_id = 2;
    for (const auto &ai_setup : mission.ai_setups) {
      Game::Systems::AI::AIStrategy strategy =
          Game::Systems::AI::AIStrategy::Balanced;

      if (ai_setup.strategy.has_value()) {
        strategy = Game::Systems::AI::AIStrategyFactory::parse_strategy(
            ai_setup.strategy.value());
      }

      ai_system->set_ai_strategy(
          ai_id, strategy, ai_setup.personality.aggression,
          ai_setup.personality.defense, ai_setup.personality.harassment);
      ai_id++;
    }
  }

  int prev_selected_player = m_selected_player_id;
  GameStateRestorer::rebuild_registries_after_load(
      m_world.get(), m_selected_player_id, m_level, m_runtime.local_owner_id);
  GameStateRestorer::rebuild_entity_cache(m_world.get(), m_entity_cache,
                                          m_runtime.local_owner_id);

  if (m_selected_player_id != prev_selected_player) {
    emit selected_player_id_changed();
  }
  emit troop_count_changed();
  emit owner_info_changed();
}

void GameEngine::configure_mission_victory_conditions() {
  if (!m_campaign_manager || !m_victoryService) {
    return;
  }

  m_campaign_manager->configure_mission_victory_conditions(
      m_victoryService.get(), m_runtime.local_owner_id);

  m_victoryService->set_victory_callback([this](const QString &state) {
    if (m_runtime.victory_state != state) {
      m_runtime.victory_state = state;
      emit victory_state_changed();

      if (state == "victory" &&
          !m_campaign_manager->current_campaign_id().isEmpty()) {
        mark_current_mission_completed();
      }
    }
  });
}

void GameEngine::configure_rain_system() {
  if (m_rainManager) {
    m_rainManager->configure(m_level.rain, m_level.biome_seed);
  }

  if (!m_rain) {
    return;
  }

  const float world_width =
      static_cast<float>(m_level.grid_width) * m_level.tile_size;
  const float world_height =
      static_cast<float>(m_level.grid_height) * m_level.tile_size;
  m_rain->configure(world_width, world_height, m_level.biome_seed,
                    m_level.rain.type);
  m_rain->set_enabled(m_level.rain.enabled);
  m_rain->set_wind_strength(m_level.rain.wind_strength);

  const float initial_intensity =
      m_rainManager ? m_rainManager->get_intensity()
                    : (m_level.rain.enabled ? m_level.rain.intensity : 0.0F);
  m_rain->set_intensity(initial_intensity);
}

void GameEngine::finalize_skirmish_load() {
  m_runtime.loading = false;
  m_loading_overlay_wait_for_first_frame = true;
  m_loading_overlay_frames_remaining = 5;
  m_loading_overlay_min_duration_ms = 1000;
  m_loading_overlay_timer.restart();
  m_finalize_progress_after_overlay = true;

  m_show_objectives_after_loading = is_campaign_mission();

  emit is_loading_changed();

  GameStateRestorer::rebuild_entity_cache(m_world.get(), m_entity_cache,
                                          m_runtime.local_owner_id);
  emit troop_count_changed();

  m_ambient_state_manager = std::make_unique<AmbientStateManager>();

  Engine::Core::EventManager::instance().publish(
      Engine::Core::AmbientStateChangedEvent(
          Engine::Core::AmbientState::PEACEFUL,
          Engine::Core::AmbientState::PEACEFUL));

  if (m_input_handler) {
    m_input_handler->set_spectator_mode(m_level.is_spectator_mode);
  }

  emit owner_info_changed();
  emit spectator_mode_changed();
}

void GameEngine::open_settings() {
  if (m_saveLoadService) {
    m_saveLoadService->open_settings();
  }
}

void GameEngine::load_save() { load_from_slot("savegame"); }

void GameEngine::save_game(const QString &filename) {
  save_to_slot(filename, filename);
}

void GameEngine::save_game_to_slot(const QString &slotName) {
  save_to_slot(slotName, slotName);
}

void GameEngine::load_game_from_slot(const QString &slotName) {
  load_from_slot(slotName);
}

auto GameEngine::load_from_slot(const QString &slot) -> bool {
  if (!m_saveLoadService || !m_world) {
    set_error("Load: not initialized");
    return false;
  }

  m_finalize_progress_after_overlay = false;
  m_loading_overlay_active = true;
  m_runtime.loading = true;
  emit is_loading_changed();

  if (!m_saveLoadService->load_game_from_slot(*m_world, slot)) {
    set_error(m_saveLoadService->get_last_error());
    m_runtime.loading = false;
    m_loading_overlay_active = false;
    m_loading_overlay_wait_for_first_frame = false;
    m_finalize_progress_after_overlay = false;
    m_show_objectives_after_loading = false;
    emit is_loading_changed();
    return false;
  }

  const QJsonObject meta = m_saveLoadService->get_last_metadata();

  if (m_campaign_manager && meta.contains("mission_mode")) {
    Game::Mission::MissionContext mission_context;
    mission_context.mode = meta["mission_mode"].toString();
    mission_context.campaign_id = meta["mission_campaign_id"].toString();
    mission_context.mission_id = meta["mission_id"].toString();
    mission_context.difficulty = meta["mission_difficulty"].toString();
    m_campaign_manager->set_mission_context(mission_context);
  }

  Game::Systems::GameStateSerializer::restore_level_from_metadata(meta,
                                                                  m_level);
  Game::Systems::GameStateSerializer::restore_camera_from_metadata(
      meta, m_camera.get(), m_viewport.width, m_viewport.height);

  Game::Systems::RuntimeSnapshot runtime_snap = to_runtime_snapshot();
  Game::Systems::GameStateSerializer::restore_runtime_from_metadata(
      meta, runtime_snap);
  apply_runtime_snapshot(runtime_snap);

  GameStateRestorer::RendererRefs renderers{
      m_renderer.get(), m_camera.get(), m_ground.get(),   m_terrain.get(),
      m_biome.get(),    m_river.get(),  m_road.get(),     m_riverbank.get(),
      m_bridge.get(),   m_fog.get(),    m_stone.get(),    m_plant.get(),
      m_pine.get(),     m_olive.get(),  m_firecamp.get(), m_rain.get()};
  GameStateRestorer::restore_environment_from_metadata(
      meta, m_world.get(), renderers, m_level, m_runtime.local_owner_id,
      m_viewport);

  auto unit_reg = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::registerBuiltInUnits(*unit_reg);
  Game::Map::MapTransformer::setFactoryRegistry(unit_reg);
  qInfo() << "Factory registry reinitialized after loading saved game";

  GameStateRestorer::rebuild_registries_after_load(
      m_world.get(), m_selected_player_id, m_level, m_runtime.local_owner_id);
  GameStateRestorer::rebuild_entity_cache(m_world.get(), m_entity_cache,
                                          m_runtime.local_owner_id);

  emit troop_count_changed();

  if (auto *ai_system = m_world->get_system<Game::Systems::AISystem>()) {
    qInfo() << "Reinitializing AI system after loading saved game";
    ai_system->reinitialize();
  }

  if (m_victoryService) {
    m_victoryService->configure(Game::Map::VictoryConfig(),
                                m_runtime.local_owner_id);
  }

  m_runtime.loading = false;
  m_loading_overlay_wait_for_first_frame = true;
  m_loading_overlay_frames_remaining = 5;
  m_loading_overlay_min_duration_ms = 1000;
  m_loading_overlay_timer.restart();
  m_finalize_progress_after_overlay = true;
  emit is_loading_changed();
  qInfo() << "Game load complete, victory/defeat checks re-enabled";

  emit selected_units_changed();
  emit owner_info_changed();
  return true;
}

auto GameEngine::save_to_slot(const QString &slot,
                              const QString &title) -> bool {
  if (!m_saveLoadService || !m_world) {
    set_error("Save: not initialized");
    return false;
  }
  Game::Systems::RuntimeSnapshot const runtime_snap = to_runtime_snapshot();
  QJsonObject meta = Game::Systems::GameStateSerializer::build_metadata(
      *m_world, m_camera.get(), m_level, runtime_snap);
  meta["title"] = title;

  if (m_campaign_manager) {
    const auto &mission_context = m_campaign_manager->current_mission_context();
    meta["mission_mode"] = mission_context.mode;
    meta["mission_campaign_id"] = mission_context.campaign_id;
    meta["mission_id"] = mission_context.mission_id;
    meta["mission_difficulty"] = mission_context.difficulty;
  }

  const QByteArray screenshot = capture_screenshot();
  if (!m_saveLoadService->save_game_to_slot(
          *m_world, slot, title, m_level.map_name, meta, screenshot)) {
    set_error(m_saveLoadService->get_last_error());
    return false;
  }
  emit save_slots_changed();
  return true;
}

auto GameEngine::get_save_slots() const -> QVariantList {
  if (!m_saveLoadService) {
    qWarning() << "Cannot get save slots: service not initialized";
    return {};
  }

  return m_saveLoadService->get_save_slots();
}

void GameEngine::refresh_save_slots() { emit save_slots_changed(); }

auto GameEngine::delete_save_slot(const QString &slotName) -> bool {
  if (!m_saveLoadService) {
    qWarning() << "Cannot delete save slot: service not initialized";
    return false;
  }

  bool const success = m_saveLoadService->delete_save_slot(slotName);

  if (!success) {
    QString const error = m_saveLoadService->get_last_error();
    qWarning() << "Failed to delete save slot:" << error;
    set_error(error);
  } else {
    emit save_slots_changed();
  }

  return success;
}

auto GameEngine::to_runtime_snapshot() const -> Game::Systems::RuntimeSnapshot {
  Game::Systems::RuntimeSnapshot snapshot;
  snapshot.paused = m_runtime.paused;
  snapshot.time_scale = m_runtime.time_scale;
  snapshot.local_owner_id = m_runtime.local_owner_id;
  snapshot.victory_state = m_runtime.victory_state;
  snapshot.cursor_mode = static_cast<int>(m_runtime.cursor_mode);
  snapshot.selected_player_id = m_selected_player_id;
  snapshot.follow_selection = m_followSelectionEnabled;
  return snapshot;
}

void GameEngine::apply_runtime_snapshot(
    const Game::Systems::RuntimeSnapshot &snapshot) {
  m_runtime.paused = snapshot.paused;
  m_runtime.time_scale = snapshot.time_scale;
  m_runtime.local_owner_id = snapshot.local_owner_id;
  m_runtime.victory_state = snapshot.victory_state;
  m_selected_player_id = snapshot.selected_player_id;
  m_followSelectionEnabled = snapshot.follow_selection;

  m_runtime.cursor_mode = static_cast<CursorMode>(snapshot.cursor_mode);
  if (m_cursor_manager) {
    m_cursor_manager->set_mode(m_runtime.cursor_mode);
  }
}

auto GameEngine::capture_screenshot() const -> QByteArray { return {}; }

void GameEngine::exit_game() {
  if (m_saveLoadService) {
    m_saveLoadService->exit_game();
  }
}

auto GameEngine::get_owner_info() const -> QVariantList {
  QVariantList result;
  const auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  const auto &owners = owner_registry.get_all_owners();

  for (const auto &owner : owners) {
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

    result.append(owner_map);
  }

  return result;
}

void GameEngine::get_selected_unit_ids(
    std::vector<Engine::Core::EntityID> &out) const {
  out.clear();
  if (!m_selectionController) {
    return;
  }
  m_selectionController->get_selected_unit_ids(out);
}

auto GameEngine::get_unit_type_key(Engine::Core::EntityID id,
                                   QString &type_key) const -> bool {
  type_key.clear();
  if (!m_world) {
    return false;
  }
  auto *e = m_world->get_entity(id);
  if (e == nullptr) {
    return false;
  }
  if (auto *u = e->get_component<Engine::Core::UnitComponent>()) {
    type_key = Game::Units::spawn_typeToQString(u->spawn_type);
    return true;
  }
  return false;
}

auto GameEngine::get_unit_info(Engine::Core::EntityID id, QString &name,
                               int &health, int &max_health, bool &is_building,
                               bool &alive, QString &nation) const -> bool {
  if (!m_world) {
    return false;
  }
  auto *e = m_world->get_entity(id);
  if (e == nullptr) {
    return false;
  }
  is_building = e->has_component<Engine::Core::BuildingComponent>();
  if (auto *u = e->get_component<Engine::Core::UnitComponent>()) {

    auto troop_type_opt = Game::Units::spawn_typeToTroopType(u->spawn_type);
    if (troop_type_opt.has_value()) {
      auto profile = Game::Systems::TroopProfileService::instance().get_profile(
          u->nation_id, *troop_type_opt);
      name = QString::fromStdString(profile.display_name);
    } else {

      name = QString::fromStdString(
          Game::Units::spawn_typeToString(u->spawn_type));
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
                                       float &stamina_ratio, bool &is_running,
                                       bool &can_run) const -> bool {
  stamina_ratio = 1.0F;
  is_running = false;
  can_run = false;

  if (!m_world) {
    return false;
  }
  auto *e = m_world->get_entity(id);
  if (e == nullptr) {
    return false;
  }

  auto *unit = e->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return false;
  }

  can_run = Game::Units::can_use_run_mode(unit->spawn_type);

  auto *stamina = e->get_component<Engine::Core::StaminaComponent>();
  if (stamina != nullptr) {
    stamina_ratio = stamina->get_stamina_ratio();
    is_running = stamina->is_running;
  }

  return true;
}

void GameEngine::on_unit_spawned(const Engine::Core::UnitSpawnedEvent &event) {
  auto &owners = Game::Systems::OwnerRegistry::instance();

  if (event.owner_id == m_runtime.local_owner_id) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.player_barracks_alive = true;
    } else {
      int const production_cost =
          Game::Units::TroopConfig::instance().getProductionCost(
              event.spawn_type);
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
}

void GameEngine::on_unit_died(const Engine::Core::UnitDiedEvent &event) {
  auto &owners = Game::Systems::OwnerRegistry::instance();

  if (event.owner_id == m_runtime.local_owner_id) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.player_barracks_alive = false;
    } else {
      int const production_cost =
          Game::Units::TroopConfig::instance().getProductionCost(
              event.spawn_type);
      m_entity_cache.player_troop_count -= production_cost;
      m_entity_cache.player_troop_count =
          std::max(0, m_entity_cache.player_troop_count);
    }
  } else if (owners.is_ai(event.owner_id)) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.enemy_barracks_count--;
      m_entity_cache.enemy_barracks_count =
          std::max(0, m_entity_cache.enemy_barracks_count);
      m_entity_cache.enemy_barracks_alive =
          (m_entity_cache.enemy_barracks_count > 0);
    }
  }
}

auto GameEngine::minimap_image() const -> QImage {
  if (m_minimap_manager) {
    return m_minimap_manager->get_image();
  }
  return QImage();
}

auto GameEngine::generate_map_preview(const QString &map_path,
                                      const QVariantList &player_configs) const
    -> QImage {
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
  return QString();
}
