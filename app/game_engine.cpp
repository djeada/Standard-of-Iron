#include "game_engine.h"

#include "cursor_manager.h"
#include "command_controller.h"
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
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
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
#include "game/systems/picking_service.h"
#include "game/systems/production_service.h"
#include "game/systems/production_system.h"
#include "game/systems/selection_system.h"
#include "game/systems/terrain_alignment_system.h"
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
  QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");
  m_pickingService = std::make_unique<Game::Systems::PickingService>();
  m_victoryService = std::make_unique<Game::Systems::VictoryService>();
  m_cameraService = std::make_unique<Game::Systems::CameraService>();

  m_cursorManager = std::make_unique<CursorManager>();
  m_hoverTracker = std::make_unique<HoverTracker>(m_pickingService.get());
  m_selectionController = std::make_unique<Game::Systems::SelectionController>(
      m_world.get(), m_pickingService.get(), this);
  m_commandController = std::make_unique<CommandController>(
      m_world.get(), m_pickingService.get(), this);

  connect(m_cursorManager.get(), &CursorManager::modeChanged, this,
          &GameEngine::cursorModeChanged);
  connect(m_cursorManager.get(), &CursorManager::globalCursorChanged, this,
          &GameEngine::globalCursorChanged);
  connect(m_selectionController.get(),
          &Game::Systems::SelectionController::selectionChanged, this,
          &GameEngine::selectedUnitsChanged);
  connect(m_selectionController.get(),
          &Game::Systems::SelectionController::selectionModelRefreshRequested,
          [this]() {
            if (m_selectedUnitsModel)
              QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");
          });

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
  onClickSelect(sx, sy, false);
}

void GameEngine::onRightClick(qreal sx, qreal sy) {
  if (!m_window)
    return;
  ensureInitialized();

  if (m_cursorManager->mode() == "patrol" || m_cursorManager->mode() == "attack") {
    setCursorMode("normal");
    return;
  }

  if (m_selectionController && m_selectionController->onRightClickClearSelection()) {
    syncSelectionFlags();
    setCursorMode("normal");
    return;
  }
}

void GameEngine::onAttackClick(qreal sx, qreal sy) {
  if (!m_window || !m_commandController)
    return;
  ensureInitialized();

  auto result = m_commandController->onAttackClick(
      sx, sy, m_window, m_viewport.width, m_viewport.height,
      m_runtime.localOwnerId, m_camera.get());

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
  if (!m_commandController)
    return;
  ensureInitialized();

  auto screenToGroundFunc = [this](const QPointF &pt, QVector3D &out) {
    return screenToGround(pt, out);
  };

  auto result = m_commandController->onPatrolClick(sx, sy, m_window,
                                                  screenToGroundFunc);

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
  if (!m_window || !m_selectionController)
    return;
  ensureInitialized();
  
  auto screenToGroundFunc = [this](const QPointF &pt, QVector3D &out) {
    return screenToGround(pt, out);
  };
  
  m_selectionController->onClickSelect(sx, sy, additive, m_window,
                                      m_viewport.width, m_viewport.height,
                                      m_runtime.localOwnerId, m_camera.get(),
                                      screenToGroundFunc);
  syncSelectionFlags();
}

void GameEngine::onAreaSelected(qreal x1, qreal y1, qreal x2, qreal y2,
                                bool additive) {
  if (!m_window || !m_selectionController)
    return;
  ensureInitialized();
  
  m_selectionController->onAreaSelected(x1, y1, x2, y2, additive, m_window,
                                       m_viewport.width, m_viewport.height,
                                       m_runtime.localOwnerId, m_camera.get());
  syncSelectionFlags();
}

void GameEngine::initialize() {
  if (!Render::GL::RenderBootstrap::initialize(*m_renderer, *m_camera)) {
    setError("Failed to initialize OpenGL renderer");
    return;
  }

  if (m_ground) {
    m_ground->configureExtent(50.0f);
  }
  m_runtime.initialized = true;
}

