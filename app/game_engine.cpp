#include "game_engine.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QVariant>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/level_loader.h"
#include "game/systems/ai_system.h"
#include "game/systems/arrow_system.h"
#include "game/systems/camera_controller.h"
#include "game/systems/camera_follow_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/command_service.h"
#include "game/systems/formation_planner.h"
#include "game/systems/movement_system.h"
#include "game/systems/patrol_system.h"
#include "game/systems/picking_service.h"
#include "game/systems/production_service.h"
#include "game/systems/production_system.h"
#include "game/systems/selection_system.h"
#include "render/geom/arrow.h"
#include "render/geom/patrol_flags.h"
#include "render/gl/bootstrap.h"
#include "render/gl/camera.h"
#include "render/gl/resources.h"
#include "render/ground/ground_renderer.h"
#include "render/scene_renderer.h"

#include "selected_units_model.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <limits>

GameEngine::GameEngine() {
  m_world = std::make_unique<Engine::Core::World>();
  m_renderer = std::make_unique<Render::GL::Renderer>();
  m_camera = std::make_unique<Render::GL::Camera>();
  m_ground = std::make_unique<Render::GL::GroundRenderer>();

  std::unique_ptr<Engine::Core::System> arrowSys =
      std::make_unique<Game::Systems::ArrowSystem>();
  m_arrowSystem = static_cast<Game::Systems::ArrowSystem *>(arrowSys.get());
  m_world->addSystem(std::move(arrowSys));

  m_world->addSystem(std::make_unique<Game::Systems::MovementSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::PatrolSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::CombatSystem>());
  m_world->addSystem(std::make_unique<Game::Systems::AISystem>());
  m_world->addSystem(std::make_unique<Game::Systems::ProductionSystem>());

  m_selectionSystem = std::make_unique<Game::Systems::SelectionSystem>();
  m_world->addSystem(std::make_unique<Game::Systems::SelectionSystem>());

  m_selectedUnitsModel = new SelectedUnitsModel(this, this);
  QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");
  m_pickingService = std::make_unique<Game::Systems::PickingService>();
}

GameEngine::~GameEngine() = default;

void GameEngine::onMapClicked(qreal sx, qreal sy) {
  if (!m_window)
    return;
  ensureInitialized();
  QVector3D hit;
  if (!screenToGround(QPointF(sx, sy), hit))
    return;
  onClickSelect(sx, sy, false);
}

void GameEngine::onRightClick(qreal sx, qreal sy) {
  if (!m_window)
    return;
  ensureInitialized();
  if (!m_selectionSystem)
    return;

  m_selectionSystem->clearSelection();
  syncSelectionFlags();
  emit selectedUnitsChanged();
  if (m_selectedUnitsModel)
    QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");

  setCursorMode("normal");
}

void GameEngine::onAttackClick(qreal sx, qreal sy) {
  if (!m_window)
    return;
  ensureInitialized();
  if (!m_selectionSystem || !m_pickingService || !m_camera || !m_world)
    return;

  const auto &selected = m_selectionSystem->getSelectedUnits();
  if (selected.empty()) {
    setCursorMode("normal");
    return;
  }

  Engine::Core::EntityID targetId = m_pickingService->pickSingle(
      float(sx), float(sy), *m_world, *m_camera, m_viewport.width,
      m_viewport.height, 0, true);

  if (targetId == 0) {

    return;
  }

  auto *targetEntity = m_world->getEntity(targetId);
  if (!targetEntity) {
    return;
  }

  auto *targetUnit = targetEntity->getComponent<Engine::Core::UnitComponent>();
  if (!targetUnit) {
    return;
  }

  if (targetUnit->ownerId == m_runtime.localOwnerId) {
    return;
  }

  Game::Systems::CommandService::attackTarget(*m_world, selected, targetId,
                                              true);

  if (m_arrowSystem) {

    auto *targetTrans =
        targetEntity->getComponent<Engine::Core::TransformComponent>();
    if (targetTrans) {
      QVector3D targetPos(targetTrans->position.x,
                          targetTrans->position.y + 1.0f,
                          targetTrans->position.z);
      QVector3D aboveTarget = targetPos + QVector3D(0, 2.0f, 0);

      m_arrowSystem->spawnArrow(aboveTarget, targetPos,
                                QVector3D(1.0f, 0.2f, 0.2f), 6.0f);
    }
  }

  setCursorMode("normal");
}

