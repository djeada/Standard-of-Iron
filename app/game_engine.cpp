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
#include "game/systems/production_system.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/visuals/visual_catalog.h"
#include "game/units/factory.h"
#include "game/units/unit.h"
#include "game/map/environment.h"

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

void GameEngine::setHoverAtScreen(qreal sx, qreal sy) {
    if (!m_window) return;
    ensureInitialized();
    // Negative coords are used by QML to signal hover exit
    if (sx < 0 || sy < 0) {
        if (m_hoveredBuildingId != 0) { m_hoveredBuildingId = 0; }
        return;
    }
    auto prevHover = m_hoveredBuildingId;
    m_hoveredBuildingId = 0;

    // Helper: project a base rectangle (XZ) to screen and return bounds
    auto projectBounds = [&](const QVector3D& center, float hx, float hz, QRectF& out) -> bool {
        QPointF p1, p2, p3, p4;
        bool ok1 = worldToScreen(QVector3D(center.x() - hx, center.y() + 0.0f, center.z() - hz), p1);
        bool ok2 = worldToScreen(QVector3D(center.x() + hx, center.y() + 0.0f, center.z() - hz), p2);
        bool ok3 = worldToScreen(QVector3D(center.x() + hx, center.y() + 0.0f, center.z() + hz), p3);
        bool ok4 = worldToScreen(QVector3D(center.x() - hx, center.y() + 0.0f, center.z() + hz), p4);
        if (!(ok1 && ok2 && ok3 && ok4)) return false;
        float minX = std::min(std::min(float(p1.x()), float(p2.x())), std::min(float(p3.x()), float(p4.x())));
        float maxX = std::max(std::max(float(p1.x()), float(p2.x())), std::max(float(p3.x()), float(p4.x())));
        float minY = std::min(std::min(float(p1.y()), float(p2.y())), std::min(float(p3.y()), float(p4.y())));
        float maxY = std::max(std::max(float(p1.y()), float(p2.y())), std::max(float(p3.y()), float(p4.y())));
        out = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
        return true;
    };

    // Hysteresis: keep current hover if mouse stays within an expanded screen-space bounds
    if (prevHover) {
        if (auto* e = m_world->getEntity(prevHover)) {
            if (e->hasComponent<Engine::Core::BuildingComponent>()) {
                if (auto* t = e->getComponent<Engine::Core::TransformComponent>()) {
                    // If production UI is active (inProgress), be extra forgiving
                    float pxPad = 12.0f;
                    if (auto* prod = e->getComponent<Engine::Core::ProductionComponent>()) {
                        if (prod->inProgress) pxPad = 24.0f;
                    }
                    const float marginXZ_keep = 1.10f;
                    const float pad_keep  = 1.12f;
                    float hxk = std::max(0.4f, t->scale.x * marginXZ_keep * pad_keep);
                    float hzk = std::max(0.4f, t->scale.z * marginXZ_keep * pad_keep);
                    QRectF bounds;
                    if (projectBounds(QVector3D(t->position.x, t->position.y, t->position.z), hxk, hzk, bounds)) {
                        bounds.adjust(-pxPad, -pxPad, pxPad, pxPad);
                        if (bounds.contains(QPointF(sx, sy))) {
                            m_hoveredBuildingId = prevHover;
                            m_hoverGraceTicks = 6;
                            return;
                        }
                    }
                }
            }
        }
    }

    float bestD2 = std::numeric_limits<float>::max();
    auto ents = m_world->getEntitiesWith<Engine::Core::TransformComponent>();
    for (auto* e : ents) {
        if (!e->hasComponent<Engine::Core::UnitComponent>()) continue;
        if (!e->hasComponent<Engine::Core::BuildingComponent>()) continue;
        auto* t = e->getComponent<Engine::Core::TransformComponent>();
        // Screen-space bounds of the building base with modest padding
        const float marginXZ = 1.10f;
        const float hoverPad  = 1.06f;
        float hx = std::max(0.4f, t->scale.x * marginXZ * hoverPad);
        float hz = std::max(0.4f, t->scale.z * marginXZ * hoverPad);
        QRectF bounds;
        if (!projectBounds(QVector3D(t->position.x, t->position.y, t->position.z), hx, hz, bounds)) continue;
        if (!bounds.contains(QPointF(sx, sy))) continue;
        // Break ties by closeness to projected center
        QPointF centerSp;
        if (!worldToScreen(QVector3D(t->position.x, t->position.y, t->position.z), centerSp)) centerSp = bounds.center();
        float dx = float(sx) - float(centerSp.x());
        float dy = float(sy) - float(centerSp.y());
        float d2 = dx*dx + dy*dy;
        if (d2 < bestD2) { bestD2 = d2; m_hoveredBuildingId = e->getId(); }
    }

    // If we acquired (or re-acquired) a hover, extend grace a bit to ride through transient changes
    if (m_hoveredBuildingId != 0 && m_hoveredBuildingId != prevHover) {
        m_hoverGraceTicks = 6;
    }

    // Hysteresis: if we had a previous hover, allow it to persist with a slightly larger screen-space pad
    if (m_hoveredBuildingId == 0 && prevHover != 0) {
        if (auto* e = m_world->getEntity(prevHover)) {
            auto* t = e->getComponent<Engine::Core::TransformComponent>();
            if (t && e->getComponent<Engine::Core::BuildingComponent>()) {
                // Grow keep pad slightly more if production is in progress
                const float marginXZ = 1.12f; // tiny extra pad
                const float keepPad  = (e->getComponent<Engine::Core::ProductionComponent>() && e->getComponent<Engine::Core::ProductionComponent>()->inProgress) ? 1.16f : 1.12f;
                float hx = std::max(0.4f, t->scale.x * marginXZ * keepPad);
                float hz = std::max(0.4f, t->scale.z * marginXZ * keepPad);
                QRectF bounds;
                if (projectBounds(QVector3D(t->position.x, t->position.y, t->position.z), hx, hz, bounds)) {
                    if (bounds.contains(QPointF(sx, sy))) {
                        m_hoveredBuildingId = prevHover;
                    }
                }
            }
        }
    }

    // Grace period: avoid rapid clear/re-acquire near edges
    if (m_hoveredBuildingId == 0 && prevHover != 0 && m_hoverGraceTicks > 0) {
        m_hoveredBuildingId = prevHover;
    }
}

