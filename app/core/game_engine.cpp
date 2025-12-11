#include "game_engine.h"

#include "../controllers/action_vfx.h"
#include "../controllers/command_controller.h"
#include "../models/audio_system_proxy.h"
#include "../models/cursor_manager.h"
#include "../models/hover_tracker.h"
#include "AudioEventHandler.h"
#include "app/models/cursor_mode.h"
#include "app/utils/engine_view_helpers.h"
#include "app/utils/movement_utils.h"
#include "app/utils/selection_utils.h"
#include "core/system.h"
#include "game/audio/AudioSystem.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QImage>
#include <QOpenGLContext>
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
#include "game/map/skirmish_loader.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/map/world_bootstrap.h"
#include "game/systems/ai_system.h"
#include "game/systems/arrow_system.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/camera_service.h"
#include "game/systems/capture_system.h"
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
#include "game/systems/save_load_service.h"
#include "game/systems/selection_system.h"
#include "game/systems/terrain_alignment_system.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/victory_service.h"
#include "game/units/factory.h"
#include "game/units/troop_config.h"
#include "render/geom/arrow.h"
#include "render/geom/patrol_flags.h"
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

  Game::Systems::NationRegistry::instance().initializeDefaults();
  Game::Systems::TroopCountRegistry::instance().initialize();
  Game::Systems::GlobalStatsRegistry::instance().initialize();

  m_world = std::make_unique<Engine::Core::World>();
  m_renderer = std::make_unique<Render::GL::Renderer>();
  m_camera = std::make_unique<Render::GL::Camera>();
  m_ground = std::make_unique<Render::GL::GroundRenderer>();
  m_terrain = std::make_unique<Render::GL::TerrainRenderer>();
  m_biome = std::make_unique<Render::GL::BiomeRenderer>();
  m_river = std::make_unique<Render::GL::RiverRenderer>();
  m_road = std::make_unique<Render::GL::RoadRenderer>();
  m_riverbank = std::make_unique<Render::GL::RiverbankRenderer>();
  m_bridge = std::make_unique<Render::GL::BridgeRenderer>();
  m_fog = std::make_unique<Render::GL::FogRenderer>();
  m_stone = std::make_unique<Render::GL::StoneRenderer>();
  m_plant = std::make_unique<Render::GL::PlantRenderer>();
  m_pine = std::make_unique<Render::GL::PineRenderer>();
  m_olive = std::make_unique<Render::GL::OliveRenderer>();
  m_firecamp = std::make_unique<Render::GL::FireCampRenderer>();

  m_passes = {m_ground.get(), m_terrain.get(),   m_river.get(),
              m_road.get(),   m_riverbank.get(), m_bridge.get(),
              m_biome.get(),  m_stone.get(),     m_plant.get(),
              m_pine.get(),   m_olive.get(),     m_firecamp.get(),
              m_fog.get()};

  std::unique_ptr<Engine::Core::System> arrow_sys =
      std::make_unique<Game::Systems::ArrowSystem>();
  m_world->add_system(std::move(arrow_sys));

  m_world->add_system(std::make_unique<Game::Systems::MovementSystem>());
  m_world->add_system(std::make_unique<Game::Systems::PatrolSystem>());
  m_world->add_system(std::make_unique<Game::Systems::CombatSystem>());
  m_world->add_system(std::make_unique<Game::Systems::HealingSystem>());
  m_world->add_system(std::make_unique<Game::Systems::CaptureSystem>());
  m_world->add_system(std::make_unique<Game::Systems::AISystem>());
  m_world->add_system(std::make_unique<Game::Systems::ProductionSystem>());
  m_world->add_system(
      std::make_unique<Game::Systems::TerrainAlignmentSystem>());
  m_world->add_system(std::make_unique<Game::Systems::CleanupSystem>());

  {
    std::unique_ptr<Engine::Core::System> sel_sys =
        std::make_unique<Game::Systems::SelectionSystem>();
    m_world->add_system(std::move(sel_sys));
  }

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
  connect(m_mapCatalog.get(), &Game::Map::MapCatalog::mapLoaded, this,
          [this](const QVariantMap &mapData) {
            m_available_maps.append(mapData);
            emit available_maps_changed();
          });
  connect(m_mapCatalog.get(), &Game::Map::MapCatalog::loadingChanged, this,
          [this](bool loading) {
            m_maps_loading = loading;
            emit maps_loading_changed();
          });
  connect(m_mapCatalog.get(), &Game::Map::MapCatalog::allMapsLoaded, this,
          [this]() { emit available_maps_changed(); });

  if (AudioSystem::getInstance().initialize()) {
    qInfo() << "AudioSystem initialized successfully";
    load_audio_resources();
  } else {
    qWarning() << "Failed to initialize AudioSystem";
  }

  m_audio_systemProxy = std::make_unique<App::Models::AudioSystemProxy>(this);

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
  if (m_selectionController && m_camera) {
    m_selectionController->on_click_select(sx, sy, false, m_viewport.width,
                                           m_viewport.height, m_camera.get(),
                                           m_runtime.local_owner_id);
  }
}

