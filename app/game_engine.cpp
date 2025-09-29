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
#include "game/systems/movement_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/selection_system.h"
#include "game/systems/arrow_system.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/visuals/visual_catalog.h"
#include "game/units/factory.h"
#include "game/map/environment.h"

#include "selected_units_model.h"
#include <cmath>
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

    m_selectionSystem = std::make_unique<Game::Systems::SelectionSystem>();
    m_world->addSystem(std::make_unique<Game::Systems::SelectionSystem>());

    // Expose internal pointers for models
    setProperty("_worldPtr", QVariant::fromValue<void*>(m_world.get()));
    // Selection system not yet constructed; set later after creation
    // Defer actual entity creation until initialize() when GL and renderer are ready
        setProperty("_selPtr", QVariant::fromValue<void*>(m_selectionSystem.get()));

        // Create selected units model (owned by GameEngine)
        m_selectedUnitsModel = new SelectedUnitsModel(this, this);
        QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");
}

GameEngine::~GameEngine() = default;

void GameEngine::onMapClicked(qreal sx, qreal sy) {
    if (!m_window) return;
    ensureInitialized();
    QVector3D hit;
    if (!screenToGround(QPointF(sx, sy), hit)) return;
    // Default behavior: treat left click as selection click (single)
    onClickSelect(sx, sy, false);
}

void GameEngine::onRightClick(qreal sx, qreal sy) {
    if (!m_window) return;
    ensureInitialized();
    QVector3D hit;
    if (!screenToGround(QPointF(sx, sy), hit)) return;
    qInfo() << "Right-click at screen" << QPointF(sx, sy) << "-> world" << hit;
    // Issue move command to all selected units
    if (!m_selectionSystem) return;
    const auto& selected = m_selectionSystem->getSelectedUnits();
    if (selected.empty()) return;
    // Simple formation: spread around click point
    const float spacing = 1.0f;
    int n = int(selected.size());
    int side = std::ceil(std::sqrt(float(n)));
    int i = 0;
    for (auto id : selected) {
        auto* e = m_world->getEntity(id);
        if (!e) continue;
        auto* mv = e->getComponent<Engine::Core::MovementComponent>();
        if (!mv) e->addComponent<Engine::Core::MovementComponent>(), mv = e->getComponent<Engine::Core::MovementComponent>();
        int gx = i % side;
        int gy = i / side;
        float ox = (gx - (side-1)*0.5f) * spacing;
        float oz = (gy - (side-1)*0.5f) * spacing;
        mv->targetX = hit.x() + ox;
        mv->targetY = hit.z() + oz;
        mv->hasTarget = true;
        qInfo() << "  move-> id=" << e->getId() << "target (x,z)=" << mv->targetX << mv->targetY;
        ++i;
    }
}

void GameEngine::onClickSelect(qreal sx, qreal sy, bool additive) {
    if (!m_window || !m_selectionSystem) return;
    ensureInitialized();
    // Pick closest unit to the cursor in screen space within a radius
    const float pickRadius = 18.0f; // pixels
    float bestDist2 = pickRadius * pickRadius;
    Engine::Core::EntityID bestId = 0;
    auto ents = m_world->getEntitiesWith<Engine::Core::TransformComponent>();
    for (auto* e : ents) {
        if (!e->hasComponent<Engine::Core::UnitComponent>()) continue;
        auto* t = e->getComponent<Engine::Core::TransformComponent>();
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->ownerId != m_localOwnerId) continue; // only select friendlies
        QPointF sp;
        if (!worldToScreen(QVector3D(t->position.x, t->position.y, t->position.z), sp)) continue;
        float dx = float(sx) - float(sp.x());
        float dy = float(sy) - float(sp.y());
        float d2 = dx*dx + dy*dy;
        if (d2 < bestDist2) { bestDist2 = d2; bestId = e->getId(); }
    }
    if (bestId) {
        // If we clicked near a unit, this is a selection click. Optionally clear previous selection.
        if (!additive) m_selectionSystem->clearSelection();
        // Clicked near a unit: (re)select it
        m_selectionSystem->selectUnit(bestId);
        syncSelectionFlags();
        emit selectedUnitsChanged();
        if (m_selectedUnitsModel) QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");
        return;
    }

    // No unit under cursor. If we have a current selection, interpret this as a move command
    const auto& selected = m_selectionSystem->getSelectedUnits();
    if (!selected.empty()) {
        QVector3D hit;
        if (!screenToGround(QPointF(sx, sy), hit)) {
            // Could not project to ground; nothing to do
            return;
        }
        // Issue formation move similar to right-click
        const float spacing = 1.0f;
        int n = int(selected.size());
        int side = std::ceil(std::sqrt(float(n)));
        int i = 0;
        for (auto id : selected) {
            auto* e = m_world->getEntity(id);
            if (!e) continue;
            auto* mv = e->getComponent<Engine::Core::MovementComponent>();
            if (!mv) e->addComponent<Engine::Core::MovementComponent>(), mv = e->getComponent<Engine::Core::MovementComponent>();
            int gx = i % side;
            int gy = i / side;
            float ox = (gx - (side-1)*0.5f) * spacing;
            float oz = (gy - (side-1)*0.5f) * spacing;
            mv->targetX = hit.x() + ox;
            mv->targetY = hit.z() + oz;
            mv->hasTarget = true;
            ++i;
        }
        // Keep existing selection; just ensure selection rings remain synced
        syncSelectionFlags();
        return;
    }

    // Nothing selected and no unit clicked: clear any lingering visuals
    if (!additive) m_selectionSystem->clearSelection();
    syncSelectionFlags();
    emit selectedUnitsChanged();
    if (m_selectedUnitsModel) QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");
}