void GameEngine::onStopCommand() {
  if (!m_selectionSystem || !m_world)
    return;
  ensureInitialized();

  const auto &selected = m_selectionSystem->getSelectedUnits();
  if (selected.empty())
    return;

  for (auto id : selected) {
    auto *entity = m_world->getEntity(id);
    if (!entity)
      continue;

    if (auto *movement =
            entity->getComponent<Engine::Core::MovementComponent>()) {
      movement->hasTarget = false;
      movement->targetX = 0.0f;
      movement->targetY = 0.0f;
      movement->path.clear();
    }

    if (auto *attack =
            entity->getComponent<Engine::Core::AttackTargetComponent>()) {
      attack->targetId = 0;
    }

    if (auto *patrol = entity->getComponent<Engine::Core::PatrolComponent>()) {
      patrol->patrolling = false;
      patrol->waypoints.clear();
    }
  }

  setCursorMode("normal");
}

void GameEngine::onPatrolClick(qreal sx, qreal sy) {
  if (!m_selectionSystem || !m_world)
    return;
  ensureInitialized();

  const auto &selected = m_selectionSystem->getSelectedUnits();
  if (selected.empty())
    return;

  QVector3D hit;
  if (!screenToGround(QPointF(sx, sy), hit))
    return;

  if (!m_patrol.hasFirstWaypoint) {
    m_patrol.firstWaypoint = hit;
    m_patrol.hasFirstWaypoint = true;

    return;
  }

  QVector3D secondWaypoint = hit;

  for (auto id : selected) {
    auto *entity = m_world->getEntity(id);
    if (!entity)
      continue;

    auto *building = entity->getComponent<Engine::Core::BuildingComponent>();
    if (building)
      continue;

    auto *patrol = entity->getComponent<Engine::Core::PatrolComponent>();
    if (!patrol) {
      patrol = entity->addComponent<Engine::Core::PatrolComponent>();
    }

    if (patrol) {
      patrol->waypoints.clear();
      patrol->waypoints.push_back(
          {m_patrol.firstWaypoint.x(), m_patrol.firstWaypoint.z()});
      patrol->waypoints.push_back({secondWaypoint.x(), secondWaypoint.z()});
      patrol->currentWaypoint = 0;
      patrol->patrolling = true;
    }

    if (auto *movement =
            entity->getComponent<Engine::Core::MovementComponent>()) {
      movement->hasTarget = false;
      movement->path.clear();
    }
    if (auto *attack =
            entity->getComponent<Engine::Core::AttackTargetComponent>()) {
      attack->targetId = 0;
    }
  }

  m_patrol.hasFirstWaypoint = false;
  setCursorMode("normal");
}

void GameEngine::setCursorMode(const QString &mode) {
  if (m_runtime.cursorMode == mode)
    return;

  if (m_runtime.cursorMode == "patrol" && mode != "patrol") {
    m_patrol.hasFirstWaypoint = false;
  }

  m_runtime.cursorMode = mode;

  if (m_window) {
    if (mode == "normal") {
      m_window->setCursor(Qt::ArrowCursor);
    } else {

      m_window->setCursor(Qt::BlankCursor);
    }
  }

  emit cursorModeChanged();

  emit globalCursorChanged();
}

qreal GameEngine::globalCursorX() const {
  if (!m_window)
    return 0;
  QPoint globalPos = QCursor::pos();
  QPoint localPos = m_window->mapFromGlobal(globalPos);
  return localPos.x();
}

qreal GameEngine::globalCursorY() const {
  if (!m_window)
    return 0;
  QPoint globalPos = QCursor::pos();
  QPoint localPos = m_window->mapFromGlobal(globalPos);
  return localPos.y();
}

void GameEngine::setHoverAtScreen(qreal sx, qreal sy) {
  if (!m_window)
    return;
  ensureInitialized();
  if (!m_pickingService || !m_camera || !m_world)
    return;

  if (sx < 0 || sy < 0) {
    if (m_runtime.cursorMode != "normal") {

      m_window->setCursor(Qt::ArrowCursor);
    }
    m_hover.buildingId = 0;
    return;
  }

  if (m_runtime.cursorMode == "normal") {
    m_window->setCursor(Qt::ArrowCursor);
  } else {
    m_window->setCursor(Qt::BlankCursor);
  }

  m_hover.buildingId =
      m_pickingService->updateHover(float(sx), float(sy), *m_world, *m_camera,
                                    m_viewport.width, m_viewport.height);
}

