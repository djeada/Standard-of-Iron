#include "game_engine.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QVariant>

#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/map/level_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/ai_system.h"
#include "game/systems/arrow_system.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/camera_controller.h"
#include "game/systems/camera_follow_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/command_service.h"
#include "game/systems/formation_planner.h"
#include "game/systems/movement_system.h"
#include "game/systems/owner_registry.h"
#include "game/systems/patrol_system.h"
#include "game/systems/picking_service.h"
#include "game/systems/production_service.h"
#include "game/systems/production_system.h"
#include "game/systems/selection_system.h"
#include "game/systems/terrain_alignment_system.h"
#include "game/units/troop_config.h"
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
#include <cmath>
#include <limits>

GameEngine::GameEngine() {
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
  m_arrowSystem = static_cast<Game::Systems::ArrowSystem *>(arrowSys.get());
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
    m_selectionSystem =
        dynamic_cast<Game::Systems::SelectionSystem *>(selSys.get());
    m_world->addSystem(std::move(selSys));
  }

  m_selectedUnitsModel = new SelectedUnitsModel(this, this);
  QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");
  m_pickingService = std::make_unique<Game::Systems::PickingService>();

  Engine::Core::EventManager::instance().subscribe<Engine::Core::UnitDiedEvent>(
      [this](const Engine::Core::UnitDiedEvent &e) {
        if (e.ownerId != m_runtime.localOwnerId) {

          int individualsPerUnit =
              Game::Units::TroopConfig::instance().getIndividualsPerUnit(
                  e.unitType);
          m_enemyTroopsDefeated += individualsPerUnit;
          emit enemyTroopsDefeatedChanged();
        }
      });
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

  const auto &sel = m_selectionSystem->getSelectedUnits();
  if (!sel.empty()) {
    m_selectionSystem->clearSelection();
    syncSelectionFlags();
    emit selectedUnitsChanged();
    if (m_selectedUnitsModel)
      QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");

    setCursorMode("normal");
    return;
  }
}