void GameEngine::onAreaSelected(qreal x1, qreal y1, qreal x2, qreal y2, bool additive) {
    if (!m_window || !m_selectionSystem) return;
    ensureInitialized();
    if (!additive) m_selectionSystem->clearSelection();
    float minX = std::min(float(x1), float(x2));
    float maxX = std::max(float(x1), float(x2));
    float minY = std::min(float(y1), float(y2));
    float maxY = std::max(float(y1), float(y2));
    auto ents = m_world->getEntitiesWith<Engine::Core::TransformComponent>();
    for (auto* e : ents) {
        if (!e->hasComponent<Engine::Core::UnitComponent>()) continue;
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->ownerId != m_localOwnerId) continue; // only area-select friendlies
        auto* t = e->getComponent<Engine::Core::TransformComponent>();
        QPointF sp;
        if (!worldToScreen(QVector3D(t->position.x, t->position.y, t->position.z), sp)) continue;
        if (sp.x() >= minX && sp.x() <= maxX && sp.y() >= minY && sp.y() <= maxY) {
            m_selectionSystem->selectUnit(e->getId());
        }
    }
    syncSelectionFlags();
    emit selectedUnitsChanged();
    if (m_selectedUnitsModel) QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");
}

void GameEngine::initialize() {
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx || !ctx->isValid()) {
        qWarning() << "GameEngine::initialize called without a current, valid OpenGL context";
        return;
    }
    // Create shared resources and inject to renderer before initialize
    m_resources = std::make_shared<Render::GL::ResourceManager>();
    m_renderer->setResources(m_resources);
    if (!m_renderer->initialize()) {
        qWarning() << "Failed to initialize renderer";
        return;
    }
    m_renderer->setCamera(m_camera.get());
    // Try load visuals JSON
    Game::Visuals::VisualCatalog visualCatalog;
    QString visualsErr;
    visualCatalog.loadFromJsonFile("assets/visuals/unit_visuals.json", &visualsErr);
    // Install unit factories
    auto unitReg = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::registerBuiltInUnits(*unitReg);
    Game::Map::MapTransformer::setFactoryRegistry(unitReg);
    // Try load map JSON
    Game::Map::MapDefinition def;
    QString mapPath = QString::fromUtf8("assets/maps/test_map.json");
    QString err;
    if (Game::Map::MapLoader::loadFromJsonFile(mapPath, def, &err)) {
        m_loadedMapName = def.name;
        // Delegate environment setup
        Game::Map::Environment::apply(def, *m_renderer, *m_camera);
        m_camFov = def.camera.fovY; m_camNear = def.camera.nearPlane; m_camFar = def.camera.farPlane;
        // Populate world via transformer (which uses factories)
        auto rt = Game::Map::MapTransformer::applyToWorld(def, *m_world, &visualCatalog);
        if (!rt.unitIds.empty()) {
            m_playerUnitId = rt.unitIds.front();
        } else {
            setupFallbackTestUnit();
        }
    } else {
        qWarning() << "Map load failed:" << err << "- using fallback unit";
        Game::Map::Environment::applyDefault(*m_renderer, *m_camera);
        m_camFov = m_camera->getFOV(); m_camNear = m_camera->getNear(); m_camFar = m_camera->getFar();
        setupFallbackTestUnit();
    }
    m_initialized = true;
}