void GameEngine::onClickSelect(qreal sx, qreal sy, bool additive) {
  if (!m_window || !m_selectionSystem)
    return;
  ensureInitialized();
  if (!m_pickingService || !m_camera || !m_world)
    return;
  Engine::Core::EntityID picked = m_pickingService->pickSingle(
      float(sx), float(sy), *m_world, *m_camera, m_viewport.width,
      m_viewport.height, m_runtime.localOwnerId, true);
  if (picked) {
    if (!additive)
      m_selectionSystem->clearSelection();
    m_selectionSystem->selectUnit(picked);
    syncSelectionFlags();
    emit selectedUnitsChanged();
    if (m_selectedUnitsModel)
      QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");
    return;
  }

  const auto &selected = m_selectionSystem->getSelectedUnits();
  if (!selected.empty()) {
    QVector3D hit;
    if (!screenToGround(QPointF(sx, sy), hit)) {
      return;
    }
    auto targets = Game::Systems::FormationPlanner::spreadFormation(
        int(selected.size()), hit, 1.0f);
    Game::Systems::CommandService::moveUnits(*m_world, selected, targets);
    syncSelectionFlags();
    return;
  }
}

void GameEngine::onAreaSelected(qreal x1, qreal y1, qreal x2, qreal y2,
                                bool additive) {
  if (!m_window || !m_selectionSystem)
    return;
  ensureInitialized();
  if (!additive)
    m_selectionSystem->clearSelection();
  if (!m_pickingService || !m_camera || !m_world)
    return;
  auto picked = m_pickingService->pickInRect(
      float(x1), float(y1), float(x2), float(y2), *m_world, *m_camera,
      m_viewport.width, m_viewport.height, m_runtime.localOwnerId);
  for (auto id : picked)
    m_selectionSystem->selectUnit(id);
  syncSelectionFlags();
  emit selectedUnitsChanged();
  if (m_selectedUnitsModel)
    QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");
}

void GameEngine::initialize() {
  if (!Render::GL::RenderBootstrap::initialize(*m_renderer, *m_camera)) {
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

  if (m_world)
    m_world->update(dt);
  syncSelectionFlags();
  checkVictoryCondition();

  int currentTroopCount = playerTroopCount();
  if (currentTroopCount != m_runtime.lastTroopCount) {
    m_runtime.lastTroopCount = currentTroopCount;
    emit troopCountChanged();
  }

  if (m_followSelectionEnabled && m_camera && m_selectionSystem && m_world) {
    Game::Systems::CameraFollowSystem cfs;
    cfs.update(*m_world, *m_selectionSystem, *m_camera);
  }
  if (m_selectedUnitsModel)
    QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh",
                              Qt::QueuedConnection);
}

void GameEngine::render(int pixelWidth, int pixelHeight) {
  if (!m_renderer || !m_world || !m_runtime.initialized)
    return;
  if (pixelWidth > 0 && pixelHeight > 0) {
    m_viewport.width = pixelWidth;
    m_viewport.height = pixelHeight;
    m_renderer->setViewport(pixelWidth, pixelHeight);
  }
  if (m_selectionSystem) {
    const auto &sel = m_selectionSystem->getSelectedUnits();
    std::vector<unsigned int> ids(sel.begin(), sel.end());
    m_renderer->setSelectedEntities(ids);
  }
  m_renderer->beginFrame();
  if (m_ground && m_renderer) {
    if (auto *res = m_renderer->resources())
      m_ground->submit(*m_renderer, *res);
  }
  if (m_renderer)
    m_renderer->setHoveredBuildingId(m_hover.buildingId);
  m_renderer->renderWorld(m_world.get());
  if (m_arrowSystem) {
    if (auto *res = m_renderer->resources())
      Render::GL::renderArrows(m_renderer.get(), res, *m_arrowSystem);
  }

  if (auto *res = m_renderer->resources()) {
    std::optional<QVector3D> previewWaypoint;
    if (m_patrol.hasFirstWaypoint) {
      previewWaypoint = m_patrol.firstWaypoint;
    }
    Render::GL::renderPatrolFlags(m_renderer.get(), res, *m_world,
                                  previewWaypoint);
  }
  m_renderer->endFrame();

  emit globalCursorChanged();
}

bool GameEngine::screenToGround(const QPointF &screenPt, QVector3D &outWorld) {
  if (!m_window || !m_camera || !m_pickingService)
    return false;
  int w = (m_viewport.width > 0 ? m_viewport.width : m_window->width());
  int h = (m_viewport.height > 0 ? m_viewport.height : m_window->height());
  return m_pickingService->screenToGround(*m_camera, w, h, screenPt, outWorld);
}