void GameEngine::on_right_click(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return;
  }

  if (m_cursorManager->mode() == CursorMode::Patrol ||
      m_cursorManager->mode() == CursorMode::Attack) {
    set_cursor_mode(CursorMode::Normal);
    return;
  }

  const auto &sel = selection_system->get_selected_units();
  if (sel.empty()) {

    return;
  }

  if (m_pickingService && m_camera) {
    Engine::Core::EntityID const target_id = m_pickingService->pick_unit_first(
        float(sx), float(sy), *m_world, *m_camera, m_viewport.width,
        m_viewport.height, 0);

    if (target_id != 0U) {
      auto *target_entity = m_world->get_entity(target_id);
      if (target_entity != nullptr) {
        auto *target_unit =
            target_entity->get_component<Engine::Core::UnitComponent>();
        if (target_unit != nullptr) {

          bool const is_enemy =
              (target_unit->owner_id != m_runtime.local_owner_id);

          if (is_enemy) {

            Game::Systems::CommandService::attack_target(*m_world, sel,
                                                         target_id, true);
            return;
          }
        }
      }
    }
  }

  if (m_pickingService && m_camera) {
    QVector3D hit;
    if (m_pickingService->screen_to_ground(QPointF(sx, sy), *m_camera,
                                           m_viewport.width, m_viewport.height,
                                           hit)) {
      auto targets = Game::Systems::FormationPlanner::spreadFormation(
          int(sel.size()), hit,
          Game::GameConfig::instance().gameplay().formationSpacingDefault);
      Game::Systems::CommandService::MoveOptions opts;
      opts.group_move = sel.size() > 1;
      Game::Systems::CommandService::moveUnits(*m_world, sel, targets, opts);
    }
  }
}

void GameEngine::on_attack_click(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (!m_commandController || !m_camera) {
    return;
  }

  auto result = m_commandController->onAttackClick(
      sx, sy, m_viewport.width, m_viewport.height, m_camera.get());

  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  if ((selection_system == nullptr) || !m_pickingService || !m_camera ||
      !m_world) {
    return;
  }

  const auto &selected = selection_system->get_selected_units();
  if (!selected.empty()) {
    Engine::Core::EntityID const target_id = m_pickingService->pick_unit_first(
        float(sx), float(sy), *m_world, *m_camera, m_viewport.width,
        m_viewport.height, 0);

    if (target_id != 0) {
      auto *target_entity = m_world->get_entity(target_id);
      if (target_entity != nullptr) {
        auto *target_unit =
            target_entity->get_component<Engine::Core::UnitComponent>();
        if ((target_unit != nullptr) &&
            target_unit->owner_id != m_runtime.local_owner_id) {
          App::Controllers::ActionVFX::spawnAttackArrow(m_world.get(),
                                                        target_id);
        }
      }
    }
  }

  if (result.resetCursorToNormal) {
    set_cursor_mode(CursorMode::Normal);
  }
}

void GameEngine::reset_movement(Engine::Core::Entity *entity) {
  App::Utils::reset_movement(entity);
}

void GameEngine::on_stop_command() {
  if (!m_commandController) {
    return;
  }
  ensure_initialized();

  auto result = m_commandController->onStopCommand();
  if (result.resetCursorToNormal) {
    set_cursor_mode(CursorMode::Normal);
  }
}

void GameEngine::on_hold_command() {
  if (!m_commandController) {
    return;
  }
  ensure_initialized();

  auto result = m_commandController->onHoldCommand();
  if (result.resetCursorToNormal) {
    set_cursor_mode(CursorMode::Normal);
  }
}

auto GameEngine::any_selected_in_hold_mode() const -> bool {
  if (!m_commandController) {
    return false;
  }
  return m_commandController->anySelectedInHoldMode();
}

void GameEngine::on_patrol_click(qreal sx, qreal sy) {
  if (!m_commandController || !m_camera) {
    return;
  }
  ensure_initialized();

  auto result = m_commandController->onPatrolClick(
      sx, sy, m_viewport.width, m_viewport.height, m_camera.get());
  if (result.resetCursorToNormal) {
    set_cursor_mode(CursorMode::Normal);
  }
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
  if (!m_hoverTracker || !m_camera || !m_world) {
    return;
  }

  m_cursorManager->updateCursorShape(m_window);

  m_hoverTracker->update_hover(float(sx), float(sy), *m_world, *m_camera,
                               m_viewport.width, m_viewport.height);
}

void GameEngine::on_click_select(qreal sx, qreal sy, bool additive) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_selectionController && m_camera) {
    m_selectionController->on_click_select(sx, sy, additive, m_viewport.width,
                                           m_viewport.height, m_camera.get(),
                                           m_runtime.local_owner_id);
  }
}

void GameEngine::on_area_selected(qreal x1, qreal y1, qreal x2, qreal y2,
                                  bool additive) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_selectionController && m_camera) {
    m_selectionController->on_area_selected(
        x1, y1, x2, y2, additive, m_viewport.width, m_viewport.height,
        m_camera.get(), m_runtime.local_owner_id);
  }
}

void GameEngine::select_all_troops() {
  ensure_initialized();
  if (m_selectionController) {
    m_selectionController->select_all_player_troops(m_runtime.local_owner_id);
  }
}

