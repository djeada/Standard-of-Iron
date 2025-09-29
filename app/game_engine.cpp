#include "game_engine.h"

#include <QQuickWindow>
#include <QOpenGLContext>
#include <QDebug>

#include "engine/core/world.h"
#include "engine/core/component.h"
#include "render/gl/renderer.h"
#include "render/gl/camera.h"
#include "game/systems/movement_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/selection_system.h"

GameEngine::GameEngine() {
    m_world    = std::make_unique<Engine::Core::World>();
    m_renderer = std::make_unique<Render::GL::Renderer>();
    m_camera   = std::make_unique<Render::GL::Camera>();

    m_world->addSystem(std::make_unique<Game::Systems::MovementSystem>());
    m_world->addSystem(std::make_unique<Game::Systems::CombatSystem>());
    m_world->addSystem(std::make_unique<Game::Systems::AISystem>());

    m_selectionSystem = std::make_unique<Game::Systems::SelectionSystem>();
    m_world->addSystem(std::make_unique<Game::Systems::SelectionSystem>());

    setupTestScene();
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
        }
    }
}

void GameEngine::initialize() {
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx || !ctx->isValid()) {
        qWarning() << "GameEngine::initialize called without a current, valid OpenGL context";
        return;
    }
    if (!m_renderer->initialize()) {
        qWarning() << "Failed to initialize renderer";
        return;
    }
    m_renderer->setCamera(m_camera.get());
    m_camera->setRTSView(QVector3D(0, 0, 0), 15.0f, 45.0f);
    m_camera->setPerspective(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
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
    }
    m_renderer->beginFrame();
    m_renderer->renderWorld(m_world.get());
    m_renderer->endFrame();
}

void GameEngine::setupTestScene() {
    auto entity = m_world->createEntity();
    m_playerUnitId = entity->getId();

    auto transform = entity->addComponent<Engine::Core::TransformComponent>();
    transform->position = {0.0f, 0.0f, 0.0f};
    transform->scale    = {0.5f, 0.5f, 0.5f};
    transform->rotation.x = -90.0f;

    auto renderable = entity->addComponent<Engine::Core::RenderableComponent>("", "");
    renderable->visible = true;

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