void GameEngine::ensureInitialized() { if (!m_initialized) initialize(); }

void GameEngine::update(float dt) {
    // Apply pause and time scaling
    if (m_paused) {
        dt = 0.0f;
    } else {
        dt *= m_timeScale;
    }
    if (m_world) m_world->update(dt);
    // Prune selection of dead units and keep flags in sync
    syncSelectionFlags();
    // Update camera follow behavior after world update so positions are fresh
    if (m_followSelectionEnabled && m_camera && m_selectionSystem && m_world) {
        const auto& sel = m_selectionSystem->getSelectedUnits();
        if (!sel.empty()) {
            // Compute centroid of selected units
            QVector3D sum(0,0,0); int count = 0;
            for (auto id : sel) {
                if (auto* e = m_world->getEntity(id)) {
                    if (auto* t = e->getComponent<Engine::Core::TransformComponent>()) {
                        sum += QVector3D(t->position.x, t->position.y, t->position.z);
                        ++count;
                    }
                }
            }
            if (count > 0) {
                QVector3D center = sum / float(count);
                // Keep target in sync with centroid so orbit/yaw use the center
                m_camera->setTarget(center);
                m_camera->updateFollow(center);
            }
        }
    }

    // Keep SelectedUnitsModel in sync with health changes even if selection IDs haven't changed
    if (m_selectedUnitsModel) QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh", Qt::QueuedConnection);
}

void GameEngine::render(int pixelWidth, int pixelHeight) {
    if (!m_renderer || !m_world || !m_initialized) return;
    if (pixelWidth > 0 && pixelHeight > 0) {
        m_viewW = pixelWidth; m_viewH = pixelHeight;
        m_renderer->setViewport(pixelWidth, pixelHeight);
           float aspect = float(pixelWidth) / float(pixelHeight);
           // Keep current camera fov/planes from map but update aspect
           m_camera->setPerspective(m_camera->getFOV(), aspect, m_camera->getNear(), m_camera->getFar());
    }
    m_renderer->beginFrame();
    m_renderer->renderWorld(m_world.get());
    // Render arrows
    if (m_arrowSystem) {
        for (const auto& arrow : m_arrowSystem->arrows()) {
            if (!arrow.active) continue;
            const QVector3D delta = arrow.end - arrow.start;
            const float dist = std::max(0.001f, delta.length());
            // Parabolic arc: height = arcHeight * 4 * t * (1-t)
            QVector3D pos = arrow.start + delta * arrow.t;
            float h = arrow.arcHeight * 4.0f * arrow.t * (1.0f - arrow.t);
            pos.setY(pos.y() + h);
            // Build model (constant visual length; thicker shaft)
            QMatrix4x4 model;
            model.translate(pos.x(), pos.y(), pos.z());
            // Yaw around Y
            QVector3D dir = delta.normalized();
            float yawDeg = std::atan2(dir.x(), dir.z()) * 180.0f / 3.14159265f;
            model.rotate(yawDeg, QVector3D(0,1,0));
            // Pitch slightly down/up depending on arc to keep tip aligned
            float vy = (arrow.end.y() - arrow.start.y()) / dist;
            float pitchDeg = -std::atan2(vy - (8.0f * arrow.arcHeight * (arrow.t - 0.5f) / dist), 1.0f) * 180.0f / 3.14159265f;
            model.rotate(pitchDeg, QVector3D(1,0,0));
            const float zScale = 0.40f;     // even shorter constant arrow length
            const float xyScale = 0.26f;    // even thicker shaft for visibility
            // Center the arrow around its position: mesh runs 0..1 along +Z, so shift back by half length
            model.translate(0.0f, 0.0f, -zScale * 0.5f);
            model.scale(xyScale, xyScale, zScale);
            m_renderer->drawMeshColored(m_resources->arrow(), model, arrow.color);
        }
    }
    m_renderer->endFrame();
}

void GameEngine::setupFallbackTestUnit() {
    // Delegate fallback unit creation to the unit factory (archer)
    auto reg = Game::Map::MapTransformer::getFactoryRegistry();
    if (reg) {
        Game::Units::SpawnParams sp;
        sp.position = QVector3D(0.0f, 0.0f, 0.0f);
        sp.playerId = 0;
        sp.unitType = "archer";
        if (auto unit = reg->create("archer", *m_world, sp)) {
            m_playerUnitId = unit->id();
            return;
        }
    }
    // As a last resort, log and skip creating a unit to avoid mixing responsibilities here.
    qWarning() << "setupFallbackTestUnit: No unit factory available for 'archer'; skipping fallback spawn";
}