void GameEngine::select_unit_by_id(int unitId) {
  ensure_initialized();
  if (!m_selectionController || (unitId <= 0)) {
    return;
  }
  m_selectionController->select_single_unit(
      static_cast<Engine::Core::EntityID>(unitId), m_runtime.local_owner_id);
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
    update_ambient_state(dt);
  }

  if (m_renderer) {
    m_renderer->updateAnimationTime(dt);
  }

  if (m_camera) {
    m_camera->update(dt);
  }

  if (m_world) {
    m_world->update(dt);

    auto &visibility_service = Game::Map::VisibilityService::instance();
    if (visibility_service.isInitialized()) {

      m_runtime.visibilityUpdateAccumulator += dt;
      const float visibility_update_interval =
          Game::GameConfig::instance().gameplay().visibility_update_interval;
      if (m_runtime.visibilityUpdateAccumulator >= visibility_update_interval) {
        m_runtime.visibilityUpdateAccumulator = 0.0F;
        visibility_service.update(*m_world, m_runtime.local_owner_id);
      }

      const auto new_version = visibility_service.version();
      if (new_version != m_runtime.visibilityVersion) {
        if (m_fog) {
          m_fog->updateMask(visibility_service.getWidth(),
                            visibility_service.getHeight(),
                            visibility_service.getTileSize(),
                            visibility_service.snapshotCells());
        }
        m_runtime.visibilityVersion = new_version;
      }
    }
  }

  if (m_victoryService && m_world) {
    m_victoryService->update(*m_world, dt);
  }

  if (m_followSelectionEnabled && m_camera && m_world && m_cameraService) {
    m_cameraService->update_follow(*m_camera, *m_world,
                                   m_followSelectionEnabled);
  }

  if (m_selectedUnitsModel != nullptr) {
    auto *selection_system =
        m_world->get_system<Game::Systems::SelectionSystem>();
    if ((selection_system != nullptr) &&
        !selection_system->get_selected_units().empty()) {
      m_runtime.selectionRefreshCounter++;
      if (m_runtime.selectionRefreshCounter >= 15) {
        m_runtime.selectionRefreshCounter = 0;
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
    m_renderer->setViewport(pixelWidth, pixelHeight);
  }
  if (auto *selection_system =
          m_world->get_system<Game::Systems::SelectionSystem>()) {
    const auto &sel = selection_system->get_selected_units();
    std::vector<unsigned int> const ids(sel.begin(), sel.end());
    m_renderer->setSelectedEntities(ids);
  }
  m_renderer->beginFrame();
  if (auto *res = m_renderer->resources()) {
    for (auto *pass : m_passes) {
      if (pass != nullptr) {
        pass->submit(*m_renderer, res);
      }
    }
  }
  if (m_renderer && m_hoverTracker) {
    m_renderer->setHoveredEntityId(m_hoverTracker->getLastHoveredEntity());
  }
  if (m_renderer) {
    m_renderer->setLocalOwnerId(m_runtime.local_owner_id);
  }
  m_renderer->renderWorld(m_world.get());
  if (auto *arrow_system = m_world->get_system<Game::Systems::ArrowSystem>()) {
    if (auto *res = m_renderer->resources()) {
      Render::GL::renderArrows(m_renderer.get(), res, *arrow_system);
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
  m_renderer->endFrame();

  qreal const current_x = global_cursor_x();
  qreal const current_y = global_cursor_y();
  if (current_x != m_runtime.lastCursorX ||
      current_y != m_runtime.lastCursorY) {
    m_runtime.lastCursorX = current_x;
    m_runtime.lastCursorY = current_y;
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
  if (!m_camera || !m_cameraService) {
    return;
  }

  m_cameraService->move(*m_camera, dx, dz);
}

void GameEngine::camera_elevate(float dy) {
  ensure_initialized();
  if (!m_camera || !m_cameraService) {
    return;
  }

  m_cameraService->elevate(*m_camera, dy);
}

void GameEngine::reset_camera() {
  ensure_initialized();
  if (!m_camera || !m_world || !m_cameraService) {
    return;
  }

  m_cameraService->resetCamera(*m_camera, *m_world, m_runtime.local_owner_id,
                               m_level.player_unit_id);
}

void GameEngine::camera_zoom(float delta) {
  ensure_initialized();
  if (!m_camera || !m_cameraService) {
    return;
  }

  m_cameraService->zoom(*m_camera, delta);
}

auto GameEngine::camera_distance() const -> float {
  if (!m_camera || !m_cameraService) {
    return 0.0F;
  }
  return m_cameraService->get_distance(*m_camera);
}

void GameEngine::camera_yaw(float degrees) {
  ensure_initialized();
  if (!m_camera || !m_cameraService) {
    return;
  }

  m_cameraService->yaw(*m_camera, degrees);
}

void GameEngine::camera_orbit(float yaw_deg, float pitch_deg) {
  ensure_initialized();
  if (!m_camera || !m_cameraService) {
    return;
  }

  if (!std::isfinite(yaw_deg) || !std::isfinite(pitch_deg)) {
    qWarning() << "GameEngine::camera_orbit received invalid input, ignoring:"
               << yaw_deg << pitch_deg;
    return;
  }

  m_cameraService->orbit(*m_camera, yaw_deg, pitch_deg);
}

void GameEngine::camera_orbit_direction(int direction, bool shift) {
  if (!m_camera || !m_cameraService) {
    return;
  }

  m_cameraService->orbit_direction(*m_camera, direction, shift);
}

void GameEngine::camera_follow_selection(bool enable) {
  ensure_initialized();
  m_followSelectionEnabled = enable;
  if (!m_camera || !m_world || !m_cameraService) {
    return;
  }

  m_cameraService->follow_selection(*m_camera, *m_world, enable);
}

void GameEngine::camera_set_follow_lerp(float alpha) {
  ensure_initialized();
  if (!m_camera || !m_cameraService) {
    return;
  }

  m_cameraService->setFollowLerp(*m_camera, alpha);
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
  return m_entityCache.playerTroopCount;
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
    m_mapCatalog->loadMapsAsync();
  }
  load_campaigns();
}

auto GameEngine::available_maps() const -> QVariantList {
  return m_available_maps;
}

auto GameEngine::available_nations() const -> QVariantList {
  QVariantList nations;
  const auto &registry = Game::Systems::NationRegistry::instance();
  const auto &all = registry.getAllNations();
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

  QVariantMap selectedCampaign;
  for (const auto &campaign : campaigns) {
    auto campaignMap = campaign.toMap();
    if (campaignMap.value("id").toString() == campaign_id) {
      selectedCampaign = campaignMap;
      break;
    }
  }

  if (selectedCampaign.isEmpty()) {
    set_error("Campaign not found: " + campaign_id);
    return;
  }

  m_current_campaign_id = campaign_id;

  QString mapPath = selectedCampaign.value("mapPath").toString();

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

  start_skirmish(mapPath, playerConfigs);
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

    m_entityCache.reset();

    Game::Map::SkirmishLoader loader(*m_world, *m_renderer, *m_camera);
    loader.setGroundRenderer(m_ground.get());
    loader.setTerrainRenderer(m_terrain.get());
    loader.setBiomeRenderer(m_biome.get());
    loader.setRiverRenderer(m_river.get());
    loader.setRoadRenderer(m_road.get());
    loader.setRiverbankRenderer(m_riverbank.get());
    loader.setBridgeRenderer(m_bridge.get());
    loader.setFogRenderer(m_fog.get());
    loader.setStoneRenderer(m_stone.get());
    loader.setPlantRenderer(m_plant.get());
    loader.setPineRenderer(m_pine.get());
    loader.setOliveRenderer(m_olive.get());
    loader.setFireCampRenderer(m_firecamp.get());

    loader.setOnOwnersUpdated([this]() { emit owner_info_changed(); });

    loader.setOnVisibilityMaskReady([this]() {
      m_runtime.visibilityVersion =
          Game::Map::VisibilityService::instance().version();
      m_runtime.visibilityUpdateAccumulator = 0.0F;
    });

    int updated_player_id = m_selectedPlayerId;
    auto result = loader.start(map_path, playerConfigs, m_selectedPlayerId,
                               updated_player_id);

    if (updated_player_id != m_selectedPlayerId) {
      m_selectedPlayerId = updated_player_id;
      emit selected_player_id_changed();
    }

    if (!result.ok && !result.errorMessage.isEmpty()) {
      set_error(result.errorMessage);
    }

    m_runtime.local_owner_id = updated_player_id;
    m_level.map_name = result.map_name;
    m_level.player_unit_id = result.player_unit_id;
    m_level.cam_fov = result.cam_fov;
    m_level.cam_near = result.cam_near;
    m_level.cam_far = result.cam_far;
    m_level.max_troops_per_player = result.max_troops_per_player;

    Game::GameConfig::instance().setMaxTroopsPerPlayer(
        result.max_troops_per_player);

    if (m_victoryService) {
      m_victoryService->configure(result.victoryConfig,
                                  m_runtime.local_owner_id);
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

    if (result.hasFocusPosition && m_camera) {
      const auto &cam_config = Game::GameConfig::instance().camera();
      m_camera->setRTSView(result.focusPosition, cam_config.defaultDistance,
                           cam_config.defaultPitch, cam_config.defaultYaw);
    }

    m_runtime.loading = false;

    if (auto *ai_system = m_world->get_system<Game::Systems::AISystem>()) {
      ai_system->reinitialize();
    }

    rebuild_entity_cache();
    auto &troops = Game::Systems::TroopCountRegistry::instance();
    troops.rebuild_from_world(*m_world);

    auto &stats_registry = Game::Systems::GlobalStatsRegistry::instance();
    stats_registry.rebuild_from_world(*m_world);

    auto &owner_registry = Game::Systems::OwnerRegistry::instance();
    const auto &all_owners = owner_registry.getAllOwners();
    for (const auto &owner : all_owners) {
      if (owner.type == Game::Systems::OwnerType::Player ||
          owner.type == Game::Systems::OwnerType::AI) {
        stats_registry.mark_game_start(owner.owner_id);
      }
    }

    m_currentAmbientState = Engine::Core::AmbientState::PEACEFUL;
    m_ambientCheckTimer = 0.0F;

    Engine::Core::EventManager::instance().publish(
        Engine::Core::AmbientStateChangedEvent(
            Engine::Core::AmbientState::PEACEFUL,
            Engine::Core::AmbientState::PEACEFUL));

    emit owner_info_changed();
  }
}

void GameEngine::open_settings() {
  if (m_saveLoadService) {
    m_saveLoadService->openSettings();
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

  if (!m_saveLoadService->loadGameFromSlot(*m_world, slot)) {
    set_error(m_saveLoadService->getLastError());
    m_runtime.loading = false;
    return false;
  }

  const QJsonObject meta = m_saveLoadService->getLastMetadata();

  Game::Systems::GameStateSerializer::restoreLevelFromMetadata(meta, m_level);
  Game::Systems::GameStateSerializer::restoreCameraFromMetadata(
      meta, m_camera.get(), m_viewport.width, m_viewport.height);

  Game::Systems::RuntimeSnapshot runtime_snap = to_runtime_snapshot();
  Game::Systems::GameStateSerializer::restoreRuntimeFromMetadata(meta,
                                                                 runtime_snap);
  apply_runtime_snapshot(runtime_snap);

  restore_environment_from_metadata(meta);

  auto unit_reg = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::registerBuiltInUnits(*unit_reg);
  Game::Map::MapTransformer::setFactoryRegistry(unit_reg);
  qInfo() << "Factory registry reinitialized after loading saved game";

  rebuild_registries_after_load();
  rebuild_entity_cache();

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
  if (!m_saveLoadService->saveGameToSlot(*m_world, slot, title,
                                         m_level.map_name, meta, screenshot)) {
    set_error(m_saveLoadService->getLastError());
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

  return m_saveLoadService->getSaveSlots();
}

void GameEngine::refresh_save_slots() { emit save_slots_changed(); }

auto GameEngine::delete_save_slot(const QString &slotName) -> bool {
  if (!m_saveLoadService) {
    qWarning() << "Cannot delete save slot: service not initialized";
    return false;
  }

  bool const success = m_saveLoadService->deleteSaveSlot(slotName);

  if (!success) {
    QString const error = m_saveLoadService->getLastError();
    qWarning() << "Failed to delete save slot:" << error;
    set_error(error);
  } else {
    emit save_slots_changed();
  }

  return success;
}

void GameEngine::exit_game() {
  if (m_saveLoadService) {
    m_saveLoadService->exitGame();
  }
}

auto GameEngine::get_owner_info() const -> QVariantList {
  QVariantList result;
  const auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  const auto &owners = owner_registry.getAllOwners();

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
      m_entityCache.playerBarracksAlive = true;
    } else {
      int const individuals_per_unit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              event.spawn_type);
      m_entityCache.playerTroopCount += individuals_per_unit;
    }
  } else if (owners.isAI(event.owner_id)) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entityCache.enemyBarracksCount++;
      m_entityCache.enemyBarracksAlive = true;
    }
  }

  auto emit_if_changed = [&] {
    if (m_entityCache.playerTroopCount != m_runtime.lastTroopCount) {
      m_runtime.lastTroopCount = m_entityCache.playerTroopCount;
      emit troop_count_changed();
    }
  };
  emit_if_changed();
}