bool GameEngine::worldToScreen(const QVector3D &world,
                               QPointF &outScreen) const {
  if (!m_camera || m_viewport.width <= 0 || m_viewport.height <= 0 ||
      !m_pickingService)
    return false;
  return m_pickingService->worldToScreen(*m_camera, m_viewport.width,
                                         m_viewport.height, world, outScreen);
}

void GameEngine::syncSelectionFlags() {
  if (!m_world || !m_selectionSystem)
    return;
  const auto &sel = m_selectionSystem->getSelectedUnits();
  std::vector<Engine::Core::EntityID> toKeep;
  toKeep.reserve(sel.size());
  for (auto id : sel) {
    if (auto *e = m_world->getEntity(id)) {
      if (auto *u = e->getComponent<Engine::Core::UnitComponent>()) {
        if (u->health > 0)
          toKeep.push_back(id);
      }
    }
  }
  if (toKeep.size() != sel.size()) {
    m_selectionSystem->clearSelection();
    for (auto id : toKeep)
      m_selectionSystem->selectUnit(id);
  }
}

void GameEngine::cameraMove(float dx, float dz) {
  ensureInitialized();
  if (!m_camera)
    return;
  Game::Systems::CameraController ctrl;
  ctrl.move(*m_camera, dx, dz);
}

void GameEngine::cameraElevate(float dy) {
  ensureInitialized();
  if (!m_camera)
    return;
  Game::Systems::CameraController ctrl;
  ctrl.elevate(*m_camera, dy);
}

void GameEngine::cameraYaw(float degrees) {
  ensureInitialized();
  if (!m_camera)
    return;
  Game::Systems::CameraController ctrl;
  ctrl.yaw(*m_camera, degrees);
}

void GameEngine::cameraOrbit(float yawDeg, float pitchDeg) {
  ensureInitialized();
  if (!m_camera)
    return;
  Game::Systems::CameraController ctrl;
  ctrl.orbit(*m_camera, yawDeg, pitchDeg);
}

void GameEngine::cameraFollowSelection(bool enable) {
  ensureInitialized();
  m_followSelectionEnabled = enable;
  if (m_camera) {
    Game::Systems::CameraController ctrl;
    ctrl.setFollowEnabled(*m_camera, enable);
  }
  if (enable && m_camera && m_selectionSystem && m_world) {
    Game::Systems::CameraFollowSystem cfs;
    cfs.snapToSelection(*m_world, *m_selectionSystem, *m_camera);
  } else if (m_camera) {
    auto pos = m_camera->getPosition();
    auto tgt = m_camera->getTarget();
    QVector3D front = (tgt - pos).normalized();
    m_camera->lookAt(pos, tgt, QVector3D(0, 1, 0));
  }
}

void GameEngine::cameraSetFollowLerp(float alpha) {
  ensureInitialized();
  if (!m_camera)
    return;
  float a = std::clamp(alpha, 0.0f, 1.0f);
  Game::Systems::CameraController ctrl;
  ctrl.setFollowLerp(*m_camera, a);
}

QObject *GameEngine::selectedUnitsModel() { return m_selectedUnitsModel; }

bool GameEngine::hasUnitsSelected() const {
  if (!m_selectionSystem)
    return false;
  const auto &sel = m_selectionSystem->getSelectedUnits();
  return !sel.empty();
}

int GameEngine::playerTroopCount() const {
  if (!m_world)
    return 0;

  int count = 0;
  auto entities = m_world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *entity : entities) {
    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if (!unit)
      continue;

    if (unit->ownerId == m_runtime.localOwnerId && unit->health > 0 &&
        unit->unitType != "barracks") {
      count++;
    }
  }
  return count;
}

bool GameEngine::hasSelectedType(const QString &type) const {
  if (!m_selectionSystem || !m_world)
    return false;
  const auto &sel = m_selectionSystem->getSelectedUnits();
  for (auto id : sel) {
    if (auto *e = m_world->getEntity(id)) {
      if (auto *u = e->getComponent<Engine::Core::UnitComponent>()) {
        if (QString::fromStdString(u->unitType) == type)
          return true;
      }
    }
  }
  return false;
}

