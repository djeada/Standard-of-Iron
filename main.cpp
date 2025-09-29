#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QDebug>
#include <QDir>
#include <QQuickWindow>
#include <QQmlContext>
#include <QSGRendererInterface>

#include "engine/core/world.h"
#include "engine/core/component.h"
#include "render/gl/renderer.h"
#include "render/gl/camera.h"
#include "game/systems/movement_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/selection_system.h"
// #include "game/systems/ai_system.h" // keep if you have AISystem; remove if not

class GameEngine : public QObject {
    Q_OBJECT
public:
    GameEngine() {
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

    Q_INVOKABLE void onMapClicked(qreal sx, qreal sy) {
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

    void setWindow(QQuickWindow* w) { m_window = w; }

    void initialize() {
        // Must be called with a current, valid GL context.
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
        qDebug() << "Game engine initialized successfully";
    }

    void ensureInitialized() { if (!m_initialized) initialize(); }

    void update(float dt) {
        if (m_world) m_world->update(dt);
    }

    void render() {
        if (!m_renderer || !m_world || !m_initialized) return;

        if (m_window) {
            const int vpW = int(m_window->width()  * m_window->effectiveDevicePixelRatio());
            const int vpH = int(m_window->height() * m_window->effectiveDevicePixelRatio());
            m_renderer->setViewport(vpW, vpH);
        }

        m_renderer->beginFrame();
        m_renderer->renderWorld(m_world.get());
        m_renderer->endFrame();
    }

private:
    void setupTestScene() {
        auto entity = m_world->createEntity();
        m_playerUnitId = entity->getId();

        auto transform = entity->addComponent<Engine::Core::TransformComponent>();
        transform->position = {0.0f, 0.0f, 0.0f};
        transform->scale    = {0.5f, 0.5f, 0.5f};
        transform->rotation.x = -90.0f; // lay quad on XZ plane

        auto renderable = entity->addComponent<Engine::Core::RenderableComponent>("", "");
        renderable->visible = true;

        auto unit = entity->addComponent<Engine::Core::UnitComponent>();
        unit->unitType = "archer";
        unit->health = 80;
        unit->maxHealth = 80;
        unit->speed = 3.0f;

        entity->addComponent<Engine::Core::MovementComponent>();

        qDebug() << "Test scene created with 1 archer (entity" << m_playerUnitId << ")";
    }

    bool screenToGround(const QPointF& screenPt, QVector3D& outWorld) {
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

    std::unique_ptr<Engine::Core::World> m_world;
    std::unique_ptr<Render::GL::Renderer> m_renderer;
    std::unique_ptr<Render::GL::Camera>   m_camera;
    std::unique_ptr<Game::Systems::SelectionSystem> m_selectionSystem;
    QQuickWindow* m_window = nullptr;
    Engine::Core::EntityID m_playerUnitId = 0;
    bool m_initialized = false;
};

int main(int argc, char *argv[])
{
    // Force desktop OpenGL + GLX path BEFORE any window is created.
    if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY") && qEnvironmentVariableIsSet("DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "xcb");   // prefer XCB/GLX over Wayland/EGL when XWayland is present
    }
    qputenv("QT_OPENGL", "desktop");         // desktop GL, not GLES/EGL
    qputenv("QSG_RHI_BACKEND", "opengl");    // OpenGL RHI
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGLRhi);

    // Request desktop GL 3.3 core.
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSamples(0);
    QSurfaceFormat::setDefaultFormat(fmt);

    QGuiApplication app(argc, argv);

    auto gameEngine = new GameEngine();

    QQmlApplicationEngine engine;
    // Expose to QML BEFORE loading (safer if QML binds early).
    engine.rootContext()->setContextProperty("game", gameEngine);
    engine.load(QUrl(QStringLiteral("qrc:/StandardOfIron/ui/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qWarning() << "Failed to load QML file";
        return -1;
    }

    QObject* rootObj = engine.rootObjects().first();
    QQuickWindow* window = qobject_cast<QQuickWindow*>(rootObj);
    if (!window) window = rootObj->findChild<QQuickWindow*>();
    if (!window) {
        qWarning() << "No QQuickWindow found for OpenGL initialization.";
        return -2;
    }

    // For now, draw GL ON TOP of QML (simplest to verify).
    window->setColor(Qt::black);

    gameEngine->setWindow(window);

    // Informative logging (no current-context check here).
    QObject::connect(window, &QQuickWindow::sceneGraphInitialized, window, [window]() {
        if (auto *ri = window->rendererInterface()) {
            auto api = ri->graphicsApi();
            const char* name = api == QSGRendererInterface::OpenGLRhi     ? "OpenGLRhi"  :
                               api == QSGRendererInterface::VulkanRhi      ? "VulkanRhi"  :
                               api == QSGRendererInterface::Direct3D11Rhi  ? "D3D11Rhi"   :
                               api == QSGRendererInterface::MetalRhi       ? "MetalRhi"   :
                               api == QSGRendererInterface::Software       ? "Software"   : "Unknown";
            qInfo() << "QSG graphicsApi:" << name;
        }
    });

    QObject::connect(window, &QQuickWindow::sceneGraphError, &app,
                     [&](QQuickWindow::SceneGraphError, const QString &msg){
        qCritical() << "Failed to initialize OpenGL scene graph:" << msg;
        app.exit(3);
    });

    // Draw AFTER Qt Quick so GL is visible on top.
    QObject::connect(window, &QQuickWindow::afterRendering, gameEngine, [gameEngine, window]() {
        auto *ri = window->rendererInterface();
        const bool isGL = ri && ri->graphicsApi() == QSGRendererInterface::OpenGLRhi;
        if (!isGL) return;

        window->beginExternalCommands(); // makes the GL context current for external GL
        QOpenGLContext* ctx = QOpenGLContext::currentContext();
        if (!ctx || !ctx->isValid()) { window->endExternalCommands(); return; }

        gameEngine->ensureInitialized();
        gameEngine->update(1.0f / 60.0f);
        gameEngine->render();

        window->endExternalCommands();
    }, Qt::DirectConnection);

    qDebug() << "Application started successfully";
    qDebug() << "Assets directory:" << QDir::currentPath() + "/assets";

    return app.exec();
}

#include "main.moc"