void GameEngine::on_unit_died(const Engine::Core::UnitDiedEvent &event) {
  auto &owners = Game::Systems::OwnerRegistry::instance();

  if (event.owner_id == m_runtime.local_owner_id) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entityCache.playerBarracksAlive = false;
    } else {
      int const individuals_per_unit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              event.spawn_type);
      m_entityCache.playerTroopCount -= individuals_per_unit;
      m_entityCache.playerTroopCount =
          std::max(0, m_entityCache.playerTroopCount);
    }
  } else if (owners.isAI(event.owner_id)) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entityCache.enemyBarracksCount--;
      m_entityCache.enemyBarracksCount =
          std::max(0, m_entityCache.enemyBarracksCount);
      m_entityCache.enemyBarracksAlive = (m_entityCache.enemyBarracksCount > 0);
    }
  }

  sync_selection_flags();

  auto emit_if_changed = [&] {
    if (m_entityCache.playerTroopCount != m_runtime.lastTroopCount) {
      m_runtime.lastTroopCount = m_entityCache.playerTroopCount;
      emit troop_count_changed();
    }
  };
  emit_if_changed();
}

void GameEngine::rebuild_entity_cache() {
  if (!m_world) {
    m_entityCache.reset();
    return;
  }

  m_entityCache.reset();

  auto &owners = Game::Systems::OwnerRegistry::instance();
  auto entities = m_world->get_entities_with<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    if (unit->owner_id == m_runtime.local_owner_id) {
      if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
        m_entityCache.playerBarracksAlive = true;
      } else {
        int const individuals_per_unit =
            Game::Units::TroopConfig::instance().getIndividualsPerUnit(
                unit->spawn_type);
        m_entityCache.playerTroopCount += individuals_per_unit;
      }
    } else if (owners.isAI(unit->owner_id)) {
      if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
        m_entityCache.enemyBarracksCount++;
        m_entityCache.enemyBarracksAlive = true;
      }
    }
  }

  auto emit_if_changed = [&] {
    if (m_entityCache.playerTroopCount != m_runtime.lastTroopCount) {
      m_runtime.lastTroopCount = m_entityCache.playerTroopCount;
      emit troop_count_changed();
    }
  };
  emit_if_changed();
}