void GameEngine::ensureInitialized() {
  if (!m_runtime.initialized)
    initialize();
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
    m_cameraService->updateFollow(*m_camera, *m_world, m_followSelectionEnabled);
  }

  if (m_selectedUnitsModel) {
    auto *selectionSystem =
        m_world->getSystem<Game::Systems::SelectionSystem>();
    if (selectionSystem && !selectionSystem->getSelectedUnits().empty()) {
      m_runtime.selectionRefreshCounter++;
      if (m_runtime.selectionRefreshCounter >= 15) {
        m_runtime.selectionRefreshCounter = 0;
        QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh",
                                  Qt::QueuedConnection);
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
    if (m_cursorManager && m_cursorManager->hasPatrolFirstWaypoint()) {
      previewWaypoint = m_cursorManager->getPatrolFirstWaypoint();
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
                                   m_window, m_viewport.width, m_viewport.height,
                                   world, outScreen);
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
  if (!m_commandController)
    return;
  
  auto screenToGroundFunc = [this](const QPointF &pt, QVector3D &out) {
    return screenToGround(pt, out);
  };
  
  m_commandController->setRallyAtScreen(sx, sy, screenToGroundFunc,
                                       m_runtime.localOwnerId);
}

QVariantList GameEngine::availableMaps() const {
  QVariantList list;
  QDir mapsDir(QStringLiteral("assets/maps"));
  if (!mapsDir.exists())
    return list;

  QStringList files =
      mapsDir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);
  for (const QString &f : files) {
    QString path = mapsDir.filePath(f);
    QFile file(path);
    QString name = f;
    QString desc;
    QSet<int> playerIds;
    if (file.open(QIODevice::ReadOnly)) {
      QByteArray data = file.readAll();
      file.close();
      QJsonParseError err;
      QJsonDocument doc = QJsonDocument::fromJson(data, &err);
      if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("name") && obj["name"].isString())
          name = obj["name"].toString();
        if (obj.contains("description") && obj["description"].isString())
          desc = obj["description"].toString();

        if (obj.contains("spawns") && obj["spawns"].isArray()) {
          QJsonArray spawns = obj["spawns"].toArray();
          for (const QJsonValue &spawnVal : spawns) {
            if (spawnVal.isObject()) {
              QJsonObject spawn = spawnVal.toObject();
              if (spawn.contains("playerId")) {
                int playerId = spawn["playerId"].toInt();
                if (playerId > 0) {
                  playerIds.insert(playerId);
                }
              }
            }
          }
        }
      }
    }
    QVariantMap entry;
    entry["name"] = name;
    entry["description"] = desc;
    entry["path"] = path;
    entry["playerCount"] = playerIds.size();
    QVariantList playerIdList;
    QList<int> sortedIds = playerIds.values();
    std::sort(sortedIds.begin(), sortedIds.end());
    for (int id : sortedIds) {
      playerIdList.append(id);
    }
    entry["playerIds"] = playerIdList;

    QString thumbnail;
    if (file.open(QIODevice::ReadOnly)) {
      QByteArray data = file.readAll();
      file.close();
      QJsonParseError err;
      QJsonDocument doc = QJsonDocument::fromJson(data, &err);
      if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("thumbnail") && obj["thumbnail"].isString()) {
          thumbnail = obj["thumbnail"].toString();
        }
      }
    }

    if (thumbnail.isEmpty()) {
      QString baseName = QFileInfo(f).baseName();
      thumbnail = QString("assets/maps/%1_thumb.png").arg(baseName);

      if (!QFileInfo::exists(thumbnail)) {
        thumbnail = "";
      }
    }
    entry["thumbnail"] = thumbnail;

    list.append(entry);
  }
  return list;
}

