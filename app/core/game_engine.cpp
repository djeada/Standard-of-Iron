#include "game_engine.h"

#include "../controllers/action_vfx.h"
#include "../controllers/command_controller.h"
#include "../models/cursor_manager.h"
#include "../models/hover_tracker.h"
#include "../utils/json_vec_utils.h"
#include "game/audio/AudioSystem.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QImage>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QSize>
#include <QVariant>
#include <set>
#include <unordered_map>

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
#include "game/visuals/team_colors.h"
#include "render/geom/arrow.h"
#include "render/geom/patrol_flags.h"
#include "render/gl/bootstrap.h"
#include "render/gl/camera.h"
#include "render/gl/resources.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/bridge_renderer.h"
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
#include <limits>

GameEngine::GameEngine() {

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

  m_passes = {m_ground.get(), m_terrain.get(), m_river.get(), m_riverbank.get(),
              m_bridge.get(), m_biome.get(),   m_stone.get(), m_plant.get(),
              m_pine.get(),   m_fog.get()};

  std::unique_ptr<Engine::Core::System> arrowSys =
      std::make_unique<Game::Systems::ArrowSystem>();
  m_world->addSystem(std::move(arrowSys));

  m_world->addSystem(std::make_unique<Game::Systems::MovementSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::PatrolSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::CombatSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::CaptureSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::AISystem>());
  m_world->addSystem(std::make_unique<Game::Systems::ProductionSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::TerrainAlignmentSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::CleanupSystem>());

  {
    std::unique_ptr<Engine::Core::System> selSys =
        std::make_unique<Game::Systems::SelectionSystem>();
    m_world->addSystem(std::move(selSys));
  }

  m_selectedUnitsModel = new SelectedUnitsModel(this, this);
  m_pickingService = std::make_unique<Game::Systems::PickingService>();
  m_victoryService = std::make_unique<Game::Systems::VictoryService>();
  m_saveLoadService = std::make_unique<Game::Systems::SaveLoadService>();
  m_cameraService = std::make_unique<Game::Systems::CameraService>();

  auto *selectionSystem = m_world->getSystem<Game::Systems::SelectionSystem>();
  m_selectionController = std::make_unique<Game::Systems::SelectionController>(
      m_world.get(), selectionSystem, m_pickingService.get());
  m_commandController = std::make_unique<App::Controllers::CommandController>(
      m_world.get(), selectionSystem, m_pickingService.get());

  m_cursorManager = std::make_unique<CursorManager>();
  m_hoverTracker = std::make_unique<HoverTracker>(m_pickingService.get());

  m_mapCatalog = std::make_unique<Game::Map::MapCatalog>(this);
  connect(m_mapCatalog.get(), &Game::Map::MapCatalog::mapLoaded, this,
          [this](QVariantMap mapData) {
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
          &App::Controllers::CommandController::attackTargetSelected, [this]() {
            if (auto *selSys =
                    m_world->getSystem<Game::Systems::SelectionSystem>()) {
              const auto &sel = selSys->getSelectedUnits();
              if (!sel.empty()) {
                auto *cam = m_camera.get();
                auto *picking = m_pickingService.get();
                if (cam && picking) {
                  Engine::Core::EntityID targetId = picking->pickUnitFirst(
                      0.0f, 0.0f, *m_world, *cam, m_viewport.width,
                      m_viewport.height, 0);
                  if (targetId != 0) {
                    App::Controllers::ActionVFX::spawnAttackArrow(m_world.get(),
                                                                  targetId);
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
            if (e.ownerId != m_runtime.localOwnerId) {

              int individualsPerUnit =
                  Game::Units::TroopConfig::instance().getIndividualsPerUnit(
                      e.unitType);
              m_enemyTroopsDefeated += individualsPerUnit;
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

void GameEngine::onMapClicked(qreal sx, qreal sy) {
  if (!m_window)
    return;
  ensureInitialized();
  if (m_selectionController && m_camera) {
    m_selectionController->onClickSelect(sx, sy, false, m_viewport.width,
                                         m_viewport.height, m_camera.get(),
                                         m_runtime.localOwnerId);
  }
}

void GameEngine::onRightClick(qreal sx, qreal sy) {
  if (!m_window)
    return;
  ensureInitialized();
  auto *selectionSystem = m_world->getSystem<Game::Systems::SelectionSystem>();
  if (!selectionSystem)
    return;

  if (m_cursorManager->mode() == CursorMode::Patrol ||
      m_cursorManager->mode() == CursorMode::Attack) {
    setCursorMode(CursorMode::Normal);
    return;
  }

  const auto &sel = selectionSystem->getSelectedUnits();
  if (!sel.empty()) {
    if (m_selectionController) {
      m_selectionController->onRightClickClearSelection();
    }
    setCursorMode(CursorMode::Normal);
    return;
  }
}

void GameEngine::onAttackClick(qreal sx, qreal sy) {
  if (!m_window)
    return;
  ensureInitialized();
  if (!m_commandController || !m_camera)
    return;

  auto result = m_commandController->onAttackClick(
      sx, sy, m_viewport.width, m_viewport.height, m_camera.get());

  auto *selectionSystem = m_world->getSystem<Game::Systems::SelectionSystem>();
  if (!selectionSystem || !m_pickingService || !m_camera || !m_world)
    return;

  const auto &selected = selectionSystem->getSelectedUnits();
  if (!selected.empty()) {
    Engine::Core::EntityID targetId = m_pickingService->pickUnitFirst(
        float(sx), float(sy), *m_world, *m_camera, m_viewport.width,
        m_viewport.height, 0);

    if (targetId != 0) {
      auto *targetEntity = m_world->getEntity(targetId);
      if (targetEntity) {
        auto *targetUnit =
            targetEntity->getComponent<Engine::Core::UnitComponent>();
        if (targetUnit && targetUnit->ownerId != m_runtime.localOwnerId) {
          App::Controllers::ActionVFX::spawnAttackArrow(m_world.get(),
                                                        targetId);
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
  if (!m_commandController)
    return;
  ensureInitialized();

  auto result = m_commandController->onStopCommand();
  if (result.resetCursorToNormal) {
    setCursorMode(CursorMode::Normal);
  }
}

void GameEngine::onHoldCommand() {
  if (!m_commandController)
    return;
  ensureInitialized();

  auto result = m_commandController->onHoldCommand();
  if (result.resetCursorToNormal) {
    setCursorMode(CursorMode::Normal);
  }
}

bool GameEngine::anySelectedInHoldMode() const {
  if (!m_commandController)
    return false;
  return m_commandController->anySelectedInHoldMode();
}

void GameEngine::onPatrolClick(qreal sx, qreal sy) {
  if (!m_commandController || !m_camera)
    return;
  ensureInitialized();

  auto result = m_commandController->onPatrolClick(
      sx, sy, m_viewport.width, m_viewport.height, m_camera.get());
  if (result.resetCursorToNormal) {
    setCursorMode(CursorMode::Normal);
  }
}

void GameEngine::updateCursor(Qt::CursorShape newCursor) {
  if (!m_window)
    return;
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
  if (!m_cursorManager)
    return;
  m_cursorManager->setMode(mode);
  m_cursorManager->updateCursorShape(m_window);
}

void GameEngine::setCursorMode(const QString &mode) {
  setCursorMode(CursorModeUtils::fromString(mode));
}

QString GameEngine::cursorMode() const {
  if (!m_cursorManager)
    return "normal";
  return m_cursorManager->modeString();
}

qreal GameEngine::globalCursorX() const {
  if (!m_cursorManager)
    return 0;
  return m_cursorManager->globalCursorX(m_window);
}

qreal GameEngine::globalCursorY() const {
  if (!m_cursorManager)
    return 0;
  return m_cursorManager->globalCursorY(m_window);
}

void GameEngine::setHoverAtScreen(qreal sx, qreal sy) {
  if (!m_window)
    return;
  ensureInitialized();
  if (!m_hoverTracker || !m_camera || !m_world)
    return;

  m_cursorManager->updateCursorShape(m_window);

  m_hoverTracker->updateHover(float(sx), float(sy), *m_world, *m_camera,
                              m_viewport.width, m_viewport.height);
}

void GameEngine::onClickSelect(qreal sx, qreal sy, bool additive) {
  if (!m_window)
    return;
  ensureInitialized();
  if (m_selectionController && m_camera) {
    m_selectionController->onClickSelect(sx, sy, additive, m_viewport.width,
                                         m_viewport.height, m_camera.get(),
                                         m_runtime.localOwnerId);
  }
}

void GameEngine::onAreaSelected(qreal x1, qreal y1, qreal x2, qreal y2,
                                bool additive) {
  if (!m_window)
    return;
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

int GameEngine::enemyTroopsDefeated() const { return m_enemyTroopsDefeated; }

QVariantMap GameEngine::getPlayerStats(int ownerId) const {
  QVariantMap result;

  auto &statsRegistry = Game::Systems::GlobalStatsRegistry::instance();
  const auto *stats = statsRegistry.getStats(ownerId);

  if (stats) {
    result["troopsRecruited"] = stats->troopsRecruited;
    result["enemiesKilled"] = stats->enemiesKilled;
    result["barracksOwned"] = stats->barracksOwned;
    result["playTimeSec"] = stats->playTimeSec;
    result["gameEnded"] = stats->gameEnded;
  } else {
    result["troopsRecruited"] = 0;
    result["enemiesKilled"] = 0;
    result["barracksOwned"] = 0;
    result["playTimeSec"] = 0.0f;
    result["gameEnded"] = false;
  }

  return result;
}

void GameEngine::update(float dt) {

  if (m_runtime.loading) {
    return;
  }

  if (m_runtime.paused) {
    dt = 0.0f;
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

    auto &visibilityService = Game::Map::VisibilityService::instance();
    if (visibilityService.isInitialized()) {

      m_runtime.visibilityUpdateAccumulator += dt;
      const float visibilityUpdateInterval =
          Game::GameConfig::instance().gameplay().visibilityUpdateInterval;
      if (m_runtime.visibilityUpdateAccumulator >= visibilityUpdateInterval) {
        m_runtime.visibilityUpdateAccumulator = 0.0f;
        visibilityService.update(*m_world, m_runtime.localOwnerId);
      }

      const auto newVersion = visibilityService.version();
      if (newVersion != m_runtime.visibilityVersion) {
        if (m_fog) {
          m_fog->updateMask(visibilityService.getWidth(),
                            visibilityService.getHeight(),
                            visibilityService.getTileSize(),
                            visibilityService.snapshotCells());
        }
        m_runtime.visibilityVersion = newVersion;
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

  if (m_selectedUnitsModel) {
    auto *selectionSystem =
        m_world->getSystem<Game::Systems::SelectionSystem>();
    if (selectionSystem && !selectionSystem->getSelectedUnits().empty()) {
      m_runtime.selectionRefreshCounter++;
      if (m_runtime.selectionRefreshCounter >= 15) {
        m_runtime.selectionRefreshCounter = 0;
        emit selectedUnitsDataChanged();
      }
    }
  }
}

void GameEngine::render(int pixelWidth, int pixelHeight) {

  if (!m_renderer || !m_world || !m_runtime.initialized || m_runtime.loading)
    return;
  if (pixelWidth > 0 && pixelHeight > 0) {
    m_viewport.width = pixelWidth;
    m_viewport.height = pixelHeight;
    m_renderer->setViewport(pixelWidth, pixelHeight);
  }
  if (auto *selectionSystem =
          m_world->getSystem<Game::Systems::SelectionSystem>()) {
    const auto &sel = selectionSystem->getSelectedUnits();
    std::vector<unsigned int> ids(sel.begin(), sel.end());
    m_renderer->setSelectedEntities(ids);
  }
  m_renderer->beginFrame();
  if (auto *res = m_renderer->resources()) {
    for (auto *pass : m_passes) {
      if (pass)
        pass->submit(*m_renderer, res);
    }
  }
  if (m_renderer && m_hoverTracker)
    m_renderer->setHoveredEntityId(m_hoverTracker->getLastHoveredEntity());
  if (m_renderer)
    m_renderer->setLocalOwnerId(m_runtime.localOwnerId);
  m_renderer->renderWorld(m_world.get());
  if (auto *arrowSystem = m_world->getSystem<Game::Systems::ArrowSystem>()) {
    if (auto *res = m_renderer->resources())
      Render::GL::renderArrows(m_renderer.get(), res, *arrowSystem);
  }

  if (auto *res = m_renderer->resources()) {
    std::optional<QVector3D> previewWaypoint;
    if (m_commandController && m_commandController->hasPatrolFirstWaypoint()) {
      previewWaypoint = m_commandController->getPatrolFirstWaypoint();
    }
    Render::GL::renderPatrolFlags(m_renderer.get(), res, *m_world,
                                  previewWaypoint);
  }
  m_renderer->endFrame();

  qreal currentX = globalCursorX();
  qreal currentY = globalCursorY();
  if (currentX != m_runtime.lastCursorX || currentY != m_runtime.lastCursorY) {
    m_runtime.lastCursorX = currentX;
    m_runtime.lastCursorY = currentY;
    emit globalCursorChanged();
  }
}

bool GameEngine::screenToGround(const QPointF &screenPt, QVector3D &outWorld) {
  return App::Utils::screenToGround(m_pickingService.get(), m_camera.get(),
                                    m_window, m_viewport.width,
                                    m_viewport.height, screenPt, outWorld);
}

bool GameEngine::worldToScreen(const QVector3D &world,
                               QPointF &outScreen) const {
  return App::Utils::worldToScreen(m_pickingService.get(), m_camera.get(),
                                   m_window, m_viewport.width,
                                   m_viewport.height, world, outScreen);
}

void GameEngine::syncSelectionFlags() {
  auto *selectionSystem = m_world->getSystem<Game::Systems::SelectionSystem>();
  if (!m_world || !selectionSystem)
    return;

  App::Utils::sanitizeSelection(m_world.get(), selectionSystem);

  if (selectionSystem->getSelectedUnits().empty()) {
    if (m_cursorManager && m_cursorManager->mode() != CursorMode::Normal) {
      setCursorMode(CursorMode::Normal);
    }
  }
}

void GameEngine::cameraMove(float dx, float dz) {
  ensureInitialized();
  if (!m_camera || !m_cameraService)
    return;

  m_cameraService->move(*m_camera, dx, dz);
}

void GameEngine::cameraElevate(float dy) {
  ensureInitialized();
  if (!m_camera || !m_cameraService)
    return;

  m_cameraService->elevate(*m_camera, dy);
}

void GameEngine::resetCamera() {
  ensureInitialized();
  if (!m_camera || !m_world || !m_cameraService)
    return;

  m_cameraService->resetCamera(*m_camera, *m_world, m_runtime.localOwnerId,
                               m_level.playerUnitId);
}

void GameEngine::cameraZoom(float delta) {
  ensureInitialized();
  if (!m_camera || !m_cameraService)
    return;

  m_cameraService->zoom(*m_camera, delta);
}

float GameEngine::cameraDistance() const {
  if (!m_camera || !m_cameraService)
    return 0.0f;
  return m_cameraService->getDistance(*m_camera);
}

void GameEngine::cameraYaw(float degrees) {
  ensureInitialized();
  if (!m_camera || !m_cameraService)
    return;

  m_cameraService->yaw(*m_camera, degrees);
}

void GameEngine::cameraOrbit(float yawDeg, float pitchDeg) {
  ensureInitialized();
  if (!m_camera || !m_cameraService)
    return;

  if (!std::isfinite(yawDeg) || !std::isfinite(pitchDeg)) {
    qWarning() << "GameEngine::cameraOrbit received invalid input, ignoring:"
               << yawDeg << pitchDeg;
    return;
  }

  m_cameraService->orbit(*m_camera, yawDeg, pitchDeg);
}

void GameEngine::cameraOrbitDirection(int direction, bool shift) {
  if (!m_camera || !m_cameraService)
    return;

  m_cameraService->orbitDirection(*m_camera, direction, shift);
}

void GameEngine::cameraFollowSelection(bool enable) {
  ensureInitialized();
  m_followSelectionEnabled = enable;
  if (!m_camera || !m_world || !m_cameraService)
    return;

  m_cameraService->followSelection(*m_camera, *m_world, enable);
}

void GameEngine::cameraSetFollowLerp(float alpha) {
  ensureInitialized();
  if (!m_camera || !m_cameraService)
    return;

  m_cameraService->setFollowLerp(*m_camera, alpha);
}

QObject *GameEngine::selectedUnitsModel() { return m_selectedUnitsModel; }

bool GameEngine::hasUnitsSelected() const {
  if (!m_selectionController)
    return false;
  return m_selectionController->hasUnitsSelected();
}

int GameEngine::playerTroopCount() const {
  return m_entityCache.playerTroopCount;
}

bool GameEngine::hasSelectedType(const QString &type) const {
  if (!m_selectionController)
    return false;
  return m_selectionController->hasSelectedType(type);
}

void GameEngine::recruitNearSelected(const QString &unitType) {
  ensureInitialized();
  if (!m_commandController)
    return;
  m_commandController->recruitNearSelected(unitType, m_runtime.localOwnerId);
}

QVariantMap GameEngine::getSelectedProductionState() const {
  QVariantMap m;
  m["hasBarracks"] = false;
  m["inProgress"] = false;
  m["timeRemaining"] = 0.0;
  m["buildTime"] = 0.0;
  m["producedCount"] = 0;
  m["maxUnits"] = 0;
  m["villagerCost"] = 1;
  if (!m_world)
    return m;
  auto *selectionSystem = m_world->getSystem<Game::Systems::SelectionSystem>();
  if (!selectionSystem)
    return m;
  Game::Systems::ProductionState st;
  Game::Systems::ProductionService::getSelectedBarracksState(
      *m_world, selectionSystem->getSelectedUnits(), m_runtime.localOwnerId,
      st);
  m["hasBarracks"] = st.hasBarracks;
  m["inProgress"] = st.inProgress;
  m["productType"] = QString::fromStdString(st.productType);
  m["timeRemaining"] = st.timeRemaining;
  m["buildTime"] = st.buildTime;
  m["producedCount"] = st.producedCount;
  m["maxUnits"] = st.maxUnits;
  m["villagerCost"] = st.villagerCost;
  m["queueSize"] = st.queueSize;

  QVariantList queueList;
  for (const auto &unitType : st.productionQueue) {
    queueList.append(QString::fromStdString(unitType));
  }
  m["productionQueue"] = queueList;

  return m;
}

QString GameEngine::getSelectedUnitsCommandMode() const {
  if (!m_world)
    return "normal";
  auto *selectionSystem = m_world->getSystem<Game::Systems::SelectionSystem>();
  if (!selectionSystem)
    return "normal";

  const auto &sel = selectionSystem->getSelectedUnits();
  if (sel.empty())
    return "normal";

  int attackingCount = 0;
  int patrollingCount = 0;
  int totalUnits = 0;

  for (auto id : sel) {
    auto *e = m_world->getEntity(id);
    if (!e)
      continue;

    auto *u = e->getComponent<Engine::Core::UnitComponent>();
    if (!u)
      continue;
    if (u->unitType == "barracks")
      continue;

    totalUnits++;

    if (e->getComponent<Engine::Core::AttackTargetComponent>())
      attackingCount++;

    auto *patrol = e->getComponent<Engine::Core::PatrolComponent>();
    if (patrol && patrol->patrolling)
      patrollingCount++;
  }

  if (totalUnits == 0)
    return "normal";

  if (patrollingCount == totalUnits)
    return "patrol";
  if (attackingCount == totalUnits)
    return "attack";

  return "normal";
}

void GameEngine::setRallyAtScreen(qreal sx, qreal sy) {
  ensureInitialized();
  if (!m_commandController || !m_camera)
    return;
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

QVariantList GameEngine::availableMaps() const { return m_availableMaps; }

void GameEngine::startSkirmish(const QString &mapPath,
                               const QVariantList &playerConfigs) {

  clearError();

  m_level.mapPath = mapPath;
  m_level.mapName = mapPath;

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

    loader.setOnOwnersUpdated([this]() { emit ownerInfoChanged(); });

    loader.setOnVisibilityMaskReady([this]() {
      m_runtime.visibilityVersion =
          Game::Map::VisibilityService::instance().version();
      m_runtime.visibilityUpdateAccumulator = 0.0f;
    });

    int updatedPlayerId = m_selectedPlayerId;
    auto result = loader.start(mapPath, playerConfigs, m_selectedPlayerId,
                               updatedPlayerId);

    if (updatedPlayerId != m_selectedPlayerId) {
      m_selectedPlayerId = updatedPlayerId;
      emit selectedPlayerIdChanged();
    }

    if (!result.ok && !result.errorMessage.isEmpty()) {
      setError(result.errorMessage);
    }

    m_runtime.localOwnerId = updatedPlayerId;
    m_level.mapName = result.mapName;
    m_level.playerUnitId = result.playerUnitId;
    m_level.camFov = result.camFov;
    m_level.camNear = result.camNear;
    m_level.camFar = result.camFar;
    m_level.maxTroopsPerPlayer = result.maxTroopsPerPlayer;

    Game::GameConfig::instance().setMaxTroopsPerPlayer(
        result.maxTroopsPerPlayer);

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
      const auto &camConfig = Game::GameConfig::instance().camera();
      m_camera->setRTSView(result.focusPosition, camConfig.defaultDistance,
                           camConfig.defaultPitch, camConfig.defaultYaw);
    }

    m_runtime.loading = false;

    if (auto *aiSystem = m_world->getSystem<Game::Systems::AISystem>()) {
      aiSystem->reinitialize();
    }

    rebuildEntityCache();
    auto &troops = Game::Systems::TroopCountRegistry::instance();
    troops.rebuildFromWorld(*m_world);

    auto &statsRegistry = Game::Systems::GlobalStatsRegistry::instance();
    statsRegistry.rebuildFromWorld(*m_world);

    auto &ownerRegistry = Game::Systems::OwnerRegistry::instance();
    const auto &allOwners = ownerRegistry.getAllOwners();
    for (const auto &owner : allOwners) {
      if (owner.type == Game::Systems::OwnerType::Player ||
          owner.type == Game::Systems::OwnerType::AI) {
        statsRegistry.markGameStart(owner.ownerId);
      }
    }

    m_currentAmbientState = Engine::Core::AmbientState::PEACEFUL;
    m_ambientCheckTimer = 0.0f;

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

bool GameEngine::loadFromSlot(const QString &slot) {
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

  Game::Systems::RuntimeSnapshot runtimeSnap = toRuntimeSnapshot();
  Game::Systems::GameStateSerializer::restoreRuntimeFromMetadata(meta,
                                                                 runtimeSnap);
  applyRuntimeSnapshot(runtimeSnap);

  restoreEnvironmentFromMetadata(meta);

  auto unitReg = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::registerBuiltInUnits(*unitReg);
  Game::Map::MapTransformer::setFactoryRegistry(unitReg);
  qInfo() << "Factory registry reinitialized after loading saved game";

  rebuildRegistriesAfterLoad();
  rebuildEntityCache();

  if (auto *aiSystem = m_world->getSystem<Game::Systems::AISystem>()) {
    qInfo() << "Reinitializing AI system after loading saved game";
    aiSystem->reinitialize();
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

bool GameEngine::saveToSlot(const QString &slot, const QString &title) {
  if (!m_saveLoadService || !m_world) {
    setError("Save: not initialized");
    return false;
  }
  Game::Systems::RuntimeSnapshot runtimeSnap = toRuntimeSnapshot();
  QJsonObject meta = Game::Systems::GameStateSerializer::buildMetadata(
      *m_world, m_camera.get(), m_level, runtimeSnap);
  meta["title"] = title;
  const QByteArray screenshot = captureScreenshot();
  if (!m_saveLoadService->saveGameToSlot(*m_world, slot, title, m_level.mapName,
                                         meta, screenshot)) {
    setError(m_saveLoadService->getLastError());
    return false;
  }
  emit saveSlotsChanged();
  return true;
}

QVariantList GameEngine::getSaveSlots() const {
  if (!m_saveLoadService) {
    qWarning() << "Cannot get save slots: service not initialized";
    return QVariantList();
  }

  return m_saveLoadService->getSaveSlots();
}

void GameEngine::refreshSaveSlots() { emit saveSlotsChanged(); }

bool GameEngine::deleteSaveSlot(const QString &slotName) {
  if (!m_saveLoadService) {
    qWarning() << "Cannot delete save slot: service not initialized";
    return false;
  }

  bool success = m_saveLoadService->deleteSaveSlot(slotName);

  if (!success) {
    QString error = m_saveLoadService->getLastError();
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

QVariantList GameEngine::getOwnerInfo() const {
  QVariantList result;
  const auto &ownerRegistry = Game::Systems::OwnerRegistry::instance();
  const auto &owners = ownerRegistry.getAllOwners();

  for (const auto &owner : owners) {
    QVariantMap ownerMap;
    ownerMap["id"] = owner.ownerId;
    ownerMap["name"] = QString::fromStdString(owner.name);
    ownerMap["teamId"] = owner.teamId;

    QString typeStr;
    switch (owner.type) {
    case Game::Systems::OwnerType::Player:
      typeStr = "Player";
      break;
    case Game::Systems::OwnerType::AI:
      typeStr = "AI";
      break;
    case Game::Systems::OwnerType::Neutral:
      typeStr = "Neutral";
      break;
    }
    ownerMap["type"] = typeStr;
    ownerMap["isLocal"] = (owner.ownerId == m_runtime.localOwnerId);

    result.append(ownerMap);
  }

  return result;
}

void GameEngine::getSelectedUnitIds(
    std::vector<Engine::Core::EntityID> &out) const {
  out.clear();
  if (!m_selectionController)
    return;
  m_selectionController->getSelectedUnitIds(out);
}

bool GameEngine::getUnitInfo(Engine::Core::EntityID id, QString &name,
                             int &health, int &maxHealth, bool &isBuilding,
                             bool &alive) const {
  if (!m_world)
    return false;
  auto *e = m_world->getEntity(id);
  if (!e)
    return false;
  isBuilding = e->hasComponent<Engine::Core::BuildingComponent>();
  if (auto *u = e->getComponent<Engine::Core::UnitComponent>()) {
    name = QString::fromStdString(u->unitType);
    health = u->health;
    maxHealth = u->maxHealth;
    alive = (u->health > 0);
    return true;
  }
  name = QStringLiteral("Entity");
  health = maxHealth = 0;
  alive = true;
  return true;
}

void GameEngine::onUnitSpawned(const Engine::Core::UnitSpawnedEvent &event) {
  auto &owners = Game::Systems::OwnerRegistry::instance();

  if (event.ownerId == m_runtime.localOwnerId) {
    if (event.unitType == "barracks") {
      m_entityCache.playerBarracksAlive = true;
    } else {
      int individualsPerUnit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              event.unitType);
      m_entityCache.playerTroopCount += individualsPerUnit;
    }
  } else if (owners.isAI(event.ownerId)) {
    if (event.unitType == "barracks") {
      m_entityCache.enemyBarracksCount++;
      m_entityCache.enemyBarracksAlive = true;
    }
  }

  auto emitIfChanged = [&] {
    if (m_entityCache.playerTroopCount != m_runtime.lastTroopCount) {
      m_runtime.lastTroopCount = m_entityCache.playerTroopCount;
      emit troopCountChanged();
    }
  };
  emitIfChanged();
}

void GameEngine::onUnitDied(const Engine::Core::UnitDiedEvent &event) {
  auto &owners = Game::Systems::OwnerRegistry::instance();

  if (event.ownerId == m_runtime.localOwnerId) {
    if (event.unitType == "barracks") {
      m_entityCache.playerBarracksAlive = false;
    } else {
      int individualsPerUnit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              event.unitType);
      m_entityCache.playerTroopCount -= individualsPerUnit;
      m_entityCache.playerTroopCount =
          std::max(0, m_entityCache.playerTroopCount);
    }
  } else if (owners.isAI(event.ownerId)) {
    if (event.unitType == "barracks") {
      m_entityCache.enemyBarracksCount--;
      m_entityCache.enemyBarracksCount =
          std::max(0, m_entityCache.enemyBarracksCount);
      m_entityCache.enemyBarracksAlive = (m_entityCache.enemyBarracksCount > 0);
    }
  }

  syncSelectionFlags();

  auto emitIfChanged = [&] {
    if (m_entityCache.playerTroopCount != m_runtime.lastTroopCount) {
      m_runtime.lastTroopCount = m_entityCache.playerTroopCount;
      emit troopCountChanged();
    }
  };
  emitIfChanged();
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
    if (!unit || unit->health <= 0)
      continue;

    if (unit->ownerId == m_runtime.localOwnerId) {
      if (unit->unitType == "barracks") {
        m_entityCache.playerBarracksAlive = true;
      } else {
        int individualsPerUnit =
            Game::Units::TroopConfig::instance().getIndividualsPerUnit(
                unit->unitType);
        m_entityCache.playerTroopCount += individualsPerUnit;
      }
    } else if (owners.isAI(unit->ownerId)) {
      if (unit->unitType == "barracks") {
        m_entityCache.enemyBarracksCount++;
        m_entityCache.enemyBarracksAlive = true;
      }
    }
  }

  auto emitIfChanged = [&] {
    if (m_entityCache.playerTroopCount != m_runtime.lastTroopCount) {
      m_runtime.lastTroopCount = m_entityCache.playerTroopCount;
      emit troopCountChanged();
    }
  };
  emitIfChanged();
}

void GameEngine::rebuildRegistriesAfterLoad() {
  if (!m_world)
    return;

  auto &ownerRegistry = Game::Systems::OwnerRegistry::instance();
  m_runtime.localOwnerId = ownerRegistry.getLocalPlayerId();

  auto &troops = Game::Systems::TroopCountRegistry::instance();
  troops.rebuildFromWorld(*m_world);

  auto &statsRegistry = Game::Systems::GlobalStatsRegistry::instance();
  statsRegistry.rebuildFromWorld(*m_world);

  const auto &allOwners = ownerRegistry.getAllOwners();
  for (const auto &owner : allOwners) {
    if (owner.type == Game::Systems::OwnerType::Player ||
        owner.type == Game::Systems::OwnerType::AI) {
      statsRegistry.markGameStart(owner.ownerId);
    }
  }

  rebuildBuildingCollisions();

  m_level.playerUnitId = 0;
  auto units = m_world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *entity : units) {
    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if (!unit)
      continue;
    if (unit->ownerId == m_runtime.localOwnerId) {
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
  if (!m_world)
    return;

  auto buildings = m_world->getEntitiesWith<Engine::Core::BuildingComponent>();
  for (auto *entity : buildings) {
    auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if (!transform || !unit)
      continue;

    registry.registerBuilding(entity->getId(), unit->unitType,
                              transform->position.x, transform->position.z,
                              unit->ownerId);
  }
}

Game::Systems::RuntimeSnapshot GameEngine::toRuntimeSnapshot() const {
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

QByteArray GameEngine::captureScreenshot() const {
  if (!m_window) {
    return {};
  }

  QImage image = m_window->grabWindow();
  if (image.isNull()) {
    return {};
  }

  const QSize targetSize(320, 180);
  QImage scaled =
      image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  QByteArray buffer;
  QBuffer qBuffer(&buffer);
  if (!qBuffer.open(QIODevice::WriteOnly)) {
    return {};
  }

  if (!scaled.save(&qBuffer, "PNG")) {
    return {};
  }

  return buffer;
}

void GameEngine::restoreEnvironmentFromMetadata(const QJsonObject &metadata) {
  if (!m_world)
    return;

  const auto fallbackGridWidth = metadata.value("gridWidth").toInt(50);
  const auto fallbackGridHeight = metadata.value("gridHeight").toInt(50);
  const float fallbackTileSize =
      static_cast<float>(metadata.value("tileSize").toDouble(1.0));

  auto &terrainService = Game::Map::TerrainService::instance();
  
  bool terrainAlreadyRestored = terrainService.isInitialized();

  Game::Map::MapDefinition def;
  QString mapError;
  bool loadedDefinition = false;
  const QString &mapPath = m_level.mapPath;

  if (!terrainAlreadyRestored && !mapPath.isEmpty()) {
    loadedDefinition =
        Game::Map::MapLoader::loadFromJsonFile(mapPath, def, &mapError);
    if (!loadedDefinition) {
      qWarning() << "GameEngine: Failed to load map definition from" << mapPath
                 << "during save load:" << mapError;
    }
  }

  if (!terrainAlreadyRestored && loadedDefinition) {
    terrainService.initialize(def);

    if (!def.name.isEmpty()) {
      m_level.mapName = def.name;
    }

    m_level.camFov = def.camera.fovY;
    m_level.camNear = def.camera.nearPlane;
    m_level.camFar = def.camera.farPlane;
  }

  if (m_renderer && m_camera) {
    if (loadedDefinition) {
      Game::Map::Environment::apply(def, *m_renderer, *m_camera);
    } else {
      Game::Map::Environment::applyDefault(*m_renderer, *m_camera);
    }
  }

  if (terrainService.isInitialized()) {
    const auto *heightMap = terrainService.getHeightMap();
    const int gridWidth = heightMap ? heightMap->getWidth() : fallbackGridWidth;
    const int gridHeight = heightMap ? heightMap->getHeight() : fallbackGridHeight;
    const float tileSize = heightMap ? heightMap->getTileSize() : fallbackTileSize;

    if (m_ground) {
      m_ground->configure(tileSize, gridWidth, gridHeight);
      m_ground->setBiome(terrainService.biomeSettings());
    }

    if (heightMap) {
      if (m_terrain) {
        m_terrain->configure(*heightMap, terrainService.biomeSettings());
      }
      if (m_river) {
        m_river->configure(heightMap->getRiverSegments(),
                           heightMap->getTileSize());
      }
      if (m_riverbank) {
        m_riverbank->configure(heightMap->getRiverSegments(), *heightMap);
      }
      if (m_bridge) {
        m_bridge->configure(heightMap->getBridges(), heightMap->getTileSize());
      }
      if (m_biome) {
        m_biome->configure(*heightMap, terrainService.biomeSettings());
        m_biome->refreshGrass();
      }
      if (m_stone) {
        m_stone->configure(*heightMap, terrainService.biomeSettings());
      }
      if (m_plant) {
        m_plant->configure(*heightMap, terrainService.biomeSettings());
      }
      if (m_pine) {
        m_pine->configure(*heightMap, terrainService.biomeSettings());
      }
    }

    Game::Systems::CommandService::initialize(gridWidth, gridHeight);

    auto &visibilityService = Game::Map::VisibilityService::instance();
    visibilityService.initialize(gridWidth, gridHeight, tileSize);
    visibilityService.computeImmediate(*m_world, m_runtime.localOwnerId);

    if (m_fog && visibilityService.isInitialized()) {
      m_fog->updateMask(
          visibilityService.getWidth(), visibilityService.getHeight(),
          visibilityService.getTileSize(), visibilityService.snapshotCells());
    }

    m_runtime.visibilityVersion = visibilityService.version();
    m_runtime.visibilityUpdateAccumulator = 0.0f;
  } else {
    if (m_renderer && m_camera) {
      Game::Map::Environment::applyDefault(*m_renderer, *m_camera);
    }

    Game::Map::MapDefinition fallbackDef;
    fallbackDef.grid.width = fallbackGridWidth;
    fallbackDef.grid.height = fallbackGridHeight;
    fallbackDef.grid.tileSize = fallbackTileSize;
    fallbackDef.maxTroopsPerPlayer = m_level.maxTroopsPerPlayer;
    terrainService.initialize(fallbackDef);

    if (m_ground) {
      m_ground->configure(fallbackTileSize, fallbackGridWidth,
                          fallbackGridHeight);
    }

    Game::Systems::CommandService::initialize(fallbackGridWidth,
                                              fallbackGridHeight);

    auto &visibilityService = Game::Map::VisibilityService::instance();
    visibilityService.initialize(fallbackGridWidth, fallbackGridHeight,
                                 fallbackTileSize);
    visibilityService.computeImmediate(*m_world, m_runtime.localOwnerId);
    if (m_fog && visibilityService.isInitialized()) {
      m_fog->updateMask(
          visibilityService.getWidth(), visibilityService.getHeight(),
          visibilityService.getTileSize(), visibilityService.snapshotCells());
    }
    m_runtime.visibilityVersion = visibilityService.version();
    m_runtime.visibilityUpdateAccumulator = 0.0f;
  }
}

bool GameEngine::hasPatrolPreviewWaypoint() const {
  return m_commandController && m_commandController->hasPatrolFirstWaypoint();
}

QVector3D GameEngine::getPatrolPreviewWaypoint() const {
  if (!m_commandController)
    return QVector3D();
  return m_commandController->getPatrolFirstWaypoint();
}

void GameEngine::updateAmbientState(float dt) {

  m_ambientCheckTimer += dt;
  const float CHECK_INTERVAL = 2.0f;

  if (m_ambientCheckTimer < CHECK_INTERVAL) {
    return;
  }
  m_ambientCheckTimer = 0.0f;

  Engine::Core::AmbientState newState = Engine::Core::AmbientState::PEACEFUL;

  if (!m_runtime.victoryState.isEmpty()) {
    if (m_runtime.victoryState == "victory") {
      newState = Engine::Core::AmbientState::VICTORY;
    } else if (m_runtime.victoryState == "defeat") {
      newState = Engine::Core::AmbientState::DEFEAT;
    }
  } else if (isPlayerInCombat()) {

    newState = Engine::Core::AmbientState::COMBAT;
  } else if (m_entityCache.enemyBarracksAlive &&
             m_entityCache.playerBarracksAlive) {

    newState = Engine::Core::AmbientState::TENSE;
  }

  if (newState != m_currentAmbientState) {
    Engine::Core::AmbientState previousState = m_currentAmbientState;
    m_currentAmbientState = newState;

    Engine::Core::EventManager::instance().publish(
        Engine::Core::AmbientStateChangedEvent(newState, previousState));

    qInfo() << "Ambient state changed from" << static_cast<int>(previousState)
            << "to" << static_cast<int>(newState);
  }
}

bool GameEngine::isPlayerInCombat() const {
  if (!m_world) {
    return false;
  }

  auto units = m_world->getEntitiesWith<Engine::Core::UnitComponent>();
  const float COMBAT_CHECK_RADIUS = 15.0f;

  for (auto *entity : units) {
    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->ownerId != m_runtime.localOwnerId || unit->health <= 0) {
      continue;
    }

    if (entity->hasComponent<Engine::Core::AttackTargetComponent>()) {
      return true;
    }

    auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
    if (!transform) {
      continue;
    }

    for (auto *otherEntity : units) {
      auto *otherUnit =
          otherEntity->getComponent<Engine::Core::UnitComponent>();
      if (!otherUnit || otherUnit->ownerId == m_runtime.localOwnerId ||
          otherUnit->health <= 0) {
        continue;
      }

      auto *otherTransform =
          otherEntity->getComponent<Engine::Core::TransformComponent>();
      if (!otherTransform) {
        continue;
      }

      float dx = transform->position.x - otherTransform->position.x;
      float dz = transform->position.z - otherTransform->position.z;
      float distSq = dx * dx + dz * dz;

      if (distSq < COMBAT_CHECK_RADIUS * COMBAT_CHECK_RADIUS) {
        return true;
      }
    }
  }

  return false;
}

void GameEngine::loadAudioResources() {
  auto &audioSys = AudioSystem::getInstance();

  QString basePath = QCoreApplication::applicationDirPath() + "/assets/audio/";

  if (audioSys.loadSound(
          "archer_voice",
          (basePath + "voices/archer_voice.wav").toStdString())) {
    qInfo() << "Loaded archer voice";
  } else {
    qWarning() << "Failed to load archer voice";
  }

  if (audioSys.loadSound(
          "knight_voice",
          (basePath + "voices/knight_voice.wav").toStdString())) {
    qInfo() << "Loaded knight voice";
  } else {
    qWarning() << "Failed to load knight voice";
  }

  if (audioSys.loadSound(
          "spearman_voice",
          (basePath + "voices/spearman_voice.wav").toStdString())) {
    qInfo() << "Loaded spearman voice";
  } else {
    qWarning() << "Failed to load spearman voice";
  }

  if (audioSys.loadMusic("music_peaceful",
                         (basePath + "music/peaceful.wav").toStdString())) {
    qInfo() << "Loaded peaceful music";
  } else {
    qWarning() << "Failed to load peaceful music";
  }

  if (audioSys.loadMusic("music_tense",
                         (basePath + "music/tense.wav").toStdString())) {
    qInfo() << "Loaded tense music";
  } else {
    qWarning() << "Failed to load tense music";
  }

  if (audioSys.loadMusic("music_combat",
                         (basePath + "music/combat.wav").toStdString())) {
    qInfo() << "Loaded combat music";
  } else {
    qWarning() << "Failed to load combat music";
  }

  if (audioSys.loadMusic("music_victory",
                         (basePath + "music/victory.wav").toStdString())) {
    qInfo() << "Loaded victory music";
  } else {
    qWarning() << "Failed to load victory music";
  }

  if (audioSys.loadMusic("music_defeat",
                         (basePath + "music/defeat.wav").toStdString())) {
    qInfo() << "Loaded defeat music";
  } else {
    qWarning() << "Failed to load defeat music";
  }

  qInfo() << "Audio resources loading complete";
}