void GameEngine::rebuild_registries_after_load() {
  if (!m_world) {
    return;
  }

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  m_runtime.local_owner_id = owner_registry.getLocalPlayerId();

  auto &troops = Game::Systems::TroopCountRegistry::instance();
  troops.rebuild_from_world(*m_world);

  auto &stats_registry = Game::Systems::GlobalStatsRegistry::instance();
  stats_registry.rebuild_from_world(*m_world);

  const auto &all_owners = owner_registry.getAllOwners();
  for (const auto &owner : all_owners) {
    if (owner.type == Game::Systems::OwnerType::Player ||
        owner.type == Game::Systems::OwnerType::AI) {
      stats_registry.mark_game_start(owner.owner_id);
    }
  }

  rebuild_building_collisions();

  m_level.player_unit_id = 0;
  auto units = m_world->get_entities_with<Engine::Core::UnitComponent>();
  for (auto *entity : units) {
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    if (unit->owner_id == m_runtime.local_owner_id) {
      m_level.player_unit_id = entity->get_id();
      break;
    }
  }

  if (m_selectedPlayerId != m_runtime.local_owner_id) {
    m_selectedPlayerId = m_runtime.local_owner_id;
    emit selected_player_id_changed();
  }
}

void GameEngine::rebuild_building_collisions() {
  auto &registry = Game::Systems::BuildingCollisionRegistry::instance();
  registry.clear();
  if (!m_world) {
    return;
  }

  auto buildings =
      m_world->get_entities_with<Engine::Core::BuildingComponent>();
  for (auto *entity : buildings) {
    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((transform == nullptr) || (unit == nullptr)) {
      continue;
    }

    registry.registerBuilding(
        entity->get_id(), Game::Units::spawn_typeToString(unit->spawn_type),
        transform->position.x, transform->position.z, unit->owner_id);
  }
}

