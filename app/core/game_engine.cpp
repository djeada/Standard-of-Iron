#include "game_engine.h"

#include "../controllers/action_vfx.h"
#include "../controllers/command_controller.h"
#include "../models/audio_system_proxy.h"
#include "../models/cursor_manager.h"
#include "../models/hover_tracker.h"
#include "AudioEventHandler.h"
#include "ambient_state_manager.h"
#include "app/models/cursor_mode.h"
#include "app/utils/engine_view_helpers.h"
#include "app/utils/movement_utils.h"
#include "app/utils/selection_utils.h"
#include "audio_resource_loader.h"
#include "camera_controller.h"
#include "core/system.h"
#include "game/audio/AudioSystem.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game_state_restorer.h"
#include "input_command_handler.h"
#include "level_orchestrator.h"
#include "minimap_manager.h"
#include "renderer_bootstrap.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QImage>
#include <QOpenGLContext>
#include <QPainter>
#include <QQuickWindow>
#include <QSize>
#include <QVariant>
#include <QVariantMap>
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
#include "game/map/environment.h"
#include "game/map/level_loader.h"
#include "game/map/map_catalog.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/minimap/minimap_generator.h"
#include "game/map/minimap/map_preview_generator.h"
#include "game/map/minimap/unit_layer.h"
#include "game/map/skirmish_loader.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/map/world_bootstrap.h"
#include "game/systems/ai_system.h"
#include "game/systems/arrow_system.h"
#include "game/systems/ballista_attack_system.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/camera_service.h"
#include "game/systems/capture_system.h"
#include "game/systems/catapult_attack_system.h"
#include "game/systems/cleanup_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/command_service.h"
#include "game/systems/formation_planner.h"
#include "game/systems/game_state_serializer.h"
#include "game/systems/global_stats_registry.h"
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
#include "game/systems/save_load_service.h"
#include "game/systems/selection_system.h"
#include "game/systems/terrain_alignment_system.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/victory_service.h"
#include "game/units/factory.h"
#include "game/units/troop_config.h"
#include "render/geom/arrow.h"
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
#include "render/ground/river_renderer.h"
#include "render/ground/riverbank_renderer.h"
#include "render/ground/road_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_renderer.h"
#include "render/scene_renderer.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
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
  m_passes = std::move(rendering.passes);

  RendererBootstrap::initialize_world_systems(*m_world);

  m_pickingService = std::make_unique<Game::Systems::PickingService>();
  m_victoryService = std::make_unique<Game::Systems::VictoryService>();
  m_saveLoadService = std::make_unique<Game::Systems::SaveLoadService>();
  m_cameraService = std::make_unique<Game::Systems::CameraService>();

  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  m_selectionController = std::make_unique<Game::Systems::SelectionController>(
      m_world.get(), selection_system, m_pickingService.get());
  m_commandController = std::make_unique<App::Controllers::CommandController>(
      m_world.get(), selection_system, m_pickingService.get());

  m_cursorManager = std::make_unique<CursorManager>();
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
      m_cursorManager.get(), m_hoverTracker.get(), m_pickingService.get(),
      m_camera.get());

  m_camera_controller = std::make_unique<CameraController>(
      m_camera.get(), m_cameraService.get(), m_world.get());

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

  connect(m_cursorManager.get(), &CursorManager::modeChanged, this,
          &GameEngine::cursor_mode_changed);
  connect(m_cursorManager.get(), &CursorManager::globalCursorChanged, this,
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
          &App::Controllers::CommandController::attack_targetSelected,
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
          &App::Controllers::CommandController::troopLimitReached, [this]() {
            set_error(
                "Maximum troop limit reached. Cannot produce more units.");
          });
  connect(m_commandController.get(),
          &App::Controllers::CommandController::hold_modeChanged, this,
          &GameEngine::hold_mode_changed);

  connect(this, SIGNAL(selected_units_changed()), m_selectedUnitsModel,
          SLOT(refresh()));
  connect(this, SIGNAL(selected_units_data_changed()), m_selectedUnitsModel,
          SLOT(refresh()));

  emit selected_units_changed();

  m_unit_died_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const Engine::Core::UnitDiedEvent &e) {
            on_unit_died(e);
            if (e.owner_id != m_runtime.local_owner_id) {

              int const individuals_per_unit =
                  Game::Units::TroopConfig::instance().getIndividualsPerUnit(
                      e.spawn_type);
              m_enemyTroopsDefeated += individuals_per_unit;
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

auto GameEngine::any_selected_in_hold_mode() const -> bool {
  if (!m_input_handler) {
    return false;
  }
  return m_input_handler->any_selected_in_hold_mode();
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
  if (!m_cursorManager) {
    return;
  }
  m_cursorManager->setMode(mode);
  m_cursorManager->updateCursorShape(m_window);
}

void GameEngine::set_cursor_mode(const QString &mode) {
  set_cursor_mode(CursorModeUtils::fromString(mode));
}

auto GameEngine::cursor_mode() const -> QString {
  if (!m_cursorManager) {
    return "normal";
  }
  return m_cursorManager->modeString();
}

auto GameEngine::global_cursor_x() const -> qreal {
  if (!m_cursorManager) {
    return 0;
  }
  return m_cursorManager->global_cursor_x(m_window);
}

auto GameEngine::global_cursor_y() const -> qreal {
  if (!m_cursorManager) {
    return 0;
  }
  return m_cursorManager->global_cursor_y(m_window);
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
    result["barracksOwned"] = stats->barracks_owned;
    result["playTimeSec"] = stats->play_time_sec;
    result["gameEnded"] = stats->game_ended;
  } else {
    result["troopsRecruited"] = 0;
    result["enemiesKilled"] = 0;
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
    if (visibility_service.is_initialized()) {
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
      m_minimap_manager->update_fog(dt, m_runtime.local_owner_id);
      auto *selection_system =
          m_world->get_system<Game::Systems::SelectionSystem>();
      m_minimap_manager->update_units(m_world.get(), selection_system);
      emit minimap_image_changed();
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
  if (auto *arrow_system = m_world->get_system<Game::Systems::ArrowSystem>()) {
    if (auto *res = m_renderer->resources()) {
      Render::GL::render_arrows(m_renderer.get(), res, *arrow_system);
    }
  }
  if (auto *projectile_system =
          m_world->get_system<Game::Systems::ProjectileSystem>()) {
    if (auto *res = m_renderer->resources()) {
      Render::GL::render_projectiles(m_renderer.get(), res, *projectile_system);
    }
  }

  if (auto *res = m_renderer->resources()) {
    std::optional<QVector3D> preview_waypoint;
    if (m_commandController && m_commandController->hasPatrolFirstWaypoint()) {
      preview_waypoint = m_commandController->getPatrolFirstWaypoint();
    }
    Render::GL::renderPatrolFlags(m_renderer.get(), res, *m_world,
                                  preview_waypoint);
  }
  m_renderer->end_frame();

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
    if (m_cursorManager && m_cursorManager->mode() != CursorMode::Normal) {
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
  m_commandController->recruitNearSelected(unit_type, m_runtime.local_owner_id);
}

auto GameEngine::get_selected_production_state() const -> QVariantMap {
  QVariantMap m;
  m["has_barracks"] = false;
  m["in_progress"] = false;
  m["time_remaining"] = 0.0;
  m["build_time"] = 0.0;
  m["produced_count"] = 0;
  m["max_units"] = 0;
  m["villager_cost"] = 1;
  if (!m_world) {
    return m;
  }
  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return m;
  }
  Game::Systems::ProductionState st;
  Game::Systems::ProductionService::getSelectedBarracksState(
      *m_world, selection_system->get_selected_units(),
      m_runtime.local_owner_id, st);
  m["has_barracks"] = st.has_barracks;
  m["in_progress"] = st.in_progress;
  m["product_type"] =
      QString::fromStdString(Game::Units::troop_typeToString(st.product_type));
  m["time_remaining"] = st.time_remaining;
  m["build_time"] = st.build_time;
  m["produced_count"] = st.produced_count;
  m["max_units"] = st.max_units;
  m["villager_cost"] = st.villager_cost;
  m["queue_size"] = st.queue_size;
  m["nation_id"] =
      QString::fromStdString(Game::Systems::nationIDToString(st.nation_id));

  QVariantList queue_list;
  for (const auto &unit_type : st.production_queue) {
    queue_list.append(
        QString::fromStdString(Game::Units::troop_typeToString(unit_type)));
  }
  m["production_queue"] = queue_list;

  return m;
}

auto GameEngine::get_unit_production_info(const QString &unit_type) const
    -> QVariantMap {
  QVariantMap info;
  const auto &config = Game::Units::TroopConfig::instance();
  std::string type_str = unit_type.toStdString();

  info["cost"] = config.getProductionCost(type_str);
  info["build_time"] = static_cast<double>(config.getBuildTime(type_str));
  info["individuals_per_unit"] = config.getIndividualsPerUnit(type_str);

  return info;
}

auto GameEngine::get_selected_units_command_mode() const -> QString {
  if (!m_world) {
    return "normal";
  }
  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return "normal";
  }

  const auto &sel = selection_system->get_selected_units();
  if (sel.empty()) {
    return "normal";
  }

  int attacking_count = 0;
  int patrolling_count = 0;
  int total_units = 0;

  for (auto id : sel) {
    auto *e = m_world->get_entity(id);
    if (e == nullptr) {
      continue;
    }

    auto *u = e->get_component<Engine::Core::UnitComponent>();
    if (u == nullptr) {
      continue;
    }
    if (u->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    total_units++;

    if (e->get_component<Engine::Core::AttackTargetComponent>() != nullptr) {
      attacking_count++;
    }

    auto *patrol = e->get_component<Engine::Core::PatrolComponent>();
    if ((patrol != nullptr) && patrol->patrolling) {
      patrolling_count++;
    }
  }

  if (total_units == 0) {
    return "normal";
  }

  if (patrolling_count == total_units) {
    return "patrol";
  }
  if (attacking_count == total_units) {
    return "attack";
  }

  return "normal";
}

void GameEngine::set_rally_at_screen(qreal sx, qreal sy) {
  ensure_initialized();
  if (!m_commandController || !m_camera) {
    return;
  }
  m_commandController->setRallyAtScreen(sx, sy, m_viewport.width,
                                        m_viewport.height, m_camera.get(),
                                        m_runtime.local_owner_id);
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
        QString::fromStdString(Game::Systems::nationIDToString(nation.id)));
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
  return m_available_campaigns;
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

  m_available_campaigns = campaigns;
  emit available_campaigns_changed();
}

void GameEngine::start_campaign_mission(const QString &campaign_id) {
  clear_error();

  if (!m_saveLoadService) {
    set_error("Save/Load service not initialized");
    return;
  }

  QString error;
  auto campaigns = m_saveLoadService->list_campaigns(&error);
  if (!error.isEmpty()) {
    set_error("Failed to load campaign: " + error);
    return;
  }

  QVariantMap selected_campaign;
  for (const auto &campaign : campaigns) {
    auto campaign_map = campaign.toMap();
    if (campaign_map.value("id").toString() == campaign_id) {
      selected_campaign = campaign_map;
      break;
    }
  }

  if (selected_campaign.isEmpty()) {
    set_error("Campaign not found: " + campaign_id);
    return;
  }

  m_current_campaign_id = campaign_id;

  QString map_path = selected_campaign.value("mapPath").toString();

  QVariantList playerConfigs;

  QVariantMap player1;
  player1.insert("player_id", 1);
  player1.insert("playerName", "Carthage");
  player1.insert("colorIndex", 0);
  player1.insert("team_id", 0);
  player1.insert("nationId", "carthage");
  player1.insert("isHuman", true);
  playerConfigs.append(player1);

  QVariantMap player2;
  player2.insert("player_id", 2);
  player2.insert("playerName", "Rome");
  player2.insert("colorIndex", 1);
  player2.insert("team_id", 1);
  player2.insert("nationId", "roman_republic");
  player2.insert("isHuman", false);
  playerConfigs.append(player2);

  start_skirmish(map_path, playerConfigs);
}

void GameEngine::mark_current_mission_completed() {
  if (m_current_campaign_id.isEmpty()) {
    qWarning() << "No active campaign mission to mark as completed";
    return;
  }

  if (!m_saveLoadService) {
    qWarning() << "Save/Load service not initialized";
    return;
  }

  QString error;
  bool success =
      m_saveLoadService->mark_campaign_completed(m_current_campaign_id, &error);
  if (!success) {
    qWarning() << "Failed to mark campaign as completed:" << error;
  } else {
    qInfo() << "Campaign mission" << m_current_campaign_id
            << "marked as completed";
    load_campaigns();
  }
}

void GameEngine::start_skirmish(const QString &map_path,
                                const QVariantList &playerConfigs) {

  clear_error();

  m_level.map_path = map_path;
  m_level.map_name = map_path;

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
    return;
  }

  if (m_world && m_renderer && m_camera) {

    m_runtime.loading = true;

    if (m_hoverTracker) {
      m_hoverTracker->update_hover(-1, -1, *m_world, *m_camera, 0, 0);
    }

    LevelOrchestrator orchestrator;
    LevelOrchestrator::RendererRefs renderers{
        m_renderer.get(), m_camera.get(), m_ground.get(),  m_terrain.get(),
        m_biome.get(),    m_river.get(),  m_road.get(),    m_riverbank.get(),
        m_bridge.get(),   m_fog.get(),    m_stone.get(),   m_plant.get(),
        m_pine.get(),     m_olive.get(),  m_firecamp.get()};

    auto visibility_ready = [this]() {
      m_runtime.visibility_version =
          Game::Map::VisibilityService::instance().version();
      m_runtime.visibility_update_accumulator = 0.0F;
    };

    auto owner_update = [this]() { emit owner_info_changed(); };

    auto load_result = orchestrator.load_skirmish(
        map_path, playerConfigs, m_selected_player_id, *m_world, renderers,
        m_level, m_entity_cache, m_victoryService.get(),
        m_minimap_manager.get(), visibility_ready, owner_update);

    if (load_result.updated_player_id != m_selected_player_id) {
      m_selected_player_id = load_result.updated_player_id;
      emit selected_player_id_changed();
    }

    if (!load_result.success) {
      set_error(load_result.error_message);
      m_runtime.loading = false;
      return;
    }

    m_runtime.local_owner_id = load_result.updated_player_id;

    if (m_victoryService) {
      m_victoryService->setVictoryCallback([this](const QString &state) {
        if (m_runtime.victory_state != state) {
          m_runtime.victory_state = state;
          emit victory_state_changed();

          if (state == "victory" && !m_current_campaign_id.isEmpty()) {
            mark_current_mission_completed();
          }
        }
      });
    }

    m_runtime.loading = false;

    GameStateRestorer::rebuild_entity_cache(m_world.get(), m_entity_cache,
                                            m_runtime.local_owner_id);

    m_ambient_state_manager = std::make_unique<AmbientStateManager>();

    Engine::Core::EventManager::instance().publish(
        Engine::Core::AmbientStateChangedEvent(
            Engine::Core::AmbientState::PEACEFUL,
            Engine::Core::AmbientState::PEACEFUL));

    emit owner_info_changed();
  }
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

  m_runtime.loading = true;

  if (!m_saveLoadService->load_game_from_slot(*m_world, slot)) {
    set_error(m_saveLoadService->get_last_error());
    m_runtime.loading = false;
    return false;
  }

  const QJsonObject meta = m_saveLoadService->get_last_metadata();

  Game::Systems::GameStateSerializer::restoreLevelFromMetadata(meta, m_level);
  Game::Systems::GameStateSerializer::restoreCameraFromMetadata(
      meta, m_camera.get(), m_viewport.width, m_viewport.height);

  Game::Systems::RuntimeSnapshot runtime_snap = to_runtime_snapshot();
  Game::Systems::GameStateSerializer::restoreRuntimeFromMetadata(meta,
                                                                 runtime_snap);
  apply_runtime_snapshot(runtime_snap);

  GameStateRestorer::RendererRefs renderers{
      m_renderer.get(), m_camera.get(), m_ground.get(),  m_terrain.get(),
      m_biome.get(),    m_river.get(),  m_road.get(),    m_riverbank.get(),
      m_bridge.get(),   m_fog.get(),    m_stone.get(),   m_plant.get(),
      m_pine.get(),     m_olive.get(),  m_firecamp.get()};
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

  if (auto *ai_system = m_world->get_system<Game::Systems::AISystem>()) {
    qInfo() << "Reinitializing AI system after loading saved game";
    ai_system->reinitialize();
  }

  if (m_victoryService) {
    m_victoryService->configure(Game::Map::VictoryConfig(),
                                m_runtime.local_owner_id);
  }

  m_runtime.loading = false;
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
  QJsonObject meta = Game::Systems::GameStateSerializer::buildMetadata(
      *m_world, m_camera.get(), m_level, runtime_snap);
  meta["title"] = title;
  const QByteArray screenshot = capture_screenshot();
  if (!m_saveLoadService->save_game_to_slot(*m_world, slot, title,
                                         m_level.map_name, meta, screenshot)) {
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
  if (m_cursorManager) {
    m_cursorManager->setMode(m_runtime.cursor_mode);
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

auto GameEngine::get_unit_info(Engine::Core::EntityID id, QString &name,
                               int &health, int &max_health, bool &isBuilding,
                               bool &alive, QString &nation) const -> bool {
  if (!m_world) {
    return false;
  }
  auto *e = m_world->get_entity(id);
  if (e == nullptr) {
    return false;
  }
  isBuilding = e->has_component<Engine::Core::BuildingComponent>();
  if (auto *u = e->get_component<Engine::Core::UnitComponent>()) {
    name =
        QString::fromStdString(Game::Units::spawn_typeToString(u->spawn_type));
    health = u->health;
    max_health = u->max_health;
    alive = (u->health > 0);
    nation = Game::Systems::nationIDToQString(u->nation_id);
    return true;
  }
  name = QStringLiteral("Entity");
  health = max_health = 0;
  alive = true;
  nation = QStringLiteral("");
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