void GameEngine::recruitNearSelected(const QString &unitType) {
  ensureInitialized();
  if (!m_world)
    return;
  if (!m_selectionSystem)
    return;
  const auto &sel = m_selectionSystem->getSelectedUnits();
  if (sel.empty())
    return;
  Game::Systems::ProductionService::startProductionForFirstSelectedBarracks(
      *m_world, sel, m_runtime.localOwnerId, unitType.toStdString());
}

QVariantMap GameEngine::getSelectedProductionState() const {
  QVariantMap m;
  m["hasBarracks"] = false;
  m["inProgress"] = false;
  m["timeRemaining"] = 0.0;
  m["buildTime"] = 0.0;
  m["producedCount"] = 0;
  m["maxUnits"] = 0;
  if (!m_selectionSystem || !m_world)
    return m;
  Game::Systems::ProductionState st;
  Game::Systems::ProductionService::getSelectedBarracksState(
      *m_world, m_selectionSystem->getSelectedUnits(), m_runtime.localOwnerId,
      st);
  m["hasBarracks"] = st.hasBarracks;
  m["inProgress"] = st.inProgress;
  m["timeRemaining"] = st.timeRemaining;
  m["buildTime"] = st.buildTime;
  m["producedCount"] = st.producedCount;
  m["maxUnits"] = st.maxUnits;
  return m;
}

QString GameEngine::getSelectedUnitsCommandMode() const {
  if (!m_selectionSystem || !m_world)
    return "normal";

  const auto &sel = m_selectionSystem->getSelectedUnits();
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
  if (!m_world || !m_selectionSystem)
    return;
  QVector3D hit;
  if (!screenToGround(QPointF(sx, sy), hit))
    return;
  Game::Systems::ProductionService::setRallyForFirstSelectedBarracks(
      *m_world, m_selectionSystem->getSelectedUnits(), m_runtime.localOwnerId,
      hit.x(), hit.z());
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
      }
    }
    QVariantMap entry;
    entry["name"] = name;
    entry["description"] = desc;
    entry["path"] = path;
    list.append(entry);
  }
  return list;
}

void GameEngine::startSkirmish(const QString &mapPath) {

  m_level.mapName = mapPath;

  if (!m_runtime.initialized) {
    initialize();
    return;
  }

  if (m_world && m_renderer && m_camera) {

    m_runtime.loading = true;

    if (m_selectionSystem) {
      m_selectionSystem->clearSelection();
    }

    if (m_renderer) {
      m_renderer->setSelectedEntities({});
      m_renderer->setHoveredBuildingId(0);
    }

    m_hover.buildingId = 0;

    m_world->clear();

    auto lr = Game::Map::LevelLoader::loadFromAssets(m_level.mapName, *m_world,
                                                     *m_renderer, *m_camera);
    if (m_ground) {
      if (lr.ok)
        m_ground->configure(lr.tileSize, lr.gridWidth, lr.gridHeight);
      else
        m_ground->configureExtent(50.0f);
    }
    m_level.mapName = lr.mapName;
    m_level.playerUnitId = lr.playerUnitId;
    m_level.camFov = lr.camFov;
    m_level.camNear = lr.camNear;
    m_level.camFar = lr.camFar;
    m_level.maxTroopsPerPlayer = lr.maxTroopsPerPlayer;

    m_runtime.loading = false;
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

void GameEngine::getSelectedUnitIds(
    std::vector<Engine::Core::EntityID> &out) const {
  out.clear();
  if (!m_selectionSystem)
    return;
  const auto &ids = m_selectionSystem->getSelectedUnits();
  out.assign(ids.begin(), ids.end());
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

void GameEngine::checkVictoryCondition() {
  if (!m_world || m_runtime.victoryState != "")
    return;

  if (m_level.mapName.isEmpty())
    return;

  bool enemyBarracksAlive = false;
  bool playerBarracksAlive = false;

  auto entities = m_world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->health <= 0)
      continue;

    if (unit->unitType == "barracks") {
      if (unit->ownerId == 2) {
        enemyBarracksAlive = true;
      } else if (unit->ownerId == m_runtime.localOwnerId) {
        playerBarracksAlive = true;
      }
    }
  }

  if (!enemyBarracksAlive) {
    m_runtime.victoryState = "victory";
    emit victoryStateChanged();
    qInfo() << "VICTORY! Enemy barracks destroyed!";
  }

  else if (!playerBarracksAlive) {
    m_runtime.victoryState = "defeat";
    emit victoryStateChanged();
    qInfo() << "DEFEAT! Your barracks was destroyed!";
  }
}