auto GameEngine::to_runtime_snapshot() const -> Game::Systems::RuntimeSnapshot {
  Game::Systems::RuntimeSnapshot snap;
  snap.paused = m_runtime.paused;
  snap.time_scale = m_runtime.time_scale;
  snap.local_owner_id = m_runtime.local_owner_id;
  snap.victory_state = m_runtime.victory_state;
  snap.cursor_mode = CursorModeUtils::toInt(m_runtime.cursor_mode);
  snap.selected_player_id = m_selectedPlayerId;
  snap.follow_selection = m_followSelectionEnabled;
  return snap;
}

void GameEngine::apply_runtime_snapshot(
    const Game::Systems::RuntimeSnapshot &snapshot) {
  m_runtime.local_owner_id = snapshot.local_owner_id;
  set_paused(snapshot.paused);
  set_game_speed(snapshot.time_scale);

  if (snapshot.victory_state != m_runtime.victory_state) {
    m_runtime.victory_state = snapshot.victory_state;
    emit victory_state_changed();
  }

  set_cursor_mode(CursorModeUtils::fromInt(snapshot.cursor_mode));

  if (snapshot.selected_player_id != m_selectedPlayerId) {
    m_selectedPlayerId = snapshot.selected_player_id;
    emit selected_player_id_changed();
  }

  if (snapshot.follow_selection != m_followSelectionEnabled) {
    m_followSelectionEnabled = snapshot.follow_selection;
    if (m_camera && m_cameraService && m_world) {
      m_cameraService->follow_selection(*m_camera, *m_world,
                                        m_followSelectionEnabled);
    }
  }
}