bool GameEngine::screenToGround(const QPointF& screenPt, QVector3D& outWorld) {
    if (!m_window || !m_camera) return false;
    return m_camera->screenToGround(float(screenPt.x()), float(screenPt.y()),
                                    float(m_window->width()), float(m_window->height()), outWorld);
}

bool GameEngine::worldToScreen(const QVector3D& world, QPointF& outScreen) const {
    if (!m_camera || m_viewW <= 0 || m_viewH <= 0) return false;
    return m_camera->worldToScreen(world, m_viewW, m_viewH, outScreen);
}

void GameEngine::clearAllSelections() {
    if (!m_world) return;
    auto ents = m_world->getEntitiesWith<Engine::Core::UnitComponent>();
    for (auto* e : ents) {
        if (auto* u = e->getComponent<Engine::Core::UnitComponent>()) u->selected = false;
    }
}

void GameEngine::syncSelectionFlags() {
    if (!m_world || !m_selectionSystem) return;
    clearAllSelections();
    const auto& sel = m_selectionSystem->getSelectedUnits();
    std::vector<Engine::Core::EntityID> toKeep;
    toKeep.reserve(sel.size());
    for (auto id : sel) {
        if (auto* e = m_world->getEntity(id)) {
            if (auto* u = e->getComponent<Engine::Core::UnitComponent>()) {
                if (u->health > 0) {
                    u->selected = true;
                    toKeep.push_back(id);
                }
            }
        }
    }
    // If any dead units were filtered out, rebuild selection
    if (toKeep.size() != sel.size()) {
        m_selectionSystem->clearSelection();
        for (auto id : toKeep) m_selectionSystem->selectUnit(id);
    }
}

// --- Camera control API (invokable from QML) ---
void GameEngine::cameraMove(float dx, float dz) {
    ensureInitialized();
    if (!m_camera) return;
    // Delegate to camera high-level pan
    m_camera->pan(dx, dz);
    // Manual control should take priority: keep follow enabled but update offset immediately
    if (m_followSelectionEnabled) m_camera->captureFollowOffset();
}

void GameEngine::cameraElevate(float dy) {
    ensureInitialized();
    if (!m_camera) return;
    // Elevate via camera API
    m_camera->elevate(dy);
    if (m_followSelectionEnabled) m_camera->captureFollowOffset();
}

void GameEngine::cameraYaw(float degrees) {
    ensureInitialized();
    if (!m_camera) return;
    // Rotate around target by yaw degrees
    m_camera->yaw(degrees);
    if (m_followSelectionEnabled) m_camera->captureFollowOffset();
}

void GameEngine::cameraOrbit(float yawDeg, float pitchDeg) {
    ensureInitialized();
    if (!m_camera) return;
    // Orbit around target via camera API
    m_camera->orbit(yawDeg, pitchDeg);
    if (m_followSelectionEnabled) m_camera->captureFollowOffset();
}

void GameEngine::cameraFollowSelection(bool enable) {
    ensureInitialized();
    m_followSelectionEnabled = enable;
    if (m_camera) m_camera->setFollowEnabled(enable);
    if (enable && m_camera && m_selectionSystem && m_world) {
        const auto& sel = m_selectionSystem->getSelectedUnits();
        if (!sel.empty()) {
            // compute immediate centroid and capture offset to maintain framing
            QVector3D sum(0,0,0); int count = 0;
            for (auto id : sel) {
                if (auto* e = m_world->getEntity(id)) {
                    if (auto* t = e->getComponent<Engine::Core::TransformComponent>()) {
                        sum += QVector3D(t->position.x, t->position.y, t->position.z);
                        ++count;
                    }
                }
            }
            if (count > 0) {
                QVector3D target = sum / float(count);
                // First set the new target, then capture offset relative to it
                m_camera->setTarget(target);
                // If no prior offset, capture one to keep current framing
                m_camera->captureFollowOffset();
            }
        }
    } else if (m_camera) {
        // Follow disabled: ensure camera vectors are stable and keep current look direction
        // No target change; just recompute basis to avoid any drift
        // (updateVectors is called inside camera lookAt/setTarget, so here we normalize front)
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
    m_camera->setFollowLerp(a);
}

QObject* GameEngine::selectedUnitsModel() { return m_selectedUnitsModel; }
