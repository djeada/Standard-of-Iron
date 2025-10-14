#include "game_engine.h"

#include "controllers/action_vfx.h"
#include "controllers/command_controller.h"
#include "cursor_manager.h"
#include "hover_tracker.h"
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QVariant>
#include <set>
#include <unordered_map>

#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/map/level_loader.h"
#include "game/map/map_catalog.h"
#include "game/map/map_transformer.h"
#include "game/map/skirmish_loader.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/map/world_bootstrap.h"
#include "game/systems/ai_system.h"
#include "game/systems/arrow_system.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/camera_service.h"
#include "game/systems/combat_system.h"
#include "game/systems/command_service.h"
#include "game/systems/formation_planner.h"
#include "game/systems/movement_system.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/patrol_system.h"
#include "game/systems/guard_system.h"
#include "game/systems/picking_service.h"
#include "game/systems/production_service.h"
#include "game/systems/production_system.h"
#include "game/systems/selection_system.h"
#include "game/systems/terrain_alignment_system.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/victory_service.h"
#include "game/units/troop_config.h"
#include "game/visuals/team_colors.h"
#include "render/geom/arrow.h"
#include "render/geom/patrol_flags.h"
#include "render/gl/bootstrap.h"
#include "render/gl/camera.h"
#include "render/gl/resources.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_renderer.h"
#include "render/scene_renderer.h"
#include "selected_units_model.h"
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

  m_world = std::make_unique<Engine::Core::World>();
  m_renderer = std::make_unique<Render::GL::Renderer>();
  m_camera = std::make_unique<Render::GL::Camera>();
  m_ground = std::make_unique<Render::GL::GroundRenderer>();
  m_terrain = std::make_unique<Render::GL::TerrainRenderer>();
  m_biome = std::make_unique<Render::GL::BiomeRenderer>();
  m_fog = std::make_unique<Render::GL::FogRenderer>();
  m_stone = std::make_unique<Render::GL::StoneRenderer>();

  std::unique_ptr<Engine::Core::System> arrowSys =
      std::make_unique<Game::Systems::ArrowSystem>();
  m_world->addSystem(std::move(arrowSys));

  m_world->addSystem(std::make_unique<Game::Systems::MovementSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::PatrolSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::GuardSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::CombatSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::AISystem>());
  m_world->addSystem(std::make_unique<Game::Systems::ProductionSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::TerrainAlignmentSystem>());

  {
    std::unique_ptr<Engine::Core::System> selSys =
        std::make_unique<Game::Systems::SelectionSystem>();
    m_world->addSystem(std::move(selSys));
  }

  m_selectedUnitsModel = new SelectedUnitsModel(this, this);
  m_pickingService = std::make_unique<Game::Systems::PickingService>();
  m_victoryService = std::make_unique<Game::Systems::VictoryService>();
  m_cameraService = std::make_unique<Game::Systems::CameraService>();

  auto *selectionSystem = m_world->getSystem<Game::Systems::SelectionSystem>();
  m_selectionController = std::make_unique<Game::Systems::SelectionController>(
      m_world.get(), selectionSystem, m_pickingService.get());
  m_commandController = std::make_unique<App::Controllers::CommandController>(
      m_world.get(), selectionSystem, m_pickingService.get());

  m_cursorManager = std::make_unique<CursorManager>();
  m_hoverTracker = std::make_unique<HoverTracker>(m_pickingService.get());

  m_mapCatalog = std::make_unique<Game::Map::MapCatalog>();
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

  connect(m_cursorManager.get(), &CursorManager::modeChanged, this,
          &GameEngine::cursorModeChanged);
  connect(m_cursorManager.get(), &CursorManager::globalCursorChanged, this,
          &GameEngine::globalCursorChanged);

  connect(m_selectionController.get(),
          &Game::Systems::SelectionController::selectionChanged, this,
          &GameEngine::selectedUnitsChanged);
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

GameEngine::~GameEngine() = default;

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

  if (m_cursorManager->mode() == "patrol" ||
      m_cursorManager->mode() == "attack") {
    setCursorMode("normal");
    return;
  }

  const auto &sel = selectionSystem->getSelectedUnits();
  if (!sel.empty()) {
    if (m_selectionController) {
      m_selectionController->onRightClickClearSelection();
    }
    setCursorMode("normal");
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
    setCursorMode("normal");
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
    setCursorMode("normal");
  }
}

void GameEngine::onPatrolClick(qreal sx, qreal sy) {
  if (!m_commandController || !m_camera)
    return;
  ensureInitialized();

  auto result = m_commandController->onPatrolClick(
      sx, sy, m_viewport.width, m_viewport.height, m_camera.get());
  if (result.resetCursorToNormal) {
    setCursorMode("normal");
  }
}