auto GameEngine::capture_screenshot() const -> QByteArray {
  if (m_window == nullptr) {
    return {};
  }

  QImage const image = m_window->grabWindow();
  if (image.isNull()) {
    return {};
  }

  const QSize target_size(320, 180);
  QImage const scaled =
      image.scaled(target_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  QByteArray buffer;
  QBuffer q_buffer(&buffer);
  if (!q_buffer.open(QIODevice::WriteOnly)) {
    return {};
  }

  if (!scaled.save(&q_buffer, "PNG")) {
    return {};
  }

  return buffer;
}

void GameEngine::restore_environment_from_metadata(
    const QJsonObject &metadata) {
  if (!m_world) {
    return;
  }

  const auto fallback_grid_width = metadata.value("grid_width").toInt(50);
  const auto fallback_grid_height = metadata.value("grid_height").toInt(50);
  const float fallback_tile_size =
      static_cast<float>(metadata.value("tile_size").toDouble(1.0));

  auto &terrain_service = Game::Map::TerrainService::instance();

  bool const terrain_already_restored = terrain_service.isInitialized();

  Game::Map::MapDefinition def;
  QString map_error;
  bool loaded_definition = false;
  const QString &map_path = m_level.map_path;

  if (!terrain_already_restored && !map_path.isEmpty()) {
    loaded_definition =
        Game::Map::MapLoader::loadFromJsonFile(map_path, def, &map_error);
    if (!loaded_definition) {
      qWarning() << "GameEngine: Failed to load map definition from" << map_path
                 << "during save load:" << map_error;
    }
  }

  if (!terrain_already_restored && loaded_definition) {
    terrain_service.initialize(def);

    if (!def.name.isEmpty()) {
      m_level.map_name = def.name;
    }

    m_level.cam_fov = def.camera.fovY;
    m_level.cam_near = def.camera.near_plane;
    m_level.cam_far = def.camera.far_plane;
  }

  if (m_renderer && m_camera) {
    if (loaded_definition) {
      Game::Map::Environment::apply(def, *m_renderer, *m_camera);
    } else {
      Game::Map::Environment::applyDefault(*m_renderer, *m_camera);
    }
  }

  if (terrain_service.isInitialized()) {
    const auto *height_map = terrain_service.getHeightMap();
    const int grid_width =
        (height_map != nullptr) ? height_map->getWidth() : fallback_grid_width;
    const int grid_height = (height_map != nullptr) ? height_map->getHeight()
                                                    : fallback_grid_height;
    const float tile_size = (height_map != nullptr) ? height_map->getTileSize()
                                                    : fallback_tile_size;

    if (m_ground) {
      m_ground->configure(tile_size, grid_width, grid_height);
      m_ground->setBiome(terrain_service.biomeSettings());
    }

    if (height_map != nullptr) {
      if (m_terrain) {
        m_terrain->configure(*height_map, terrain_service.biomeSettings());
      }
      if (m_river) {
        m_river->configure(height_map->getRiverSegments(),
                           height_map->getTileSize());
      }
      if (m_road) {
        m_road->configure(terrain_service.road_segments(),
                          height_map->getTileSize());
      }
      if (m_riverbank) {
        m_riverbank->configure(height_map->getRiverSegments(), *height_map);
      }
      if (m_bridge) {
        m_bridge->configure(height_map->getBridges(),
                            height_map->getTileSize());
      }
      if (m_biome) {
        m_biome->configure(*height_map, terrain_service.biomeSettings());
        m_biome->refreshGrass();
      }
      if (m_stone) {
        m_stone->configure(*height_map, terrain_service.biomeSettings());
      }
      if (m_plant) {
        m_plant->configure(*height_map, terrain_service.biomeSettings());
      }
      if (m_pine) {
        m_pine->configure(*height_map, terrain_service.biomeSettings());
      }
      if (m_olive) {
        m_olive->configure(*height_map, terrain_service.biomeSettings());
      }
      if (m_firecamp) {
        m_firecamp->configure(*height_map, terrain_service.biomeSettings());
      }
    }

    Game::Systems::CommandService::initialize(grid_width, grid_height);

    auto &visibility_service = Game::Map::VisibilityService::instance();
    visibility_service.initialize(grid_width, grid_height, tile_size);
    visibility_service.computeImmediate(*m_world, m_runtime.local_owner_id);

    if (m_fog && visibility_service.isInitialized()) {
      m_fog->updateMask(
          visibility_service.getWidth(), visibility_service.getHeight(),
          visibility_service.getTileSize(), visibility_service.snapshotCells());
    }

    m_runtime.visibilityVersion = visibility_service.version();
    m_runtime.visibilityUpdateAccumulator = 0.0F;
  } else {
    if (m_renderer && m_camera) {
      Game::Map::Environment::applyDefault(*m_renderer, *m_camera);
    }

    Game::Map::MapDefinition fallback_def;
    fallback_def.grid.width = fallback_grid_width;
    fallback_def.grid.height = fallback_grid_height;
    fallback_def.grid.tile_size = fallback_tile_size;
    fallback_def.max_troops_per_player = m_level.max_troops_per_player;
    terrain_service.initialize(fallback_def);

    if (m_ground) {
      m_ground->configure(fallback_tile_size, fallback_grid_width,
                          fallback_grid_height);
    }

    Game::Systems::CommandService::initialize(fallback_grid_width,
                                              fallback_grid_height);

    auto &visibility_service = Game::Map::VisibilityService::instance();
    visibility_service.initialize(fallback_grid_width, fallback_grid_height,
                                  fallback_tile_size);
    visibility_service.computeImmediate(*m_world, m_runtime.local_owner_id);
    if (m_fog && visibility_service.isInitialized()) {
      m_fog->updateMask(
          visibility_service.getWidth(), visibility_service.getHeight(),
          visibility_service.getTileSize(), visibility_service.snapshotCells());
    }
    m_runtime.visibilityVersion = visibility_service.version();
    m_runtime.visibilityUpdateAccumulator = 0.0F;
  }
}

auto GameEngine::has_patrol_preview_waypoint() const -> bool {
  return m_commandController && m_commandController->hasPatrolFirstWaypoint();
}

auto GameEngine::get_patrol_preview_waypoint() const -> QVector3D {
  if (!m_commandController) {
    return {};
  }
  return m_commandController->getPatrolFirstWaypoint();
}

void GameEngine::update_ambient_state(float dt) {

  m_ambientCheckTimer += dt;
  const float check_interval = 2.0F;

  if (m_ambientCheckTimer < check_interval) {
    return;
  }
  m_ambientCheckTimer = 0.0F;

  Engine::Core::AmbientState new_state = Engine::Core::AmbientState::PEACEFUL;

  if (!m_runtime.victory_state.isEmpty()) {
    if (m_runtime.victory_state == "victory") {
      new_state = Engine::Core::AmbientState::VICTORY;
    } else if (m_runtime.victory_state == "defeat") {
      new_state = Engine::Core::AmbientState::DEFEAT;
    }
  } else if (is_player_in_combat()) {

    new_state = Engine::Core::AmbientState::COMBAT;
  } else if (m_entityCache.enemyBarracksAlive &&
             m_entityCache.playerBarracksAlive) {

    new_state = Engine::Core::AmbientState::TENSE;
  }

  if (new_state != m_currentAmbientState) {
    Engine::Core::AmbientState const previous_state = m_currentAmbientState;
    m_currentAmbientState = new_state;

    Engine::Core::EventManager::instance().publish(
        Engine::Core::AmbientStateChangedEvent(new_state, previous_state));

    qInfo() << "Ambient state changed from" << static_cast<int>(previous_state)
            << "to" << static_cast<int>(new_state);
  }
}

auto GameEngine::is_player_in_combat() const -> bool {
  if (!m_world) {
    return false;
  }

  auto units = m_world->get_entities_with<Engine::Core::UnitComponent>();
  const float combat_check_radius = 15.0F;

  for (auto *entity : units) {
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != m_runtime.local_owner_id ||
        unit->health <= 0) {
      continue;
    }

    if (entity->has_component<Engine::Core::AttackTargetComponent>()) {
      return true;
    }

    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }

    for (auto *other_entity : units) {
      auto *other_unit =
          other_entity->get_component<Engine::Core::UnitComponent>();
      if ((other_unit == nullptr) ||
          other_unit->owner_id == m_runtime.local_owner_id ||
          other_unit->health <= 0) {
        continue;
      }

      auto *other_transform =
          other_entity->get_component<Engine::Core::TransformComponent>();
      if (other_transform == nullptr) {
        continue;
      }

      float const dx = transform->position.x - other_transform->position.x;
      float const dz = transform->position.z - other_transform->position.z;
      float const dist_sq = dx * dx + dz * dz;

      if (dist_sq < combat_check_radius * combat_check_radius) {
        return true;
      }
    }
  }

  return false;
}

