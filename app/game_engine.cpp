#include "game_engine.h"

#include <QQuickWindow>
#include <QOpenGLContext>
#include <QDebug>
#include <QVariant>

#include "game/core/world.h"
#include "game/core/component.h"
#include "render/gl/renderer.h"
#include "render/gl/camera.h"
#include "render/gl/resources.h"
#include "render/geom/arrow.h"
#include "render/gl/bootstrap.h"
#include "game/map/level_loader.h"
#include "game/systems/movement_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/selection_system.h"
#include "game/systems/arrow_system.h"
#include "game/systems/production_system.h"
#include "game/systems/picking_service.h"
#include "game/systems/formation_planner.h"
#include "game/systems/command_service.h"
#include "game/systems/production_service.h"
#include "game/systems/camera_follow_system.h"
#include "game/systems/camera_controller.h"

#include "selected_units_model.h"
#include <cmath>
#include <limits>

GameEngine::GameEngine() {
    m_world    = std::make_unique<Engine::Core::World>();
    m_renderer = std::make_unique<Render::GL::Renderer>();
    m_camera   = std::make_unique<Render::GL::Camera>();

    std::unique_ptr<Engine::Core::System> arrowSys = std::make_unique<Game::Systems::ArrowSystem>();
    m_arrowSystem = static_cast<Game::Systems::ArrowSystem*>(arrowSys.get());
    m_world->addSystem(std::move(arrowSys));

    m_world->addSystem(std::make_unique<Game::Systems::MovementSystem>());
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
    if (!m_window) return;
    ensureInitialized();
    QVector3D hit;
    if (!screenToGround(QPointF(sx, sy), hit)) return;
    onClickSelect(sx, sy, false);
}

void GameEngine::onRightClick(qreal sx, qreal sy) {
    if (!m_window) return;
    ensureInitialized();
    QVector3D hit;
    if (!screenToGround(QPointF(sx, sy), hit)) return;
    qInfo() << "Right-click at screen" << QPointF(sx, sy) << "-> world" << hit;
    if (!m_selectionSystem) return;
    const auto& selected = m_selectionSystem->getSelectedUnits();
    if (selected.empty()) return;
    auto targets = Game::Systems::FormationPlanner::spreadFormation(int(selected.size()), hit, 1.0f);
    Game::Systems::CommandService::moveUnits(*m_world, selected, targets);
}

void GameEngine::setHoverAtScreen(qreal sx, qreal sy) {
    if (!m_window) return;
    ensureInitialized();
    if (!m_pickingService || !m_camera || !m_world) return;
    m_hover.buildingId = m_pickingService->updateHover(float(sx), float(sy), *m_world, *m_camera, m_viewport.width, m_viewport.height);
}

void GameEngine::onClickSelect(qreal sx, qreal sy, bool additive) {
    if (!m_window || !m_selectionSystem) return;
    ensureInitialized();
    if (!m_pickingService || !m_camera || !m_world) return;
    Engine::Core::EntityID picked = m_pickingService->pickSingle(float(sx), float(sy), *m_world, *m_camera, m_viewport.width, m_viewport.height, m_runtime.localOwnerId, true);
    if (picked) {
        if (!additive) m_selectionSystem->clearSelection();
        m_selectionSystem->selectUnit(picked);
        syncSelectionFlags();
        emit selectedUnitsChanged();
        if (m_selectedUnitsModel) QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");
        return;
    }

    const auto& selected = m_selectionSystem->getSelectedUnits();
    if (!selected.empty()) {
        QVector3D hit;
        if (!screenToGround(QPointF(sx, sy), hit)) {
            return;
        }
        auto targets = Game::Systems::FormationPlanner::spreadFormation(int(selected.size()), hit, 1.0f);
        Game::Systems::CommandService::moveUnits(*m_world, selected, targets);
        syncSelectionFlags();
        return;
    }
}