void GameEngine::onAttackClick(qreal sx, qreal sy) {
  if (!m_window)
    return;
  ensureInitialized();
  if (!m_selectionSystem || !m_pickingService || !m_camera || !m_world)
    return;
  (void)sx;
  (void)sy;

  const auto &selected = m_selectionSystem->getSelectedUnits();
  if (selected.empty()) {
    setCursorMode("normal");
    return;
  }

  Engine::Core::EntityID targetId =
      m_pickingService->pickUnitFirst(float(sx), float(sy), *m_world, *m_camera,
                                      m_viewport.width, m_viewport.height, 0);

  if (targetId == 0) {
    setCursorMode("normal");
    return;
  }

  auto *targetEntity = m_world->getEntity(targetId);
  if (!targetEntity) {
    return;
  }

  auto *targetUnit = targetEntity->getComponent<Engine::Core::UnitComponent>();
  if (!targetUnit) {
    (void)targetId;
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
      auto *transform =
          entity->getComponent<Engine::Core::TransformComponent>();
      movement->hasTarget = false;
      movement->path.clear();
      movement->pathPending = false;
      movement->pendingRequestId = 0;
      movement->repathCooldown = 0.0f;
      if (transform) {
        movement->targetX = transform->position.x;
        movement->targetY = transform->position.z;
        movement->goalX = transform->position.x;
        movement->goalY = transform->position.z;
      } else {
        movement->targetX = 0.0f;
        movement->targetY = 0.0f;
        movement->goalX = 0.0f;
        movement->goalY = 0.0f;
      }
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
      movement->pathPending = false;
      movement->pendingRequestId = 0;
      movement->repathCooldown = 0.0f;
      if (auto *transform =
              entity->getComponent<Engine::Core::TransformComponent>()) {
        movement->targetX = transform->position.x;
        movement->targetY = transform->position.z;
        movement->goalX = transform->position.x;
        movement->goalY = transform->position.z;
      }
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
    m_hover.entityId = 0;
    return;
  }

  if (m_runtime.cursorMode == "normal") {
    m_window->setCursor(Qt::ArrowCursor);
  } else {
    m_window->setCursor(Qt::BlankCursor);
  }

  m_hover.entityId =
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
    Game::Systems::CommandService::MoveOptions opts;
    opts.groupMove = selected.size() > 1;
    Game::Systems::CommandService::moveUnits(*m_world, selected, targets, opts);
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

      m_runtime.visibilityUpdateCounter++;
      if (m_runtime.visibilityUpdateCounter >= 3) {
        m_runtime.visibilityUpdateCounter = 0;
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

  if (m_selectedUnitsModel && m_selectionSystem &&
      !m_selectionSystem->getSelectedUnits().empty()) {
    m_runtime.selectionRefreshCounter++;
    if (m_runtime.selectionRefreshCounter >= 15) {
      m_runtime.selectionRefreshCounter = 0;
      QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh",
                                Qt::QueuedConnection);
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
  if (m_renderer)
    m_renderer->setHoveredEntityId(m_hover.entityId);
  if (m_renderer)
    m_renderer->setLocalOwnerId(m_runtime.localOwnerId);
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

  qreal currentX = globalCursorX();
  qreal currentY = globalCursorY();
  if (currentX != m_runtime.lastCursorX || currentY != m_runtime.lastCursorY) {
    m_runtime.lastCursorX = currentX;
    m_runtime.lastCursorY = currentY;
    emit globalCursorChanged();
  }
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

  if (m_selectionSystem->getSelectedUnits().empty()) {
    if (m_runtime.cursorMode != "normal") {
      setCursorMode("normal");
    }
  }
}

void GameEngine::cameraMove(float dx, float dz) {
  ensureInitialized();
  if (!m_camera)
    return;

  float scale = 0.12f;
  if (m_camera) {
    float dist = m_camera->getDistance();
    scale = std::max(0.12f, dist * 0.05f);
  }
  Game::Systems::CameraController ctrl;
  ctrl.move(*m_camera, dx * scale, dz * scale);
}

void GameEngine::cameraElevate(float dy) {
  ensureInitialized();
  if (!m_camera)
    return;
  Game::Systems::CameraController ctrl;

  float distance = m_camera ? m_camera->getDistance() : 10.0f;
  float scale = std::clamp(distance * 0.05f, 0.1f, 5.0f);
  ctrl.moveUp(*m_camera, dy * scale);
}

void GameEngine::resetCamera() {
  ensureInitialized();
  if (!m_camera || !m_world)
    return;

  Engine::Core::Entity *focusEntity = nullptr;
  for (auto *e : m_world->getEntitiesWith<Engine::Core::UnitComponent>()) {
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
  if (!focusEntity && m_level.playerUnitId != 0)
    focusEntity = m_world->getEntity(m_level.playerUnitId);

  if (focusEntity) {
    if (auto *t =
            focusEntity->getComponent<Engine::Core::TransformComponent>()) {
      QVector3D center(t->position.x, t->position.y, t->position.z);
      if (m_camera) {
        m_camera->setRTSView(center, 12.0f, 45.0f, 225.0f);
      }
    }
  }
}

void GameEngine::cameraZoom(float delta) {
  ensureInitialized();
  if (!m_camera)
    return;
  Game::Systems::CameraController ctrl;
  ctrl.zoomDistance(*m_camera, delta);
}

float GameEngine::cameraDistance() const {
  if (!m_camera)
    return 0.0f;
  return m_camera->getDistance();
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

  if (!std::isfinite(yawDeg) || !std::isfinite(pitchDeg)) {
    qWarning() << "GameEngine::cameraOrbit received invalid input, ignoring:"
               << yawDeg << pitchDeg;
    return;
  }

  Game::Systems::CameraController ctrl;
  ctrl.orbit(*m_camera, yawDeg, pitchDeg);
}

void GameEngine::cameraOrbitDirection(int direction, bool shift) {

  float step = shift ? 8.0f : 4.0f;
  float pitch = step * float(direction);
  cameraOrbit(0.0f, pitch);
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

      int individualsPerUnit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              unit->unitType);
      count += individualsPerUnit;
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
  m["villagerCost"] = 1;
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
  m["villagerCost"] = st.villagerCost;
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

      m_renderer->pause();

      m_renderer->lockWorldForModification();
      m_renderer->setSelectedEntities({});
      m_renderer->setHoveredEntityId(0);
    }

    m_hover.entityId = 0;

    m_world->clear();

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
    }

    auto &ownerRegistry = Game::Systems::OwnerRegistry::instance();
    ownerRegistry.clear();

    int playerOwnerId = m_selectedPlayerId;

    ownerRegistry.registerOwnerWithId(
        playerOwnerId, Game::Systems::OwnerType::Player, "Player");

    for (int id : mapPlayerIds) {
      if (id != playerOwnerId) {
        ownerRegistry.registerOwnerWithId(id, Game::Systems::OwnerType::AI,
                                          "AI " + std::to_string(id));
      }
    }

    ownerRegistry.setLocalPlayerId(playerOwnerId);
    m_runtime.localOwnerId = playerOwnerId;

    Game::Map::MapTransformer::setLocalOwnerId(m_runtime.localOwnerId);

    auto lr = Game::Map::LevelLoader::loadFromAssets(m_level.mapName, *m_world,
                                                     *m_renderer, *m_camera);
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
    } else {
      m_runtime.visibilityVersion = 0;
    }

    m_level.mapName = lr.mapName;
    m_level.playerUnitId = lr.playerUnitId;
    m_level.camFov = lr.camFov;
    m_level.camNear = lr.camNear;
    m_level.camFar = lr.camFar;
    m_level.maxTroopsPerPlayer = lr.maxTroopsPerPlayer;

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

          m_camera->setRTSView(center, 12.0f, 45.0f, 225.0f);
        }
      }
    }
    m_runtime.loading = false;

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
      if (Game::Systems::OwnerRegistry::instance().isAI(unit->ownerId)) {
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