void GameEngine::load_audio_resources() {
  auto &audio_sys = AudioSystem::getInstance();

  QString const base_path =
      QCoreApplication::applicationDirPath() + "/assets/audio/";
  qInfo() << "Loading audio resources from:" << base_path;

  QDir const audio_dir(base_path);
  if (!audio_dir.exists()) {
    qWarning() << "Audio assets directory does not exist:" << base_path;
    qWarning() << "Application directory:"
               << QCoreApplication::applicationDirPath();
    return;
  }

  if (audio_sys.loadSound("archer_voice",
                          (base_path + "voices/archer_voice.wav").toStdString(),
                          AudioCategory::VOICE)) {
    qInfo() << "Loaded archer voice";
  } else {
    qWarning() << "Failed to load archer voice from:"
               << (base_path + "voices/archer_voice.wav");
  }

  if (audio_sys.loadSound(
          "swordsman_voice",
          (base_path + "voices/swordsman_voice.wav").toStdString(),
          AudioCategory::VOICE)) {
    qInfo() << "Loaded swordsman voice";
  } else {
    qWarning() << "Failed to load swordsman voice from:"
               << (base_path + "voices/swordsman_voice.wav");
  }

  if (audio_sys.loadSound(
          "spearman_voice",
          (base_path + "voices/spearman_voice.wav").toStdString(),
          AudioCategory::VOICE)) {
    qInfo() << "Loaded spearman voice";
  } else {
    qWarning() << "Failed to load spearman voice from:"
               << (base_path + "voices/spearman_voice.wav");
  }

  if (audio_sys.loadMusic("music_peaceful",
                          (base_path + "music/peaceful.wav").toStdString())) {
    qInfo() << "Loaded peaceful music";
  } else {
    qWarning() << "Failed to load peaceful music from:"
               << (base_path + "music/peaceful.wav");
  }

  if (audio_sys.loadMusic("music_tense",
                          (base_path + "music/tense.wav").toStdString())) {
    qInfo() << "Loaded tense music";
  } else {
    qWarning() << "Failed to load tense music from:"
               << (base_path + "music/tense.wav");
  }

  if (audio_sys.loadMusic("music_combat",
                          (base_path + "music/combat.wav").toStdString())) {
    qInfo() << "Loaded combat music";
  } else {
    qWarning() << "Failed to load combat music from:"
               << (base_path + "music/combat.wav");
  }

  if (audio_sys.loadMusic("music_victory",
                          (base_path + "music/victory.wav").toStdString())) {
    qInfo() << "Loaded victory music";
  } else {
    qWarning() << "Failed to load victory music from:"
               << (base_path + "music/victory.wav");
  }

  if (audio_sys.loadMusic("music_defeat",
                          (base_path + "music/defeat.wav").toStdString())) {
    qInfo() << "Loaded defeat music";
  } else {
    qWarning() << "Failed to load defeat music from:"
               << (base_path + "music/defeat.wav");
  }

  qInfo() << "Audio resources loading complete";
}