void GameEngine::onGuardClick(qreal sx, qreal sy) {
  if (!m_commandController || !m_camera)
    return;
  ensureInitialized();

  auto result = m_commandController->onGuardClick(
      sx, sy, m_viewport.width, m_viewport.height, m_camera.get());
  if (result.resetCursorToNormal) {
    setCursorMode("normal");
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

void GameEngine::setCursorMode(const QString &mode) {
  if (!m_cursorManager)
    return;
  m_cursorManager->setMode(mode);
  m_cursorManager->updateCursorShape(m_window);
}

QString GameEngine::cursorMode() const {
  if (!m_cursorManager)
    return "normal";
  return m_cursorManager->mode();
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

void GameEngine::update(float dt) {

  if (m_runtime.loading) {
    return;
  }

  if (m_runtime.paused) {
    dt = 0.0f;
  } else {
    dt *= m_runtime.timeScale;
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
  syncSelectionFlags();

  if (m_victoryService && m_world) {
    m_victoryService->update(*m_world, dt);
  }

  int currentTroopCount = playerTroopCount();
  if (currentTroopCount != m_runtime.lastTroopCount) {
    m_runtime.lastTroopCount = currentTroopCount;
    emit troopCountChanged();
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
  if (m_ground && m_renderer) {
    if (auto *res = m_renderer->resources())
      m_ground->submit(*m_renderer, *res);
  }
  if (m_terrain && m_renderer) {
    if (auto *res = m_renderer->resources())
      m_terrain->submit(*m_renderer, *res);
  }
  if (m_biome && m_renderer) {
    m_biome->submit(*m_renderer);
  }
  if (m_stone && m_renderer) {
    m_stone->submit(*m_renderer);
  }
  if (m_fog && m_renderer) {
    if (auto *res = m_renderer->resources())
      m_fog->submit(*m_renderer, *res);
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
    if (m_cursorManager && m_cursorManager->mode() != "normal") {
      setCursorMode("normal");
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
  m["timeRemaining"] = st.timeRemaining;
  m["buildTime"] = st.buildTime;
  m["producedCount"] = st.producedCount;
  m["maxUnits"] = st.maxUnits;
  m["villagerCost"] = st.villagerCost;
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
  int guardingCount = 0;
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

    auto *guard = e->getComponent<Engine::Core::GuardComponent>();
    if (guard && guard->isGuarding)
      guardingCount++;
  }

  if (totalUnits == 0)
    return "normal";

  if (guardingCount == totalUnits)
    return "guard";
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

  m_level.mapName = mapPath;

  m_runtime.victoryState = "";

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
    loader.setFogRenderer(m_fog.get());
    loader.setStoneRenderer(m_stone.get());

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
    Game::Systems::TroopCountRegistry::instance().rebuildFromWorld(*m_world);

    emit ownerInfoChanged();
  }
}

void GameEngine::openSettings() { qInfo() << "Open settings requested"; }

void GameEngine::loadSave() {

  qInfo() << "Load save requested (not implemented)";
}

void GameEngine::exitGame() {
  qInfo() << "Exit requested";
  QCoreApplication::quit();
}

QVariantList GameEngine::getOwnerInfo() const {
  QVariantList result;
  const auto &ownerRegistry = Game::Systems::OwnerRegistry::instance();
  const auto &owners = ownerRegistry.getAllOwners();

  for (const auto &owner : owners) {
    QVariantMap ownerMap;
    ownerMap["id"] = owner.ownerId;
    ownerMap["name"] = QString::fromStdString(owner.name);

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
  if (event.ownerId == m_runtime.localOwnerId) {
    if (event.unitType == "barracks") {
      m_entityCache.playerBarracksAlive = true;
    } else {
      int individualsPerUnit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              event.unitType);
      m_entityCache.playerTroopCount += individualsPerUnit;
    }
  } else if (Game::Systems::OwnerRegistry::instance().isAI(event.ownerId)) {
    if (event.unitType == "barracks") {
      m_entityCache.enemyBarracksCount++;
      m_entityCache.enemyBarracksAlive = true;
    }
  }
}

void GameEngine::onUnitDied(const Engine::Core::UnitDiedEvent &event) {
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
  } else if (Game::Systems::OwnerRegistry::instance().isAI(event.ownerId)) {
    if (event.unitType == "barracks") {
      m_entityCache.enemyBarracksCount--;
      m_entityCache.enemyBarracksCount =
          std::max(0, m_entityCache.enemyBarracksCount);
      m_entityCache.enemyBarracksAlive = (m_entityCache.enemyBarracksCount > 0);
    }
  }
}

void GameEngine::rebuildEntityCache() {
  if (!m_world) {
    m_entityCache.reset();
    return;
  }

  m_entityCache.reset();

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
    } else if (Game::Systems::OwnerRegistry::instance().isAI(unit->ownerId)) {
      if (unit->unitType == "barracks") {
        m_entityCache.enemyBarracksCount++;
        m_entityCache.enemyBarracksAlive = true;
      }
    }
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