void GameEngine::startSkirmish(const QString &mapPath,
                               const QVariantList &playerConfigs) {

  clearError();

  m_level.mapName = mapPath;

  m_runtime.victoryState = "";

  if (!m_runtime.initialized) {
    initialize();
    return;
  }

  if (m_world && m_renderer && m_camera) {

    m_runtime.loading = true;

    if (auto *selectionSystem =
            m_world->getSystem<Game::Systems::SelectionSystem>()) {
      selectionSystem->clearSelection();
    }

    if (m_renderer) {

      m_renderer->pause();

      m_renderer->lockWorldForModification();
      m_renderer->setSelectedEntities({});
      m_renderer->setHoveredEntityId(0);
    }

    if (m_hoverTracker) {
      m_hoverTracker->updateHover(-1, -1, *m_world, *m_camera, 0, 0);
    }

    m_world->clear();

    m_entityCache.reset();

    Game::Systems::BuildingCollisionRegistry::instance().clear();

    QSet<int> mapPlayerIds;
    QFile mapFile(mapPath);
    if (mapFile.open(QIODevice::ReadOnly)) {
      QByteArray data = mapFile.readAll();
      mapFile.close();
      QJsonParseError err;
      QJsonDocument doc = QJsonDocument::fromJson(data, &err);
      if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("spawns") && obj["spawns"].isArray()) {
          QJsonArray spawns = obj["spawns"].toArray();
          for (const QJsonValue &spawnVal : spawns) {
            if (spawnVal.isObject()) {
              QJsonObject spawn = spawnVal.toObject();
              if (spawn.contains("playerId")) {
                int playerId = spawn["playerId"].toInt();
                if (playerId > 0) {
                  mapPlayerIds.insert(playerId);
                }
              }
            }
          }
        }
      }
    } else {
      qWarning() << "Could not open map file for reading player IDs:" << mapPath;
    }

    auto &ownerRegistry = Game::Systems::OwnerRegistry::instance();
    ownerRegistry.clear();

    int playerOwnerId = m_selectedPlayerId;

    if (!mapPlayerIds.contains(playerOwnerId)) {
      if (!mapPlayerIds.isEmpty()) {
        QList<int> sortedIds = mapPlayerIds.values();
        std::sort(sortedIds.begin(), sortedIds.end());
        playerOwnerId = sortedIds.first();
        qWarning() << "Selected player ID" << m_selectedPlayerId
                   << "not found in map spawns. Using" << playerOwnerId
                   << "instead.";
        m_selectedPlayerId = playerOwnerId;
        emit selectedPlayerIdChanged();
      } else {
        qWarning() << "No valid player spawns found in map. Using default "
                      "player ID"
                   << playerOwnerId;
      }
    }

    ownerRegistry.setLocalPlayerId(playerOwnerId);
    m_runtime.localOwnerId = playerOwnerId;

    std::unordered_map<int, int> teamOverrides;
    QVariantList savedPlayerConfigs;
    std::set<int> processedPlayerIds;

    if (!playerConfigs.isEmpty()) {

      for (const QVariant &configVar : playerConfigs) {
        QVariantMap config = configVar.toMap();
        int playerId = config.value("playerId", -1).toInt();
        int teamId = config.value("teamId", 0).toInt();
        QString colorHex = config.value("colorHex", "#FFFFFF").toString();
        bool isHuman = config.value("isHuman", false).toBool();

        if (isHuman && playerId != playerOwnerId) {
          playerId = playerOwnerId;
        }

        if (processedPlayerIds.count(playerId) > 0) {
          continue;
        }

        if (playerId >= 0) {
          processedPlayerIds.insert(playerId);
          teamOverrides[playerId] = teamId;

          QVariantMap updatedConfig = config;
          updatedConfig["playerId"] = playerId;
          savedPlayerConfigs.append(updatedConfig);
        }
      }
    }

    Game::Map::MapTransformer::setLocalOwnerId(m_runtime.localOwnerId);
    Game::Map::MapTransformer::setPlayerTeamOverrides(teamOverrides);

    auto lr = Game::Map::LevelLoader::loadFromAssets(m_level.mapName, *m_world,
                                                     *m_renderer, *m_camera);

    if (!lr.ok && !lr.errorMessage.isEmpty()) {
      setError(lr.errorMessage);
    }

    if (!savedPlayerConfigs.isEmpty()) {
      for (const QVariant &configVar : savedPlayerConfigs) {
        QVariantMap config = configVar.toMap();
        int playerId = config.value("playerId", -1).toInt();
        QString colorHex = config.value("colorHex", "#FFFFFF").toString();

        if (playerId >= 0 && colorHex.startsWith("#") &&
            colorHex.length() == 7) {
          bool ok;
          int r = colorHex.mid(1, 2).toInt(&ok, 16);
          int g = colorHex.mid(3, 2).toInt(&ok, 16);
          int b = colorHex.mid(5, 2).toInt(&ok, 16);
          ownerRegistry.setOwnerColor(playerId, r / 255.0f, g / 255.0f,
                                      b / 255.0f);
        }
      }

      if (m_world) {
        auto entities = m_world->getEntitiesWith<Engine::Core::UnitComponent>();
        std::unordered_map<int, int> ownerEntityCount;
        for (auto *entity : entities) {
          auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
          auto *renderable =
              entity->getComponent<Engine::Core::RenderableComponent>();
          if (unit && renderable) {
            QVector3D tc = Game::Visuals::teamColorForOwner(unit->ownerId);
            renderable->color[0] = tc.x();
            renderable->color[1] = tc.y();
            renderable->color[2] = tc.z();
            ownerEntityCount[unit->ownerId]++;
          }
        }
      }
    }
    auto &terrainService = Game::Map::TerrainService::instance();

    if (m_ground) {
      if (lr.ok)
        m_ground->configure(lr.tileSize, lr.gridWidth, lr.gridHeight);
      else
        m_ground->configureExtent(50.0f);
      if (terrainService.isInitialized())
        m_ground->setBiome(terrainService.biomeSettings());
    }

    if (m_terrain) {
      if (terrainService.isInitialized() && terrainService.getHeightMap()) {
        m_terrain->configure(*terrainService.getHeightMap(),
                             terrainService.biomeSettings());
      }
    }

    if (m_biome) {
      if (terrainService.isInitialized() && terrainService.getHeightMap()) {
        m_biome->configure(*terrainService.getHeightMap(),
                           terrainService.biomeSettings());
      }
    }

    if (m_stone) {
      if (terrainService.isInitialized() && terrainService.getHeightMap()) {
        m_stone->configure(*terrainService.getHeightMap(),
                           terrainService.biomeSettings());
      }
    }

    int mapWidth = lr.ok ? lr.gridWidth : 100;
    int mapHeight = lr.ok ? lr.gridHeight : 100;
    Game::Systems::CommandService::initialize(mapWidth, mapHeight);

    auto &visibilityService = Game::Map::VisibilityService::instance();
    visibilityService.initialize(mapWidth, mapHeight, lr.tileSize);
    if (m_world)
      visibilityService.computeImmediate(*m_world, m_runtime.localOwnerId);
    if (m_fog && visibilityService.isInitialized()) {
      m_fog->updateMask(
          visibilityService.getWidth(), visibilityService.getHeight(),
          visibilityService.getTileSize(), visibilityService.snapshotCells());
      m_runtime.visibilityVersion = visibilityService.version();
      m_runtime.visibilityUpdateAccumulator = 0.0f;
    } else {
      m_runtime.visibilityVersion = 0;
      m_runtime.visibilityUpdateAccumulator = 0.0f;
    }

    m_level.mapName = lr.mapName;
    m_level.playerUnitId = lr.playerUnitId;
    m_level.camFov = lr.camFov;
    m_level.camNear = lr.camNear;
    m_level.camFar = lr.camFar;
    m_level.maxTroopsPerPlayer = lr.maxTroopsPerPlayer;

    if (m_victoryService) {
      m_victoryService->configure(lr.victoryConfig, m_runtime.localOwnerId);
      m_victoryService->setVictoryCallback([this](const QString &state) {
        if (m_runtime.victoryState != state) {
          m_runtime.victoryState = state;
          emit victoryStateChanged();
        }
      });
    }

    if (m_biome) {
      m_biome->refreshGrass();
    }

    if (m_renderer) {

      m_renderer->unlockWorldForModification();
      m_renderer->resume();
    }

    if (m_world && m_camera) {
      Engine::Core::Entity *focusEntity = nullptr;

      auto candidates = m_world->getEntitiesWith<Engine::Core::UnitComponent>();
      for (auto *e : candidates) {
        if (!e)
          continue;
        auto *u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u)
          continue;
        if (u->unitType == "barracks" && u->ownerId == m_runtime.localOwnerId &&
            u->health > 0) {
          focusEntity = e;
          break;
        }
      }

      if (!focusEntity && m_level.playerUnitId != 0) {
        focusEntity = m_world->getEntity(m_level.playerUnitId);
      }

      if (focusEntity) {
        if (auto *t =
                focusEntity->getComponent<Engine::Core::TransformComponent>()) {
          QVector3D center(t->position.x, t->position.y, t->position.z);

          const auto &camConfig = Game::GameConfig::instance().camera();
          m_camera->setRTSView(center, camConfig.defaultDistance,
                               camConfig.defaultPitch, camConfig.defaultYaw);
        }
      }
    }
    m_runtime.loading = false;

    if (auto *aiSystem = m_world->getSystem<Game::Systems::AISystem>()) {
      aiSystem->reinitialize();
    }

    rebuildEntityCache();

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
  return m_cursorManager && m_cursorManager->hasPatrolFirstWaypoint();
}

QVector3D GameEngine::getPatrolPreviewWaypoint() const {
  if (!m_cursorManager)
    return QVector3D();
  return m_cursorManager->getPatrolFirstWaypoint();
}