void GameEngine::onAreaSelected(qreal x1, qreal y1, qreal x2, qreal y2, bool additive) {
    if (!m_window || !m_selectionSystem) return;
    ensureInitialized();
    if (!additive) m_selectionSystem->clearSelection();
    if (!m_pickingService || !m_camera || !m_world) return;
    auto picked = m_pickingService->pickInRect(float(x1), float(y1), float(x2), float(y2), *m_world, *m_camera, m_viewport.width, m_viewport.height, m_runtime.localOwnerId);
    for (auto id : picked) m_selectionSystem->selectUnit(id);
    syncSelectionFlags();
    emit selectedUnitsChanged();
    if (m_selectedUnitsModel) QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");
}

void GameEngine::initialize() {
    if (!Render::GL::RenderBootstrap::initialize(*m_renderer, *m_camera, m_resources)) {
        return;
    }
    QString mapPath = QString::fromUtf8("assets/maps/test_map.json");
    auto lr = Game::Map::LevelLoader::loadFromAssets(mapPath, *m_world, *m_renderer, *m_camera);
    m_level.mapName = lr.mapName;
    m_level.playerUnitId = lr.playerUnitId;
    m_level.camFov = lr.camFov; m_level.camNear = lr.camNear; m_level.camFar = lr.camFar;
    m_runtime.initialized = true;
}

void GameEngine::ensureInitialized() { if (!m_runtime.initialized) initialize(); }

void GameEngine::update(float dt) {
    if (m_runtime.paused) {
        dt = 0.0f;
    } else {
        dt *= m_runtime.timeScale;
    }
    if (m_world) m_world->update(dt);
    syncSelectionFlags();
    if (m_followSelectionEnabled && m_camera && m_selectionSystem && m_world) {
        Game::Systems::CameraFollowSystem cfs;
        cfs.update(*m_world, *m_selectionSystem, *m_camera);
    }
    if (m_selectedUnitsModel) QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh", Qt::QueuedConnection);
}

void GameEngine::render(int pixelWidth, int pixelHeight) {
    if (!m_renderer || !m_world || !m_runtime.initialized) return;
    if (pixelWidth > 0 && pixelHeight > 0) {
        m_viewport.width = pixelWidth; m_viewport.height = pixelHeight;
        m_renderer->setViewport(pixelWidth, pixelHeight);
    }
    if (m_selectionSystem) {
        const auto& sel = m_selectionSystem->getSelectedUnits();
        std::vector<unsigned int> ids(sel.begin(), sel.end());
        m_renderer->setSelectedEntities(ids);
    }
    m_renderer->beginFrame();
    if (m_renderer) m_renderer->setHoveredBuildingId(m_hover.buildingId);
    m_renderer->renderWorld(m_world.get());
    if (m_arrowSystem) { Render::GL::renderArrows(m_renderer.get(), m_resources.get(), *m_arrowSystem); }
    m_renderer->endFrame();
}

bool GameEngine::screenToGround(const QPointF& screenPt, QVector3D& outWorld) {
    if (!m_window || !m_camera || !m_pickingService) return false;
    int w = (m_viewport.width > 0 ? m_viewport.width : m_window->width());
    int h = (m_viewport.height > 0 ? m_viewport.height : m_window->height());
    return m_pickingService->screenToGround(*m_camera, w, h, screenPt, outWorld);
}

bool GameEngine::worldToScreen(const QVector3D& world, QPointF& outScreen) const {
    if (!m_camera || m_viewport.width <= 0 || m_viewport.height <= 0 || !m_pickingService) return false;
    return m_pickingService->worldToScreen(*m_camera, m_viewport.width, m_viewport.height, world, outScreen);
}

void GameEngine::syncSelectionFlags() {
    if (!m_world || !m_selectionSystem) return;
    const auto& sel = m_selectionSystem->getSelectedUnits();
    std::vector<Engine::Core::EntityID> toKeep;
    toKeep.reserve(sel.size());
    for (auto id : sel) {
        if (auto* e = m_world->getEntity(id)) {
            if (auto* u = e->getComponent<Engine::Core::UnitComponent>()) {
                if (u->health > 0) toKeep.push_back(id);
            }
        }
    }
    if (toKeep.size() != sel.size()) {
        m_selectionSystem->clearSelection();
        for (auto id : toKeep) m_selectionSystem->selectUnit(id);
    }
}

