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
#include "game/systems/movement_system.h"
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
#include "render/ground/pine_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/river_renderer.h"
#include "render/ground/riverbank_renderer.h"
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
    : QObject(parent), m_selectedUnitsModel(new SelectedUnitsModel(this, this)) {

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
  m_riverbank = std::make_unique<Render::GL::RiverbankRenderer>();
  m_bridge = std::make_unique<Render::GL::BridgeRenderer>();
  m_fog = std::make_unique<Render::GL::FogRenderer>();
  m_stone = std::make_unique<Render::GL::StoneRenderer>();
  m_plant = std::make_unique<Render::GL::PlantRenderer>();
  m_pine = std::make_unique<Render::GL::PineRenderer>();
  m_firecamp = std::make_unique<Render::GL::FireCampRenderer>();

  m_passes = {m_ground.get(),    m_terrain.get(), m_river.get(),
              m_riverbank.get(), m_bridge.get(),  m_biome.get(),
              m_stone.get(),     m_plant.get(),   m_pine.get(),
              m_firecamp.get(),  m_fog.get()};

  std::unique_ptr<Engine::Core::System> arrow_sys =
      std::make_unique<Game::Systems::ArrowSystem>();
  m_world->addSystem(std::move(arrow_sys));

  m_world->addSystem(std::make_unique<Game::Systems::MovementSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::PatrolSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::CombatSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::CaptureSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::AISystem>());
  m_world->addSystem(std::make_unique<Game::Systems::ProductionSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::TerrainAlignmentSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::CleanupSystem>());

  {
    std::unique_ptr<Engine::Core::System> sel_sys =
        std::make_unique<Game::Systems::SelectionSystem>();
    m_world->addSystem(std::move(sel_sys));
  }

  m_pickingService = std::make_unique<Game::Systems::PickingService>();
  m_victoryService = std::make_unique<Game::Systems::VictoryService>();
  m_saveLoadService = std::make_unique<Game::Systems::SaveLoadService>();
  m_cameraService = std::make_unique<Game::Systems::CameraService>();

  auto *selection_system = m_world->getSystem<Game::Systems::SelectionSystem>();
  m_selectionController = std::make_unique<Game::Systems::SelectionController>(
      m_world.get(), selection_system, m_pickingService.get());
  m_commandController = std::make_unique<App::Controllers::CommandController>(
      m_world.get(), selection_system, m_pickingService.get());

  m_cursorManager = std::make_unique<CursorManager>();
  m_hoverTracker = std::make_unique<HoverTracker>(m_pickingService.get());

  m_mapCatalog = std::make_unique<Game::Map::MapCatalog>(this);
  connect(m_mapCatalog.get(), &Game::Map::MapCatalog::mapLoaded, this,
          [this](const QVariantMap &mapData) {
            m_availableMaps.append(mapData);
            emit availableMapsChanged();
          });
  connect(m_mapCatalog.get(), &Game::Map::MapCatalog::loadingChanged, this,
          [this](bool loading) {
            m_mapsLoading = loading;
            emit mapsLoadingChanged();
          });
  connect(m_mapCatalog.get(), &Game::Map::MapCatalog::allMapsLoaded, this,
          [this]() { emit availableMapsChanged(); });

  if (AudioSystem::getInstance().initialize()) {
    qInfo() << "AudioSystem initialized successfully";
    loadAudioResources();
  } else {
    qWarning() << "Failed to initialize AudioSystem";
  }

  m_audio_systemProxy = std::make_unique<App::Models::AudioSystemProxy>(this);

  m_audioEventHandler =
      std::make_unique<Game::Audio::AudioEventHandler>(m_world.get());
  if (m_audioEventHandler->initialize()) {
    qInfo() << "AudioEventHandler initialized successfully";

    m_audioEventHandler->loadUnitVoiceMapping("archer", "archer_voice");
    m_audioEventHandler->loadUnitVoiceMapping("knight", "knight_voice");
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
          &GameEngine::cursorModeChanged);
  connect(m_cursorManager.get(), &CursorManager::globalCursorChanged, this,
          &GameEngine::globalCursorChanged);

  connect(m_selectionController.get(),
          &Game::Systems::SelectionController::selectionChanged, this,
          &GameEngine::selectedUnitsChanged);
  connect(m_selectionController.get(),
          &Game::Systems::SelectionController::selectionChanged, this,
          &GameEngine::syncSelectionFlags);
  connect(m_selectionController.get(),
          &Game::Systems::SelectionController::selectionModelRefreshRequested,
          this, &GameEngine::selectedUnitsDataChanged);
  connect(m_commandController.get(),
          &App::Controllers::CommandController::attack_targetSelected,
          [this]() {
            if (auto *sel_sys =
                    m_world->getSystem<Game::Systems::SelectionSystem>()) {
              const auto &sel = sel_sys->getSelectedUnits();
              if (!sel.empty()) {
                auto *cam = m_camera.get();
                auto *picking = m_pickingService.get();
                if ((cam != nullptr) && (picking != nullptr)) {
                  Engine::Core::EntityID const target_id =
                      picking->pickUnitFirst(0.0F, 0.0F, *m_world, *cam,
                                             m_viewport.width,
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
            setError("Maximum troop limit reached. Cannot produce more units.");
          });

  connect(this, SIGNAL(selectedUnitsChanged()), m_selectedUnitsModel,
          SLOT(refresh()));
  connect(this, SIGNAL(selectedUnitsDataChanged()), m_selectedUnitsModel,
          SLOT(refresh()));

  emit selectedUnitsChanged();

  m_unitDiedSubscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const Engine::Core::UnitDiedEvent &e) {
            onUnitDied(e);
            if (e.owner_id != m_runtime.localOwnerId) {

              int const individuals_per_unit =
                  Game::Units::TroopConfig::instance().getIndividualsPerUnit(
                      e.spawn_type);
              m_enemyTroopsDefeated += individuals_per_unit;
              emit enemyTroopsDefeatedChanged();
            }
          });

  m_unitSpawnedSubscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>(
          [this](const Engine::Core::UnitSpawnedEvent &e) {
            onUnitSpawned(e);
          });
}

GameEngine::~GameEngine() {

  if (m_audioEventHandler) {
    m_audioEventHandler->shutdown();
  }
  AudioSystem::getInstance().shutdown();
  qInfo() << "AudioSystem shut down";
}

void GameEngine::cleanupOpenGLResources() {
  qInfo() << "Cleaning up OpenGL resources...";
  
  // Check if we have a valid OpenGL context
  // If not, skip OpenGL cleanup and just reset the pointers
  // Qt will handle OpenGL resource cleanup when the context is destroyed
  QOpenGLContext *context = QOpenGLContext::currentContext();
  const bool hasValidContext = (context != nullptr);
  
  if (!hasValidContext) {
    qInfo() << "No valid OpenGL context, skipping OpenGL cleanup";
  }
  
  // Shutdown renderer and all OpenGL-dependent resources
  // Only call shutdown if we have a valid context
  if (m_renderer && hasValidContext) {
    m_renderer->shutdown();
    qInfo() << "Renderer shut down";
  }
  
  // Clear render passes that reference renderer resources
  m_passes.clear();
  
  // Reset all renderer-dependent unique_ptrs
  // These will call destructors which may try to access OpenGL
  // If no valid context, the destructors should be defensive
  m_ground.reset();
  m_terrain.reset();
  m_biome.reset();
  m_river.reset();
  m_riverbank.reset();
  m_bridge.reset();
  m_fog.reset();
  m_stone.reset();
  m_plant.reset();
  m_pine.reset();
  m_firecamp.reset();
  
  // Reset renderer and resources
  m_renderer.reset();
  m_resources.reset();
  
  qInfo() << "OpenGL resources cleaned up";
}

void GameEngine::onMapClicked(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensureInitialized();
  if (m_selectionController && m_camera) {
    m_selectionController->onClickSelect(sx, sy, false, m_viewport.width,
                                         m_viewport.height, m_camera.get(),
                                         m_runtime.localOwnerId);
  }
}

void GameEngine::onRightClick(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensureInitialized();
  auto *selection_system = m_world->getSystem<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return;
  }

  if (m_cursorManager->mode() == CursorMode::Patrol ||
      m_cursorManager->mode() == CursorMode::Attack) {
    setCursorMode(CursorMode::Normal);
    return;
  }

  const auto &sel = selection_system->getSelectedUnits();
  if (sel.empty()) {

    return;
  }

  if (m_pickingService && m_camera) {
    Engine::Core::EntityID const target_id = m_pickingService->pickUnitFirst(
        float(sx), float(sy), *m_world, *m_camera, m_viewport.width,
        m_viewport.height, 0);

    if (target_id != 0U) {
      auto *target_entity = m_world->getEntity(target_id);
      if (target_entity != nullptr) {
        auto *target_unit =
            target_entity->getComponent<Engine::Core::UnitComponent>();
        if (target_unit != nullptr) {

          bool const is_enemy =
              (target_unit->owner_id != m_runtime.localOwnerId);

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
    if (m_pickingService->screenToGround(QPointF(sx, sy), *m_camera,
                                         m_viewport.width, m_viewport.height,
                                         hit)) {
      auto targets = Game::Systems::FormationPlanner::spreadFormation(
          int(sel.size()), hit,
          Game::GameConfig::instance().gameplay().formationSpacingDefault);
      Game::Systems::CommandService::MoveOptions opts;
      opts.groupMove = sel.size() > 1;
      Game::Systems::CommandService::moveUnits(*m_world, sel, targets, opts);
    }
  }
}

void GameEngine::onAttackClick(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensureInitialized();
  if (!m_commandController || !m_camera) {
    return;
  }

  auto result = m_commandController->onAttackClick(
      sx, sy, m_viewport.width, m_viewport.height, m_camera.get());

  auto *selection_system = m_world->getSystem<Game::Systems::SelectionSystem>();
  if ((selection_system == nullptr) || !m_pickingService || !m_camera ||
      !m_world) {
    return;
  }

  const auto &selected = selection_system->getSelectedUnits();
  if (!selected.empty()) {
    Engine::Core::EntityID const target_id = m_pickingService->pickUnitFirst(
        float(sx), float(sy), *m_world, *m_camera, m_viewport.width,
        m_viewport.height, 0);

    if (target_id != 0) {
      auto *target_entity = m_world->getEntity(target_id);
      if (target_entity != nullptr) {
        auto *target_unit =
            target_entity->getComponent<Engine::Core::UnitComponent>();
        if ((target_unit != nullptr) &&
            target_unit->owner_id != m_runtime.localOwnerId) {
          App::Controllers::ActionVFX::spawnAttackArrow(m_world.get(),
                                                        target_id);
        }
      }
    }
  }

  if (result.resetCursorToNormal) {
    setCursorMode(CursorMode::Normal);
  }
}

void GameEngine::resetMovement(Engine::Core::Entity *entity) {
  App::Utils::resetMovement(entity);
}

void GameEngine::onStopCommand() {
  if (!m_commandController) {
    return;
  }
  ensureInitialized();

  auto result = m_commandController->onStopCommand();
  if (result.resetCursorToNormal) {
    setCursorMode(CursorMode::Normal);
  }
}

void GameEngine::onHoldCommand() {
  if (!m_commandController) {
    return;
  }
  ensureInitialized();

  auto result = m_commandController->onHoldCommand();
  if (result.resetCursorToNormal) {
    setCursorMode(CursorMode::Normal);
  }
}

auto GameEngine::anySelectedInHoldMode() const -> bool {
  if (!m_commandController) {
    return false;
  }
  return m_commandController->anySelectedInHoldMode();
}

void GameEngine::onPatrolClick(qreal sx, qreal sy) {
  if (!m_commandController || !m_camera) {
    return;
  }
  ensureInitialized();

  auto result = m_commandController->onPatrolClick(
      sx, sy, m_viewport.width, m_viewport.height, m_camera.get());
  if (result.resetCursorToNormal) {
    setCursorMode(CursorMode::Normal);
  }
}

void GameEngine::updateCursor(Qt::CursorShape newCursor) {
  if (m_window == nullptr) {
    return;
  }
  if (m_runtime.currentCursor != newCursor) {
    m_runtime.currentCursor = newCursor;
    m_window->setCursor(newCursor);
  }
}

void GameEngine::setError(const QString &errorMessage) {
  if (m_runtime.lastError != errorMessage) {
    m_runtime.lastError = errorMessage;
    qCritical() << "GameEngine error:" << errorMessage;
    emit lastErrorChanged();
  }
}

void GameEngine::setCursorMode(CursorMode mode) {
  if (!m_cursorManager) {
    return;
  }
  m_cursorManager->setMode(mode);
  m_cursorManager->updateCursorShape(m_window);
}

void GameEngine::setCursorMode(const QString &mode) {
  setCursorMode(CursorModeUtils::fromString(mode));
}

auto GameEngine::cursorMode() const -> QString {
  if (!m_cursorManager) {
    return "normal";
  }
  return m_cursorManager->modeString();
}

auto GameEngine::globalCursorX() const -> qreal {
  if (!m_cursorManager) {
    return 0;
  }
  return m_cursorManager->globalCursorX(m_window);
}

auto GameEngine::globalCursorY() const -> qreal {
  if (!m_cursorManager) {
    return 0;
  }
  return m_cursorManager->globalCursorY(m_window);
}

void GameEngine::setHoverAtScreen(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensureInitialized();
  if (!m_hoverTracker || !m_camera || !m_world) {
    return;
  }

  m_cursorManager->updateCursorShape(m_window);

  m_hoverTracker->updateHover(float(sx), float(sy), *m_world, *m_camera,
                              m_viewport.width, m_viewport.height);
}

void GameEngine::onClickSelect(qreal sx, qreal sy, bool additive) {
  if (m_window == nullptr) {
    return;
  }
  ensureInitialized();
  if (m_selectionController && m_camera) {
    m_selectionController->onClickSelect(sx, sy, additive, m_viewport.width,
                                         m_viewport.height, m_camera.get(),
                                         m_runtime.localOwnerId);
  }
}

void GameEngine::onAreaSelected(qreal x1, qreal y1, qreal x2, qreal y2,
                                bool additive) {
  if (m_window == nullptr) {
    return;
  }
  ensureInitialized();
  if (m_selectionController && m_camera) {
    m_selectionController->onAreaSelected(
        x1, y1, x2, y2, additive, m_viewport.width, m_viewport.height,
        m_camera.get(), m_runtime.localOwnerId);
  }
}

void GameEngine::selectAllTroops() {
  ensureInitialized();
  if (m_selectionController) {
    m_selectionController->selectAllPlayerTroops(m_runtime.localOwnerId);
  }
}

void GameEngine::ensureInitialized() {
  QString error;
  Game::Map::WorldBootstrap::ensureInitialized(
      m_runtime.initialized, *m_renderer, *m_camera, m_ground.get(), &error);
  if (!error.isEmpty()) {
    setError(error);
  }
}

auto GameEngine::enemyTroopsDefeated() const -> int {
  return m_enemyTroopsDefeated;
}

auto GameEngine::getPlayerStats(int owner_id) -> QVariantMap {
  QVariantMap result;

  auto &stats_registry = Game::Systems::GlobalStatsRegistry::instance();
  const auto *stats = stats_registry.getStats(owner_id);

  if (stats != nullptr) {
    result["troopsRecruited"] = stats->troopsRecruited;
    result["enemiesKilled"] = stats->enemiesKilled;
    result["barracksOwned"] = stats->barracksOwned;
    result["playTimeSec"] = stats->playTimeSec;
    result["gameEnded"] = stats->gameEnded;
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
    dt *= m_runtime.timeScale;
  }

  if (!m_runtime.paused && !m_runtime.loading) {
    updateAmbientState(dt);
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
        visibility_service.update(*m_world, m_runtime.localOwnerId);
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
    m_cameraService->updateFollow(*m_camera, *m_world,
                                  m_followSelectionEnabled);
  }

  if (m_selectedUnitsModel != nullptr) {
    auto *selection_system =
        m_world->getSystem<Game::Systems::SelectionSystem>();
    if ((selection_system != nullptr) &&
        !selection_system->getSelectedUnits().empty()) {
      m_runtime.selectionRefreshCounter++;
      if (m_runtime.selectionRefreshCounter >= 15) {
        m_runtime.selectionRefreshCounter = 0;
        emit selectedUnitsDataChanged();
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
          m_world->getSystem<Game::Systems::SelectionSystem>()) {
    const auto &sel = selection_system->getSelectedUnits();
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
    m_renderer->setLocalOwnerId(m_runtime.localOwnerId);
  }
  m_renderer->renderWorld(m_world.get());
  if (auto *arrow_system = m_world->getSystem<Game::Systems::ArrowSystem>()) {
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

  qreal const current_x = globalCursorX();
  qreal const current_y = globalCursorY();
  if (current_x != m_runtime.lastCursorX ||
      current_y != m_runtime.lastCursorY) {
    m_runtime.lastCursorX = current_x;
    m_runtime.lastCursorY = current_y;
    emit globalCursorChanged();
  }
}

auto GameEngine::screenToGround(const QPointF &screenPt,
                                QVector3D &outWorld) -> bool {
  return App::Utils::screenToGround(m_pickingService.get(), m_camera.get(),
                                    m_window, m_viewport.width,
                                    m_viewport.height, screenPt, outWorld);
}

auto GameEngine::worldToScreen(const QVector3D &world,
                               QPointF &outScreen) const -> bool {
  return App::Utils::worldToScreen(m_pickingService.get(), m_camera.get(),
                                   m_window, m_viewport.width,
                                   m_viewport.height, world, outScreen);
}

void GameEngine::syncSelectionFlags() {
  auto *selection_system = m_world->getSystem<Game::Systems::SelectionSystem>();
  if (!m_world || (selection_system == nullptr)) {
    return;
  }

  App::Utils::sanitizeSelection(m_world.get(), selection_system);

  if (selection_system->getSelectedUnits().empty()) {
    if (m_cursorManager && m_cursorManager->mode() != CursorMode::Normal) {
      setCursorMode(CursorMode::Normal);
    }
  }
}

void GameEngine::cameraMove(float dx, float dz) {
  ensureInitialized();
  if (!m_camera || !m_cameraService) {
    return;
  }

  m_cameraService->move(*m_camera, dx, dz);
}

void GameEngine::cameraElevate(float dy) {
  ensureInitialized();
  if (!m_camera || !m_cameraService) {
    return;
  }

  m_cameraService->elevate(*m_camera, dy);
}

void GameEngine::resetCamera() {
  ensureInitialized();
  if (!m_camera || !m_world || !m_cameraService) {
    return;
  }

  m_cameraService->resetCamera(*m_camera, *m_world, m_runtime.localOwnerId,
                               m_level.playerUnitId);
}

void GameEngine::cameraZoom(float delta) {
  ensureInitialized();
  if (!m_camera || !m_cameraService) {
    return;
  }

  m_cameraService->zoom(*m_camera, delta);
}

auto GameEngine::cameraDistance() const -> float {
  if (!m_camera || !m_cameraService) {
    return 0.0F;
  }
  return m_cameraService->getDistance(*m_camera);
}

void GameEngine::cameraYaw(float degrees) {
  ensureInitialized();
  if (!m_camera || !m_cameraService) {
    return;
  }

  m_cameraService->yaw(*m_camera, degrees);
}

void GameEngine::cameraOrbit(float yaw_deg, float pitch_deg) {
  ensureInitialized();
  if (!m_camera || !m_cameraService) {
    return;
  }

  if (!std::isfinite(yaw_deg) || !std::isfinite(pitch_deg)) {
    qWarning() << "GameEngine::cameraOrbit received invalid input, ignoring:"
               << yaw_deg << pitch_deg;
    return;
  }

  m_cameraService->orbit(*m_camera, yaw_deg, pitch_deg);
}

void GameEngine::cameraOrbitDirection(int direction, bool shift) {
  if (!m_camera || !m_cameraService) {
    return;
  }

  m_cameraService->orbitDirection(*m_camera, direction, shift);
}

void GameEngine::cameraFollowSelection(bool enable) {
  ensureInitialized();
  m_followSelectionEnabled = enable;
  if (!m_camera || !m_world || !m_cameraService) {
    return;
  }

  m_cameraService->followSelection(*m_camera, *m_world, enable);
}

void GameEngine::cameraSetFollowLerp(float alpha) {
  ensureInitialized();
  if (!m_camera || !m_cameraService) {
    return;
  }

  m_cameraService->setFollowLerp(*m_camera, alpha);
}

auto GameEngine::selectedUnitsModel() -> QObject * {
  return m_selectedUnitsModel;
}

auto GameEngine::audio_system() -> QObject * {
  return m_audio_systemProxy.get();
}

auto GameEngine::hasUnitsSelected() const -> bool {
  if (!m_selectionController) {
    return false;
  }
  return m_selectionController->hasUnitsSelected();
}

auto GameEngine::playerTroopCount() const -> int {
  return m_entityCache.playerTroopCount;
}

auto GameEngine::hasSelectedType(const QString &type) const -> bool {
  if (!m_selectionController) {
    return false;
  }
  return m_selectionController->hasSelectedType(type);
}

void GameEngine::recruitNearSelected(const QString &unit_type) {
  ensureInitialized();
  if (!m_commandController) {
    return;
  }
  m_commandController->recruitNearSelected(unit_type, m_runtime.localOwnerId);
}

auto GameEngine::getSelectedProductionState() const -> QVariantMap {
  QVariantMap m;
  m["has_barracks"] = false;
  m["inProgress"] = false;
  m["timeRemaining"] = 0.0;
  m["buildTime"] = 0.0;
  m["producedCount"] = 0;
  m["maxUnits"] = 0;
  m["villagerCost"] = 1;
  if (!m_world) {
    return m;
  }
  auto *selection_system = m_world->getSystem<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return m;
  }
  Game::Systems::ProductionState st;
  Game::Systems::ProductionService::getSelectedBarracksState(
      *m_world, selection_system->getSelectedUnits(), m_runtime.localOwnerId,
      st);
  m["has_barracks"] = st.has_barracks;
  m["inProgress"] = st.inProgress;
  m["product_type"] =
      QString::fromStdString(Game::Units::troop_typeToString(st.product_type));
  m["timeRemaining"] = st.timeRemaining;
  m["buildTime"] = st.buildTime;
  m["producedCount"] = st.producedCount;
  m["maxUnits"] = st.maxUnits;
  m["villagerCost"] = st.villagerCost;
  m["queueSize"] = st.queueSize;

  QVariantList queue_list;
  for (const auto &unit_type : st.productionQueue) {
    queue_list.append(
        QString::fromStdString(Game::Units::troop_typeToString(unit_type)));
  }
  m["productionQueue"] = queue_list;

  return m;
}

auto GameEngine::getSelectedUnitsCommandMode() const -> QString {
  if (!m_world) {
    return "normal";
  }
  auto *selection_system = m_world->getSystem<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return "normal";
  }

  const auto &sel = selection_system->getSelectedUnits();
  if (sel.empty()) {
    return "normal";
  }

  int attacking_count = 0;
  int patrolling_count = 0;
  int total_units = 0;

  for (auto id : sel) {
    auto *e = m_world->getEntity(id);
    if (e == nullptr) {
      continue;
    }

    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (u == nullptr) {
      continue;
    }
    if (u->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    total_units++;

    if (e->getComponent<Engine::Core::AttackTargetComponent>() != nullptr) {
      attacking_count++;
    }

    auto *patrol = e->getComponent<Engine::Core::PatrolComponent>();
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

void GameEngine::setRallyAtScreen(qreal sx, qreal sy) {
  ensureInitialized();
  if (!m_commandController || !m_camera) {
    return;
  }
  m_commandController->setRallyAtScreen(sx, sy, m_viewport.width,
                                        m_viewport.height, m_camera.get(),
                                        m_runtime.localOwnerId);
}

void GameEngine::startLoadingMaps() {
  m_availableMaps.clear();
  if (m_mapCatalog) {
    m_mapCatalog->loadMapsAsync();
  }
}

auto GameEngine::availableMaps() const -> QVariantList {
  return m_availableMaps;
}

void GameEngine::startSkirmish(const QString &map_path,
                               const QVariantList &playerConfigs) {

  clearError();

  m_level.map_path = map_path;
  m_level.map_name = map_path;

  if (!m_runtime.victoryState.isEmpty()) {
    m_runtime.victoryState = "";
    emit victoryStateChanged();
  }
  m_enemyTroopsDefeated = 0;

  if (!m_runtime.initialized) {
    ensureInitialized();
    return;
  }

  if (m_world && m_renderer && m_camera) {

    m_runtime.loading = true;

    if (m_hoverTracker) {
      m_hoverTracker->updateHover(-1, -1, *m_world, *m_camera, 0, 0);
    }

    m_entityCache.reset();

    Game::Map::SkirmishLoader loader(*m_world, *m_renderer, *m_camera);
    loader.setGroundRenderer(m_ground.get());
    loader.setTerrainRenderer(m_terrain.get());
    loader.setBiomeRenderer(m_biome.get());
    loader.setRiverRenderer(m_river.get());
    loader.setRiverbankRenderer(m_riverbank.get());
    loader.setBridgeRenderer(m_bridge.get());
    loader.setFogRenderer(m_fog.get());
    loader.setStoneRenderer(m_stone.get());
    loader.setPlantRenderer(m_plant.get());
    loader.setPineRenderer(m_pine.get());
    loader.setFireCampRenderer(m_firecamp.get());

    loader.setOnOwnersUpdated([this]() { emit ownerInfoChanged(); });

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
      emit selectedPlayerIdChanged();
    }

    if (!result.ok && !result.errorMessage.isEmpty()) {
      setError(result.errorMessage);
    }

    m_runtime.localOwnerId = updated_player_id;
    m_level.map_name = result.map_name;
    m_level.playerUnitId = result.playerUnitId;
    m_level.camFov = result.camFov;
    m_level.camNear = result.camNear;
    m_level.camFar = result.camFar;
    m_level.max_troops_per_player = result.max_troops_per_player;

    Game::GameConfig::instance().setMaxTroopsPerPlayer(
        result.max_troops_per_player);

    if (m_victoryService) {
      m_victoryService->configure(result.victoryConfig, m_runtime.localOwnerId);
      m_victoryService->setVictoryCallback([this](const QString &state) {
        if (m_runtime.victoryState != state) {
          m_runtime.victoryState = state;
          emit victoryStateChanged();
        }
      });
    }

    if (result.hasFocusPosition && m_camera) {
      const auto &cam_config = Game::GameConfig::instance().camera();
      m_camera->setRTSView(result.focusPosition, cam_config.defaultDistance,
                           cam_config.defaultPitch, cam_config.defaultYaw);
    }

    m_runtime.loading = false;

    if (auto *ai_system = m_world->getSystem<Game::Systems::AISystem>()) {
      ai_system->reinitialize();
    }

    rebuildEntityCache();
    auto &troops = Game::Systems::TroopCountRegistry::instance();
    troops.rebuildFromWorld(*m_world);

    auto &stats_registry = Game::Systems::GlobalStatsRegistry::instance();
    stats_registry.rebuildFromWorld(*m_world);

    auto &owner_registry = Game::Systems::OwnerRegistry::instance();
    const auto &all_owners = owner_registry.getAllOwners();
    for (const auto &owner : all_owners) {
      if (owner.type == Game::Systems::OwnerType::Player ||
          owner.type == Game::Systems::OwnerType::AI) {
        stats_registry.markGameStart(owner.owner_id);
      }
    }

    m_currentAmbientState = Engine::Core::AmbientState::PEACEFUL;
    m_ambientCheckTimer = 0.0F;

    Engine::Core::EventManager::instance().publish(
        Engine::Core::AmbientStateChangedEvent(
            Engine::Core::AmbientState::PEACEFUL,
            Engine::Core::AmbientState::PEACEFUL));

    emit ownerInfoChanged();
  }
}

void GameEngine::openSettings() {
  if (m_saveLoadService) {
    m_saveLoadService->openSettings();
  }
}

void GameEngine::loadSave() { loadFromSlot("savegame"); }

void GameEngine::saveGame(const QString &filename) {
  saveToSlot(filename, filename);
}

void GameEngine::saveGameToSlot(const QString &slotName) {
  saveToSlot(slotName, slotName);
}

void GameEngine::loadGameFromSlot(const QString &slotName) {
  loadFromSlot(slotName);
}

auto GameEngine::loadFromSlot(const QString &slot) -> bool {
  if (!m_saveLoadService || !m_world) {
    setError("Load: not initialized");
    return false;
  }

  m_runtime.loading = true;

  if (!m_saveLoadService->loadGameFromSlot(*m_world, slot)) {
    setError(m_saveLoadService->getLastError());
    m_runtime.loading = false;
    return false;
  }

  const QJsonObject meta = m_saveLoadService->getLastMetadata();

  Game::Systems::GameStateSerializer::restoreLevelFromMetadata(meta, m_level);
  Game::Systems::GameStateSerializer::restoreCameraFromMetadata(
      meta, m_camera.get(), m_viewport.width, m_viewport.height);

  Game::Systems::RuntimeSnapshot runtime_snap = toRuntimeSnapshot();
  Game::Systems::GameStateSerializer::restoreRuntimeFromMetadata(meta,
                                                                 runtime_snap);
  applyRuntimeSnapshot(runtime_snap);

  restoreEnvironmentFromMetadata(meta);

  auto unit_reg = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::registerBuiltInUnits(*unit_reg);
  Game::Map::MapTransformer::setFactoryRegistry(unit_reg);
  qInfo() << "Factory registry reinitialized after loading saved game";

  rebuildRegistriesAfterLoad();
  rebuildEntityCache();

  if (auto *ai_system = m_world->getSystem<Game::Systems::AISystem>()) {
    qInfo() << "Reinitializing AI system after loading saved game";
    ai_system->reinitialize();
  }

  if (m_victoryService) {
    m_victoryService->configure(Game::Map::VictoryConfig(),
                                m_runtime.localOwnerId);
  }

  m_runtime.loading = false;
  qInfo() << "Game load complete, victory/defeat checks re-enabled";

  emit selectedUnitsChanged();
  emit ownerInfoChanged();
  return true;
}

auto GameEngine::saveToSlot(const QString &slot, const QString &title) -> bool {
  if (!m_saveLoadService || !m_world) {
    setError("Save: not initialized");
    return false;
  }
  Game::Systems::RuntimeSnapshot const runtime_snap = toRuntimeSnapshot();
  QJsonObject meta = Game::Systems::GameStateSerializer::buildMetadata(
      *m_world, m_camera.get(), m_level, runtime_snap);
  meta["title"] = title;
  const QByteArray screenshot = captureScreenshot();
  if (!m_saveLoadService->saveGameToSlot(*m_world, slot, title,
                                         m_level.map_name, meta, screenshot)) {
    setError(m_saveLoadService->getLastError());
    return false;
  }
  emit saveSlotsChanged();
  return true;
}

auto GameEngine::getSaveSlots() const -> QVariantList {
  if (!m_saveLoadService) {
    qWarning() << "Cannot get save slots: service not initialized";
    return {};
  }

  return m_saveLoadService->getSaveSlots();
}

void GameEngine::refreshSaveSlots() { emit saveSlotsChanged(); }

auto GameEngine::deleteSaveSlot(const QString &slotName) -> bool {
  if (!m_saveLoadService) {
    qWarning() << "Cannot delete save slot: service not initialized";
    return false;
  }

  bool const success = m_saveLoadService->deleteSaveSlot(slotName);

  if (!success) {
    QString const error = m_saveLoadService->getLastError();
    qWarning() << "Failed to delete save slot:" << error;
    setError(error);
  } else {
    emit saveSlotsChanged();
  }

  return success;
}

void GameEngine::exitGame() {
  if (m_saveLoadService) {
    m_saveLoadService->exitGame();
  }
}

auto GameEngine::getOwnerInfo() const -> QVariantList {
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
    owner_map["isLocal"] = (owner.owner_id == m_runtime.localOwnerId);

    result.append(owner_map);
  }

  return result;
}

void GameEngine::getSelectedUnitIds(
    std::vector<Engine::Core::EntityID> &out) const {
  out.clear();
  if (!m_selectionController) {
    return;
  }
  m_selectionController->getSelectedUnitIds(out);
}

auto GameEngine::getUnitInfo(Engine::Core::EntityID id, QString &name,
                             int &health, int &max_health, bool &isBuilding,
                             bool &alive) const -> bool {
  if (!m_world) {
    return false;
  }
  auto *e = m_world->getEntity(id);
  if (e == nullptr) {
    return false;
  }
  isBuilding = e->hasComponent<Engine::Core::BuildingComponent>();
  if (auto *u = e->getComponent<Engine::Core::UnitComponent>()) {
    name =
        QString::fromStdString(Game::Units::spawn_typeToString(u->spawn_type));
    health = u->health;
    max_health = u->max_health;
    alive = (u->health > 0);
    return true;
  }
  name = QStringLiteral("Entity");
  health = max_health = 0;
  alive = true;
  return true;
}

void GameEngine::onUnitSpawned(const Engine::Core::UnitSpawnedEvent &event) {
  auto &owners = Game::Systems::OwnerRegistry::instance();

  if (event.owner_id == m_runtime.localOwnerId) {
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
      emit troop_countChanged();
    }
  };
  emit_if_changed();
}

void GameEngine::onUnitDied(const Engine::Core::UnitDiedEvent &event) {
  auto &owners = Game::Systems::OwnerRegistry::instance();

  if (event.owner_id == m_runtime.localOwnerId) {
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

  syncSelectionFlags();

  auto emit_if_changed = [&] {
    if (m_entityCache.playerTroopCount != m_runtime.lastTroopCount) {
      m_runtime.lastTroopCount = m_entityCache.playerTroopCount;
      emit troop_countChanged();
    }
  };
  emit_if_changed();
}

void GameEngine::rebuildEntityCache() {
  if (!m_world) {
    m_entityCache.reset();
    return;
  }

  m_entityCache.reset();

  auto &owners = Game::Systems::OwnerRegistry::instance();
  auto entities = m_world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    if (unit->owner_id == m_runtime.localOwnerId) {
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
      emit troop_countChanged();
    }
  };
  emit_if_changed();
}

void GameEngine::rebuildRegistriesAfterLoad() {
  if (!m_world) {
    return;
  }

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  m_runtime.localOwnerId = owner_registry.getLocalPlayerId();

  auto &troops = Game::Systems::TroopCountRegistry::instance();
  troops.rebuildFromWorld(*m_world);

  auto &stats_registry = Game::Systems::GlobalStatsRegistry::instance();
  stats_registry.rebuildFromWorld(*m_world);

  const auto &all_owners = owner_registry.getAllOwners();
  for (const auto &owner : all_owners) {
    if (owner.type == Game::Systems::OwnerType::Player ||
        owner.type == Game::Systems::OwnerType::AI) {
      stats_registry.markGameStart(owner.owner_id);
    }
  }

  rebuildBuildingCollisions();

  m_level.playerUnitId = 0;
  auto units = m_world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *entity : units) {
    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    if (unit->owner_id == m_runtime.localOwnerId) {
      m_level.playerUnitId = entity->getId();
      break;
    }
  }

  if (m_selectedPlayerId != m_runtime.localOwnerId) {
    m_selectedPlayerId = m_runtime.localOwnerId;
    emit selectedPlayerIdChanged();
  }
}

void GameEngine::rebuildBuildingCollisions() {
  auto &registry = Game::Systems::BuildingCollisionRegistry::instance();
  registry.clear();
  if (!m_world) {
    return;
  }

  auto buildings = m_world->getEntitiesWith<Engine::Core::BuildingComponent>();
  for (auto *entity : buildings) {
    auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if ((transform == nullptr) || (unit == nullptr)) {
      continue;
    }

    registry.registerBuilding(
        entity->getId(), Game::Units::spawn_typeToString(unit->spawn_type),
        transform->position.x, transform->position.z, unit->owner_id);
  }
}

auto GameEngine::toRuntimeSnapshot() const -> Game::Systems::RuntimeSnapshot {
  Game::Systems::RuntimeSnapshot snap;
  snap.paused = m_runtime.paused;
  snap.timeScale = m_runtime.timeScale;
  snap.localOwnerId = m_runtime.localOwnerId;
  snap.victoryState = m_runtime.victoryState;
  snap.cursorMode = CursorModeUtils::toInt(m_runtime.cursorMode);
  snap.selectedPlayerId = m_selectedPlayerId;
  snap.followSelection = m_followSelectionEnabled;
  return snap;
}

void GameEngine::applyRuntimeSnapshot(
    const Game::Systems::RuntimeSnapshot &snapshot) {
  m_runtime.localOwnerId = snapshot.localOwnerId;
  setPaused(snapshot.paused);
  setGameSpeed(snapshot.timeScale);

  if (snapshot.victoryState != m_runtime.victoryState) {
    m_runtime.victoryState = snapshot.victoryState;
    emit victoryStateChanged();
  }

  setCursorMode(CursorModeUtils::fromInt(snapshot.cursorMode));

  if (snapshot.selectedPlayerId != m_selectedPlayerId) {
    m_selectedPlayerId = snapshot.selectedPlayerId;
    emit selectedPlayerIdChanged();
  }

  if (snapshot.followSelection != m_followSelectionEnabled) {
    m_followSelectionEnabled = snapshot.followSelection;
    if (m_camera && m_cameraService && m_world) {
      m_cameraService->followSelection(*m_camera, *m_world,
                                       m_followSelectionEnabled);
    }
  }
}

auto GameEngine::captureScreenshot() const -> QByteArray {
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

void GameEngine::restoreEnvironmentFromMetadata(const QJsonObject &metadata) {
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

    m_level.camFov = def.camera.fovY;
    m_level.camNear = def.camera.near_plane;
    m_level.camFar = def.camera.far_plane;
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
      if (m_firecamp) {
        m_firecamp->configure(*height_map, terrain_service.biomeSettings());
      }
    }

    Game::Systems::CommandService::initialize(grid_width, grid_height);

    auto &visibility_service = Game::Map::VisibilityService::instance();
    visibility_service.initialize(grid_width, grid_height, tile_size);
    visibility_service.computeImmediate(*m_world, m_runtime.localOwnerId);

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
    visibility_service.computeImmediate(*m_world, m_runtime.localOwnerId);
    if (m_fog && visibility_service.isInitialized()) {
      m_fog->updateMask(
          visibility_service.getWidth(), visibility_service.getHeight(),
          visibility_service.getTileSize(), visibility_service.snapshotCells());
    }
    m_runtime.visibilityVersion = visibility_service.version();
    m_runtime.visibilityUpdateAccumulator = 0.0F;
  }
}

auto GameEngine::hasPatrolPreviewWaypoint() const -> bool {
  return m_commandController && m_commandController->hasPatrolFirstWaypoint();
}

auto GameEngine::getPatrolPreviewWaypoint() const -> QVector3D {
  if (!m_commandController) {
    return {};
  }
  return m_commandController->getPatrolFirstWaypoint();
}

void GameEngine::updateAmbientState(float dt) {

  m_ambientCheckTimer += dt;
  const float check_interval = 2.0F;

  if (m_ambientCheckTimer < check_interval) {
    return;
  }
  m_ambientCheckTimer = 0.0F;

  Engine::Core::AmbientState new_state = Engine::Core::AmbientState::PEACEFUL;

  if (!m_runtime.victoryState.isEmpty()) {
    if (m_runtime.victoryState == "victory") {
      new_state = Engine::Core::AmbientState::VICTORY;
    } else if (m_runtime.victoryState == "defeat") {
      new_state = Engine::Core::AmbientState::DEFEAT;
    }
  } else if (isPlayerInCombat()) {

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

auto GameEngine::isPlayerInCombat() const -> bool {
  if (!m_world) {
    return false;
  }

  auto units = m_world->getEntitiesWith<Engine::Core::UnitComponent>();
  const float combat_check_radius = 15.0F;

  for (auto *entity : units) {
    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != m_runtime.localOwnerId ||
        unit->health <= 0) {
      continue;
    }

    if (entity->hasComponent<Engine::Core::AttackTargetComponent>()) {
      return true;
    }

    auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }

    for (auto *other_entity : units) {
      auto *other_unit =
          other_entity->getComponent<Engine::Core::UnitComponent>();
      if ((other_unit == nullptr) ||
          other_unit->owner_id == m_runtime.localOwnerId ||
          other_unit->health <= 0) {
        continue;
      }

      auto *other_transform =
          other_entity->getComponent<Engine::Core::TransformComponent>();
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

void GameEngine::loadAudioResources() {
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

  if (audio_sys.loadSound("knight_voice",
                          (base_path + "voices/knight_voice.wav").toStdString(),
                          AudioCategory::VOICE)) {
    qInfo() << "Loaded knight voice";
  } else {
    qWarning() << "Failed to load knight voice from:"
               << (base_path + "voices/knight_voice.wav");
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