void GameEngine::onClickSelect(qreal sx, qreal sy, bool additive) {
    if (!m_window || !m_selectionSystem) return;
    ensureInitialized();
    // Pick closest unit to the cursor in screen space within a radius
    const float baseUnitPickRadius = 18.0f;       // pixels
    const float baseBuildingPickRadius = 28.0f;   // base px, used as fallback when projection fails
    float bestUnitDist2 = std::numeric_limits<float>::max();
    float bestBuildingDist2 = std::numeric_limits<float>::max();
    Engine::Core::EntityID bestUnitId = 0;
    Engine::Core::EntityID bestBuildingId = 0;
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
        if (e->hasComponent<Engine::Core::BuildingComponent>()) {
            // Prefer accurate hit test using projected 3D bounding box (covers roof/body)
            bool hit = false;
            float pickDist2 = d2; // default to center distance for tie-break
            // Generous half-extents to cover composed mesh (foundation/roof overhangs)
            const float marginXZ = 1.6f;
            const float marginY  = 1.2f;
            float hx = std::max(0.6f, t->scale.x * marginXZ);
            float hz = std::max(0.6f, t->scale.z * marginXZ);
            float hy = std::max(0.5f, t->scale.y * marginY);
            QVector<QPointF> pts;
            pts.reserve(8);
            auto project = [&](const QVector3D& w){ QPointF sp; if (worldToScreen(w, sp)) { pts.push_back(sp); return true; } return false; };
            bool ok =
                project(QVector3D(t->position.x - hx, t->position.y + 0.0f, t->position.z - hz)) &&
                project(QVector3D(t->position.x + hx, t->position.y + 0.0f, t->position.z - hz)) &&
                project(QVector3D(t->position.x + hx, t->position.y + 0.0f, t->position.z + hz)) &&
                project(QVector3D(t->position.x - hx, t->position.y + 0.0f, t->position.z + hz)) &&
                project(QVector3D(t->position.x - hx, t->position.y + hy,   t->position.z - hz)) &&
                project(QVector3D(t->position.x + hx, t->position.y + hy,   t->position.z - hz)) &&
                project(QVector3D(t->position.x + hx, t->position.y + hy,   t->position.z + hz)) &&
                project(QVector3D(t->position.x - hx, t->position.y + hy,   t->position.z + hz));
            if (ok && pts.size() == 8) {
                float minX = pts[0].x(), maxX = pts[0].x();
                float minY = pts[0].y(), maxY = pts[0].y();
                for (const auto& p2 : pts) { minX = std::min(minX, float(p2.x())); maxX = std::max(maxX, float(p2.x())); minY = std::min(minY, float(p2.y())); maxY = std::max(maxY, float(p2.y())); }
                if (float(sx) >= minX && float(sx) <= maxX && float(sy) >= minY && float(sy) <= maxY) {
                    hit = true;
                    // Use distance to center for tie-break when overlapping multiple buildings
                    pickDist2 = d2;
                }
            }
            if (!hit) {
                // Fallback to a scaled circular radius if projection failed
                float scaleXZ = std::max(std::max(t->scale.x, t->scale.z), 1.0f);
                float rp = baseBuildingPickRadius * scaleXZ;
                float r2 = rp * rp;
                if (d2 <= r2) hit = true;
            }
            if (hit && pickDist2 < bestBuildingDist2) { bestBuildingDist2 = pickDist2; bestBuildingId = e->getId(); }
        } else {
            float r2 = baseUnitPickRadius * baseUnitPickRadius;
            if (d2 <= r2 && d2 < bestUnitDist2) { bestUnitDist2 = d2; bestUnitId = e->getId(); }
        }
    }
    // Decide selection target by closest entity under cursor within radius
    if (bestBuildingId && (!bestUnitId || bestBuildingDist2 <= bestUnitDist2)) {
        if (!additive) m_selectionSystem->clearSelection();
        m_selectionSystem->selectUnit(bestBuildingId);
        syncSelectionFlags();
        emit selectedUnitsChanged();
        if (m_selectedUnitsModel) QMetaObject::invokeMethod(m_selectedUnitsModel, "refresh");
        return;
    }
    if (bestUnitId) {
        // If we clicked near a unit, this is a selection click. Optionally clear previous selection.
        if (!additive) m_selectionSystem->clearSelection();
        // Clicked near a unit: (re)select it
        m_selectionSystem->selectUnit(bestUnitId);
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
        // Exclude buildings from rectangle selection
        if (e->hasComponent<Engine::Core::BuildingComponent>()) continue;
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
        // Spawn a starting barracks for the local player near origin if none present
        bool hasBarracks = false;
        for (auto* e : m_world->getEntitiesWith<Engine::Core::UnitComponent>()) {
            if (auto* u = e->getComponent<Engine::Core::UnitComponent>()) {
                if (u->unitType == "barracks" && u->ownerId == m_localOwnerId) { hasBarracks = true; break; }
            }
        }
        if (!hasBarracks) {
            auto reg2 = Game::Map::MapTransformer::getFactoryRegistry();
            if (reg2) {
                Game::Units::SpawnParams sp;
                sp.position = QVector3D(-4.0f, 0.0f, -3.0f);
                sp.playerId = m_localOwnerId;
                sp.unitType = "barracks";
                reg2->create("barracks", *m_world, sp);
            }
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
        // Decay hover grace window
        if (m_hoverGraceTicks > 0) --m_hoverGraceTicks;
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
    // Provide hovered id for subtle outline
    if (m_renderer) m_renderer->setHoveredBuildingId(m_hoveredBuildingId);
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
    // Prefer the active GL viewport size; fall back to window size if not set yet
    float w = (m_viewW > 0 ? float(m_viewW) : float(m_window->width()));
    float h = (m_viewH > 0 ? float(m_viewH) : float(m_window->height()));
    return m_camera->screenToGround(float(screenPt.x()), float(screenPt.y()), w, h, outWorld);
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
    // Find first selected barracks of local player
    for (auto id : sel) {
        if (auto* e = m_world->getEntity(id)) {
            auto* u = e->getComponent<Engine::Core::UnitComponent>();
            auto* t = e->getComponent<Engine::Core::TransformComponent>();
            auto* p = e->getComponent<Engine::Core::ProductionComponent>();
            if (!u || !t) continue;
            if (u->unitType == "barracks" && u->ownerId == m_localOwnerId) {
                if (!p) { p = e->addComponent<Engine::Core::ProductionComponent>(); }
                if (!p) return;
                if (p->producedCount >= p->maxUnits) return; // cap reached
                if (p->inProgress) return; // already building
                // Start a new production order
                p->productType = unitType.toStdString();
                // Build time could vary by unit type in future; keep current
                p->timeRemaining = p->buildTime;
                p->inProgress = true;
                return;
            }
        }
    }
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
    const auto& sel = m_selectionSystem->getSelectedUnits();
    for (auto id : sel) {
        if (auto* e = m_world->getEntity(id)) {
            if (auto* u = e->getComponent<Engine::Core::UnitComponent>()) {
                if (u->unitType == "barracks") {
                    m["hasBarracks"] = true;
                    if (auto* p = e->getComponent<Engine::Core::ProductionComponent>()) {
                        m["inProgress"] = p->inProgress;
                        m["timeRemaining"] = p->timeRemaining;
                        m["buildTime"] = p->buildTime;
                        m["producedCount"] = p->producedCount;
                        m["maxUnits"] = p->maxUnits;
                    }
                    break;
                }
            }
        }
    }
    return m;
}

void GameEngine::setRallyAtScreen(qreal sx, qreal sy) {
    ensureInitialized();
    if (!m_world || !m_selectionSystem) return;
    QVector3D hit;
    if (!screenToGround(QPointF(sx, sy), hit)) return;
    const auto& sel = m_selectionSystem->getSelectedUnits();
    for (auto id : sel) {
        if (auto* e = m_world->getEntity(id)) {
            if (auto* u = e->getComponent<Engine::Core::UnitComponent>()) {
                if (u->unitType == "barracks") {
                    if (auto* p = e->getComponent<Engine::Core::ProductionComponent>()) {
                        p->rallyX = hit.x();
                        p->rallyZ = hit.z();
                        p->rallySet = true;
                        return;
                    }
                }
            }
        }
    }
}