void GameEngine::cameraMove(float dx, float dz) {
    ensureInitialized();
    if (!m_camera) return;
    Game::Systems::CameraController ctrl;
    ctrl.move(*m_camera, dx, dz);
}

void GameEngine::cameraElevate(float dy) {
    ensureInitialized();
    if (!m_camera) return;
    Game::Systems::CameraController ctrl;
    ctrl.elevate(*m_camera, dy);
}

void GameEngine::cameraYaw(float degrees) {
    ensureInitialized();
    if (!m_camera) return;
    Game::Systems::CameraController ctrl;
    ctrl.yaw(*m_camera, degrees);
}

void GameEngine::cameraOrbit(float yawDeg, float pitchDeg) {
    ensureInitialized();
    if (!m_camera) return;
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
        m_camera->lookAt(pos, tgt, QVector3D(0,1,0));
    }
}

void GameEngine::cameraSetFollowLerp(float alpha) {
    ensureInitialized();
    if (!m_camera) return;
    float a = std::clamp(alpha, 0.0f, 1.0f);
    Game::Systems::CameraController ctrl;
    ctrl.setFollowLerp(*m_camera, a);
}

QObject* GameEngine::selectedUnitsModel() { return m_selectedUnitsModel; }

bool GameEngine::hasSelectedType(const QString& type) const {
    if (!m_selectionSystem || !m_world) return false;
    const auto& sel = m_selectionSystem->getSelectedUnits();
    for (auto id : sel) {
        if (auto* e = m_world->getEntity(id)) {
            if (auto* u = e->getComponent<Engine::Core::UnitComponent>()) {
                if (QString::fromStdString(u->unitType) == type) return true;
            }
        }
    }
    return false;
}

void GameEngine::recruitNearSelected(const QString& unitType) {
    ensureInitialized();
    if (!m_world) return;
    if (!m_selectionSystem) return;
    const auto& sel = m_selectionSystem->getSelectedUnits();
    if (sel.empty()) return;
    Game::Systems::ProductionService::startProductionForFirstSelectedBarracks(*m_world, sel, m_runtime.localOwnerId, unitType.toStdString());
}

QVariantMap GameEngine::getSelectedProductionState() const {
    QVariantMap m;
    m["hasBarracks"] = false;
    m["inProgress"] = false;
    m["timeRemaining"] = 0.0;
    m["buildTime"] = 0.0;
    m["producedCount"] = 0;
    m["maxUnits"] = 0;
    if (!m_selectionSystem || !m_world) return m;
    Game::Systems::ProductionState st;
    Game::Systems::ProductionService::getSelectedBarracksState(*m_world, m_selectionSystem->getSelectedUnits(), m_runtime.localOwnerId, st);
    m["hasBarracks"] = st.hasBarracks;
    m["inProgress"] = st.inProgress;
    m["timeRemaining"] = st.timeRemaining;
    m["buildTime"] = st.buildTime;
    m["producedCount"] = st.producedCount;
    m["maxUnits"] = st.maxUnits;
    return m;
}

void GameEngine::setRallyAtScreen(qreal sx, qreal sy) {
    ensureInitialized();
    if (!m_world || !m_selectionSystem) return;
    QVector3D hit;
    if (!screenToGround(QPointF(sx, sy), hit)) return;
    Game::Systems::ProductionService::setRallyForFirstSelectedBarracks(*m_world, m_selectionSystem->getSelectedUnits(), m_runtime.localOwnerId, hit.x(), hit.z());
}

void GameEngine::getSelectedUnitIds(std::vector<Engine::Core::EntityID>& out) const {
    out.clear();
    if (!m_selectionSystem) return;
    const auto& ids = m_selectionSystem->getSelectedUnits();
    out.assign(ids.begin(), ids.end());
}

bool GameEngine::getUnitInfo(Engine::Core::EntityID id, QString& name, int& health, int& maxHealth,
                             bool& isBuilding, bool& alive) const {
    if (!m_world) return false;
    auto* e = m_world->getEntity(id);
    if (!e) return false;
    isBuilding = e->hasComponent<Engine::Core::BuildingComponent>();
    if (auto* u = e->getComponent<Engine::Core::UnitComponent>()) {
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
