#include "game_engine.h"

#include <QQuickWindow>
#include <QOpenGLContext>
#include <QDebug>

#include "engine/core/world.h"
#include "engine/core/component.h"
#include "render/gl/renderer.h"
#include "render/gl/camera.h"
#include "render/gl/resources.h"
#include "game/systems/movement_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/selection_system.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/visuals/visual_catalog.h"

GameEngine::GameEngine() {
    m_world    = std::make_unique<Engine::Core::World>();
    m_renderer = std::make_unique<Render::GL::Renderer>();
    m_camera   = std::make_unique<Render::GL::Camera>();

    m_world->addSystem(std::make_unique<Game::Systems::MovementSystem>());
    m_world->addSystem(std::make_unique<Game::Systems::CombatSystem>());
    m_world->addSystem(std::make_unique<Game::Systems::AISystem>());

    m_selectionSystem = std::make_unique<Game::Systems::SelectionSystem>();
    m_world->addSystem(std::make_unique<Game::Systems::SelectionSystem>());

    // Defer actual entity creation until initialize() when GL and renderer are ready
}

GameEngine::~GameEngine() = default;

void GameEngine::onMapClicked(qreal sx, qreal sy) {
    if (!m_window) return;
    ensureInitialized();
    QVector3D hit;
    if (!screenToGround(QPointF(sx, sy), hit)) return;
    if (auto* entity = m_world->getEntity(m_playerUnitId)) {
        if (auto* move = entity->getComponent<Engine::Core::MovementComponent>()) {
            move->targetX = hit.x();
            move->targetY = hit.z();
            move->hasTarget = true;
            // Set selected to true to visualize with ring
            if (auto* unit = entity->getComponent<Engine::Core::UnitComponent>()) {
                unit->selected = true;
            }
        }
    }
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
    // Try load map JSON
    Game::Map::MapDefinition def;
    QString mapPath = QString::fromUtf8("assets/maps/test_map.json");
    QString err;
    if (Game::Map::MapLoader::loadFromJsonFile(mapPath, def, &err)) {
        m_loadedMapName = def.name;
    // Configure camera
    m_camera->setRTSView(def.camera.center, def.camera.distance, def.camera.tiltDeg);
    // Cache perspective params and set initial perspective (aspect will be set in render)
    m_camFov = def.camera.fovY; m_camNear = def.camera.nearPlane; m_camFar = def.camera.farPlane;
    m_camera->setPerspective(m_camFov, 16.0f/9.0f, m_camNear, m_camFar);

        // Configure grid on renderer
        Render::GL::Renderer::GridParams gp;
        gp.cellSize = def.grid.tileSize;
        gp.extent = std::max(def.grid.width, def.grid.height) * def.grid.tileSize * 0.5f; // half-size plane scale
        m_renderer->setGridParams(gp);

        // Populate world
        auto rt = Game::Map::MapTransformer::applyToWorld(def, *m_world, &visualCatalog);
        if (!rt.unitIds.empty()) {
            m_playerUnitId = rt.unitIds.front();
        } else {
            setupFallbackTestUnit();
        }
    } else {
        qWarning() << "Map load failed:" << err << "- using fallback unit";
    m_camera->setRTSView(QVector3D(0, 0, 0), 15.0f, 45.0f);
    m_camFov = 45.0f; m_camNear = 0.1f; m_camFar = 1000.0f;
    m_camera->setPerspective(m_camFov, 16.0f/9.0f, m_camNear, m_camFar);
        setupFallbackTestUnit();
    }
    m_initialized = true;
}

void GameEngine::ensureInitialized() { if (!m_initialized) initialize(); }

void GameEngine::update(float dt) {
    if (m_world) m_world->update(dt);
}

void GameEngine::render(int pixelWidth, int pixelHeight) {
    if (!m_renderer || !m_world || !m_initialized) return;
    if (pixelWidth > 0 && pixelHeight > 0) {
        m_renderer->setViewport(pixelWidth, pixelHeight);
           float aspect = float(pixelWidth) / float(pixelHeight);
           // Keep current camera fov/planes from map but update aspect
           m_camera->setPerspective(m_camera->getFOV(), aspect, m_camera->getNear(), m_camera->getFar());
    }
    m_renderer->beginFrame();
    m_renderer->renderWorld(m_world.get());
    m_renderer->endFrame();
}

void GameEngine::setupFallbackTestUnit() {
    auto entity = m_world->createEntity();
    m_playerUnitId = entity->getId();

    auto transform = entity->addComponent<Engine::Core::TransformComponent>();
    transform->position = {0.0f, 0.0f, 0.0f};
    transform->scale    = {0.5f, 0.5f, 0.5f};
    // Keep upright; camera provides the tilt

    auto renderable = entity->addComponent<Engine::Core::RenderableComponent>("", "");
    renderable->visible = true;
    renderable->mesh = Engine::Core::RenderableComponent::MeshKind::Capsule;
    renderable->color[0] = 0.8f; renderable->color[1] = 0.9f; renderable->color[2] = 1.0f;

    auto unit = entity->addComponent<Engine::Core::UnitComponent>();
    unit->unitType = "archer";
    unit->health = 80;
    unit->maxHealth = 80;
    unit->speed = 3.0f;

    entity->addComponent<Engine::Core::MovementComponent>();
}

bool GameEngine::screenToGround(const QPointF& screenPt, QVector3D& outWorld) {
    if (!m_window || !m_camera) return false;
    float w = float(m_window->width());
    float h = float(m_window->height());
    if (w <= 0 || h <= 0) return false;

    float x = (2.0f * float(screenPt.x()) / w) - 1.0f;
    float y = 1.0f - (2.0f * float(screenPt.y()) / h);

    bool ok = false;
    QMatrix4x4 invVP = (m_camera->getProjectionMatrix() * m_camera->getViewMatrix()).inverted(&ok);
    if (!ok) return false;

    QVector4D nearClip(x, y, 0.0f, 1.0f);
    QVector4D farClip (x, y, 1.0f, 1.0f);
    QVector4D nearWorld4 = invVP * nearClip;
    QVector4D farWorld4  = invVP * farClip;
    if (nearWorld4.w() == 0.0f || farWorld4.w() == 0.0f) return false;
    QVector3D rayOrigin = (nearWorld4 / nearWorld4.w()).toVector3D();
    QVector3D rayEnd    = (farWorld4  / farWorld4.w()).toVector3D();
    QVector3D rayDir    = (rayEnd - rayOrigin).normalized();

    if (qFuzzyIsNull(rayDir.y())) return false;
    float t = -rayOrigin.y() / rayDir.y();
    if (t < 0.0f) return false;
    outWorld = rayOrigin + rayDir * t;
    return true;
}
